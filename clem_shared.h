#ifndef CLEM_SHARED_H
#define CLEM_SHARED_H

#include <stdint.h>

/** Enable for diagnostic level debugging - this will increase memory usage and
 *  slow down the emulator
 */
#define CLEM_DIAGNOSTIC_DEBUG 1

typedef uint64_t clem_clocks_time_t;
typedef uint32_t clem_clocks_duration_t;

#define CLEM_TIME_UNINITIALIZED ((clem_clocks_time_t)(-1))

/** All mmio memory operations can have this option - both onboard and
 *  card operations
 */
#define CLEM_OP_IO_READ_NO_OP 0x01
#define CLEM_IS_IO_READ_NO_OP(_flags_) (((_flags_)&CLEM_OP_IO_READ_NO_OP) != 0)

/* Attempt to mimic VDA and VPA per memory access */
#define CLEM_MEM_FLAG_OPCODE_FETCH (0x3)
#define CLEM_MEM_FLAG_DATA (0x2)
#define CLEM_MEM_FLAG_PROGRAM (0x1)
#define CLEM_MEM_FLAG_NULL (0x0)

#define CLEM_OP_IO_DEVSEL 0x80

#define CLEM_CARD_NMI 0x40000000
#define CLEM_CARD_IRQ 0x80000000

/* Some basic shared debugging defines */
#define CLEM_DEBUG_LOG_DEBUG 0
#define CLEM_DEBUG_LOG_INFO 1
#define CLEM_DEBUG_LOG_WARN 2
#define CLEM_DEBUG_LOG_UNIMPL 3
#define CLEM_DEBUG_LOG_FATAL 4

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
  void *context;
  void (*io_reset)(struct ClemensClock *clock, void *context);
  void (*io_read)(uint8_t *data, uint8_t adr, uint8_t flags, void *context);
  void (*io_write)(uint8_t data, uint8_t adr, uint8_t flags, void *context);
  uint32_t (*io_sync)(struct ClemensClock *clock, void *context);
} ClemensCard;

#ifdef __cplusplus
}
#endif

#endif
