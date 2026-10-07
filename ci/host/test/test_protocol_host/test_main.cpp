#include "tests/protocol_tests.h"
#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    lobbsRunProtocolTests();
    return UNITY_END();
}
