#include "emulator.h"
#include "unity.h"

void setUp() {}

void tearDown(void) {}

void test_adc_imm(void) {}

void test_adc_abs(void) {}

void test_adc_abslong(void) {}

void test_adc_dp(void) {}

void test_adc_dp_indirect(void) {}

void test_adc_dp_indirect_long(void) {}

void test_adc_abs_i_x(void) {}

void test_adc_abs_long_i_x(void) {}

void test_adc_abs_i_y(void) {}

void test_adc_dp_i_x(void) {}

void test_adc_dp_i_indirect_x(void) {}

void test_adc_dp_indirect_i_y(void) {}

void test_adc_dp_indirect_long_i_y(void) {}

void test_adc_stack_rel(void) {}

void test_adc_stack_rel_indirect_i_y(void) {}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_adc_imm);
    RUN_TEST(test_adc_abs);
    RUN_TEST(test_adc_abslong);
    RUN_TEST(test_adc_dp);
    RUN_TEST(test_adc_dp_indirect);
    RUN_TEST(test_adc_dp_indirect_long);
    RUN_TEST(test_adc_abs_i_x);
    RUN_TEST(test_adc_abs_long_i_x);
    RUN_TEST(test_adc_abs_i_y);
    RUN_TEST(test_adc_dp_i_x);
    RUN_TEST(test_adc_dp_i_indirect_x);
    RUN_TEST(test_adc_dp_indirect_i_y);
    RUN_TEST(test_adc_dp_indirect_long_i_y);
    RUN_TEST(test_adc_stack_rel);
    RUN_TEST(test_adc_stack_rel_indirect_i_y);

    return UNITY_END();
}
