#ifndef CLEM_SHARED_H
#define CLEM_SHARED_H

#include <stdint.h>

typedef uint64_t clem_clocks_time_t;
typedef uint32_t clem_clocks_duration_t;

#define CLEM_SERIALIZER_ALLOCATION_FPI_MEMORY_BANK   0
#define CLEM_SERIALIZER_ALLOCATION_MEGA2_MEMORY_BANK 1
#define CLEM_SERIALIZER_ALLOCATION_DISK_NIB_3_5      2
#define CLEM_SERIALIZER_ALLOCATION_DISK_NIB_5_25     3
#define CLEM_SERIALIZER_ALLOCATION_AUDIO_BUFFER      4

typedef uint8_t *(*ClemensSerializerAllocateCb)(unsigned /* type */, unsigned /* amount */,
                                                void * /* context */);

#define CLEM_TIME_UNINITIALIZED ((clem_clocks_time_t)(-1))

/** A bit confusing and created to avoid floating point math whenever possible
 *  (whether this was a good choice given modern architectures... ?)
 *
 *  Used for calculating our system clock.  These values are relative to each other.
 *
 *  The clocks per mega2 cycle (PHI0) value will always be the largest.
 *  Yet since most time calculations in the emulator are done with fixed point-like
 *  math, the aim is to keep the clocks count per cycle high enough for fixed
 *  math to work with unsigned 32-bit numbers.
 *
 *  SO DON'T CHANGE THESE UNLESS ALL DEPENDENT DEFINE/CALCULATIONS THAT THESE
 *  VALUES TRICKLE DOWN REMAIN VALID.   IF YOU DO, TEST *EVERYTHING* (IWM, DIAGNOSTICS)
 *
 *  Based on this, care should be taken when attempting to emulate a 8mhz machine
 *  in the future - though most I/O is performed using PHI0 cycles.
 *
 *  If you divide the CLEM_CLOCKS_PHI0_CYCLE by the CLEM_CLOCKS_PHI2_FAST_CYCLE
 *  the value will be the effective maximum clock speed in Mhz of the CPU.
 */

#define CLEM_CLOCKS_14MHZ_CYCLE     200U                           // 14.318 Mhz
#define CLEM_CLOCKS_7MHZ_CYCLE      400U                           // 7.159  Mhz
#define CLEM_CLOCKS_4MHZ_CYCLE      700U                           // 4.091  Mhz
#define CLEM_CLOCKS_PHI2_FAST_CYCLE (CLEM_CLOCKS_14MHZ_CYCLE * 5)  // 2.864  Mhz
#define CLEM_CLOCKS_2MHZ_CYCLE      (CLEM_CLOCKS_14MHZ_CYCLE * 7)  // 2.045  Mhz
#define CLEM_CLOCKS_Q3_CYCLE        CLEM_CLOCKS_2MHZ_CYCLE         // 2Mhz cycle with stretch
#define CLEM_CLOCKS_PHI0_CYCLE      (CLEM_CLOCKS_14MHZ_CYCLE * 14) // 1.023  Mhz with stretch

// IMPORTANT! This is rounded from 69.8ns - which when scaling up to PHI0 translates
//            to 977.7 ns without rounding.  Due to the stretch cycle, this effectively
//            rounds up to 980ns, which is really what most system timings rely on
//            So, rounding up.  Bon chance.
#define CLEM_14MHZ_CYCLE_NS 70U
#define CLEM_PHI0_CYCLE_NS  980U

#define CLEM_MEGA2_CYCLES_PER_SECOND 1023000U

/** _clock_ is of type ClemensClock
    BEWARE - these macros act on sub second time intervals (per frame deltas.)
    Do not use these utilities to calculate values over long time intervals
*/
#define clem_calc_ns_step_from_clocks(_clocks_step_)                                               \
    ((unsigned)(CLEM_14MHZ_CYCLE_NS * (_clocks_step_) / CLEM_CLOCKS_14MHZ_CYCLE))

#define clem_calc_clocks_step_from_ns(_ns_)                                                        \
    ((clem_clocks_duration_t)((_ns_)*CLEM_CLOCKS_14MHZ_CYCLE) / CLEM_14MHZ_CYCLE_NS)

/* intentional - paranthesized expression should be done first to avoid precision loss*/
#define clem_calc_secs_from_clocks(_clock_)                                                        \
    ((CLEM_14MHZ_CYCLE_NS * (uint64_t)((_clock_)->ts / CLEM_CLOCKS_14MHZ_CYCLE)) * 1.0e-9)

/*
    Now to the rest of the defines
*/

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
