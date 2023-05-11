#ifndef CLEM_HOST_DISK_UTILS_HPP
#define CLEM_HOST_DISK_UTILS_HPP

#include "cinek/buffertypes.hpp"
#include "clem_mmio_types.h"
#include <string_view>

namespace ClemensDiskUtilities {

constexpr unsigned kMaximumHDDSizeInMB = 32;
constexpr unsigned kBlocksPerMB = 2048;

std::string_view getDriveName(ClemensDriveType driveType);

ClemensDriveType getDriveType(std::string_view driveName);

bool createDisk(cinek::Range<uint8_t> decodeBuffer, const std::string &path,
                ClemensDriveType driveType);

//  returns the number of blocks written (if == blockCount, then OK)
unsigned createProDOSHardDisk(const std::string &path, unsigned blockCount);

} // namespace ClemensDiskUtilities

#endif
