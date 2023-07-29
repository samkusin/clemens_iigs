#ifndef CLEM_HOST_LOAD_SNAPSHOT_UI_HPP
#define CLEM_HOST_LOAD_SNAPSHOT_UI_HPP

#include <string>
#include <vector>

#include "core/clem_snapshot.hpp"

class ClemensCommandQueue;

class ClemensLoadSnapshotUI {
  public:
    bool isStarted() const;
    void start(ClemensCommandQueue &backend, const std::string &snapshotDir);
    bool frame(float width, float height, ClemensCommandQueue &backend);
    void stop(ClemensCommandQueue &backend);
    void succeeded();
    void fail();

  private:
    enum class Mode { None, Browser, WaitForResponse, Succeeded, Failed, Cancelled };

    Mode mode_ = Mode::None;
    std::string snapshotDir_;
    char snapshotName_[128];
    bool resumeExecutionOnExit_;

    void refresh();
    void loadSnapshotImage(unsigned snapshotIndex);

    std::vector<std::string> snapshotNames_;
    std::vector<ClemensSnapshotMetadata> snapshotMetadatas_;

    uintptr_t snapshotImage_ = 0;
    int snapshotImageWidth_ = 0;
    int snapshotImageHeight_ = 0;
    int snapshotIndex_ = -1;
};

#endif
