#ifndef CLEM_HOST_SETTINGS_UI_HPP
#define CLEM_HOST_SETTINGS_UI_HPP

#include "clem_configuration.hpp"
#include "clem_file_browser.hpp"

class ClemensSettingsUI {
  public:
    ClemensSettingsUI(ClemensConfiguration &config);

    void start();
    void stop();

    void frame();

  private:
    ClemensConfiguration &config_;
    ClemensFileBrowser fileBrowser_;
    enum class Mode { None, Main, ROMFileBrowse };
    Mode mode_;
};

#endif
