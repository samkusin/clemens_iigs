

#include "machine.hpp"

#include "fmt/color.h"
#include "fmt/format.h"

#include "json.hpp"

#include <fstream>
#include <iostream>

/*
 harness.reset();
    harness.step(16, 0);

    if (!storage.insertDisk(harness.getMMIO(), kClemensDrive_5_25_D1, "data/dos_3_3_master.woz")) {
        harness.log(CLEM_DEBUG_LOG_WARN, "Failed to insert disk");
    }
    harness.step(1000, 0);
    storage.update(harness.getMMIO());
    storage.ejectDisk(harness.getMMIO(), kClemensDrive_5_25_D1);
*/

/*
    { "act": "reset" },
    { "act": "step", "param": 100 },
    { "act": "insert_disk", "param": {"drive":"s6d1", "disk":"data/dos_3_3_master.woz"]},
*/

bool execute(ClemensTestHarness &harness, std::basic_istream<char> &input) {
    nlohmann::json manifest;
    bool commandOk = true;

    while (!input.eof() && !input.fail() && commandOk && !harness.hasFailed()) {
        auto manifest = nlohmann::json::parse(input, nullptr, false);
        if (manifest.is_discarded()) {
            fmt::print(stderr, "Parse error\n");
            return false;
        }
        commandOk = false;
        if (manifest.is_object()) {
            commandOk = harness.run(manifest);
            if (!commandOk) {
                fmt::print(stderr, "Command syntax not valid:\n{}\n", manifest.dump());
            }
        } else if (manifest.is_array()) {
            for (auto item : manifest) {
                if (item.is_object()) {
                    commandOk = harness.run(item);
                } else {
                    commandOk = false;
                }
                if (!commandOk) {
                    fmt::print(stderr, "Command syntax not valid:\n{}\n", item.dump());
                    break;
                }
            }
        }
    }

    return commandOk;
}

int main(int argc, const char *argv[]) {
    ClemensTestHarness harness;
    int argsUsed = 1;
    int argsExpected = 0;
    const char *argOption = nullptr;

    while (argsUsed < argc - 1) {
        // TODO add arguments
        argsUsed++;
    }
    if (argsExpected != 0) {
        std::cerr << "Argument " << argOption << " expects a value." << std::endl;
        exit(1);
    }

    bool executeSuccess = false;
    if (argsUsed < argc) {
        std::ifstream inp(argv[argsUsed]);
        if (!inp.is_open()) {
            std::cerr << "Failed to open input stream " << argv[argsUsed] << std::endl;
            exit(1);
        }
        executeSuccess = execute(harness, inp);
    } else {
        executeSuccess = execute(harness, std::cin);
    }

    if (harness.hasFailed() || !executeSuccess) {
        fmt::print(stderr, fg(fmt::terminal_color::red), "FAILED\n");
    } else {
        fmt::print(fg(fmt::terminal_color::bright_green), "OK\n");
    }
    return 0;
}
