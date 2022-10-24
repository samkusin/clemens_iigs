#include "clem_serializer.hpp"
#include "clem_disk_utils.hpp"
#include "clem_host_utils.hpp"
#include "emulator.h"
#include "serializer.h"

#include "fmt/format.h"

#include "iocards/mockingboard.h"

namespace ClemensSerializer {

void saveDiskMetadata(mpack_writer_t* writer, const ClemensWOZDisk& container,
                      const ClemensBackendDiskDriveState& state) {
  //  serialize the drive state - we only need the path and whether we are
  //  ejecting the disk, which is state managed by the host's backend vs the
  //  disk device
  //  the nibbilized disk is stored within the machine and is saved there
  mpack_build_map(writer);

  mpack_write_cstr(writer, "image");
  mpack_write_cstr(writer, state.imagePath.c_str());
  mpack_write_cstr(writer, "ejecting");
  mpack_write_bool(writer, state.isEjecting);

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

bool loadDiskMetadata(mpack_reader_t* reader, ClemensWOZDisk& container,
                      ClemensBackendDiskDriveState& state) {
  //  unserialize the drive state
  //  unserialize the WOZ header
  //  the nibbilized disk is unserialized into the machine object and is not
  //    handled here
  char value[256];
  mpack_expect_map(reader);
  mpack_expect_cstr_match(reader, "image");
  mpack_expect_cstr(reader, value, sizeof(value));
  state.imagePath = value;
  mpack_expect_cstr_match(reader, "ejecting");
  state.isEjecting = mpack_expect_bool(reader);

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


bool save(std::string outputPath, ClemensMachine* machine, size_t driveCount,
          const ClemensWOZDisk* containers, const ClemensBackendDiskDriveState* states) {
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
  mpack_build_map(&writer);
  mpack_write_cstr(&writer, "machine");
  //  TODO: ROM1 machine ROM version needs to be serialized.. in the clemens
  //        library so remember to do this
  clemens_serialize_machine(&writer, machine);

  mpack_write_cstr(&writer, "bram");
  mpack_write_bin(&writer, (char *)clemens_rtc_get_bram(machine, NULL),
                  CLEM_RTC_BRAM_SIZE);

  mpack_write_cstr(&writer, "slots");
  {
    mpack_start_array(&writer, 7);
    for (int slotIndex = 0; slotIndex < 7; ++slotIndex) {
      const char* cardName = (machine->card_slot[slotIndex] != NULL) ?
          machine->card_slot[slotIndex]->io_name(machine->card_slot[slotIndex]->context) : NULL;
      mpack_write_cstr_or_nil(&writer, cardName);
    }
    mpack_finish_array(&writer);
  }
  mpack_write_cstr(&writer, "cards");
  {
    mpack_build_map(&writer);
    for (int slotIndex = 0; slotIndex < 7; ++slotIndex) {
      if (!machine->card_slot[slotIndex]) continue;
      const char* cardName =
        machine->card_slot[slotIndex]->io_name(machine->card_slot[slotIndex]->context);
      mpack_write_cstr(&writer, cardName);
      if (!strncmp(cardName, kClemensCardMockingboardName, 64)) {
        clem_card_mockingboard_serialize(&writer, machine->card_slot[slotIndex]);
      } else {
        mpack_write_nil(&writer);
      }
    }
    mpack_complete_map(&writer);
  }
  mpack_write_cstr(&writer, "disks");
  {
    mpack_start_array(&writer, kClemensDrive_Count);
    for (size_t driveIndex = 0; driveIndex < driveCount; ++driveIndex) {
      saveDiskMetadata(&writer, containers[driveIndex], states[driveIndex]);
    }
    mpack_finish_array(&writer);
  }
  mpack_complete_map(&writer);
  mpack_writer_destroy(&writer);
  return true;
}

bool load(std::string outputPath, ClemensMachine* machine, size_t driveCount,
          ClemensWOZDisk* containers, ClemensBackendDiskDriveState* states,
          ClemensSerializerAllocateCb alloc_cb,
          void *context) {
  char str[256];

  mpack_reader_t reader;
  mpack_reader_init_filename(&reader, outputPath.c_str());
  mpack_expect_map(&reader);
  //  "machine"
  mpack_expect_cstr(&reader, str, sizeof(str));
  if (!clemens_unserialize_machine(&reader, machine, alloc_cb, context)) {
    // power off the machine
    mpack_reader_destroy(&reader);
    return false;
  }
  // "bram"
  mpack_expect_cstr(&reader, str, sizeof(str));
  if (mpack_expect_bin(&reader) == CLEM_RTC_BRAM_SIZE) {
    mpack_read_bytes(&reader, (char *)machine->mmio.dev_rtc.bram,
                     CLEM_RTC_BRAM_SIZE);
  }
  mpack_done_bin(&reader);
  clemens_rtc_set_bram_dirty(machine);

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
      if (!strncmp(str, slots[i].c_str(), sizeof(str))) {
        destroyCard(machine->card_slot[i]);
        machine->card_slot[i] = createCard(kClemensCardMockingboardName);
        if (!strncmp(str, kClemensCardMockingboardName, sizeof(str))) {
          clem_card_mockingboard_unserialize(&reader, machine->card_slot[i], alloc_cb, context);
        } else {
          mpack_expect_nil(&reader);
        }
      } else {
        mpack_expect_nil(&reader);
      }
    }
    mpack_done_map(&reader);
  }

  //  "disks"
  //  load woz filenames - the actual images have already been
  //  unserialized inside clemens_unserialize_machine
  mpack_expect_cstr(&reader, str, sizeof(str));
  {
    mpack_expect_array(&reader);
    for (size_t driveIndex = 0; driveIndex < driveCount; ++driveIndex) {
      if (!loadDiskMetadata(&reader, containers[driveIndex], states[driveIndex])) {
        fmt::print("Loading emulator snapshot failed due to disk image '{}' at drive '{}'.",
                   states[driveIndex].imagePath.empty() ? "none defined" :
                   states[driveIndex].imagePath,
                   ClemensDiskUtilities::getDriveName(static_cast<ClemensDriveType>(driveIndex)));
        return false;
      }
    }
    mpack_done_array(&reader);
  }
  mpack_done_map(&reader);
  mpack_reader_destroy(&reader);
  return true;
}

}  // namespace ClemensSerializer
