#ifndef CLEM_HOST_IMPORT_DISK_HPP
#define CLEM_HOST_IMPORT_DISK_HPP

#include "clem_woz.h"
#include "cinek/fixedstack.hpp"

#include <string>
#include <vector>
#include <utility>

struct Clemens2IMGDisk;

class ClemensDiskImporter {
public:
  ClemensDiskImporter(ClemensDriveType driveType, size_t count);
  ~ClemensDiskImporter();
  ClemensWOZDisk* add(std::string path);
  bool build(std::string outputPath);

private:
  struct DiskRecord {
    ClemensWOZDisk disk;
    char name[128];
    DiskRecord* next;
  };

  size_t calculateRequiredMemory(ClemensDriveType driveType, size_t count) const;
  DiskRecord* parseWOZ(uint8_t* bits_data, uint8_t* bits_data_end);
  DiskRecord* parse2IMG(uint8_t* bits_data, uint8_t* bits_data_end);
  DiskRecord* parseImage(uint8_t* bits_data, uint8_t* bits_data_end, unsigned type);
  DiskRecord* nibblizeImage(Clemens2IMGDisk* disk);

  cinek::FixedStack memory_;
  DiskRecord* head_;
  DiskRecord* tail_;
  ClemensDriveType driveType_;
};

#endif
