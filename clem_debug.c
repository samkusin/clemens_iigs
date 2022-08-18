#include "clem_debug.h"
#include "clem_device.h"
#include "external/cross_endian.h"

#include <stdarg.h>
#include <stdio.h>

static FILE *g_fp_debug_out = NULL;
static char s_clem_debug_buffer[65536 * 102];
static unsigned s_clem_debug_cnt = 0;
static ClemensMachine *s_clem_machine = NULL;

void clem_debug_context(ClemensMachine *context) { s_clem_machine = context; }

void clem_debug_log(int log_level, const char *fmt, ...) {
  char *buffer;
  va_list arg_list;
  if (!s_clem_machine)
    return;
  buffer = s_clem_machine->mmio.dev_debug.log_buffer;
  va_start(arg_list, fmt);
  vsnprintf(buffer, CLEM_DEBUG_LOG_BUFFER_SIZE, fmt, arg_list);
  va_end(arg_list);
  s_clem_machine->logger_fn(log_level, s_clem_machine, buffer);
}

char *clem_debug_acquire_trace(unsigned amt) {
  char *next;
  unsigned next_debug_cnt = s_clem_debug_cnt + amt;
  if (next_debug_cnt >= sizeof(s_clem_debug_buffer)) {
    clem_debug_trace_flush();
  }
  next = &s_clem_debug_buffer[s_clem_debug_cnt];
  s_clem_debug_cnt = next_debug_cnt;
  return next;
}

void clem_debug_trace_flush() {
  if (!g_fp_debug_out && s_clem_debug_cnt > 0) {
    g_fp_debug_out = fopen("debug.out", "wb");
  }
  if (g_fp_debug_out) {
    fwrite(s_clem_debug_buffer, 1, s_clem_debug_cnt, g_fp_debug_out);
    fflush(g_fp_debug_out);
  }
  s_clem_debug_cnt = 0;
}

static void _clem_print_io_reg_counters(struct ClemensDeviceDebugger *dbg) {
  for (unsigned i = 0; i < 256; ++i) {
    if (dbg->ioreg_read_ctr[i] || dbg->ioreg_write_ctr[i]) {
      CLEM_LOG("IO %02X RW (%u, %u)", i & 0xff, dbg->ioreg_read_ctr[i],
               dbg->ioreg_write_ctr[i]);
    }
  }
}

void clem_debug_reset(struct ClemensDeviceDebugger *dbg) {
  memset(dbg, 0, sizeof(*dbg));
}

void clem_debug_counters(struct ClemensDeviceDebugger *dbg) {
  _clem_print_io_reg_counters(dbg);
}

void clem_debug_break(struct ClemensDeviceDebugger *dbg,
                      struct Clemens65C816 *cpu, unsigned debug_reason,
                      unsigned param0, unsigned param1) {
  CLEM_WARN("PC=%02X:%04X: DBR=%02X P=%02X", cpu->regs.PBR, cpu->regs.PC,
            cpu->regs.DBR, cpu->regs.P);
  switch (debug_reason) {
  case CLEM_DEBUG_BREAK_UNIMPL_IOREAD:
    _clem_print_io_reg_counters(dbg);
    CLEM_UNIMPLEMENTED("IO Read: %04X, %02X", param0, param1);
    break;
  case CLEM_DEBUG_BREAK_UNIMPL_IOWRITE:
    _clem_print_io_reg_counters(dbg);
    CLEM_UNIMPLEMENTED("IO Write: %04X, %02X", param0, param1);
    break;
  default:
    break;
  }
}

void clem_debug_iwm_start(ClemensMachine *context) {
  clem_iwm_debug_start(&context->mmio.dev_iwm);
}

void clem_debug_iwm_stop(ClemensMachine *context) {
  clem_iwm_debug_stop(&context->mmio.dev_iwm);
}

struct ClemensIIGSMemoryHandle {
  uint32_t machine_addr;
  uint32_t addr;
  uint16_t attrs;
  uint16_t owner;
  uint32_t sz;
  uint32_t prev;
  uint32_t next;
};

// TODO: Does this work for ROM 01?
#define CLEM_DEBUG_IIGS_MMGR_HANDLE_ADDR  0x00e11700
#define CLEM_DEBUG_IIGS_MMGR_HANDLE_ADDR_TAIL 0x00e11b00

static void _debug_toolbox_mmgr(ClemensMachine *context) {
  //  insepct memory handles
  //  https://github.com/TomHarte/CLK/wiki/Investigation:-The-Apple-IIgs-Memory-Manager
  //  TODO: traverse into e0 bank as the memory pointers can go there apparently
  struct ClemensIIGSMemoryHandle mmgr_handle;
  const uint8_t* base_mem = context->mega2_bank_map[1];
  const uint8_t* mem = base_mem + (CLEM_DEBUG_IIGS_MMGR_HANDLE_ADDR & 0xffff);
  unsigned base_addr = 0x00e10000;
  unsigned count = 0;
  while (count < 1000) {
    mmgr_handle.machine_addr = base_addr +
      (uint16_t)((uintptr_t)(mem - base_mem) & 0xffff);
    mmgr_handle.addr = le32toh(*(uint32_t*)(mem));
    mmgr_handle.attrs = le16toh(*(uint16_t*)(mem + 4));
    mmgr_handle.owner = le16toh(*(uint16_t*)(mem + 6));
    mmgr_handle.sz = le16toh(*(uint32_t*)(mem + 8));
    mmgr_handle.prev = le16toh(*(uint32_t*)(mem + 12));
    mmgr_handle.next = le16toh(*(uint32_t*)(mem + 16));
    if (mmgr_handle.sz > 0) {
      CLEM_LOG("[debug.toolbox.mmgr]: handle @ %08X: [$%08X:%08X] attrs: %04X, size: %08X",
              mmgr_handle.machine_addr, mmgr_handle.addr,
              mmgr_handle.addr + mmgr_handle.sz - 1,
              mmgr_handle.attrs, mmgr_handle.sz);
    } else {
      CLEM_LOG("[debug.toolbox.mmgr]: handle @ %08X: [$%08X] attrs: %04X, no size",
              mmgr_handle.machine_addr, mmgr_handle.addr, mmgr_handle.attrs);
    }
    CLEM_LOG("[debug.toolbox.mmgr]:          prev: %08X", mmgr_handle.prev);
    CLEM_LOG("[debug.toolbox.mmgr]:          next: %08X", mmgr_handle.next);
    if (mmgr_handle.next == 0) break;
    if ((mmgr_handle.next & 0x00ff0000) == 0x00e10000) {
      base_addr = 0x00e10000;
      base_mem = context->mega2_bank_map[1];
      mem = base_mem + (mmgr_handle.next & 0xffff);
    } else if ((mmgr_handle.next & 0x00ff0000) == 0x00e00000) {
      base_addr = 0x00e00000;
      base_mem = context->mega2_bank_map[1];
      mem = base_mem + (mmgr_handle.next & 0xffff);
    } else {
      CLEM_WARN("[debug.toolbox.mmgr]: handle located at unexpected bank %02X",
                (mmgr_handle.next >> 16) & 0xff);
    }
    ++count;
  }
  CLEM_LOG("[debug.toolbox.mmgr]: count = %u", count);
}

void clemens_debug_status_toolbox(ClemensMachine *context, unsigned id) {
  switch (id) {
    case CLEM_DEBUG_TOOLBOX_MMGR:
      _debug_toolbox_mmgr(context);
      break;
  }
}
