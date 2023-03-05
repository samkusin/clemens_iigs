#ifndef CLEM_HOST_SETTINGS_UI_HPP
#define CLEM_HOST_SETTINGS_UI_HPP

#include "clem_configuration.hpp"

class ClemensSettingsUI {
  public:
    bool isStarted() const;
    void start(const ClemensConfiguration &config);
    bool frame(float width, float height);
    ClemensConfiguration getConfiguration() const { return config_; }
    bool shouldBeCommitted() const;
    void stop();

  private:
    enum class Mode { None, Active, ROMFileBrowse, ROMFileBrowseError, Commit, Cancelled };
    Mode mode_ = Mode::None;
    ClemensConfiguration config_;
    std::string errorMessage_;
    char romFilename_[64];
    int ramSizeKB_;
    bool nonstandardRAMSize_;
};

#endif
