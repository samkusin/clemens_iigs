#ifndef CLEM_HOST_SERIALIZER_HPP
#define CLEM_HOST_SERIALIZER_HPP

#include "clem_host_shared.hpp"
#include "clem_smartport.h"
#include "clem_woz.h"

class ClemensSmartPortDisk;

namespace ClemensSerializer {

bool save(std::string outputPath, ClemensMachine *machine, ClemensMMIO *mmio, size_t driveCount,
          const ClemensWOZDisk *containers, const ClemensBackendDiskDriveState *driveStates,
          size_t smartPortCount, const ClemensSmartPortDisk *smartPortDisks,
          const ClemensBackendDiskDriveState *smartPortStates,
          const std::vector<ClemensBackendBreakpoint> &breakpoints);

bool load(std::string outputPath, ClemensMachine *machine, ClemensMMIO *mmio, size_t driveCount,
          ClemensWOZDisk *containers, ClemensBackendDiskDriveState *driveStates,
          size_t smartPortCount, ClemensSmartPortDisk *smartPortDisks,
          ClemensBackendDiskDriveState *smartPortStates,
          std::vector<ClemensBackendBreakpoint> &breakpoints, ClemensSerializerAllocateCb alloc_cb,
          void *context);
} // namespace ClemensSerializer

#endif
