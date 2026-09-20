#include <cassert>
#include <climits>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include "../src/AsxNanoStreamDecoderLight.h"

static std::vector<std::pair<std::string, int> > commands;

static void capture(String key, int value) {
    commands.push_back(std::make_pair(std::string(key.c_str()), value));
}

static void reject(const std::string& input) {
    commands.clear();
    AsxNanoStreamDecoder decoder(capture);
    decoder.parse(String(input));
    assert(commands.empty());
}

static void validCommands() {
    AsxNanoStreamDecoder decoder(capture);
    commands.clear();
    decoder.parse("KLED:1#KRELAY:0#KTEMP_OFFSET:-15#KSET:+60#1b0b1b");
    const std::vector<std::pair<std::string, int> > expected = {
        {"LED", 1}, {"RELAY", 0}, {"TEMP_OFFSET", -15}, {"SET", 60},
        {"BIN", 1}, {"BIN", 0}, {"BIN", 1}
    };
    assert(commands == expected);
    commands.clear();
    decoder.parse("~~NOISE~~KRESET:1#@@junk1b");
    assert(commands.size() == 2);
    assert(commands[0] == std::make_pair(std::string("RESET"), 1));
    assert(commands[1] == std::make_pair(std::string("BIN"), 1));
    commands.clear();
    decoder.parse(String(std::string("KMAX:") + std::to_string(INT_MAX) +
        "#KMIN:" + std::to_string(INT_MIN) + "#KZERO:-0#"));
    assert(commands.size() == 3);
    assert(commands[0].second == INT_MAX);
    assert(commands[1].second == INT_MIN);
    assert(commands[2].second == 0);
    AsxNanoStreamDecoder noCallback(nullptr);
    noCallback.parse("KLED:1#1b");
}

static void malformedCommands() {
    const char* invalid[] = {
        "", "K", "KLED", "KLED:1", "KLED#", "K:1#", "KLED:#",
        "KLED:+#", "KLED:-#", "KLED:garbage#", "KLED:1junk#", "KLED:1.0#",
        "KLED: 1#", "KLED:1 #", "KLED:1:2#", "KLED:--1#", "KLED:+-1#",
        "KLE\nD:1#", "KLED:1b#", "KLED1b#", "KLED:1b", "KLED:KRELAY:1#",
        "2b", "9b", "11b", "01b", "101b", "-1b", "+1b"
    };
    for (const char* input : invalid) reject(input);
    reject(std::string("KLED:") + std::to_string(static_cast<long long>(INT_MAX) + 1) + "#");
    reject(std::string("KLED:") + std::to_string(static_cast<long long>(INT_MIN) - 1) + "#");
    reject("KLED:" + std::string(100000, '9') + "#");
    reject(std::string(100000, 'K'));
    reject(std::string("KLED:1\0junk#", 12));

    // The first ':' must belong to this command, never a later '#'-delimited one.
    commands.clear();
    AsxNanoStreamDecoder decoder(capture);
    decoder.parse("KLED#KRELAY:1#");
    assert(commands.size() == 1);
    assert(commands[0] == std::make_pair(std::string("RELAY"), 1));

    // Invalid commands cannot hide binary callbacks; recovery starts after '#'.
    commands.clear();
    decoder.parse("KLED:garbage1b#KRELAY:1#2b0b");
    assert(commands.size() == 2);
    assert(commands[0] == std::make_pair(std::string("RELAY"), 1));
    assert(commands[1] == std::make_pair(std::string("BIN"), 0));

    // Every truncation of a command must be side-effect free.
    const std::string complete = "KRELAY:-123#";
    for (size_t length = 0; length < complete.size(); ++length)
        reject(complete.substr(0, length));
    for (unsigned int byte = 0; byte <= 255; ++byte) {
        if ((byte >= '0' && byte <= '9') || byte == '#') continue;
        reject(std::string("KRELAY:1") + static_cast<char>(byte) + "#");
    }
}

int main() {
    validCommands();
    malformedCommands();
    std::cout << "Decoder Light safety tests passed\n";
}
