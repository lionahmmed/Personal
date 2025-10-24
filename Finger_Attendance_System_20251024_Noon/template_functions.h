#ifndef TEMPLATE_FUNCTIONS_H
#define TEMPLATE_FUNCTIONS_H

#include "config.h"
#include "globals.h"
#include "utility_functions.h"

const uint32_t MODULE_ADDRESS = 0xFFFFFFFFUL;

const uint8_t PID_COMMAND = 0x01;
const uint8_t PID_DATA = 0x02;
const uint8_t PID_ACK = 0x07;
const uint8_t PID_END = 0x08;

const uint8_t INS_UPCHAR = 0x08;
const uint8_t INS_DOWNCHAR = 0x09;

const uint32_t SERIAL_READ_TIMEOUT_MS = 1200;
const uint8_t MAX_RETRY = 3;

// Fingerprint standard command codes
#define CMD_GENIMG 0x01
#define CMD_IMAGE2TZ 0x02
#define CMD_REGMODEL 0x05
#define CMD_STORE 0x06
#define CMD_SEARCH 0x04
#define CMD_DELETE 0x0C
#define CMD_EMPTY 0x0D
#define CMD_LOADCHAR 0x07
#define CMD_TEMPLATECOUNT 0x1D

// Forward declaration for external function
extern String getNameByID(int id);

// ---------------- Protocol Helper Functions ----------------
static void writeUint32BigEndian(Stream &s, uint32_t v) {
  s.write((uint8_t)((v >> 24) & 0xFF));
  s.write((uint8_t)((v >> 16) & 0xFF));
  s.write((uint8_t)((v >> 8) & 0xFF));
  s.write((uint8_t)(v & 0xFF));
}

static void writeUint16BigEndian(Stream &s, uint16_t v) {
  s.write((uint8_t)((v >> 8) & 0xFF));
  s.write((uint8_t)(v & 0xFF));
}

static int readBytesWithTimeout(Stream &s, uint8_t *buf, size_t len, uint32_t timeoutMs) {
  uint32_t start = millis();
  size_t pos = 0;
  while (pos < len && (millis() - start) < timeoutMs) {
    if (s.available()) {
      int ch = s.read();
      if (ch >= 0) buf[pos++] = (uint8_t)ch;
    } else {
      delay(1);
    }
  }
  return (int)pos;
}

static void flushSerialInput(Stream &s, uint32_t timeoutMs = 50) {
  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    while (s.available()) s.read();
    delay(1);
  }
}

static int readPacket(Stream &s, uint8_t *contentBuf, size_t maxContent, size_t *contentLen, uint32_t timeoutMs = SERIAL_READ_TIMEOUT_MS) {
  uint8_t hdr[9];
  if (readBytesWithTimeout(s, hdr, 9, timeoutMs) != 9) return -1;
  if (hdr[0] != 0xEF || hdr[1] != 0x01) return -1;
  uint8_t pid = hdr[6];
  uint16_t lenField = ((uint16_t)hdr[7] << 8) | hdr[8];
  if (lenField < 2) return -1;
  uint16_t cLen = lenField - 2;
  if (cLen > maxContent) return -1;
  if (readBytesWithTimeout(s, contentBuf, cLen + 2, timeoutMs) != (int)(cLen + 2)) return -1;
  *contentLen = cLen;
  return pid;
}

static void sendCommandPacket(Stream &s, uint8_t instruction, const uint8_t *params = nullptr, uint16_t paramsLen = 0) {
  uint16_t packetContentLen = 1 + paramsLen;
  uint16_t lengthField = packetContentLen + 2;

  s.write(0xEF);
  s.write(0x01);
  writeUint32BigEndian(s, MODULE_ADDRESS);
  s.write(PID_COMMAND);
  writeUint16BigEndian(s, lengthField);

  s.write(instruction);
  if (params && paramsLen) s.write(params, paramsLen);

  uint32_t sum = PID_COMMAND + ((lengthField >> 8) & 0xFF) + (lengthField & 0xFF);
  sum += instruction;
  for (uint16_t i = 0; i < paramsLen; i++) sum += params[i];
  writeUint16BigEndian(s, (uint16_t)sum);
}

static int readAck(Stream &s, uint8_t *contentBuf = nullptr, size_t maxContent = 0, size_t *outContentLen = nullptr, uint32_t timeoutMs = SERIAL_READ_TIMEOUT_MS) {
  uint8_t localBuf[520];
  size_t cLen = 0;
  int pid = readPacket(s, localBuf, sizeof(localBuf), &cLen, timeoutMs);

  if (pid < 0) {
    Serial.printf("readAck: Timeout after %lu ms (no packet)\n", timeoutMs);
    return -1;
  }

  if (pid != PID_ACK) {
    Serial.printf("readAck: Expected PID_ACK(0x%02X), got 0x%02X\n", PID_ACK, pid);
    return -1;
  }

  if (cLen < 1) {
    Serial.println("readAck: Packet too short");
    return -1;
  }

  uint8_t conf = localBuf[0];

  if (conf == 0x00) {
    Serial.println("readAck: Success (0x00)");
  } else {
    Serial.printf("readAck: Confirmation code 0x%02X\n", conf);
  }

  if (contentBuf && outContentLen) {
    size_t toCopy = min((size_t)cLen, maxContent);
    memcpy(contentBuf, localBuf, toCopy);
    *outContentLen = toCopy;
  }

  return (int)conf;
}

static int sendCmdAndGetAck(uint8_t cmd, const uint8_t *params = nullptr, uint16_t len = 0, uint32_t timeoutMs = SERIAL_READ_TIMEOUT_MS) {
  flushSerialInput(mySerial, 10);
  sendCommandPacket(mySerial, cmd, params, len);
  return readAck(mySerial, nullptr, 0, nullptr, timeoutMs);
}

// ---------------- Core Template Functions ----------------
bool uploadTemplateFromModule(uint8_t pageID, const char *filename) {
  uint8_t params[1] = { 0x01 };
  
  for (uint8_t attempt = 0; attempt < MAX_RETRY; ++attempt) {
    flushSerialInput(mySerial, 10);
    sendCommandPacket(mySerial, INS_UPCHAR, params, 1);

    int conf = readAck(mySerial, nullptr, 0, nullptr, SERIAL_READ_TIMEOUT_MS);
    if (conf < 0) {
      Serial.println("No initial ACK for UpChar");
      continue;
    }
    if (conf != 0x00) {
      Serial.printf("UpChar ACK error: 0x%02X\n", conf);
      delay(150);
      continue;
    }

    File f = SD.open(filename, FILE_WRITE);
    if (!f) {
      Serial.printf("SD open write failed: %s\n", filename);
      return false;
    }

    bool finished = false;
    uint32_t start = millis();
    uint32_t totalReceived = 0;

    while (!finished && (millis() - start) < 15000) {
      uint8_t content[520];
      size_t contentLen = 0;
      int pid = readPacket(mySerial, content, sizeof(content), &contentLen, SERIAL_READ_TIMEOUT_MS);

      if (pid < 0) {
        f.close();
        Serial.println("Timeout reading data packet");
        return false;
      }

      if (pid == PID_DATA || pid == PID_END) {
        if (contentLen > 0) {
          f.write(content, contentLen);
          totalReceived += contentLen;
        }
        
        if (pid == PID_END) {
          finished = true;
          break;
        }
      } else if (pid == PID_ACK) {
        finished = true;
        break;
      } else {
        f.close();
        Serial.printf("Unexpected PID during UpChar: 0x%02X\n", pid);
        return false;
      }
    }
    f.close();

    if (finished) {
      Serial.printf("✅ Template exported: %s (%lu bytes)\n", filename, totalReceived);
      return true;
    }
  }
  return false;
}

bool downloadTemplateToModuleWithVerify(uint8_t charBufferID, const char *filename) {
  File f = SD.open(filename, FILE_READ);
  if (!f) {
    Serial.println("Failed to open template file");
    return false;
  }
  
  uint32_t totalSize = f.size();
  
  // Check if file size is reasonable
  if (totalSize == 0 || totalSize > 2048) {
    Serial.printf("Invalid template size: %lu bytes\n", totalSize);
    f.close();
    return false;
  }
  
  uint8_t *templateData = (uint8_t *)malloc(totalSize);
  if (!templateData) {
    Serial.println("Memory allocation failed");
    f.close();
    return false;
  }
  
  if (f.read(templateData, totalSize) != totalSize) {
    Serial.println("Failed to read template data");
    free(templateData);
    f.close();
    return false;
  }
  f.close();

  for (uint8_t attempt = 0; attempt < MAX_RETRY; ++attempt) {
    flushSerialInput(mySerial, 100);
    
    uint8_t params[1] = { charBufferID };
    sendCommandPacket(mySerial, INS_DOWNCHAR, params, 1);

    int conf = readAck(mySerial, nullptr, 0, nullptr, 3000);
    if (conf != 0x00) {
      Serial.printf("DownChar command failed: 0x%02X, attempt %d\n", conf, attempt + 1);
      delay(500);
      continue;
    }

    const size_t CHUNK_SIZE = 128;
    uint32_t sent = 0;

    while (sent < totalSize) {
      size_t chunkSize = min((size_t)(totalSize - sent), CHUNK_SIZE);
      bool isLast = (sent + chunkSize >= totalSize);
      uint8_t pid = isLast ? PID_END : PID_DATA;
      uint16_t lengthField = chunkSize + 2;
      
      mySerial.write(0xEF);
      mySerial.write(0x01);
      writeUint32BigEndian(mySerial, MODULE_ADDRESS);
      mySerial.write(pid);
      writeUint16BigEndian(mySerial, lengthField);
      mySerial.write(templateData + sent, chunkSize);
      
      uint32_t sum = pid + ((lengthField >> 8) & 0xFF) + (lengthField & 0xFF);
      for (size_t i = 0; i < chunkSize; i++) sum += templateData[sent + i];
      writeUint16BigEndian(mySerial, (uint16_t)sum);
      
      sent += chunkSize;
      delay(50);
    }

    delay(300);
    free(templateData);
    return true;
  }

  free(templateData);
  return false;
}

bool validateTemplate(const char *filename) {
  File f = SD.open(filename, FILE_READ);
  if (!f) {
    Serial.printf("Cannot open: %s\n", filename);
    return false;
  }

  uint32_t fileSize = f.size();
  uint8_t header[4];
  f.read(header, 4);
  f.close();

  if (header[0] == 0xEF && header[1] == 0x01) {
    Serial.println("❌ Old format detected - re-export needed");
    return false;
  }

  if (fileSize < 100 || fileSize > 1024) {
    Serial.printf("❌ Invalid size: %lu bytes\n", fileSize);
    return false;
  }

  Serial.printf("✅ Valid template: %lu bytes\n", fileSize);
  return true;
}

bool captureFinger() {
  int r = sendCmdAndGetAck(CMD_GENIMG);
  if (r == 0x00) return true;
  Serial.printf("GenImg failed: 0x%02X\n", r);
  return false;
}

bool convertToTemplate(uint8_t bufferID) {
  uint8_t p[1] = { bufferID };
  int r = sendCmdAndGetAck(CMD_IMAGE2TZ, p, 1);
  if (r == 0x00) return true;
  Serial.printf("Image2TZ failed: 0x%02X\n", r);
  return false;
}

bool createModel() {
  int r = sendCmdAndGetAck(CMD_REGMODEL);
  if (r == 0x00) return true;
  Serial.printf("RegModel failed: 0x%02X\n", r);
  return false;
}

bool storeModel(uint8_t bufferID, uint16_t pageID) {
  uint8_t p[3] = { bufferID, (uint8_t)(pageID >> 8), (uint8_t)(pageID & 0xFF) };
  int r = sendCmdAndGetAck(CMD_STORE, p, 3);
  if (r == 0x00) return true;
  Serial.printf("Store failed: 0x%02X\n", r);
  return false;
}

bool deleteTemplate(uint16_t pageID, uint16_t count = 1) {
  uint8_t p[4] = { (uint8_t)(pageID >> 8), (uint8_t)(pageID & 0xFF),
                   (uint8_t)(count >> 8), (uint8_t)(count & 0xFF) };
  int r = sendCmdAndGetAck(CMD_DELETE, p, 4);
  if (r == 0x00) return true;
  Serial.printf("Delete failed: 0x%02X\n", r);
  return false;
}

bool clearDatabase() {
  int r = sendCmdAndGetAck(CMD_EMPTY);
  if (r == 0x00) return true;
  Serial.printf("Clear DB failed: 0x%02X\n", r);
  return false;
}

int getTemplateCount() {
  flushSerialInput(mySerial, 10);
  sendCommandPacket(mySerial, CMD_TEMPLATECOUNT, nullptr, 0);
  uint8_t buf[64];
  size_t gotLen = 0;
  int conf = readAck(mySerial, buf, sizeof(buf), &gotLen, SERIAL_READ_TIMEOUT_MS);
  if (conf < 0) {
    Serial.println("Failed to get template count");
    return -1;
  }
  if (gotLen >= 3) {
    uint16_t count = (uint16_t)buf[1] << 8 | (uint16_t)buf[2];
    Serial.printf("Templates stored: %u\n", count);
    return (int)count;
  }
  Serial.println("Unexpected template count response");
  return -1;
}

bool searchFinger(uint8_t bufferID, uint16_t &foundPage, uint16_t &score) {
  uint8_t params[6] = { bufferID, 0x00, 0x00, 0x01, 0x2C, 0x00 };
  flushSerialInput(mySerial, 10);
  sendCommandPacket(mySerial, CMD_SEARCH, params, 6);
  uint8_t resp[16];
  size_t got = 0;
  int conf = readAck(mySerial, resp, sizeof(resp), &got, SERIAL_READ_TIMEOUT_MS);
  if (conf < 0) {
    Serial.println("Search read timeout");
    return false;
  }
  if (conf == 0x00 && got >= 5) {
    foundPage = ((uint16_t)resp[1] << 8) | resp[2];
    score = ((uint16_t)resp[3] << 8) | resp[4];
    return true;
  }
  return false;
}

void sendEndPacket() {
  uint16_t lf = 2;
  mySerial.write(0xEF);
  mySerial.write(0x01);
  writeUint32BigEndian(mySerial, MODULE_ADDRESS);
  mySerial.write(PID_END);
  writeUint16BigEndian(mySerial, lf);
  uint16_t chk = PID_END + ((lf >> 8) & 0xFF) + (lf & 0xFF);
  writeUint16BigEndian(mySerial, chk);
}

bool checkForAck(uint32_t timeoutMs) {
  Serial.println("Checking for ACK...");
  int ack = readAck(mySerial, nullptr, 0, nullptr, timeoutMs);
  if (ack == 0x00) {
    Serial.println("Got success ACK!");
    return true;
  } else if (ack >= 0) {
    Serial.printf("Got ACK with code: 0x%02X\n", ack);
  } else {
    Serial.println("No ACK received");
  }
  return false;
}

int findNextAvailableTemplateID() {
  for (int id = 1; id <= 300; id++) {
    uint8_t params[3] = { 0x01, (uint8_t)((id >> 8) & 0xFF), (uint8_t)(id & 0xFF) };
    flushSerialInput(mySerial, 10);
    sendCommandPacket(mySerial, CMD_LOADCHAR, params, 3);
    int conf = readAck(mySerial, nullptr, 0, nullptr, SERIAL_READ_TIMEOUT_MS);
    if (conf != 0x00) {
      return id;
    }
  }
  return -1;
}

String generateFingerprintID(int id) {
  DateTime now = rtc.now();
  String uniqueId = "FPR" + 
                   device_id.substring(device_id.length() - 3) +
                   String(now.year()) + 
                   String(now.month()) + 
                   String(now.day()) + 
                   String(now.hour()) + 
                   String(now.minute()) + 
                   String(id);
  
  Serial.printf("Generated finger_id for ID %d: %s\n", id, uniqueId.c_str());
  return uniqueId;
}

bool saveTemplateToSD(const String &filename, const uint8_t *data, uint16_t length) {
  File file = SD.open(filename, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing: " + filename);
    return false;
  }
  
  size_t bytesWritten = file.write(data, length);
  file.close();
  
  if (bytesWritten == length) {
    Serial.printf("Template saved: %s (%d bytes)\n", filename.c_str(), bytesWritten);
    return true;
  } else {
    Serial.printf("Failed to save template. Written: %d, Expected: %d\n", bytesWritten, length);
    return false;
  }
}

bool saveTemplateToSD(const String &filename, const String &data) {
  File file = SD.open(filename, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing: " + filename);
    return false;
  }
  if (file.print(data) == data.length()) {
    file.close();
    return true;
  }
  file.close();
  return false;
}

bool appendTemplateToSD(const String &filename, const String &data) {
  File file = SD.open(filename, FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open file for appending: " + filename);
    return false;
  }
  if (file.print(data) == data.length()) {
    file.close();
    return true;
  }
  file.close();
  return false;
}

bool exportRealFingerprintTemplate(int id) {
  Serial.printf("Exporting template for ID: %d using raw protocol\n", id);
  
  if (!SD.exists("/templates")) {
    SD.mkdir("/templates");
  }
  
  char filename[32];
  sprintf(filename, "/templates/fp_%03d.bin", id);
  
  uint8_t loadParams[3] = { 0x01, (uint8_t)((id >> 8) & 0xFF), (uint8_t)(id & 0xFF) };
  flushSerialInput(mySerial, 10);
  sendCommandPacket(mySerial, CMD_LOADCHAR, loadParams, 3);
  int conf = readAck(mySerial, nullptr, 0, nullptr, SERIAL_READ_TIMEOUT_MS);
  if (conf != 0x00) {
    Serial.printf("LoadChar failed for ID %d: 0x%02X\n", id, conf);
    return false;
  }
  
  if (uploadTemplateFromModule(id, filename)) {
    Serial.printf("✅ Template exported: ID %d -> %s\n", id, filename);
    
    String metaFilename = "/templates/fp_" + String(id) + ".dat";
    String metadata = "R307_RAW_TEMPLATE\n";
    metadata += "ID:" + String(id) + "\n";
    metadata += "NAME:" + getNameByID(id) + "\n";
    metadata += "DEVICE:" + device_id + "\n";
    metadata += "TIMESTAMP:" + String(rtc.now().unixtime()) + "\n";
    
    File sizeCheck = SD.open(filename, FILE_READ);
    if (sizeCheck) {
      metadata += "SIZE:" + String(sizeCheck.size()) + "\n";
      sizeCheck.close();
    }
    
    metadata += "FORMAT:RAW_R307_BINARY\n";
    metadata += "PROTOCOL:UPCHAR_0x08\n";
    
    if (saveTemplateToSD(metaFilename, metadata)) {
      Serial.printf("Metadata saved: %s\n", metaFilename.c_str());
    }
    
    return true;
  }
  
  return false;
}

bool importRealFingerprintTemplate(int id, const String &filename) {
  Serial.printf("Importing template to ID: %d from: %s\n", id, filename.c_str());
  
  String fullPath = "/templates/" + filename;
  if (!SD.exists(fullPath)) {
    Serial.println("Template file not found: " + fullPath);
    return false;
  }

  if (!validateTemplate(fullPath.c_str())) {
    Serial.println("Template validation failed");
    return false;
  }

  if (downloadTemplateToModuleWithVerify(1, fullPath.c_str())) {
    if (storeModel(1, (uint16_t)id)) {
      Serial.printf("✅ Template imported successfully: %s -> ID %d\n", filename.c_str(), id);
      
      String logEntry = "IMPORT_SUCCESS: FILE=" + filename + 
                       ", TARGET_ID=" + String(id) +
                       ", TIME=" + String(rtc.now().unixtime()) + 
                       ", DEVICE=" + device_id + "\n";
      appendTemplateToSD("/templates/import_log.txt", logEntry);
      
      return true;
    } else {
      Serial.println("❌ Failed to store template to database");
    }
  } else {
    Serial.println("❌ Failed to download template to module");
  }
  
  return false;
}

bool exportFingerprintTemplateAsDat(int id) {
  return exportRealFingerprintTemplate(id);
}

bool importFingerprintTemplateFromDat(int id, const String &filename) {
  return importRealFingerprintTemplate(id, filename);
}

bool exportFingerprintTemplate(int id) {
  return exportRealFingerprintTemplate(id);
}

bool importFingerprintTemplate(int id, const String &templateData) {
  Serial.printf("String-based import not supported in raw protocol for ID: %d\n", id);
  return false;
}

void exportAllTemplates() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Exporting Templates...");
  display.println("Format: Raw Binary");
  display.display();

  if (!SD.exists("/templates")) {
    SD.mkdir("/templates");
  }

  int exportedCount = 0;
  int totalCount = getTemplateCount();
  
  if (totalCount <= 0) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("No templates found");
    display.display();
    delay(2000);
    return;
  }

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Exporting Templates...");
  display.setCursor(0, 16);
  display.printf("Total: %d", totalCount);
  display.display();
  
  for (int id = 1; id <= 300; id++) {
    uint8_t params[3] = { 0x01, (uint8_t)((id >> 8) & 0xFF), (uint8_t)(id & 0xFF) };
    flushSerialInput(mySerial, 10);
    sendCommandPacket(mySerial, CMD_LOADCHAR, params, 3);
    int conf = readAck(mySerial, nullptr, 0, nullptr, SERIAL_READ_TIMEOUT_MS);
    
    if (conf == 0x00) {
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("Exporting...");
      display.setCursor(0, 16);
      display.printf("ID: %d", id);
      display.setCursor(0, 32);
      display.printf("Progress: %d/%d", exportedCount + 1, totalCount);
      display.display();
      
      if (exportRealFingerprintTemplate(id)) {
        exportedCount++;
      }
      delay(500);
    }
  }

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Export Complete");
  display.setCursor(0, 16);
  display.printf("Exported: %d/%d", exportedCount, totalCount);
  display.setCursor(0, 32);
  display.println("Check /templates/");
  display.println("on SD card");
  display.display();
  
  buzzerSuccess();
  delay(3000);
}

void importTemplateFromFile() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Template Import");
  display.setCursor(0, 16);
  display.println("Select .bin file");
  display.setCursor(0, 40);
  display.println("Press to continue");
  display.display();

  unsigned long startTime = millis();
  while (millis() - startTime < 10000) {
    if (digitalRead(BUTTON_PIN) == LOW) {
      delay(50);
      if (digitalRead(BUTTON_PIN) == LOW) {
        File root = SD.open("/templates");
        String binFile = "";
        
        while (true) {
          File entry = root.openNextFile();
          if (!entry) break;
          String fileName = String(entry.name());
          if (!entry.isDirectory() && fileName.endsWith(".bin")) {
            binFile = fileName;
            break;
          }
          entry.close();
        }
        root.close();
        
        if (binFile.length() > 0) {
          int newId = findNextAvailableTemplateID();
          if (newId != -1) {
            display.clearDisplay();
            display.setCursor(0, 0);
            display.println("Importing...");
            display.setCursor(0, 16);
            display.printf("File: %s", binFile.c_str());
            display.setCursor(0, 32);
            display.printf("To ID: %d", newId);
            display.display();
            
            if (importRealFingerprintTemplate(newId, binFile)) {
              successMessage("Imported: " + binFile);
            } else {
              failMessage("Import failed");
            }
          } else {
            failMessage("No space");
          }
        } else {
          failMessage("No .bin files");
        }
        return;
      }
    }
    delay(100);
  }
  
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Import cancelled");
  display.display();
  delay(2000);
}

void listTemplateFiles() {
  Serial.println("\n=== Available Template Files ===");
  
  if (!SD.exists("/templates")) {
    Serial.println("No templates directory found");
    return;
  }
  
  File root = SD.open("/templates");
  if (!root) {
    Serial.println("Failed to open templates directory");
    return;
  }
  
  int datCount = 0;
  int binCount = 0;
  int totalSize = 0;
  
  while (true) {
    File entry = root.openNextFile();
    if (!entry) break;
    
    String fileName = String(entry.name());
    if (!entry.isDirectory()) {
      if (fileName.endsWith(".dat")) {
        Serial.printf("  DAT: %s (%d bytes)\n", entry.name(), entry.size());
        datCount++;
      } else if (fileName.endsWith(".bin")) {
        Serial.printf("  BIN: %s (%d bytes)\n", entry.name(), entry.size());
        binCount++;
        totalSize += entry.size();
      }
    }
    entry.close();
  }
  root.close();
  
  Serial.printf("Total: %d .dat files, %d .bin files\n", datCount, binCount);
  Serial.printf("Total binary size: %d bytes\n", totalSize);
  Serial.println("Format: Raw R307 binary (UpChar/DownChar protocol)");
  Serial.println("================================\n");
}

void showTemplateTransferHelp() {
  Serial.println("\n=== R307 Raw Template Transfer ===");
  Serial.println("Using direct R307 protocol commands:");
  Serial.println("• UPCHAR (0x08) - Export templates from module");
  Serial.println("• DOWNCHAR (0x09) - Import templates to module");
  Serial.println("• LOADCHAR (0x07) - Load template to buffer");
  Serial.println("• STORE (0x06) - Save template to database");
  Serial.println("• Template format: Raw binary (512-768 bytes)");
  Serial.println("• Files: .bin (raw data) + .dat (metadata)");
  Serial.println("===================================\n");
}

// ============================================
// COMPLETE FINGERPRINT TEMPLATE SYNC SOLUTION
// Add these functions to template_functions.h
// ============================================

// Convert binary template to Base64 for transmission
String templateToBase64(const uint8_t* data, size_t length) {
  const char* base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  String result = "";
  
  int i = 0;
  uint8_t array_3[3];
  uint8_t array_4[4];
  
  while (length--) {
    array_3[i++] = *(data++);
    if (i == 3) {
      array_4[0] = (array_3[0] & 0xfc) >> 2;
      array_4[1] = ((array_3[0] & 0x03) << 4) + ((array_3[1] & 0xf0) >> 4);
      array_4[2] = ((array_3[1] & 0x0f) << 2) + ((array_3[2] & 0xc0) >> 6);
      array_4[3] = array_3[2] & 0x3f;
      
      for(i = 0; i < 4; i++)
        result += base64_chars[array_4[i]];
      i = 0;
    }
  }
  
  if (i) {
    for(int j = i; j < 3; j++)
      array_3[j] = '\0';
      
    array_4[0] = (array_3[0] & 0xfc) >> 2;
    array_4[1] = ((array_3[0] & 0x03) << 4) + ((array_3[1] & 0xf0) >> 4);
    array_4[2] = ((array_3[1] & 0x0f) << 2) + ((array_3[2] & 0xc0) >> 6);
    
    for (int j = 0; j < i + 1; j++)
      result += base64_chars[array_4[j]];
      
    while(i++ < 3)
      result += '=';
  }
  
  return result;
}

// Read template from SD card and convert to Base64
String readTemplateAsBase64(const String &filename) {
  File file = SD.open(filename, FILE_READ);
  if (!file) {
    Serial.println("Failed to open template file: " + filename);
    return "";
  }
  
  size_t fileSize = file.size();
  if (fileSize == 0 || fileSize > 2048) {
    Serial.printf("Invalid template size: %d bytes\n", fileSize);
    file.close();
    return "";
  }
  
  uint8_t* buffer = (uint8_t*)malloc(fileSize);
  if (!buffer) {
    Serial.println("Memory allocation failed");
    file.close();
    return "";
  }
  
  size_t bytesRead = file.read(buffer, fileSize);
  file.close();
  
  if (bytesRead != fileSize) {
    Serial.println("Failed to read complete template");
    free(buffer);
    return "";
  }
  
  String base64Data = templateToBase64(buffer, fileSize);
  free(buffer);
  
  Serial.printf("Template converted to Base64: %d bytes -> %d chars\n", 
                fileSize, base64Data.length());
  
  return base64Data;
}

// Enhanced function to capture and save template with full data
bool captureAndSaveTemplateWithData(int id, const String &name) {
  Serial.printf("Capturing template for ID %d with full data\n", id);
  
  // Ensure templates directory exists
  if (!SD.exists("/templates")) {
    SD.mkdir("/templates");
  }
  
  // Generate template filename
  char templateFile[40];
  sprintf(templateFile, "/templates/fp_%03d.bin", id);
  
  // Load the fingerprint template into buffer 1
  uint8_t loadParams[3] = { 0x01, (uint8_t)((id >> 8) & 0xFF), (uint8_t)(id & 0xFF) };
  flushSerialInput(mySerial, 10);
  sendCommandPacket(mySerial, CMD_LOADCHAR, loadParams, 3);
  int conf = readAck(mySerial, nullptr, 0, nullptr, SERIAL_READ_TIMEOUT_MS);
  
  if (conf != 0x00) {
    Serial.printf("Failed to load template for ID %d: 0x%02X\n", id, conf);
    return false;
  }
  
  // Upload (export) the template from sensor to SD card
  if (!uploadTemplateFromModule(id, templateFile)) {
    Serial.println("Failed to export template to SD card");
    return false;
  }
  
  // Verify the exported file
  if (!SD.exists(templateFile)) {
    Serial.println("Template file not created");
    return false;
  }
  
  File checkFile = SD.open(templateFile, FILE_READ);
  size_t templateSize = checkFile.size();
  checkFile.close();
  
  if (templateSize < 100 || templateSize > 2048) {
    Serial.printf("Invalid template size: %d bytes\n", templateSize);
    return false;
  }
  
  Serial.printf("✅ Template saved: %s (%d bytes)\n", templateFile, templateSize);
  
  // Generate metadata
  DateTime now = rtc.now();
  String timestamp = String(now.year()) + "-" + 
                    (now.month() < 10 ? "0" : "") + String(now.month()) + "-" + 
                    (now.day() < 10 ? "0" : "") + String(now.day()) + "T" + 
                    (now.hour() < 10 ? "0" : "") + String(now.hour()) + ":" + 
                    (now.minute() < 10 ? "0" : "") + String(now.minute()) + ":" + 
                    (now.second() < 10 ? "0" : "") + String(now.second()) + "Z";
  
  // Generate unique finger_id
  String finger_id = generateFingerprintID(id);
  
  // Save to pending sync file with template path
  String record = String(id) + "," + name + "," + timestamp + "," + 
                  finger_id + "," + String(templateFile);
  
  File pendingFile = SD.open(PENDING_FINGERPRINTS_FILE, FILE_APPEND);
  if (!pendingFile) {
    Serial.println("Failed to save to pending queue");
    return false;
  }
  
  pendingFile.println(record);
  pendingFile.close();
  
  Serial.println("✅ Template queued for server sync");
  return true;
}

#endif