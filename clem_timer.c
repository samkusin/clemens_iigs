#include "clem_device.h"
#include "clem_mmio_defs.h"


void clem_timer_reset(struct ClemensDeviceTimer* timer) {
    timer->flags = 0;
}

void clem_timer_sync(
    struct ClemensDeviceTimer* timer,
    uint32_t delta_us
) {
    timer->irq_1sec_us += delta_us;
    timer->irq_qtrsec_us += delta_us;

    while (timer->irq_1sec_us >= CLEM_MEGA2_TIMER_1SEC_US) {
        timer->irq_1sec_us -= CLEM_MEGA2_TIMER_1SEC_US;
        if (timer->flags & CLEM_MMIO_TIMER_1SEC_ENABLED) {
            timer->irq_line |= CLEM_IRQ_TIMER_RTC_1SEC;
        }
    }
    while (timer->irq_qtrsec_us >= CLEM_MEGA2_TIMER_QSEC_US) {
        timer->irq_qtrsec_us -= CLEM_MEGA2_TIMER_QSEC_US;
        if (timer->flags & CLEM_MMIO_TIMER_QSEC_ENABLED) {
            timer->irq_line |= CLEM_IRQ_TIMER_QSEC;
        }
    }
}
