

#include "machine.hpp"

#include "fmt/color.h"
#include "fmt/format.h"

#include "disklib/clem_storage_unit.hpp"

int main(int argc, const char *argv[]) {
    ClemensTestHarness harness;
    ClemensStorageUnit storage;

    harness.reset();
    harness.step(16, 0);

    if (!storage.insertDisk(harness.getMMIO(), kClemensDrive_5_25_D1, "data/dos_3_3_master.woz")) {
        harness.log(CLEM_DEBUG_LOG_WARN, "Failed to insert disk");
    }
    harness.step(1000, 0);
    storage.update(harness.getMMIO());
    storage.ejectDisk(harness.getMMIO(), kClemensDrive_5_25_D1);
    if (harness.hasFailed()) {
        fmt::print(stderr, fg(fmt::terminal_color::red), "FAILED\n");
    } else {
        fmt::print(fg(fmt::terminal_color::bright_green), "OK\n");
    }
    return 0;
}
