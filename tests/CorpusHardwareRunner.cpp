#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <iomanip>
#include <sstream>
#include <cstdint>

// Include Arduino shim first
#include "Arduino.h"

// Include ASX NanoStream headers
#include "ASXNanoStreamEncoder.h"
#include "AsxEncoderV2.h"
#include "ASXProfileV2.h"
#include "AsxNanoStreamDecoderLight.h"

struct DecodedCommandEntry
{
    std::string key;
    int value;
};

static std::vector<DecodedCommandEntry> g_decodedCommands;

static void TestCommandCallback(String key, int value)
{
    g_decodedCommands.push_back({key.toStdString(), value});
}

static std::string ToHex(const uint8_t* buffer, size_t length)
{
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < length; ++i)
    {
        ss << std::setw(2) << static_cast<int>(buffer[i]);
    }
    return ss.str();
}

int main(int argc, char** argv)
{
    bool allPassed = true;
    std::cout << "================================================================" << std::endl;
    std::cout << "ASXNanoStream C++ Hardware & Compiler Corpus Runner (NS-07.01.01)" << std::endl;
    std::cout << "================================================================" << std::endl;

#if defined(__clang__)
    std::string compilerName = "Clang";
    std::string compilerVer = __clang_version__;
#elif defined(__GNUC__)
    std::string compilerName = "GCC";
    std::string compilerVer = std::to_string(__GNUC__) + "." + std::to_string(__GNUC_MINOR__) + "." + std::to_string(__GNUC_PATCHLEVEL__);
#elif defined(_MSC_VER)
    std::string compilerName = "MSVC";
    std::string compilerVer = std::to_string(_MSC_VER);
#else
    std::string compilerName = "Unknown";
    std::string compilerVer = "Unknown";
#endif

    std::cout << "Compiler: " << compilerName << " (" << compilerVer << ")" << std::endl;
    std::cout << "Standard: C++" << __cplusplus << std::endl;
    std::cout << "Host Architecture Word Size: " << (sizeof(void*) * 8) << "-bit" << std::endl;
    std::cout << "----------------------------------------------------------------" << std::endl;

    std::stringstream json;
    json << "{\n";
    json << "  \"metadata\": {\n";
    json << "    \"task\": \"NS-07.01.01\",\n";
    json << "    \"compiler\": \"" << compilerName << "\",\n";
    json << "    \"compiler_version\": \"" << compilerVer << "\",\n";
    json << "    \"cpp_standard\": \"" << __cplusplus << "\",\n";
    json << "    \"pointer_size_bits\": " << (sizeof(void*) * 8) << "\n";
    json << "  },\n";

    // -------------------------------------------------------------
    // PART 1: Host object sizes; MCU SRAM requires a target build and measurement.
    // -------------------------------------------------------------
    size_t sizeAsxV1 = sizeof(AsxNanoStream);
    size_t sizeAsxV2 = sizeof(AsxEncoderV2);
    size_t v2FrameBufferCapacity = 128; // Standard MTU buffer
    size_t totalSramV2Static = sizeAsxV2 + v2FrameBufferCapacity;

    std::cout << "\n[1] HOST OBJECT SIZES (not target MCU SRAM usage):" << std::endl;
    std::cout << "  - sizeof(AsxNanoStream V1): " << sizeAsxV1 << " bytes" << std::endl;
    std::cout << "  - sizeof(AsxEncoderV2):     " << sizeAsxV2 << " bytes" << std::endl;
    std::cout << "  - V2 Static Frame Buffer:   " << v2FrameBufferCapacity << " bytes" << std::endl;
    std::cout << "  - Host V2 Object + Buffer: " << totalSramV2Static << " bytes (excludes samples, stack and runtime)" << std::endl;

    json << "  \"host_object_sizes\": {\n";
    json << "    \"target_mcu_sram_measured\": false,\n";
    json << "    \"sizeof_asx_v1\": " << sizeAsxV1 << ",\n";
    json << "    \"sizeof_asx_v2\": " << sizeAsxV2 << ",\n";
    json << "    \"v2_frame_buffer_bytes\": " << v2FrameBufferCapacity << ",\n";
    json << "    \"v2_object_and_buffer_bytes\": " << totalSramV2Static << "\n";
    json << "  },\n";

    // -------------------------------------------------------------
    // PART 2: V1 Common Vector Corpus Execution (C++)
    // -------------------------------------------------------------
    std::cout << "\n[2] RUNNING V1 CANONICAL VECTOR CORPUS:" << std::endl;
    json << "  \"v1_vectors\": [\n";

    struct V1TestCase {
        std::string id;
        std::string expectedPayload;
        void (*run)(AsxNanoStream& enc);
    };

    std::vector<V1TestCase> v1Cases = {
        {
            "V1-VAL-01",
            "B250#+10v-5v+0v",
            [](AsxNanoStream& enc) {
                enc.setBaseline(250);
                enc.addAnalog(260); // +10v
                enc.addAnalog(255); // -5v
                enc.addAnalog(255); // +0v
            }
        },
        {
            "V1-VAL-02",
            "1b1b",
            [](AsxNanoStream& enc) {
                enc.addBinary(true);
                enc.addBinary(true);
            }
        },
        {
            "V1-VAL-03",
            "x3-1b50ms",
            [](AsxNanoStream& enc) {
                enc.addBinary(true, 50);
                enc.addBinary(true, 50);
                enc.addBinary(true, 50);
            }
        },
        {
            "V1-VAL-04",
            "B100#x2-+10vB500#+5v",
            [](AsxNanoStream& enc) {
                enc.setBaseline(100);
                enc.addAnalog(110);
                enc.addAnalog(120);
                enc.setBaseline(500);
                enc.addAnalog(505);
            }
        },
        {
            "V1-VAL-05",
            "B-150#-20v1000ms+10v1000ms",
            [](AsxNanoStream& enc) {
                enc.setBaseline(-150);
                enc.addAnalog(-170, 1000);
                enc.addAnalog(-160, 1000);
            }
        },
        {
            "V1-VAL-06",
            "B0#+5v65535ms",
            [](AsxNanoStream& enc) {
                enc.setBaseline(0);
                enc.addAnalog(5, 65535);
            }
        },
        {
            "V1-VAL-07",
            "B-2147483648#+4294967295v",
            [](AsxNanoStream& enc) {
                enc.setBaseline(-2147483647 - 1);
                enc.addAnalog(2147483647);
            }
        },
        {
            "V1-VAL-08",
            "B50#1b100ms+5v200ms0b100ms-3v200ms",
            [](AsxNanoStream& enc) {
                enc.setBaseline(50);
                enc.addBinary(true, 100);
                enc.addAnalog(55, 200);
                enc.addBinary(false, 100);
                enc.addAnalog(52, 200);
            }
        }
    };

    for (size_t i = 0; i < v1Cases.size(); ++i)
    {
        const auto& tc = v1Cases[i];
        AsxNanoStream enc;
        tc.run(enc);

        int reportedLen = enc.length();
        String payload = enc.getPayload();
        std::string actualStr = payload.toStdString();
        bool match = (actualStr == tc.expectedPayload);
        bool lenMatch = (reportedLen == (int)actualStr.length());
        allPassed = allPassed && match && lenMatch;

        std::cout << "  - " << tc.id << ": " << (match && lenMatch ? "PASS" : "FAIL")
                  << " (bytes=" << actualStr.length() << " payload=\"" << actualStr << "\")" << std::endl;

        json << "    {\n";
        json << "      \"id\": \"" << tc.id << "\",\n";
        json << "      \"expected_payload\": \"" << tc.expectedPayload << "\",\n";
        json << "      \"actual_payload\": \"" << actualStr << "\",\n";
        json << "      \"length_reported\": " << reportedLen << ",\n";
        json << "      \"length_actual\": " << actualStr.length() << ",\n";
        json << "      \"passed\": " << (match && lenMatch ? "true" : "false") << "\n";
        json << "    }" << (i + 1 < v1Cases.size() ? ",\n" : "\n");
    }
    json << "  ],\n";

    // -------------------------------------------------------------
    // PART 3: V2 Modes and Profiles Execution (C++)
    // -------------------------------------------------------------
    std::cout << "\n[3] RUNNING V2 MODES & PROFILES (C++ BINARY CODEC):" << std::endl;
    json << "  \"v2_frames\": [\n";

    uint8_t buffer[256];
    struct V2ModeTestCase {
        std::string name;
        uint32_t profileId;
        uint32_t profileVersion;
        uint32_t sequenceNumber;
        uint32_t channelId;
        AsxDataType dataType;
        std::vector<int64_t> samples;
        AsxModeId explicitMode; // or -1 for auto
        bool isAuto;
    };

    // 60 samples oscillating 250 / 251
    std::vector<int64_t> oscillating60(60);
    for (int i = 0; i < 60; ++i) oscillating60[i] = (i % 2 == 0) ? 250 : 251;

    // 60 constant samples 250
    std::vector<int64_t> constant60(60, 250);

    // 60 ramp samples 200..259
    std::vector<int64_t> ramp60(60);
    for (int i = 0; i < 60; ++i) ramp60[i] = 200 + i;

    // 60 delta {-1, 0, 1} samples around 500 (humidity)
    std::vector<int64_t> deltas60(60);
    deltas60[0] = 500;
    for (int i = 1; i < 60; ++i) deltas60[i] = deltas60[i-1] + ((i % 3 == 0) ? 0 : ((i % 3 == 1) ? 1 : -1));

    // Plateaus 60 samples
    std::vector<int64_t> plateaus60(60);
    for (int i = 0; i < 60; ++i) plateaus60[i] = 200 + (i / 10) * 5;

    std::vector<V2ModeTestCase> v2Cases = {
        { "RAW_MODE_60_OSCILLATING", 1, 1, 101, 0, AsxDataType::Int16, oscillating60, AsxModeId::RAW, false },
        { "CONSTANT_MODE_60_SAMPLES", 1, 1, 102, 0, AsxDataType::Int16, constant60, AsxModeId::CONSTANT, false },
        { "DELTA_VARINT_MODE_60_RAMP", 1, 1, 103, 0, AsxDataType::Int16, ramp60, AsxModeId::DELTA_VARINT, false },
        { "DELTA_BITPACK_MODE_60_HUMIDITY", 2, 1, 104, 0, AsxDataType::UInt16, deltas60, AsxModeId::DELTA_BITPACK, false },
        { "RANGE_BITPACK_MODE_60_OSCILLATING", 1, 1, 105, 0, AsxDataType::Int16, oscillating60, AsxModeId::RANGE_BITPACK, false },
        { "DELTA_RLE_MODE_60_PLATEAUS", 1, 1, 106, 0, AsxDataType::Int16, plateaus60, AsxModeId::DELTA_RLE, false },
        { "AUTO_SELECTOR_60_OSCILLATING", 1, 1, 107, 0, AsxDataType::Int16, oscillating60, AsxModeId::RANGE_BITPACK, true },
        { "AUTO_SELECTOR_60_CONSTANT", 1, 1, 108, 0, AsxDataType::Int16, constant60, AsxModeId::CONSTANT, true }
    };

    for (size_t i = 0; i < v2Cases.size(); ++i)
    {
        const auto& tc = v2Cases[i];
        memset(buffer, 0, sizeof(buffer));
        AsxEncoderV2 v2(buffer, sizeof(buffer));

        bool frameOk = v2.BeginFrame(tc.profileId, tc.profileVersion, tc.sequenceNumber);

        bool blockOk = false;
        if (tc.isAuto)
        {
            blockOk = v2.EncodeBlockAuto(tc.channelId, tc.dataType, tc.samples.data(), tc.samples.size());
        }
        else
        {
            switch (tc.explicitMode)
            {
                case AsxModeId::RAW:
                    blockOk = v2.EncodeBlockRAW(tc.channelId, tc.dataType, tc.samples.data(), tc.samples.size());
                    break;
                case AsxModeId::CONSTANT:
                    blockOk = v2.EncodeBlockCONSTANT(tc.channelId, tc.dataType, tc.samples[0], tc.samples.size());
                    break;
                case AsxModeId::DELTA_VARINT:
                    blockOk = v2.EncodeBlockDELTA_VARINT(tc.channelId, tc.dataType, tc.samples.data(), tc.samples.size());
                    break;
                case AsxModeId::DELTA_BITPACK:
                    blockOk = v2.EncodeBlockDELTA_BITPACK(tc.channelId, tc.dataType, tc.samples.data(), tc.samples.size());
                    break;
                case AsxModeId::RANGE_BITPACK:
                    blockOk = v2.EncodeBlockRANGE_BITPACK(tc.channelId, tc.dataType, tc.samples.data(), tc.samples.size());
                    break;
                case AsxModeId::DELTA_RLE:
                    blockOk = v2.EncodeBlockDELTA_RLE(tc.channelId, tc.dataType, tc.samples.data(), tc.samples.size());
                    break;
            }
        }

        bool endOk = v2.EndFrame();
        const bool pass = frameOk && blockOk && endOk;
        allPassed = allPassed && pass;

        size_t frameLen = v2.GetLength();
        std::string hexStr = ToHex(v2.GetBuffer(), frameLen);

        std::cout << "  - " << tc.name << ": " << (pass ? "PASS" : "FAIL") << " length=" << frameLen << " bytes (hex: "
                  << hexStr.substr(0, std::min<size_t>(32, hexStr.length())) << "...)" << std::endl;

        json << "    {\n";
        json << "      \"name\": \"" << tc.name << "\",\n";
        json << "      \"passed\": " << (pass ? "true" : "false") << ",\n";
        json << "      \"profile_id\": " << tc.profileId << ",\n";
        json << "      \"profile_version\": " << tc.profileVersion << ",\n";
        json << "      \"sequence_number\": " << tc.sequenceNumber << ",\n";
        json << "      \"channel_id\": " << tc.channelId << ",\n";
        json << "      \"data_type\": " << static_cast<int>(tc.dataType) << ",\n";
        json << "      \"sample_count\": " << tc.samples.size() << ",\n";
        json << "      \"frame_length_bytes\": " << frameLen << ",\n";
        json << "      \"frame_hex\": \"" << hexStr << "\",\n";
        json << "      \"samples\": [";
        for (size_t s = 0; s < tc.samples.size(); ++s)
        {
            json << tc.samples[s] << (s + 1 < tc.samples.size() ? "," : "");
        }
        json << "]\n";
        json << "    }" << (i + 1 < v2Cases.size() ? ",\n" : "\n");
    }
    json << "  ],\n";

    // -------------------------------------------------------------
    // PART 4: Embedded Downlink Decoder Validation (C++)
    // -------------------------------------------------------------
    std::cout << "\n[4] VALIDATING EMBEDDED DOWNLINK DECODER (AsxNanoStreamDecoderLight):" << std::endl;
    json << "  \"embedded_decoder_tests\": [\n";

    struct DownlinkTestCase {
        std::string name;
        std::string payload;
        std::vector<DecodedCommandEntry> expected;
    };

    std::vector<DownlinkTestCase> downlinkCases = {
        {
            "SINGLE_KEY_VALUE",
            "KLED:1#",
            { {"LED", 1} }
        },
        {
            "MULTI_KEY_VALUE",
            "KLED:1#KRELAY:0#KFREQ:60#",
            { {"LED", 1}, {"RELAY", 0}, {"FREQ", 60} }
        },
        {
            "BINARY_LITERALS",
            "1b0b1b",
            { {"BIN", 1}, {"BIN", 0}, {"BIN", 1} }
        },
        {
            "MIXED_KEY_AND_BINARY",
            "KINTERVAL:300#1b",
            { {"INTERVAL", 300}, {"BIN", 1} }
        },
        {
            "NEGATIVE_VALUE",
            "KTEMP_OFFSET:-15#",
            { {"TEMP_OFFSET", -15} }
        },
        {
            "NOISE_TOLERANCE",
            "~~NOISE~~KRESET:1#@@junk1b",
            { {"RESET", 1}, {"BIN", 1} }
        }
    };

    AsxNanoStreamDecoder decoder(TestCommandCallback);

    for (size_t i = 0; i < downlinkCases.size(); ++i)
    {
        const auto& tc = downlinkCases[i];
        g_decodedCommands.clear();

        decoder.parse(String(tc.payload.c_str()));

        bool pass = (g_decodedCommands.size() == tc.expected.size());
        if (pass)
        {
            for (size_t c = 0; c < tc.expected.size(); ++c)
            {
                if (g_decodedCommands[c].key != tc.expected[c].key ||
                    g_decodedCommands[c].value != tc.expected[c].value)
                {
                    pass = false;
                    break;
                }
            }
        }

        std::cout << "  - " << tc.name << ": " << (pass ? "PASS" : "FAIL")
                  << " (callbacks=" << g_decodedCommands.size() << " payload=\"" << tc.payload << "\")" << std::endl;
        allPassed = allPassed && pass;

        json << "    {\n";
        json << "      \"name\": \"" << tc.name << "\",\n";
        json << "      \"payload\": \"" << tc.payload << "\",\n";
        json << "      \"passed\": " << (pass ? "true" : "false") << ",\n";
        json << "      \"decoded\": [\n";
        for (size_t c = 0; c < g_decodedCommands.size(); ++c)
        {
            json << "        { \"key\": \"" << g_decodedCommands[c].key << "\", \"value\": " << g_decodedCommands[c].value << " }"
                 << (c + 1 < g_decodedCommands.size() ? ",\n" : "\n");
        }
        json << "      ]\n";
        json << "    }" << (i + 1 < downlinkCases.size() ? ",\n" : "\n");
    }
    json << "  ],\n";
    json << "  \"all_passed\": " << (allPassed ? "true" : "false") << "\n";
    json << "}\n";

    std::string outPath = "build/cpp-hardware-validation-results.json";
    if (argc > 1)
    {
        outPath = argv[1];
    }

    std::ofstream outFile(outPath);
    if (outFile.is_open())
    {
        outFile << json.str();
        outFile.close();
        if (!outFile)
        {
            std::cerr << "ERROR: Could not write output file: " << outPath << std::endl;
            return 1;
        }
        std::cout << "\nResults successfully written to: " << outPath << std::endl;
    }
    else
    {
        std::cerr << "\nERROR: Could not open output file: " << outPath << std::endl;
        return 1;
    }

    std::cout << "\n================================================================" << std::endl;
    std::cout << (allPassed ? "All C++ host corpus tests PASSED!" : "C++ host corpus tests FAILED!") << std::endl;
    std::cout << "================================================================" << std::endl;

    return allPassed ? 0 : 1;
}
