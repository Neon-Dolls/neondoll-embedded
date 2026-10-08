#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "neondoll.hpp"

extern "C" void app_main(void)
{
    printf("Hello neondoll!\n");
    neondoll_init();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        printf("neondoll tick\n");
    }
}
