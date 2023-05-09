#include "clem_disk_utils.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace ClemensDiskUtilities {

static std::array<std::string_view, 4> sDriveNames = {"s5d1", "s5d2", "s6d1", "s6d2"};

struct ClemensWOZDisk *parseWOZ(struct ClemensWOZDisk *woz, cinek::ConstRange<uint8_t> &image) {
    const uint8_t *bits_data_current = clem_woz_check_header(image.first, cinek::length(image));
    if (!bits_data_current) {
        return nullptr;
    }
    const uint8_t *bits_data_end = image.second;

    struct ClemensWOZChunkHeader chunkHeader;
    bool hasError = false;
    while ((bits_data_current = clem_woz_parse_chunk_header(
                &chunkHeader, bits_data_current, bits_data_end - bits_data_current)) != nullptr) {
        switch (chunkHeader.type) {
        case CLEM_WOZ_CHUNK_INFO:
            bits_data_current = clem_woz_parse_info_chunk(woz, &chunkHeader, bits_data_current,
                                                          chunkHeader.data_size);
            break;
        case CLEM_WOZ_CHUNK_TMAP:
            bits_data_current = clem_woz_parse_tmap_chunk(woz, &chunkHeader, bits_data_current,
                                                          chunkHeader.data_size);
            break;
        case CLEM_WOZ_CHUNK_TRKS:
            bits_data_current = clem_woz_parse_trks_chunk(woz, &chunkHeader, bits_data_current,
                                                          chunkHeader.data_size);
            break;
        case CLEM_WOZ_CHUNK_WRIT:
            break;
        case CLEM_WOZ_CHUNK_META:
            // bits_data_current = clem_woz_parse_meta_chunk(woz, &chunkHeader, bits_data_current,
            //                                               chunkHeader.data_size);
            //  skip for now
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
    if (driveType == kClemensDrive_Invalid)
        return "invalid";
    return sDriveNames[driveType];
}

ClemensDriveType getDriveType(std::string_view driveName) {
    auto driveIt = std::find(sDriveNames.begin(), sDriveNames.end(), driveName);
    if (driveIt == sDriveNames.end())
        return kClemensDrive_Invalid;
    unsigned driveIndex = unsigned(driveIt - sDriveNames.begin());
    return static_cast<ClemensDriveType>(driveIndex);
}

struct ClemensWOZDisk *createWOZ(struct ClemensWOZDisk *woz, const struct ClemensNibbleDisk *nib) {
    if (nib->disk_type == CLEM_DISK_TYPE_5_25) {
        woz->disk_type = CLEM_WOZ_DISK_5_25;
        woz->boot_type = CLEM_WOZ_BOOT_5_25_16;
    } else if (nib->disk_type == CLEM_DISK_TYPE_3_5) {
        woz->disk_type = CLEM_WOZ_DISK_3_5;
        woz->boot_type = CLEM_WOZ_BOOT_UNDEFINED;
    }
    // these images come from non-copy protected sources - which implies
    // synchronization
    woz->flags = CLEM_WOZ_SUPPORT_UNKNOWN | CLEM_WOZ_IMAGE_CLEANED | CLEM_WOZ_IMAGE_SYNCHRONIZED;
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

    woz->nib = const_cast<struct ClemensNibbleDisk *>(nib);
    return woz;
}

//  TODO: creating a ClemensNibbleDisk should be a clem_disk.h utility instead
//        of the placing the code here.
void createEmptyDisk(ClemensDriveType driveType, ClemensNibbleDisk &nib) {
    unsigned max_track_size_bytes = 0, track_byte_offset = 0;

    auto trackDataSize = nib.bits_data_end - nib.bits_data;
    memset(nib.bits_data, 0x96, trackDataSize);
    nib.is_write_protected = false;
    nib.is_double_sided = false;

    switch (driveType) {
    case kClemensDrive_3_5_D1:
    case kClemensDrive_3_5_D2:
        nib.disk_type = CLEM_DISK_TYPE_3_5;
        nib.is_double_sided = true; // TODO: an option?
        nib.bit_timing_ns = CLEM_DISK_3_5_BIT_TIMING_NS;
        nib.track_count = nib.is_double_sided ? 160 : 80;
        for (unsigned i = 0; i < CLEM_DISK_LIMIT_QTR_TRACKS; ++i) {
            if (nib.is_double_sided) {
                nib.meta_track_map[i] = i;
            } else if ((i % 2) == 0) {
                nib.meta_track_map[i] = (i / 2);
            } else {
                nib.meta_track_map[i] = 0xff;
            }
            nib.track_initialized[i] = 0;
            nib.track_byte_offset[i] = 0;
            nib.track_byte_count[i] = 0;
            nib.track_bits_count[i] = 0;
        }
        track_byte_offset = 0;
        for (unsigned region_index = 0; region_index < 5; ++region_index) {
            unsigned bits_cnt = CLEM_DISK_35_CALC_BYTES_FROM_SECTORS(
                                    g_clem_max_sectors_per_region_35[region_index]) *
                                8;
            max_track_size_bytes = bits_cnt / 8;
            for (unsigned i = g_clem_track_start_per_region_35[region_index];
                 i < g_clem_track_start_per_region_35[region_index + 1];) {
                unsigned track_index = i;
                if (!nib.is_double_sided) {
                    track_index /= 2;
                    i += 2;
                } else {
                    i += 1;
                }
                nib.track_initialized[track_index] = 1;
                nib.track_byte_offset[track_index] = track_byte_offset;
                nib.track_byte_count[track_index] = max_track_size_bytes;
                nib.track_bits_count[track_index] = bits_cnt;
                track_byte_offset += max_track_size_bytes;
            }
        }
        break;

    case kClemensDrive_5_25_D1:
    case kClemensDrive_5_25_D2:
        nib.disk_type = CLEM_DISK_TYPE_5_25;
        nib.bit_timing_ns = CLEM_DISK_5_25_BIT_TIMING_NS;
        nib.track_count = 35;
        max_track_size_bytes = CLEM_DISK_525_BYTES_PER_TRACK;
        for (unsigned i = 0; i < CLEM_DISK_LIMIT_QTR_TRACKS; ++i) {
            if ((i % 4) == 0 || (i % 4) == 1) {
                nib.meta_track_map[i] = (i / 4);
            } else {
                nib.meta_track_map[i] = 0xff;
            }
            nib.track_initialized[i] = 0;
            nib.track_byte_offset[i] = 0;
            nib.track_byte_count[i] = 0;
            nib.track_bits_count[i] = 0;
        }
        track_byte_offset = 0;
        for (unsigned i = 0; i < nib.track_count; ++i) {
            nib.track_initialized[i] = 1;
            nib.track_byte_offset[i] = track_byte_offset;
            nib.track_byte_count[i] = max_track_size_bytes;
            nib.track_bits_count[i] = CLEM_DISK_BLANK_TRACK_BIT_LENGTH_525;
            track_byte_offset += max_track_size_bytes;
        }
        break;

    default:
        assert(false);
        break;
    }
}

} // namespace ClemensDiskUtilities
