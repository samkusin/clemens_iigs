#include "clem_import_disk.hpp"

#include "clem_2img.h"
#include "clem_disk_utils.hpp"
#include "clem_woz.h"

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>

size_t ClemensDiskImporter::calculateRequiredMemory(ClemensDriveType driveType,
                                                    size_t count) const {
    size_t size = ClemensDiskUtilities::calculateNibRequiredMemory(driveType) * count;
    assert(size > 0);
    // account for larger source disk input data - this is probably overkill but
    // but should cover all edge cases
    size += (size * 2);
    // account for metadata in the output 2mgs - again overkill
    size += (count * 1024);
    // the actual input disk records
    size += CK_ALIGN_SIZE_TO_ARCH(sizeof(DiskRecord) * count);
    size += CK_ALIGN_SIZE_TO_ARCH(sizeof(ClemensNibbleDisk) * count);
    return size;
}

ClemensDiskImporter::ClemensDiskImporter(ClemensDriveType driveType, size_t count)
    : memory_(calculateRequiredMemory(driveType, count),
              malloc(calculateRequiredMemory(driveType, count))),
      head_(nullptr), tail_(nullptr), driveType_(driveType) {}

ClemensDiskImporter::~ClemensDiskImporter() { free(memory_.getHead()); }

ClemensWOZDisk *ClemensDiskImporter::add(std::string path) {
    std::filesystem::path fsPath = path;
    std::ifstream input(fsPath, std::ios_base::in | std::ios_base::binary);
    auto fsPathExtension = fsPath.extension();
    if (input.fail()) {
        return nullptr;
    }

    input.seekg(0, std::ios_base::end);
    size_t inputImageSize = input.tellg();
    if (inputImageSize > memory_.remaining()) {
        return nullptr;
    }

    uint8_t *bits_start = (uint8_t *)memory_.allocate(inputImageSize);
    uint8_t *bits_end = bits_start + inputImageSize;
    input.seekg(0);
    input.read((char *)bits_start, inputImageSize);
    if (input.fail() || input.bad())
        return nullptr;

    DiskRecord *record = nullptr;
    if (fsPathExtension == ".woz") {
        record = parseWOZ(bits_start, bits_end);
    } else if (fsPathExtension == ".2mg") {
        record = parse2IMG(bits_start, bits_end);
    } else if (fsPathExtension == ".dsk" || fsPathExtension == ".do") {
        record = parseImage(bits_start, bits_end, CLEM_DISK_FORMAT_DOS);
    } else if (fsPathExtension == ".po") {
        record = parseImage(bits_start, bits_end, CLEM_DISK_FORMAT_PRODOS);
    }
    if (!record)
        return nullptr;
    auto fsName = fsPath.filename();
    fsName.string().copy(record->name, sizeof(record->name), 0);
    if (!head_) {
        head_ = record;
        tail_ = record;
    } else {
        tail_->next = record;
    }
    tail_ = record;
    return &record->disk;
}

auto ClemensDiskImporter::parseWOZ(uint8_t *bits_data, uint8_t *bits_data_end) -> DiskRecord * {
    DiskRecord *record = memory_.newItem<DiskRecord>();
    if (!record)
        return nullptr;

    ClemensWOZDisk *sourceDisk = &record->disk;
    sourceDisk->nib = memory_.newItem<ClemensNibbleDisk>();
    if (!sourceDisk->nib)
        return nullptr;
    size_t bits_size = ClemensDiskUtilities::calculateNibRequiredMemory(driveType_);
    sourceDisk->nib->bits_data = (uint8_t *)memory_.allocate(bits_size);
    sourceDisk->nib->bits_data_end = sourceDisk->nib->bits_data + bits_size;

    cinek::ConstRange<uint8_t> buffer(bits_data, bits_data_end);
    if (!ClemensDiskUtilities::parseWOZ(sourceDisk, buffer)) {
        return nullptr;
    }
    record->name[0] = '\0';
    record->next = nullptr;
    return record;
}

// TODO: move equivalent 2IMG, etc parse utilities into ClemensDiskUtilities

auto ClemensDiskImporter::parse2IMG(uint8_t *bits_data, uint8_t *bits_data_end) -> DiskRecord * {
    struct Clemens2IMGDisk disk {};
    disk.nib = memory_.newItem<ClemensNibbleDisk>();
    if (!clem_2img_parse_header(&disk, bits_data, bits_data_end))
        return nullptr;

    return nibblizeImage(&disk);
}

auto ClemensDiskImporter::parseImage(uint8_t *bits_data, uint8_t *bits_data_end, unsigned type)
    -> DiskRecord * {
    struct Clemens2IMGDisk disk {};
    disk.nib = memory_.newItem<ClemensNibbleDisk>();
    if (!clem_2img_generate_header(&disk, type, bits_data, bits_data_end, 0))
        return nullptr;

    return nibblizeImage(&disk);
}

auto ClemensDiskImporter::nibblizeImage(Clemens2IMGDisk *disk) -> DiskRecord * {
    if (!disk->nib)
        return nullptr;

    size_t bits_size = ClemensDiskUtilities::calculateNibRequiredMemory(driveType_);
    disk->nib->bits_data = (uint8_t *)memory_.allocate(bits_size);
    disk->nib->bits_data_end = disk->nib->bits_data + bits_size;
    if (disk->block_count <= 280 || (disk->data_end - disk->data) <= 140 * 1024) {
        disk->nib->disk_type = CLEM_DISK_TYPE_5_25;
    } else if (disk->block_count <= 1600 || (disk->data_end - disk->data) <= 800 * 1024) {
        disk->nib->disk_type = CLEM_DISK_TYPE_3_5;
    } else {
        disk->nib->disk_type = CLEM_DISK_TYPE_NONE;
    }

    if (!clem_2img_nibblize_data(disk))
        return nullptr;

    DiskRecord *record = memory_.newItem<DiskRecord>();
    if (!record)
        return nullptr;

    ClemensDiskUtilities::createWOZ(&record->disk, disk->nib);

    record->name[0] = '\0';
    record->next = nullptr;
    return record;
}

bool ClemensDiskImporter::build(std::string outputDirPath) {
    unsigned scratchBufferSize = 0;
    for (DiskRecord *record = head_; record; record = record->next) {
        scratchBufferSize = std::max(scratchBufferSize, record->disk.max_track_size_bytes *
                                                            record->disk.nib->track_count);
    }
    scratchBufferSize += 4096;

    uint8_t *writeBuffer = reinterpret_cast<uint8_t *>(memory_.allocate(scratchBufferSize));
    for (DiskRecord *record = head_; record; record = record->next) {
        size_t writeOutCount = scratchBufferSize;
        if (!clem_woz_serialize(&record->disk, writeBuffer, &writeOutCount)) {
            return false;
        }
        auto outputFileName =
            std::filesystem::path(std::string(record->name)).stem().string() + ".woz";
        auto outputFilePath = std::filesystem::path(outputDirPath) / outputFileName;
        std::ofstream out(outputFilePath, std::ios_base::out | std::ios_base::binary);
        if (out.fail()) {
            return false;
        }
        out.write((char *)writeBuffer, writeOutCount);
        if (out.fail() || out.bad()) {
            return false;
        }
    }
    return true;
}
