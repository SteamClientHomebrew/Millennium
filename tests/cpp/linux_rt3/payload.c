#include <stdio.h>

static int starts;
static int stops;

int StartMillennium(void)
{
    ++starts;
    puts("MILLENNIUM_TEST_STARTED");
    return 0;
}

int StopMillennium(void)
{
    ++stops;
    puts("MILLENNIUM_TEST_STOPPED");
    return 0;
}

__attribute__((destructor)) static void summarize(void)
{
    printf("MILLENNIUM_TEST_SUMMARY starts=%d stops=%d\n", starts, stops);
}
