#include "clem_host_platform.h"

#include <X11/XKBlib.h>
#include <X11/keysym.h>

#include <time.h>
#include <assert.h>

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>

#include <uuid/uuid.h>

#define CLEM_HOST_LINUX_CLOCK_ID CLOCK_MONOTONIC

static struct timespec s_timer_resolution;
static Display* s_x11_display = NULL;

//  seems to be a reliable way to call getcpu() regardless of glibc/distribution
static inline unsigned local_getcpu() {
    #ifdef SYS_getcpu
    int cpu, status;
    status = syscall(SYS_getcpu, &cpu, NULL, NULL);
    return (status == -1) ? status : cpu;
    #else
    return UINT32_MAX; // unavailable
    #endif
}

static inline long _clem_host_calc_ns(ClemensHostTimePoint* tp) {
  struct timespec* ts = (struct timespec*)(&tp->data[0]);
  return ts->tv_nsec + ts->tv_sec * 1000000000;
}

void clem_host_x11_init(const void* display) {
  //XModifierKeymap* keymap;
  s_x11_display = (Display *)display;
  //keymap = XGetModifierMapping(s_x11_display);
  //keymap = XInsertModifiermapEntry(keymap, (KeyCode)XK_Caps_Lock, LockMapIndex);
  //XSetModifierMapping(s_x11_display, keymap);
  //XFreeModifiermap(keymap);
}

void clem_host_timepoint_init() {
  //  TODO: CLOCK_PROCESS_CPUTIME_ID?
  clock_getres(CLEM_HOST_LINUX_CLOCK_ID, &s_timer_resolution);
  assert(sizeof(s_timer_resolution) <= sizeof(ClemensHostTimePoint));
}

void clem_host_timepoint_now(ClemensHostTimePoint* tp) {
  struct timespec* ts = (struct timespec*)(&tp->data[0]);
  clock_gettime(CLEM_HOST_LINUX_CLOCK_ID, ts);
}

float clem_host_timepoint_deltaf(ClemensHostTimePoint* t1,
                                 ClemensHostTimePoint* t0) {
  long diff_ns = _clem_host_calc_ns(t1) - _clem_host_calc_ns(t0);
  return (float)(diff_ns * 1e-9);
}

double clem_host_timepoint_deltad(ClemensHostTimePoint* t1,
                                  ClemensHostTimePoint* t0) {
  long diff_ns = _clem_host_calc_ns(t1) - _clem_host_calc_ns(t0);
  return diff_ns * 1e-9;
}


bool clem_host_get_caps_lock_state() {
  XModifierKeymap* keymap = XGetModifierMapping(s_x11_display);
  int i;
  for (i = 0; i < keymap->max_keypermod; ++i) {
    if (keymap->modifiermap[LockMapIndex]) return true;
  }
  XFreeModifiermap(keymap);
  return false;
}

unsigned clem_host_get_processor_number() {
  return local_getcpu();
}

void clem_host_uuid_gen(ClemensHostUUID* uuid) {
  assert(sizeof(uuid_t) <= sizeof(uuid->data));
  uuid_generate(uuid->data);
}
