#ifndef CLEM_HOST_SETTINGS_UI_HPP
#define CLEM_HOST_SETTINGS_UI_HPP

#include "clem_configuration.hpp"
#include "clem_file_browser.hpp"

class ClemensSettingsUI {
  public:
    ClemensSettingsUI(ClemensConfiguration &config);

    void start();
    void stop();

    bool frame();

  private:
    ClemensConfiguration &config_;
    ClemensFileBrowser fileBrowser_;
    enum class Mode { None, Main, ROMFileBrowse };
    Mode mode_;
    bool romFileExists_;

    int cardCounts_[CLEM_CARD_SLOT_COUNT];
};

#endif
