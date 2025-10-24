#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include "config.h"
#include "globals.h"

// void IRAM_ATTR onFingerTouch() {
//   fingerTouched = true;
// }
void IRAM_ATTR onFingerTouch() {
    static unsigned long lastTrigger = 0;
    unsigned long now = millis();
    if (now - lastTrigger > 500) {  // 500ms debounce
        fingerTouched = true;
        lastTrigger = now;
    }
}

void IRAM_ATTR onButtonPress() {
  if (!buttonBeingHandled) buttonPressedFlag = true;
}

#endif