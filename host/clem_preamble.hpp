#ifndef CLEM_HOST_PREAMBLE_HPP
#define CLEM_HOST_PREAMBLE_HPP

#include "clem_host_shared.hpp"

struct ClemensConfiguration;

/**
 * @brief Handle pre-initialization and any other premlinary GUI before running the emulator.
 *
 * This class extends ClemensFront GUI by introducing the emulator and loading the configuration.
 *
 */
class ClemensPreamble {
  public:
    enum class Result { Ok, Active, Exit };

    ClemensPreamble(ClemensConfiguration &config);

    Result frame(int width, int height);

  private:
    ClemensConfiguration &config_;
    enum Mode {
        //  Load configuration and check whether to display welcome or proceed to
        //  the emulator
        Start,
        //  This is a new version (or first time run) so display the new version
        //  welcome screen
        NewVersion,
        //  First time user help
        FirstUse,
        //  Something happened where we need to exit the app
        Exit,
        //  Everything OK - run the emulator!
        Continue
    };

    Mode mode_;
};

#endif
