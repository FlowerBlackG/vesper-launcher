// SPDX-License-Identifier: MulanPSL-2.0

/*
 * Vesper Launcher Entry
 * 创建于 2024年2月25日 京沪高铁上
 */

#include <iostream>
#include <set>
#include <map>
#include <string>
#include <vector>

#include "./log/Log.h"
#include "config.h"

using namespace std;

static struct {
    map<string, string> variables;
    set<string> flags;
    vector<string> values;
} userArgs;

static map<string, string> envVars;

static struct {
    int __todo = 0; // todo
} config;

static void usage() {
    cout << "vesper-launcher " << VESPER_LAUNCHER_VERSION_NAME << endl;
    cout << "check ";
    ConsoleColorPad::setCoutColor(0x2f, 0x90, 0xb9);
    cout << "https://github.com/FlowerBlackG/vesper-launcher";
    ConsoleColorPad::setCoutColor();
    cout << " for usage." << endl;
}

static void version() {
    cout << "vesper launcher" << endl;
    cout << "  version: "
        << VESPER_LAUNCHER_VERSION_NAME
        << " (" 
        << VESPER_LAUNCHER_VERSION_CODE  
        << ")"
        << endl;
}

static int parseArgs(int argc, const char** argv) {

    auto& flags = userArgs.flags;
    auto& variables = userArgs.variables;
    auto& values = userArgs.values;
    
    string key;
    string value;

    for (int idx = 1; idx < argc; idx++) {
        value = argv[idx];
        bool isKey = value.starts_with("--");
        if (isKey) {
            if (key != "") {
                if (flags.contains(key)) {
                    LOG_WARN("redefine: ", key);
                }
                flags.insert(key);
            }

            key = value;
        } else { // not a key
            if (key != "") {
                if (variables.contains(key)) {
                    LOG_WARN("redefine: ", key, ", which previously is ", variables[key]);
                }
                variables[key] = value;
                key.clear();
            } else {
                values.push_back(value);
            }
        }
    }

    if (key != "") {
        if (flags.contains(key)) {
            LOG_WARN("redefine: ", key);
        }
        flags.insert(key);
    }

    return 0;
}

static void processEnvVars(const char** env) {
    for (int i = 0; env[i]; i++) {
        string it = env[i];
        auto splitPos = it.find('=');
        if (splitPos == string::npos) {
            LOG_WARN("no '=' in env variable: ", it);
            continue;
        }
        envVars[it.substr(0, splitPos)] = it.substr(splitPos + 1);
    }
}

static int processPureQueryCmds() {
    if (userArgs.flags.contains("--version")) {
        version();
        return 1;
    } 
    
    if (userArgs.flags.contains("--usage") || userArgs.flags.contains("--help")) {
        usage();
        return 1;
    }

    return 0;
}

int main(int argc, const char* argv[], const char* env[]) {
    parseArgs(argc, argv);
    processEnvVars(env);

    if (processPureQueryCmds()) {
        return 0;
    }

    // todo

    return 0;
}
