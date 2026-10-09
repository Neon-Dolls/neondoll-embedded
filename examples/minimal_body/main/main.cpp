#include <neondoll/platform.hpp>
#include <stdio.h>

void app_main(void)
{
    printf("Calling neondoll_init...\n");
    neondoll_init();
    printf("neondoll_init returned.\n");

    printf("Calling neondoll_deinit...\n");
    neondoll_deinit();
    printf("neondoll_deinit returned.\n");
}