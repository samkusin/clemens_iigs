#ifndef CLEM_HOST_SETTINGS_UI_HPP
#define CLEM_HOST_SETTINGS_UI_HPP

#include "clem_configuration.hpp"

class ClemensSettingsUI {
  public:
    ClemensSettingsUI(ClemensConfiguration &config);
    void frame();

  private:
    ClemensConfiguration &config_;
};

#endif
