#include "clem_debug.h"
#include "clem_device.h"

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
