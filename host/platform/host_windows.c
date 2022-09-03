

#include "clem_host_platform.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

static LARGE_INTEGER s_counter_frequency;

void clem_host_timepoint_init() {
  QueryPerformanceFrequency(&s_counter_frequency);
}

void clem_host_timepoint_now(ClemensHostTimePoint* tp) {
  LARGE_INTEGER counter;

  QueryPerformanceCounter(&counter);
  memcpy(&tp->data[0], &counter.QuadPart, sizeof(tp->data));
}

float clem_host_timepoint_deltaf(
  ClemensHostTimePoint* t1,
  ClemensHostTimePoint* t0
) {
  int64_t counter0 = *(int64_t *)(&t0->data[0]);
  int64_t counter1 = *(int64_t *)(&t1->data[0]);
  return (counter1 - counter0) / (float)s_counter_frequency.QuadPart;
}

double clem_host_timepoint_deltad(
  ClemensHostTimePoint* t1,
  ClemensHostTimePoint* t0
) {
  int64_t counter0 = *(int64_t *)(&t0->data[0]);
  int64_t counter1 = *(int64_t *)(&t1->data[0]);
  return (counter1 - counter0) / (double)s_counter_frequency.QuadPart;
}

bool clem_host_get_caps_lock_state() {
  return (GetKeyState(VK_CAPITAL) & 0x1) ? true : false;
}

unsigned clem_host_get_processor_number() {
  return (unsigned)GetCurrentProcessorNumber();
}
