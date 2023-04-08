#include "clem_serializer.hpp"
#include "clem_disk_utils.hpp"
#include "clem_host_utils.hpp"
#include "clem_smartport_disk.hpp"
#include "emulator.h"
#include "emulator_mmio.h"
#include "serializer.h"
#include "smartport/prodos_hdd32.h"

#include "fmt/format.h"

#include "iocards/mockingboard.h"

namespace ClemensSerializer {

void saveBackendDiskDriveState(mpack_writer_t *writer, const ClemensBackendDiskDriveState &state) {
    mpack_write_cstr(writer, "image");
    mpack_write_cstr(writer, state.imagePath.c_str());
    mpack_write_cstr(writer, "ejecting");
    mpack_write_bool(writer, state.isEjecting);
}

void loadBackendDiskDriveState(mpack_reader_t *reader, ClemensBackendDiskDriveState &state) {
    char value[1024];
    mpack_expect_cstr_match(reader, "image");
    mpack_expect_cstr(reader, value, sizeof(value));
    state.imagePath = value;
    mpack_expect_cstr_match(reader, "ejecting");
    state.isEjecting = mpack_expect_bool(reader);
}

void saveDiskMetadata(mpack_writer_t *writer, const ClemensWOZDisk &container,
                      const ClemensBackendDiskDriveState &state) {
    //  serialize the drive state - we only need the path and whether we are
    //  ejecting the disk, which is state managed by the host's backend vs the
    //  disk device
    //  the nibbilized disk is stored within the machine and is saved there

    mpack_build_map(writer);

    saveBackendDiskDriveState(writer, state);

    //  we only need to serialize the WOZ INFO header since the TMAP and TRKS
    //  chunks are defined in the nibblized disk
    mpack_write_cstr(writer, "woz.version");
    mpack_write_u32(writer, container.version);
    mpack_write_cstr(writer, "woz.disk_type");
    mpack_write_u32(writer, container.disk_type);
    mpack_write_cstr(writer, "woz.flags");
    mpack_write_u32(writer, container.flags);
    mpack_write_cstr(writer, "woz.creator");
    mpack_write_bin(writer, container.creator, sizeof(container.creator));
    mpack_write_cstr(writer, "woz.boot_type");
    mpack_write_u32(writer, container.boot_type);
    mpack_write_cstr(writer, "woz.max_track_size_bytes");
    mpack_write_u32(writer, container.max_track_size_bytes);

    mpack_complete_map(writer);
}

bool loadDiskMetadata(mpack_reader_t *reader, ClemensWOZDisk &container,
                      ClemensBackendDiskDriveState &state) {
    //  unserialize the drive state
    //  unserialize the WOZ header
    //  the nibbilized disk is unserialized into the machine object and is not
    //    handled here
    mpack_expect_map(reader);

    loadBackendDiskDriveState(reader, state);

    //  we only need to serialize the WOZ INFO header since the TMAP and TRKS
    //  chunks are defined in the nibblized disk
    mpack_expect_cstr_match(reader, "woz.version");
    container.version = mpack_expect_u32(reader);
    mpack_expect_cstr_match(reader, "woz.disk_type");
    container.disk_type = mpack_expect_u32(reader);
    mpack_expect_cstr_match(reader, "woz.flags");
    container.flags = mpack_expect_u32(reader);
    mpack_expect_cstr_match(reader, "woz.creator");
    mpack_expect_bin_buf(reader, container.creator, sizeof(container.creator));
    mpack_expect_cstr_match(reader, "woz.boot_type");
    container.boot_type = mpack_expect_u32(reader);
    mpack_expect_cstr_match(reader, "woz.max_track_size_bytes");
    container.max_track_size_bytes = mpack_expect_u32(reader);

    mpack_done_map(reader);
    return true;
}

void saveSmartPortMetadata(mpack_writer_t *writer, ClemensSmartPortDevice *device,
                           const ClemensSmartPortDisk &disk,
                           const ClemensBackendDiskDriveState &state) {
    mpack_build_map(writer);

    saveBackendDiskDriveState(writer, state);

    mpack_write_cstr(writer, "disk");
    if (disk.hasImage()) {
        disk.serialize(writer, device);
    } else {
        mpack_write_nil(writer);
    }
    mpack_complete_map(writer);
}

bool loadSmartPortMetadata(mpack_reader_t *reader, ClemensSmartPortDevice *device,
                           ClemensSmartPortDisk &disk, ClemensBackendDiskDriveState &state,
                           ClemensSerializerAllocateCb alloc_cb, void *context) {
    mpack_expect_map(reader);

    loadBackendDiskDriveState(reader, state);

    mpack_expect_cstr_match(reader, "disk");
    if (mpack_peek_tag(reader).type == mpack_type_nil) {
        mpack_expect_nil(reader);
    } else {
        //  creating the device is important so that all callbacks and bindings
        //  to the emulator are ready before unserializing the device data.
        disk.createSmartPortDevice(device);
        disk.unserialize(reader, device, alloc_cb, context);
    }
    mpack_done_map(reader);

    return true;
}

bool save(std::string outputPath, ClemensMachine *machine, ClemensMMIO *mmio, size_t driveCount,
          const ClemensWOZDisk *containers, const ClemensBackendDiskDriveState *driveStates,
          size_t smartPortCount, const ClemensSmartPortDisk *smartPortDisks,
          const ClemensBackendDiskDriveState *smartPortStates,
          const std::vector<ClemensBackendBreakpoint> &breakpoints) {
    mpack_writer_t writer;
    //  this save buffer is probably, unnecessarily big - but it's just used for
    //  saves and freed
    //
    //  {
    //    machine state
    //    bram blob
    //    disk[ { woz/2img, path }]
    //  }
    //
    mpack_writer_init_filename(&writer, outputPath.c_str());
    if (mpack_writer_error(&writer) != mpack_ok) {
        return false;
    }

    mpack_build_map(&writer);
    mpack_write_cstr(&writer, "machine");
    //  TODO: ROM1 machine ROM version needs to be serialized.. in the clemens
    //        library so remember to do this
    clemens_serialize_machine(&writer, machine);
    mpack_write_cstr(&writer, "mmio");
    clemens_serialize_mmio(&writer, mmio);

    mpack_write_cstr(&writer, "bram");
    mpack_write_bin(&writer, (char *)clemens_rtc_get_bram(mmio, NULL), CLEM_RTC_BRAM_SIZE);

    //  slots and cards indices are linked 1:1 here - this means card names
    //  are considered unique - if this changes, then we'll have to redo this
    mpack_write_cstr(&writer, "slots");
    {
        mpack_start_array(&writer, CLEM_CARD_SLOT_COUNT);
        for (int slotIndex = 0; slotIndex < CLEM_CARD_SLOT_COUNT; ++slotIndex) {
            // TODO: clemens_mmio_card_get_name()
            const char *cardName =
                (mmio->card_slot[slotIndex] != NULL)
                    ? mmio->card_slot[slotIndex]->io_name(mmio->card_slot[slotIndex]->context)
                    : NULL;
            mpack_write_cstr_or_nil(&writer, cardName);
        }
        mpack_finish_array(&writer);
    }
    mpack_write_cstr(&writer, "cards");
    {
        mpack_build_map(&writer);
        for (int slotIndex = 0; slotIndex < CLEM_CARD_SLOT_COUNT; ++slotIndex) {
            if (!mmio->card_slot[slotIndex])
                continue;
            const char *cardName =
                mmio->card_slot[slotIndex]->io_name(mmio->card_slot[slotIndex]->context);
            mpack_write_cstr(&writer, cardName);
            if (!strncmp(cardName, kClemensCardMockingboardName, 64)) {
                clem_card_mockingboard_serialize(&writer, mmio->card_slot[slotIndex]);
            } else {
                mpack_write_nil(&writer);
            }
        }
        mpack_complete_map(&writer);
    }
    mpack_write_cstr(&writer, "disks");
    {
        mpack_start_array(&writer, driveCount);
        for (size_t driveIndex = 0; driveIndex < driveCount; ++driveIndex) {
            saveDiskMetadata(&writer, containers[driveIndex], driveStates[driveIndex]);
        }
        mpack_finish_array(&writer);
    }
    mpack_write_cstr(&writer, "smartport");
    {
        mpack_start_array(&writer, smartPortCount);
        for (size_t driveIndex = 0; driveIndex < smartPortCount; ++driveIndex) {
            saveSmartPortMetadata(&writer, &mmio->active_drives.smartport[driveIndex].device,
                                  smartPortDisks[driveIndex], smartPortStates[driveIndex]);
        }
        mpack_finish_array(&writer);
    }
    mpack_write_cstr(&writer, "breakpoints");
    {
        mpack_start_array(&writer, (uint32_t)(breakpoints.size() & 0xffffffff));
        for (auto &breakpoint : breakpoints) {
            mpack_build_map(&writer);
            mpack_write_cstr(&writer, "type");
            mpack_write_i32(&writer, static_cast<int>(breakpoint.type));
            mpack_write_cstr(&writer, "address");
            mpack_write_u32(&writer, breakpoint.address);
            mpack_complete_map(&writer);
        }
        mpack_finish_array(&writer);
    }
    mpack_complete_map(&writer);
    auto writerError = mpack_writer_destroy(&writer);
    if (writerError != mpack_ok) {
        fmt::print("serializer save failed with error {}@ \n", mpack_error_to_string(writerError));
    }
    return (writerError == mpack_ok);
}

bool load(std::string outputPath, ClemensMachine *machine, ClemensMMIO *mmio, size_t driveCount,
          ClemensWOZDisk *containers, ClemensBackendDiskDriveState *driveStates,
          size_t smartPortCount, ClemensSmartPortDisk *smartPortDisks,
          ClemensBackendDiskDriveState *smartPortStates,
          std::vector<ClemensBackendBreakpoint> &breakpoints, ClemensSerializerAllocateCb alloc_cb,
          void *context) {
    char str[256];

    mpack_reader_t reader;

    mpack_reader_init_filename(&reader, outputPath.c_str());
    if (mpack_reader_error(&reader) != mpack_ok) {
        return false;
    }
    mpack_expect_map(&reader);
    //  "machine"
    mpack_expect_cstr_match(&reader, "machine");
    if (!clemens_unserialize_machine(&reader, machine, alloc_cb, context)) {
        // power off the machine
        mpack_reader_destroy(&reader);
        return false;
    }
    mpack_expect_cstr_match(&reader, "mmio");
    if (!clemens_unserialize_mmio(&reader, mmio, alloc_cb, context)) {
        // power off the machine
        mpack_reader_destroy(&reader);
        return false;
    }
    // "bram"
    mpack_expect_cstr(&reader, str, sizeof(str));
    if (mpack_expect_bin(&reader) == CLEM_RTC_BRAM_SIZE) {
        mpack_read_bytes(&reader, (char *)mmio->dev_rtc.bram, CLEM_RTC_BRAM_SIZE);
    }
    mpack_done_bin(&reader);
    clemens_rtc_set_bram_dirty(mmio);

    //  slots and card data - see saveState TODOs that address why this is
    //  hardcoded for now.
    std::array<std::string, 7> slots;
    mpack_expect_cstr_match(&reader, "slots");
    {
        mpack_expect_array_match(&reader, 7);
        for (int i = 0; i < 7; ++i) {
            // TODO: allow card slot configuration - right now we hard code cards into
            //       their slots
            if (mpack_peek_tag(&reader).type != mpack_type_nil) {
                mpack_expect_cstr(&reader, str, sizeof(str));
                slots[i] = str;
            } else {
                mpack_expect_nil(&reader);
            }
        }
        mpack_done_array(&reader);
    }
    mpack_expect_cstr_match(&reader, "cards");
    {
        uint32_t card_count = mpack_expect_map(&reader);
        for (uint32_t i = 0; i < card_count; ++i) {
            mpack_expect_cstr(&reader, str, sizeof(str));
            int slotId = -1;
            for (uint32_t slotIndex = 0; slotIndex < 7; ++slotIndex) {
                if (slots[slotIndex] == str) {
                    slotId = slotIndex;
                    break;
                }
            }
            if (slotId >= 0) {
                if (!strncmp(str, slots[slotId].c_str(), sizeof(str))) {
                    destroyCard(mmio->card_slot[slotId]);
                    mmio->card_slot[slotId] = createCard(kClemensCardMockingboardName);
                    if (!strncmp(str, kClemensCardMockingboardName, sizeof(str))) {
                        clem_card_mockingboard_unserialize(&reader, mmio->card_slot[slotId],
                                                           alloc_cb, context);
                    } else {
                        mpack_expect_nil(&reader);
                    }
                }
                continue;
            }
            mpack_expect_nil(&reader);
        }
        mpack_done_map(&reader);
    }

    //  "disks"
    //  load woz filenames - the actual images have already been
    //  unserialized inside clemens_unserialize_machine
    mpack_expect_cstr_match(&reader, "disks");
    {
        unsigned count = mpack_expect_array(&reader);
        count = std::min(count, (unsigned)driveCount);
        for (size_t driveIndex = 0; driveIndex < count; ++driveIndex) {
            if (!loadDiskMetadata(&reader, containers[driveIndex], driveStates[driveIndex])) {
                fmt::print(
                    "Loading emulator snapshot failed due to disk image '{}' at drive '{}'.",
                    driveStates[driveIndex].imagePath.empty() ? "none defined"
                                                              : driveStates[driveIndex].imagePath,
                    ClemensDiskUtilities::getDriveName(static_cast<ClemensDriveType>(driveIndex)));
                return false;
            }
        }
        mpack_done_array(&reader);
    }

    mpack_expect_cstr_match(&reader, "smartport");
    {
        unsigned count = mpack_expect_array(&reader);
        count = std::min(count, (unsigned)smartPortCount);
        for (size_t driveIndex = 0; driveIndex < count; ++driveIndex) {
            if (!loadSmartPortMetadata(&reader, &mmio->active_drives.smartport[driveIndex].device,
                                       smartPortDisks[driveIndex], smartPortStates[driveIndex],
                                       alloc_cb, context)) {
                fmt::print(
                    "Loading emulator snapshot failed due to SmartPort image '{}' at drive '{}'.",
                    driveStates[driveIndex].imagePath.empty() ? "none defined"
                                                              : driveStates[driveIndex].imagePath,
                    driveIndex);
                return false;
            }
        }
        mpack_done_array(&reader);
    }

    mpack_expect_cstr_match(&reader, "breakpoints");
    {
        uint32_t breakpointCount = mpack_expect_array(&reader);
        breakpoints.clear();
        breakpoints.reserve(breakpointCount);
        for (uint32_t breakpointIdx = 0; breakpointIdx < breakpointCount; ++breakpointIdx) {
            breakpoints.emplace_back();
            auto &breakpoint = breakpoints.back();
            mpack_expect_map(&reader);
            mpack_expect_cstr_match(&reader, "type");
            breakpoint.type =
                static_cast<ClemensBackendBreakpoint::Type>(mpack_expect_i32(&reader));
            mpack_expect_cstr_match(&reader, "address");
            breakpoint.address = mpack_expect_u32(&reader);
            mpack_done_map(&reader);
        }
        mpack_done_array(&reader);
    }
    mpack_done_map(&reader);
    auto readerError = mpack_reader_destroy(&reader);
    if (readerError != mpack_ok) {
        fmt::print("serializer load failed with error: {}\n", mpack_error_to_string(readerError));
    }
    return (readerError == mpack_ok);
}

} // namespace ClemensSerializer
