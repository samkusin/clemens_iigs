#include "clem_disk_library.hpp"
#include "clem_woz.h"

#include "fmt/format.h"

#include <algorithm>
#include <fstream>

ClemensDiskLibrary::ClemensDiskLibrary(std::filesystem::path libraryRootPath, unsigned diskType,
                                       unsigned diskSetInitialCount, unsigned diskEntryInitialCount)
    : diskType_(diskType), libraryRootPath_(std::move(libraryRootPath)),
      libraryRootIterator_(libraryRootPath_) {
    diskSets_.reserve(diskSetInitialCount);
    diskEntries_.reserve(diskEntryInitialCount);
}

void ClemensDiskLibrary::reset(unsigned diskType) {
    diskSets_.clear();
    diskEntries_.clear();
    if (!std::filesystem::exists(libraryRootPath_)) {
        std::error_code errc{};
        if (!std::filesystem::create_directories(libraryRootPath_, errc)) {
            fmt::print(stderr, "Unable to create library directory {} with error {}\n",
                       libraryRootPath_.string(), errc.message());
        }
    }
    libraryRootIterator_ = std::filesystem::directory_iterator(libraryRootPath_);
    diskType_ = diskType;
}

void ClemensDiskLibrary::update() {
    if (libraryRootIterator_ == std::filesystem::directory_iterator()) {
        return;
    }

    //  add all disks within a set in one pass
    auto &dirEntry = *libraryRootIterator_;
    if (!dirEntry.is_directory()) {
        libraryRootIterator_++;
        return;
    }
    DiskEntryNode diskSetEntryNode;
    diskSetEntryNode.entry.location = dirEntry.path().filename();
    int prevDiskEntryIndex = -1;
    uint8_t wozBuffer[80];
    for (auto &childDirEntry : std::filesystem::directory_iterator(dirEntry.path())) {
        if (childDirEntry.path().extension() != ".woz")
            continue;

        std::ifstream wozFile(childDirEntry.path(), std::ios_base::in | std::ios_base::binary);
        if (!wozFile.is_open())
            continue;
        if (wozFile.read((char *)wozBuffer, sizeof(wozBuffer)).fail())
            continue;

        //  TODO: move into a utility
        //  This block reads the info chunk of a WOZ file for more info on the image
        struct ClemensWOZDisk wozDisk {};
        const uint8_t *bits_data_current = clem_woz_check_header(&wozBuffer[0], sizeof(wozBuffer));
        if (!bits_data_current)
            continue;
        const uint8_t *bits_data_end = &wozBuffer[0] + sizeof(wozBuffer);

        struct ClemensWOZChunkHeader chunkHeader;
        bool readInfoHeader = false;
        while ((bits_data_current =
                    clem_woz_parse_chunk_header(&chunkHeader, bits_data_current,
                                                bits_data_end - bits_data_current)) != nullptr) {
            switch (chunkHeader.type) {
            case CLEM_WOZ_CHUNK_INFO:
                bits_data_current = clem_woz_parse_info_chunk(
                    &wozDisk, &chunkHeader, bits_data_current, chunkHeader.data_size);
                readInfoHeader = true;
                break;
            default:
                break;
            }
            if (readInfoHeader)
                break;
        }
        if (!readInfoHeader)
            continue;
        //  the disk set will contain disks of the same type, so we can quickly move
        //  onto the next disk set if we find a disk image not matching our desired
        //  type
        if (wozDisk.disk_type != diskType_)
            break;

        //  add disk entry
        DiskEntryNode diskEntryNode;
        diskEntryNode.entry.location = childDirEntry.path();
        diskEntryNode.entry.diskType = wozDisk.disk_type;
        diskEntries_.emplace_back(diskEntryNode);

        int thisDiskEntryIndex = int(diskEntries_.size() - 1);
        if (diskSetEntryNode.nextEntryIndex == -1) {
            diskSetEntryNode.nextEntryIndex = thisDiskEntryIndex;
            prevDiskEntryIndex = diskSetEntryNode.nextEntryIndex;
        } else {
            diskEntries_[prevDiskEntryIndex].nextEntryIndex = thisDiskEntryIndex;
        }
        prevDiskEntryIndex = thisDiskEntryIndex;
    }
    if (diskSetEntryNode.nextEntryIndex >= 0) {
        auto it = std::lower_bound(diskSets_.begin(), diskSets_.end(), diskSetEntryNode,
                                   [](const DiskEntryNode &l, const DiskEntryNode &r) {
                                       return l.entry.location.string() < r.entry.location;
                                   });
        diskSets_.emplace(it, diskSetEntryNode);
    }
    libraryRootIterator_++;
}

void ClemensDiskLibrary::iterate(std::function<void(const DiskEntry &)> callback) {
    for (auto &diskSetNode : diskSets_) {
        int nextEntryIndex = diskSetNode.nextEntryIndex;
        while (nextEntryIndex >= 0) {
            callback(diskEntries_[nextEntryIndex].entry);
            nextEntryIndex = diskEntries_[nextEntryIndex].nextEntryIndex;
        }
    }
}

void ClemensDiskLibrary::iterateSets(std::function<void(const DiskEntry &)> callback) {
    for (auto &diskSetNode : diskSets_) {
        callback(diskSetNode.entry);
    }
}
