#ifndef CLEM_HOST_SERIALIZER_HPP
#define CLEM_HOST_SERIALIZER_HPP

#include "clem_host_shared.hpp"
#include "clem_woz.h"

namespace ClemensSerializer {
  bool save(std::string outputPath, ClemensMachine* machine, size_t driveCount,
            const ClemensWOZDisk* containers, const ClemensBackendDiskDriveState* states,
            const std::vector<ClemensBackendBreakpoint>& breakpoints);

  bool load(std::string outputPath, ClemensMachine* machine, size_t driveCount,
            ClemensWOZDisk* containers, ClemensBackendDiskDriveState* states,
            std::vector<ClemensBackendBreakpoint>& breakpoints,
            ClemensSerializerAllocateCb alloc_cb,
            void *context);
}  // namespace ClemensSerializer

#endif
