#include "keypad.h"
#include <stddef.h>

#define DEBOUNCE_SAMPLES  3U
#define HOLD_TICKS        80U
#define EVENT_QUEUE_SIZE  8U

static GPIO_TypeDef *const colPort[4] = {GPIOA, GPIOB, GPIOB, GPIOB};
static const uint16_t colPin[4] = {
    GPIO_PIN_8, GPIO_PIN_13, GPIO_PIN_14, GPIO_PIN_15
};

static GPIO_TypeDef *const rowPort[4] = {GPIOA, GPIOA, GPIOA, GPIOA};
static const uint16_t rowPin[4] = {
    GPIO_PIN_12, GPIO_PIN_11, GPIO_PIN_10, GPIO_PIN_9
};

static const char keymap[4][4] = {
    {'*', '0', '#', 'A'},
    {'7', '8', '9', 'B'},
    {'4', '5', '6', 'C'},
    {'1', '2', '3', 'D'}
};

static TIM_HandleTypeDef *keypadTimer;

static volatile KeypadEvent eventQueue[EVENT_QUEUE_SIZE];
static volatile uint8_t queueHead;
static volatile uint8_t queueTail;
static volatile uint32_t droppedEvents;

static char candidateKey;
static uint8_t candidateCount;
static char stableKey;
static uint8_t holdTicks;
static bool holdEventSent;

static void Keypad_QueueEvent(KeypadEventType type, char key)
{
    uint8_t next = (uint8_t)((queueHead + 1U) % EVENT_QUEUE_SIZE);

    if (next == queueTail)
    {
        droppedEvents++;
        return;
    }

    eventQueue[queueHead].type = type;
    eventQueue[queueHead].key = key;
    queueHead = next;
}

static char Keypad_ReadRaw(void)
{
    for (uint8_t c = 0; c < 4U; c++)
    {
        for (uint8_t i = 0; i < 4U; i++)
        {
            HAL_GPIO_WritePin(colPort[i], colPin[i], GPIO_PIN_SET);
        }

        HAL_GPIO_WritePin(colPort[c], colPin[c], GPIO_PIN_RESET);

        for (uint8_t r = 0; r < 4U; r++)
        {
            if (HAL_GPIO_ReadPin(rowPort[r], rowPin[r]) == GPIO_PIN_RESET)
            {
                HAL_GPIO_WritePin(colPort[c], colPin[c], GPIO_PIN_SET);
                return keymap[r][c];
            }
        }
    }

    return '\0';
}

static void Keypad_ProcessScan(void)
{
    char rawKey = Keypad_ReadRaw();

    if (rawKey == candidateKey)
    {
        if (candidateCount < DEBOUNCE_SAMPLES)
        {
            candidateCount++;
        }
    }
    else
    {
        candidateKey = rawKey;
        candidateCount = 1U;
    }

    if ((candidateCount >= DEBOUNCE_SAMPLES) &&
        (candidateKey != stableKey))
    {
        if (stableKey != '\0')
        {
            Keypad_QueueEvent(KEYPAD_EVENT_RELEASED, stableKey);
        }

        stableKey = candidateKey;
        holdTicks = 0U;
        holdEventSent = false;

        if (stableKey != '\0')
        {
            Keypad_QueueEvent(KEYPAD_EVENT_PRESSED, stableKey);
        }
    }

    if ((stableKey != '\0') && !holdEventSent)
    {
        if (holdTicks < HOLD_TICKS)
        {
            holdTicks++;
        }

        if (holdTicks >= HOLD_TICKS)
        {
            Keypad_QueueEvent(KEYPAD_EVENT_HELD, stableKey);
            holdEventSent = true;
        }
    }
}

HAL_StatusTypeDef Keypad_Init(TIM_HandleTypeDef *timer)
{
    if (timer == NULL)
    {
        return HAL_ERROR;
    }

    keypadTimer = timer;
    queueHead = 0U;
    queueTail = 0U;
    droppedEvents = 0U;
    candidateKey = '\0';
    candidateCount = 0U;
    stableKey = '\0';
    holdTicks = 0U;
    holdEventSent = false;

    HAL_StatusTypeDef status = HAL_TIM_Base_Start_IT(keypadTimer);
    if (status != HAL_OK)
    {
        keypadTimer = NULL;
    }

    return status;
}

bool Keypad_GetEvent(KeypadEvent *event)
{
    if ((event == NULL) || (queueTail == queueHead))
    {
        return false;
    }

    event->type = eventQueue[queueTail].type;
    event->key = eventQueue[queueTail].key;
    queueTail = (uint8_t)((queueTail + 1U) % EVENT_QUEUE_SIZE);

    return true;
}

uint32_t Keypad_GetDroppedEventCount(void)
{
    return droppedEvents;
}

void Keypad_OnTimerElapsed(TIM_HandleTypeDef *timer)
{
    if (timer == keypadTimer)
    {
        Keypad_ProcessScan();
    }
}
