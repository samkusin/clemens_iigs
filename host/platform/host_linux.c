#include "clem_host_platform.h"

#include <X11/XKBlib.h>
#include <X11/keysym.h>

#include <assert.h>

#define _GNU_SOURCE
#include <sys/syscall.h>
#include <unistd.h>

#include <uuid/uuid.h>

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

unsigned clem_host_get_processor_number() { return local_getcpu(); }

void clem_host_uuid_gen(ClemensHostUUID *uuid) {
    assert(sizeof(uuid_t) <= sizeof(uuid->data));
    uuid_generate(uuid->data);
}

void clem_joystick_open_devices(unsigned provider) {}

unsigned clem_joystick_poll(ClemensHostJoystick *joysticks) { return 0; }

void clem_joystick_close_devices() {}
