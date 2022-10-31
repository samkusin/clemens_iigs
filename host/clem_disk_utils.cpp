#include "clem_disk_utils.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>

namespace ClemensDiskUtilities {

static std::array<std::string_view, 4> sDriveNames = {
  "s5d1", "s5d2", "s6d1", "s6d2"
};

struct ClemensWOZDisk* parseWOZ(
  struct ClemensWOZDisk* woz, cinek::ConstRange<uint8_t>& image) {
  const uint8_t* bits_data_current = clem_woz_check_header(
    image.first, cinek::length(image));
  if (!bits_data_current) {
    return nullptr;
  }
  const uint8_t* bits_data_end = image.second;

  struct ClemensWOZChunkHeader chunkHeader;
  bool hasError = false;
  while ((bits_data_current = clem_woz_parse_chunk_header(
            &chunkHeader, bits_data_current, bits_data_end - bits_data_current)) != nullptr) {
    switch (chunkHeader.type) {
    case CLEM_WOZ_CHUNK_INFO:
      bits_data_current = clem_woz_parse_info_chunk(woz, &chunkHeader,
                                                    bits_data_current,
                                                    chunkHeader.data_size);
      break;
    case CLEM_WOZ_CHUNK_TMAP:
      bits_data_current = clem_woz_parse_tmap_chunk(woz, &chunkHeader,
                                                    bits_data_current,
                                                    chunkHeader.data_size);
      break;
    case CLEM_WOZ_CHUNK_TRKS:
      bits_data_current = clem_woz_parse_trks_chunk(woz, &chunkHeader,
                                                    bits_data_current,
                                                    chunkHeader.data_size);
      break;
    case CLEM_WOZ_CHUNK_WRIT:
      break;
    case CLEM_WOZ_CHUNK_META:
      bits_data_current = clem_woz_parse_meta_chunk(woz, &chunkHeader,
                                                    bits_data_current,
                                                    chunkHeader.data_size);
      // skip for now
      break;
    default:
      break;
    }
    hasError = bits_data_current == nullptr;
    image.first = bits_data_current;
  }
  return !hasError ? woz : nullptr;
}

size_t calculateNibRequiredMemory(ClemensDriveType driveType) {
  size_t size = 0;
  switch (driveType) {
    case kClemensDrive_3_5_D1:
    case kClemensDrive_3_5_D2:
      size = CLEM_DISK_35_MAX_DATA_SIZE;
      break;
    case kClemensDrive_5_25_D1:
    case kClemensDrive_5_25_D2:
      size = CLEM_DISK_525_MAX_DATA_SIZE;
      break;
    default:
      break;
  }
  return size;
}

std::string_view getDriveName(ClemensDriveType driveType) {
  if (driveType == kClemensDrive_Invalid) return "invalid";
  return sDriveNames[driveType];
}

ClemensDriveType getDriveType(std::string_view driveName) {
  auto driveIt = std::find(sDriveNames.begin(), sDriveNames.end(), driveName);
  if (driveIt == sDriveNames.end()) return kClemensDrive_Invalid;
  unsigned driveIndex = unsigned(driveIt - sDriveNames.begin());
  return static_cast<ClemensDriveType>(driveIndex);
}


} // end namespace
