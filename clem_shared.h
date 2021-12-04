#ifndef CLEM_SHARED_H
#define CLEM_SHARED_H

#include <stdint.h>

typedef uint64_t clem_clocks_time_t;
typedef uint32_t clem_clocks_duration_t;

#define CLEM_TIME_UNINITIALIZED         ((clem_clocks_time_t)(-1))

/** All mmio memory operations can have this option - both onboard and
 *  card operations
 */
#define CLEM_OP_IO_READ_NO_OP       0x01
#define CLEM_IS_IO_READ_NO_OP(_flags_) (((_flags_) & CLEM_OP_IO_READ_NO_OP) != 0)

/* MUST be the same as CLEM_IRQ_ON in clem_defs.h */
#define CLEM_CARD_IRQ               0x80000000


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

typedef struct {
    void* context;
    void (*io_reset)(struct ClemensClock* clock, void* context);
    void (*io_read)(uint8_t* data, uint16_t adr, uint8_t flags, void* context);
    void (*io_write)(uint8_t data, uint16_t adr, uint8_t flags, void* context);
    uint32_t (*io_sync)(struct ClemensClock* clock, void* context);
} ClemensCard;


#ifdef __cplusplus
}
#endif


#endif
