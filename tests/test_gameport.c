#include "emulator.h"
#include "unity.h"

#include "clem_device.h"
#include "clem_mmio_defs.h"

#include <stdio.h>

//  This test just checks the 'ADB' component for gameport functionality (i.e.
//  not the CPU side)
//
//  Setup an 'ADB' machine that calls the sync(), input() and read/write register
//  methods only.
//
//  Call read or write functions to trigger I/O logic
//  Run sync() for an amount of time
//  Check results
//

static struct ClemensDeviceADB adb_device;
static clem_clocks_time_t emulator_ref_ts;

static void gameport_sync(clem_clocks_duration_t delta_clocks) {
    struct ClemensClock clocks;
    clocks.ts = emulator_ref_ts;
    clocks.ref_step = CLEM_CLOCKS_MEGA2_CYCLE;
    clem_gameport_sync(&adb_device.gameport, &clocks);
    emulator_ref_ts += delta_clocks;
}

static uint32_t _test_util_get_paddle_time_ns(unsigned index) {
    return adb_device.gameport.paddle_timer_ns[index];
}

static uint32_t _test_util_get_reference_paddle_time_ns(unsigned index) {
    return CLEM_GAMEPORT_CALCULATE_TIME_NS(&adb_device, index);
}

void setUp(void) {
    emulator_ref_ts = 0;
    memset(&adb_device, 0, sizeof(adb_device));
    clem_adb_reset(&adb_device);
}

void tearDown(void) {}

void test_clem_gameport_reset(void) {
    uint32_t paddle_time_ns;

    //  no paddle input, timer should be set to zero meaning no input
    clem_adb_read_switch(&adb_device, CLEM_MMIO_REG_PTRIG, 0);
    TEST_ASSERT_BIT_HIGH(7, adb_device.gameport.paddle_timer_state[0]);
    TEST_ASSERT_BIT_HIGH(7, adb_device.gameport.paddle_timer_state[1]);
    TEST_ASSERT_BIT_HIGH(7, adb_device.gameport.paddle_timer_state[2]);
    TEST_ASSERT_BIT_HIGH(7, adb_device.gameport.paddle_timer_state[3]);
    paddle_time_ns = _test_util_get_paddle_time_ns(0);
    TEST_ASSERT_EQUAL_UINT32(0, paddle_time_ns);
    paddle_time_ns = _test_util_get_paddle_time_ns(1);
    TEST_ASSERT_EQUAL_UINT32(0, paddle_time_ns);
    paddle_time_ns = _test_util_get_paddle_time_ns(2);
    TEST_ASSERT_EQUAL_UINT32(0, paddle_time_ns);
    paddle_time_ns = _test_util_get_paddle_time_ns(3);
    TEST_ASSERT_EQUAL_UINT32(0, paddle_time_ns);
}

static void button_util(uint8_t mask, uint8_t expected, uint8_t reg) {}

void test_clem_gameport_buttons_01(void) {
    struct ClemensInputEvent input;
    unsigned mask;
    uint8_t result;
    char msg[8];

    for (mask = 0; mask <= 0xff; ++mask) {
        snprintf(msg, sizeof(msg), "mask=%02x", mask);
        input.type = kClemensInputType_Paddle;
        input.value_a = 0;
        input.value_b = 0;
        input.gameport_button_mask = mask | CLEM_GAMEPORT_BUTTON_MASK_JOYSTICK_0;
        clem_adb_device_input(&adb_device, &input);
        result = clem_adb_read_switch(&adb_device, CLEM_MMIO_REG_SW0, 0);
        if (mask & 0x55) {
            TEST_ASSERT_BIT_HIGH_MESSAGE(7, result, msg);
        } else {
            TEST_ASSERT_BIT_LOW_MESSAGE(7, result, msg);
        }
        result = clem_adb_read_switch(&adb_device, CLEM_MMIO_REG_SW1, 0);
        if (mask & 0xAA) {
            TEST_ASSERT_BIT_HIGH_MESSAGE(7, result, msg);
        } else {
            TEST_ASSERT_BIT_LOW_MESSAGE(7, result, msg);
        }
        result = clem_adb_read_switch(&adb_device, CLEM_MMIO_REG_SW2, 0);
        TEST_ASSERT_BIT_LOW_MESSAGE(7, result, msg);
        result = clem_adb_read_switch(&adb_device, CLEM_MMIO_REG_SW3, 0);
        TEST_ASSERT_BIT_LOW_MESSAGE(7, result, msg);
    }
}

void test_clem_gameport_buttons_23(void) {
    struct ClemensInputEvent input;
    unsigned mask;
    uint8_t result;
    char msg[8];

    for (mask = 0; mask <= 0xff; ++mask) {
        snprintf(msg, sizeof(msg), "mask=%02x", mask);
        input.type = kClemensInputType_Paddle;
        input.value_a = 0;
        input.value_b = 0;
        input.gameport_button_mask = mask | CLEM_GAMEPORT_BUTTON_MASK_JOYSTICK_1;
        clem_adb_device_input(&adb_device, &input);
        result = clem_adb_read_switch(&adb_device, CLEM_MMIO_REG_SW0, 0);
        TEST_ASSERT_BIT_LOW_MESSAGE(7, result, msg);
        result = clem_adb_read_switch(&adb_device, CLEM_MMIO_REG_SW1, 0);
        TEST_ASSERT_BIT_LOW_MESSAGE(7, result, msg);
        result = clem_adb_read_switch(&adb_device, CLEM_MMIO_REG_SW2, 0);
        if (mask & 0x55) {
            TEST_ASSERT_BIT_HIGH_MESSAGE(7, result, msg);
        } else {
            TEST_ASSERT_BIT_LOW_MESSAGE(7, result, msg);
        }
        result = clem_adb_read_switch(&adb_device, CLEM_MMIO_REG_SW3, 0);
        if (mask & 0xAA) {
            TEST_ASSERT_BIT_HIGH_MESSAGE(7, result, msg);
        } else {
            TEST_ASSERT_BIT_LOW_MESSAGE(7, result, msg);
        }
    }
}

static void _test_util_paddle_xy(unsigned paddle_mask, uint8_t padl_x, uint8_t padl_y, int16_t x,
                                 int16_t y) {
    struct ClemensInputEvent input;
    uint32_t paddle_time_ns[2];
    uint8_t result;
    unsigned padl_x_idx = padl_x - CLEM_MMIO_REG_PADDL0;
    unsigned padl_y_idx = padl_y - CLEM_MMIO_REG_PADDL0;

    char msg[32];
    snprintf(msg, sizeof(msg), "x:%d y:%d", x, y);
    // printf("TEST START: %s\n", msg);

    //  *with* input, paddle time should be 2 microseconds and the paddle bit
    //  should switch from high to low after the calculated timeout
    input.type = kClemensInputType_Paddle;
    input.value_a = x;
    input.value_b = y;
    input.gameport_button_mask = paddle_mask;
    clem_adb_device_input(&adb_device, &input);

    //  validate the paddle time matches the reference paddle time
    clem_adb_read_switch(&adb_device, CLEM_MMIO_REG_PTRIG, 0);
    paddle_time_ns[0] = _test_util_get_paddle_time_ns(padl_x_idx);
    paddle_time_ns[1] = _test_util_get_paddle_time_ns(padl_y_idx);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(_test_util_get_reference_paddle_time_ns(padl_x_idx),
                                     paddle_time_ns[0], msg);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(_test_util_get_reference_paddle_time_ns(padl_y_idx),
                                     paddle_time_ns[1], msg);
    //  validate paddle bits are high *until* timeout
    // printf("Time: x:%d, y:%d\n", paddle_time_ns[0], paddle_time_ns[1]);
    while (paddle_time_ns[0] > 0 || paddle_time_ns[1] > 0) {
        result = clem_adb_read_switch(&adb_device, padl_x, 0);
        if (paddle_time_ns[0] > 0) {
            TEST_ASSERT_BIT_HIGH_MESSAGE(7, result, msg);
        } else {
            TEST_ASSERT_BIT_LOW_MESSAGE(7, result, msg);
        }
        result = clem_adb_read_switch(&adb_device, padl_y, 0);
        if (paddle_time_ns[1] > 0) {
            TEST_ASSERT_BIT_HIGH_MESSAGE(7, result, msg);
        } else {
            TEST_ASSERT_BIT_LOW_MESSAGE(7, result, msg);
        }
        gameport_sync(CLEM_CLOCKS_MEGA2_CYCLE);
        paddle_time_ns[0] = _test_util_get_paddle_time_ns(padl_x_idx);
        paddle_time_ns[1] = _test_util_get_paddle_time_ns(padl_y_idx);
    }
    result = clem_adb_read_switch(&adb_device, padl_x, 0);
    TEST_ASSERT_BIT_LOW_MESSAGE(7, result, msg);
    result = clem_adb_read_switch(&adb_device, padl_y, 0);
    TEST_ASSERT_BIT_LOW_MESSAGE(7, result, msg);
    // printf("TEST   END: %s\n", msg);
}

void test_clem_gameport_paddle_01(void) {
    _test_util_paddle_xy(CLEM_GAMEPORT_BUTTON_MASK_JOYSTICK_0, CLEM_MMIO_REG_PADDL0,
                         CLEM_MMIO_REG_PADDL1, 0, 0);
    _test_util_paddle_xy(CLEM_GAMEPORT_BUTTON_MASK_JOYSTICK_0, CLEM_MMIO_REG_PADDL0,
                         CLEM_MMIO_REG_PADDL1, CLEM_GAMEPORT_PADDLE_AXIS_VALUE_MAX / 2, 0);
    _test_util_paddle_xy(CLEM_GAMEPORT_BUTTON_MASK_JOYSTICK_0, CLEM_MMIO_REG_PADDL0,
                         CLEM_MMIO_REG_PADDL1, CLEM_GAMEPORT_PADDLE_AXIS_VALUE_MAX, 0);
    _test_util_paddle_xy(CLEM_GAMEPORT_BUTTON_MASK_JOYSTICK_0, CLEM_MMIO_REG_PADDL0,
                         CLEM_MMIO_REG_PADDL1, CLEM_GAMEPORT_PADDLE_AXIS_VALUE_MAX,
                         CLEM_GAMEPORT_PADDLE_AXIS_VALUE_MAX / 2);
    _test_util_paddle_xy(CLEM_GAMEPORT_BUTTON_MASK_JOYSTICK_0, CLEM_MMIO_REG_PADDL0,
                         CLEM_MMIO_REG_PADDL1, CLEM_GAMEPORT_PADDLE_AXIS_VALUE_MAX,
                         CLEM_GAMEPORT_PADDLE_AXIS_VALUE_MAX);
}

void test_clem_gameport_paddle_23(void) {
    _test_util_paddle_xy(CLEM_GAMEPORT_BUTTON_MASK_JOYSTICK_1, CLEM_MMIO_REG_PADDL2,
                         CLEM_MMIO_REG_PADDL3, 0, 0);
    _test_util_paddle_xy(CLEM_GAMEPORT_BUTTON_MASK_JOYSTICK_1, CLEM_MMIO_REG_PADDL2,
                         CLEM_MMIO_REG_PADDL3, CLEM_GAMEPORT_PADDLE_AXIS_VALUE_MAX / 2, 0);
    _test_util_paddle_xy(CLEM_GAMEPORT_BUTTON_MASK_JOYSTICK_1, CLEM_MMIO_REG_PADDL2,
                         CLEM_MMIO_REG_PADDL3, CLEM_GAMEPORT_PADDLE_AXIS_VALUE_MAX, 0);
    _test_util_paddle_xy(CLEM_GAMEPORT_BUTTON_MASK_JOYSTICK_1, CLEM_MMIO_REG_PADDL2,
                         CLEM_MMIO_REG_PADDL3, CLEM_GAMEPORT_PADDLE_AXIS_VALUE_MAX,
                         CLEM_GAMEPORT_PADDLE_AXIS_VALUE_MAX / 2);
    _test_util_paddle_xy(CLEM_GAMEPORT_BUTTON_MASK_JOYSTICK_1, CLEM_MMIO_REG_PADDL2,
                         CLEM_MMIO_REG_PADDL3, CLEM_GAMEPORT_PADDLE_AXIS_VALUE_MAX,
                         CLEM_GAMEPORT_PADDLE_AXIS_VALUE_MAX);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_clem_gameport_reset);
    RUN_TEST(test_clem_gameport_buttons_01);
    RUN_TEST(test_clem_gameport_buttons_23);
    RUN_TEST(test_clem_gameport_paddle_01);
    RUN_TEST(test_clem_gameport_paddle_23);
    return UNITY_END();
}
