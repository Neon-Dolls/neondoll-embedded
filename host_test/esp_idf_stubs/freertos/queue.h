#pragma once
#include "FreeRTOS.h"
struct FakeQueue;
using QueueHandle_t = FakeQueue*;
QueueHandle_t xQueueCreate(UBaseType_t, UBaseType_t);
BaseType_t xQueueSend(QueueHandle_t, const void*, TickType_t);
BaseType_t xQueueReceive(QueueHandle_t, void*, TickType_t);
BaseType_t xQueueReset(QueueHandle_t);
void vQueueDelete(QueueHandle_t);
