#include "clem_types.h"
#include "unity.h"

#include "clem_mmio.h"
#include "clem_vgc.h"

#include <string.h>

static ClemensMMIO mmio;
static struct ClemensTimeSpec tspec;

static uint8_t e0_bank[64 * 1024];
static uint8_t e1_bank[64 * 1024];

static void fixture_sync(int ticks) {
    struct ClemensClock clocks;
    for (int i = 0; i < ticks; ++i) {
        clocks.ts = tspec.clocks_spent;
        clocks.ref_step = tspec.clocks_step_mega2;
        clem_vgc_sync(&mmio.vgc, &clocks, e0_bank, e1_bank);
        tspec.clocks_spent += tspec.clocks_step;
    }
}

void setUp(void) {
    tspec.clocks_spent = 0;
    tspec.clocks_step_mega2 = CLEM_CLOCKS_MEGA2_CYCLE;
    tspec.clocks_step_fast = CLEM_CLOCKS_FAST_CYCLE;
    tspec.clocks_step = CLEM_CLOCKS_MEGA2_CYCLE;

    memset(&mmio, 0, sizeof(mmio));
    clem_mmio_reset(&mmio, tspec.clocks_step_mega2);
}

void tearDown(void) {}

void test_clem_vgc_set_txtpage2(void) {
    uint8_t reg_value;
    bool mega2_access;
    //  from text 1 to text 2 and back, verifying softswtiches
    //  also verify statereg
    reg_value = clem_mmio_read(
        &mmio, &tspec, CLEM_MMIO_MAKE_IO_ADDRESS(CLEM_MMIO_REG_TXTPAGE2_TEST), 0, &mega2_access);
    TEST_ASSERT_BIT_LOW(7, reg_value);
    reg_value = clem_mmio_read(&mmio, &tspec, CLEM_MMIO_MAKE_IO_ADDRESS(CLEM_MMIO_REG_STATEREG), 0,
                               &mega2_access);
    TEST_ASSERT_BIT_LOW(6, reg_value);
    fixture_sync(1);
    clem_mmio_write(&mmio, &tspec, 0, CLEM_MMIO_MAKE_IO_ADDRESS(CLEM_MMIO_REG_TXTPAGE2), 0,
                    &mega2_access);
    fixture_sync(1);
    reg_value = clem_mmio_read(
        &mmio, &tspec, CLEM_MMIO_MAKE_IO_ADDRESS(CLEM_MMIO_REG_TXTPAGE2_TEST), 0, &mega2_access);
    TEST_ASSERT_BIT_HIGH(7, reg_value);
    reg_value = clem_mmio_read(&mmio, &tspec, CLEM_MMIO_MAKE_IO_ADDRESS(CLEM_MMIO_REG_STATEREG), 0,
                               &mega2_access);
    TEST_ASSERT_BIT_HIGH(6, reg_value);
    fixture_sync(1);
    clem_mmio_write(&mmio, &tspec, 0, CLEM_MMIO_MAKE_IO_ADDRESS(CLEM_MMIO_REG_TXTPAGE1), 0,
                    &mega2_access);
    fixture_sync(1);
    reg_value = clem_mmio_read(
        &mmio, &tspec, CLEM_MMIO_MAKE_IO_ADDRESS(CLEM_MMIO_REG_TXTPAGE2_TEST), 0, &mega2_access);
    TEST_ASSERT_BIT_LOW(7, reg_value);
    reg_value = clem_mmio_read(&mmio, &tspec, CLEM_MMIO_MAKE_IO_ADDRESS(CLEM_MMIO_REG_STATEREG), 0,
                               &mega2_access);
    TEST_ASSERT_BIT_LOW(6, reg_value);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_clem_vgc_set_txtpage2);
    return UNITY_END();
}
