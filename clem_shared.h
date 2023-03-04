#ifndef CLEM_SHARED_H
#define CLEM_SHARED_H

#include <stdint.h>

typedef uint64_t clem_clocks_time_t;
typedef uint32_t clem_clocks_duration_t;

typedef uint8_t *(*ClemensSerializerAllocateCb)(unsigned, void *);

#define CLEM_TIME_UNINITIALIZED ((clem_clocks_time_t)(-1))

/** A bit confusing... used for calculating our system clock.  These values
 *  are relative to each other.
 *
 *  The clocks per mega2 cycle value will always be the largest.
 *
 *  If you divide the CLEM_CLOCKS_MEGA2_CYCLE by the CLEM_CLOCKS_FAST_CYCLE
 *  the value will be the effective maximum clock speed in Mhz of the CPU.
 */
#define CLEM_CLOCKS_FAST_CYCLE       1023U
#define CLEM_CLOCKS_MEGA2_CYCLE      2864U
#define CLEM_MEGA2_CYCLE_NS          978
#define CLEM_MEGA2_CYCLES_PER_SECOND 1023000U

/* Attempt to mimic VDA and VPA per memory access */
#define CLEM_MEM_FLAG_BUS_IO       (0x4)
#define CLEM_MEM_FLAG_OPCODE_FETCH (0x3)
#define CLEM_MEM_FLAG_DATA         (0x2)
#define CLEM_MEM_FLAG_PROGRAM      (0x1)
#define CLEM_MEM_FLAG_NULL         (0x0)

/** All mmio memory operations can have this option - both onboard and
 *  card operations
 */
#define CLEM_OP_IO_NO_OP          0x01
#define CLEM_IS_IO_NO_OP(_flags_) (((_flags_)&CLEM_OP_IO_NO_OP) != 0)

#define CLEM_OP_IO_CARD   0x40
#define CLEM_OP_IO_DEVSEL 0x80

#define CLEM_CARD_NMI 0x40000000
#define CLEM_CARD_IRQ 0x80000000

/* Some basic shared debugging defines */
#define CLEM_DEBUG_LOG_DEBUG  0
#define CLEM_DEBUG_LOG_INFO   1
#define CLEM_DEBUG_LOG_WARN   2
#define CLEM_DEBUG_LOG_UNIMPL 3
#define CLEM_DEBUG_LOG_FATAL  4

#ifdef __cplusplus
extern "C" {
#endif

/* Typically this is passed around as the current time for the machine
   and is guaranteed to be shared between machine and any external cards
   based on the ref_step.

   For example, MMIO clocks use the Mega2 reference step
*/
struct ClemensClock {
    clem_clocks_time_t ts;
    clem_clocks_duration_t ref_step;
};

/** _clock_ is of type ClemensClock */
#define clem_calc_secs_from_clocks(_clock_)                                                        \
    ((CLEM_MEGA2_CYCLE_NS * (uint64_t)((_clock_)->ts / (_clock_)->ref_step)) * 1.0e-9)

/* BEWARE - these macros act on sub second time intervals (per frame deltas.)
   Do not use these utilities to calculate values over long time intervals
*/
/** _clocks_step_ and _clocks_step_reference_ is of type clem_clocks_duration_t
 */
#define clem_calc_ns_step_from_clocks(_clocks_step_, _clocks_step_reference_)                      \
    ((uint32_t)(CLEM_MEGA2_CYCLE_NS * (uint64_t)(_clocks_step_) / (_clocks_step_reference_)))

/** _ns_ should be a delta time - this will break for deltas > 1ms */
#define clem_calc_clocks_step_from_ns(_ns_, _clocks_step_reference_)                               \
    ((clem_clocks_duration_t)((_ns_) * (_clocks_step_reference_)) / CLEM_MEGA2_CYCLE_NS)

/** _ns_ should be a delta time < 1 second */
#define clem_calc_clocks_step_from_ns_long(_ns_, _clocks_step_reference_)                          \
    ((clem_clocks_duration_t)((clem_clocks_time_t)(_ns_) * (_clocks_step_reference_) /             \
                              CLEM_MEGA2_CYCLE_NS))

//#define clem_calc_clocks_step_from_ns(_ns_, _clocks_step_reference_)                               \
//   ((clem_clocks_duration_t)(((float)(_ns_) / CLEM_MEGA2_CYCLE_NS) * (_clocks_step_reference_)))

#define clem_calc_clocks_remainder_from_ns(_ns_, _clocks_step_reference_)                          \
    ((clem_clocks_duration_t)((_ns_) * (_clocks_step_reference_)) % CLEM_MEGA2_CYCLE_NS)

/**
 * @brief Defines the abstract interface to slot-based card hardware
 *
 */
typedef struct {
    void *context;
    void (*io_reset)(struct ClemensClock *clock, void *context);
    void (*io_read)(struct ClemensClock *clock, uint8_t *data, uint8_t adr, uint8_t flags,
                    void *context);
    void (*io_write)(struct ClemensClock *clock, uint8_t data, uint8_t adr, uint8_t flags,
                     void *context);
    uint32_t (*io_sync)(struct ClemensClock *clock, void *context);
    const char *(*io_name)(void *context);
} ClemensCard;

#ifdef __cplusplus
}
#endif

#endif
