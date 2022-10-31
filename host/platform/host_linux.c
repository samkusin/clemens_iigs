#include "clem_host_platform.h"

#include <assert.h>

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>

#include <uuid/uuid.h>

unsigned clem_host_get_processor_number() {
  return local_getcpu();
}

void clem_host_uuid_gen(ClemensHostUUID* uuid) {
  assert(sizeof(uuid_t) <= sizeof(uuid->data));
  uuid_generate(uuid->data);
}
