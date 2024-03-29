#ifndef CLEM_HOST_SERIALIZER_HPP
#define CLEM_HOST_SERIALIZER_HPP

#include "clem_apple2gs_config.hpp"
#include "external/mpack.h"

#include <array>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include <functional>

class ClemensAppleIIGS;
class ClemensSystemListener;

struct ClemensSnapshotMetadata {
    time_t timestamp;
    std::array<std::string, kClemensDrive_Count> disks;
    std::array<std::string, kClemensSmartPortDiskLimit> smartDisks;
    std::vector<uint8_t> imageData;   // png
};

struct ClemensSnapshotPNG {
  const unsigned char* data;
  size_t size;
};

class ClemensSnapshot {
  public:
    static constexpr uint32_t kClemensSnapshotVersion = 1;

    ClemensSnapshot(const std::string &path);

    bool serialize(ClemensAppleIIGS &gs, const ClemensSnapshotPNG& image,
                   std::function<bool(mpack_writer_t *, ClemensAppleIIGS &)> customCb);
    std::unique_ptr<ClemensAppleIIGS>
    unserialize(ClemensSystemListener &listener,
                std::function<bool(mpack_reader_t *, ClemensAppleIIGS &)> customCb);

    std::pair<ClemensSnapshotMetadata, bool> unserializeMetadata();

  private:
    bool unserializeHeader(FILE *fp);
    std::pair<ClemensSnapshotMetadata, bool> unserializeMetadata(mpack_reader_t &reader);

  private:
    std::string path_;
    std::string origin_;
    enum class ValidationStep { None, Header, Metadata, Machine, Custom };
    ValidationStep validationStep_;
    std::string validationData_;

    void validation(ValidationStep step);
    void validationError(const std::string &tag);
};

#endif
