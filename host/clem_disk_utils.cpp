#include "clem_disk_utils.hpp"

#include <algorithm>
#include <array>
#include <cstring>
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

struct ClemensWOZDisk* createWOZ(struct ClemensWOZDisk* woz,
                                 const struct ClemensNibbleDisk* nib) {
  if (nib->disk_type == CLEM_DISK_TYPE_5_25) {
    woz->disk_type = CLEM_WOZ_DISK_5_25;
    woz->boot_type = CLEM_WOZ_BOOT_5_25_16;
  } else if (nib->disk_type == CLEM_DISK_TYPE_3_5) {
    woz->disk_type = CLEM_WOZ_DISK_3_5;
    woz->boot_type = CLEM_WOZ_BOOT_UNDEFINED;
  }
  // these images come from non-copy protected sources - which implies
  // synchronization
  woz->flags =
      CLEM_WOZ_SUPPORT_UNKNOWN | CLEM_WOZ_IMAGE_CLEANED | CLEM_WOZ_IMAGE_SYNCHRONIZED;
  if (nib->is_double_sided) {
    woz->flags |= CLEM_WOZ_IMAGE_DOUBLE_SIDED;
  }
  if (nib->is_write_protected) {
    woz->flags |= CLEM_WOZ_IMAGE_WRITE_PROTECT;
  }
  woz->required_ram_kb = 0;
  woz->max_track_size_bytes = 0;

  for (unsigned i = 0; i < nib->track_count; ++i) {
    if (nib->track_byte_count[i] > woz->max_track_size_bytes) {
      woz->max_track_size_bytes = nib->track_byte_count[i];
    }
  }
  //  block align the byte count
  woz->max_track_size_bytes = ((woz->max_track_size_bytes + 511) / 512) * 512;
  woz->version = 2;
  memset(woz->creator, 0x20, sizeof(woz->creator));
  woz->creator[0] = 'C';
  woz->creator[1] = 'l';
  woz->creator[2] = 'e';
  woz->creator[3] = 'm';
  woz->creator[4] = 'e';
  woz->creator[5] = 'n';
  woz->creator[6] = 's';
  woz->creator[8] = 'H';
  woz->creator[9] = 'o';
  woz->creator[10] = 's';
  woz->creator[11] = 't';

  woz->nib = const_cast<struct ClemensNibbleDisk*>(nib);
  return woz;
}

} // end namespace
