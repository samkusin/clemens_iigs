#include "munit/munit.h"

int value = 0xffff;

void test1(void) {
  TEST_ASSERT_EQUAL_HEX16(value, 0xffff);
}

void main(void) {

}
