#include "clem_device.h"
#include "clem_mmio_defs.h"


void clem_timer_reset(struct ClemensDeviceTimer* timer) {
    timer->irq_1sec_ms = 0;
    timer->irq_qtrsec_ms = 0;
    timer->flags = 0;
}

uint32_t clem_timer_sync(
    struct ClemensDeviceTimer* timer,
    uint32_t delta_ms,
    uint32_t irq_line
) {
    timer->irq_1sec_ms += delta_ms;
    timer->irq_qtrsec_ms += delta_ms;

    while (timer->irq_1sec_ms >= CLEM_MEGA2_TIMER_1SEC_MS) {
        timer->irq_1sec_ms -= CLEM_MEGA2_TIMER_1SEC_MS;
        if (timer->flags & CLEM_MMIO_TIMER_1SEC_ENABLED) {
            irq_line |= CLEM_IRQ_TIMER_RTC_1SEC;
        }
    }
    while (timer->irq_qtrsec_ms >= CLEM_MEGA2_TIMER_QSEC_MS) {
        timer->irq_1sec_ms -= CLEM_MEGA2_TIMER_QSEC_MS;
        if (timer->flags & CLEM_MMIO_TIMER_QSEC_ENABLED) {
            irq_line |= CLEM_IRQ_TIMER_QSEC;
        }
    }

    return irq_line;
}
