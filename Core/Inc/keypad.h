#ifndef KEYPAD_H
#define KEYPAD_H

#include "main.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    KEYPAD_EVENT_PRESSED,
    KEYPAD_EVENT_HELD,
    KEYPAD_EVENT_RELEASED
} KeypadEventType;

typedef struct
{
    KeypadEventType type;
    char key;
} KeypadEvent;

HAL_StatusTypeDef Keypad_Init(TIM_HandleTypeDef *timer);
void Keypad_OnTimerElapsed(TIM_HandleTypeDef *timer);
bool Keypad_GetEvent(KeypadEvent *event);
uint32_t Keypad_GetDroppedEventCount(void);

#endif
