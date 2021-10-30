#ifndef CLEM_SHARED_H
#define CLEM_SHARED_H

#include <stdint.h>

typedef uint64_t clem_clocks_time_t;
typedef uint32_t clem_clocks_duration_t;

#define CLEM_TIME_UNINITIALIZED         ((clem_clocks_time_t)(-1))

/** All mmio memory operations can have this option - both onboard and
 *  card operations
 */
#define CLEM_OP_IO_READ_NO_OP        0x01
#define CLEM_IS_IO_READ_NO_OP(_flags_) (((_flags_) & CLEM_OP_IO_READ_NO_OP) != 0)

/** These are returned via io_sync and applied to the main IRQ line */
#define CLEM_IRQ_SLOT_1                     (0x01000000)
#define CLEM_IRQ_SLOT_2                     (0x02000000)
#define CLEM_IRQ_SLOT_3                     (0x04000000)
#define CLEM_IRQ_SLOT_4                     (0x08000000)
#define CLEM_IRQ_SLOT_5                     (0x10000000)
#define CLEM_IRQ_SLOT_6                     (0x20000000)
#define CLEM_IRQ_SLOT_7                     (0x40000000)


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
    void (*io_read)(uint8_t* data, uint16_t adr, uint8_t flags, void* context);
    void (*io_write)(uint8_t data, uint16_t adr, uint8_t flags, void* context);
    uint32_t (*io_sync)(struct ClemensClock* clock, void* context);
} ClemensCard;


#ifdef __cplusplus
}
#endif


#endif