#include "ckaudio/core.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

static LARGE_INTEGER s_counter_frequency;

void ckaudio_timepoint_init() {
    QueryPerformanceFrequency(&s_counter_frequency);
}

void ckaudio_timepoint_make_null(CKAudioTimePoint *tp) { memset(tp->data, 0xff, sizeof(tp->data)); }

bool ckaudio_timepoint_is_null(CKAudioTimePoint *tp) {
  for (unsigned i = 0; i < sizeof(tp->data); ++i) {
    if (tp->data[i] != 0xff)
      return false;
  }
  return true;
}

void ckaudio_timepoint_now(CKAudioTimePoint *tp) {
    LARGE_INTEGER counter;

    QueryPerformanceCounter(&counter);
    memcpy(&tp->data[0], &counter.QuadPart, sizeof(tp->data));
}

float ckaudio_timepoint_deltaf(CKAudioTimePoint *t1, CKAudioTimePoint *t0) {
    int64_t counter0 = *(int64_t *)(&t0->data[0]);
    int64_t counter1 = *(int64_t *)(&t1->data[0]);
    return (counter1 - counter0) / (float)s_counter_frequency.QuadPart;
}

double ckaudio_timepoint_deltad(CKAudioTimePoint *t1, CKAudioTimePoint *t0) {
    int64_t counter0 = *(int64_t *)(&t0->data[0]);
    int64_t counter1 = *(int64_t *)(&t1->data[0]);
    return (counter1 - counter0) / (double)s_counter_frequency.QuadPart;
}

float ckaudio_timepoint_stepf(CKAudioTimePoint *t) {
    CKAudioTimePoint last = *t;
    ckaudio_timepoint_now(t);
    return ckaudio_timepoint_deltaf(t, &last);
}

double ckaudio_timepoint_stepd(CKAudioTimePoint *t) {
    CKAudioTimePoint last = *t;
    ckaudio_timepoint_now(t);
    return ckaudio_timepoint_deltad(t, &last);
}
