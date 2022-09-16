

#include "clem_host_platform.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <combaseapi.h>

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

void clem_host_uuid_gen(ClemensHostUUID* uuid) {
  GUID guid;
  ZeroMemory(&guid, sizeof(guid));
  CoCreateGuid(&guid);
  uuid->data[0] = (char)((guid.Data1 >> 24) & 0xff);
  uuid->data[1] = (char)((guid.Data1 >> 16) & 0xff);
  uuid->data[2] = (char)((guid.Data1 >> 8) & 0xff);
  uuid->data[3] = (char)(guid.Data1 & 0xff);
  uuid->data[4] = (char)((guid.Data2 >> 8) & 0xff);
  uuid->data[5] = (char)(guid.Data2 & 0xff);
  uuid->data[6] = (char)((guid.Data3 >> 8) & 0xff);
  uuid->data[7] = (char)(guid.Data3 & 0xff);
  uuid->data[8] = guid.Data4[0];
  uuid->data[9] = guid.Data4[1];
  uuid->data[10] = guid.Data4[2];
  uuid->data[11] = guid.Data4[3];
  uuid->data[12] = guid.Data4[4];
  uuid->data[13] = guid.Data4[5];
  uuid->data[14] = guid.Data4[6];
  uuid->data[15] = guid.Data4[7];
}
