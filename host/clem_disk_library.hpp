#ifndef CLEM_HOST_DISK_LIBRARY_HPP
#define CLEM_HOST_DISK_LIBRARY_HPP

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "clem_disk.h"

//  Contains entries for all imported disk sets.
//  - Add an entry keyed by pathname relative to the configured library folder.
//  - Iteration is the most common operation (for UI purposes)
//  - Add is performed on demand on a sorted vector
//  - Remove is not supported - the idea is that the library is recreated
//    periodically from scratch.  This limiation may be addressed if
//    performance due to filesystem issues becomes a concern.
//
class ClemensDiskLibrary {
  public:
    ClemensDiskLibrary(std::filesystem::path libraryRootPath, unsigned diskType,
                       unsigned diskSetInitialCount, unsigned diskEntryInitialCount);

    void reset(unsigned diskType);
    void update();

    struct DiskEntry {
        std::filesystem::path location;
        unsigned diskType = CLEM_DISK_TYPE_NONE;
        explicit operator bool() const { return diskType != CLEM_DISK_TYPE_NONE; }
    };

    void iterate(std::function<void(const DiskEntry &)> callback);
    void iterateSets(std::function<void(const DiskEntry &)> callback);

    const std::filesystem::path getLibraryRootPath() const { return libraryRootPath_; }

  private:
    struct DiskEntryNode {
        DiskEntry entry;
        int nextEntryIndex = -1;
    };
    unsigned diskType_;
    std::filesystem::path libraryRootPath_;
    std::filesystem::directory_iterator libraryRootIterator_;
    std::vector<DiskEntryNode> diskSets_;
    std::vector<DiskEntryNode> diskEntries_;
};

#endif
