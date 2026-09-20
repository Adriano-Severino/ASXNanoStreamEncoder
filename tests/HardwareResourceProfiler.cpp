#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>

// ============================================================================
// 1. Heap Allocation Tracker
// ============================================================================
static size_t g_heapAllocCount = 0;
static size_t g_heapAllocBytes = 0;
static bool g_trackHeap = false;

void* operator new(size_t size)
{
    if (g_trackHeap)
    {
        g_heapAllocCount++;
        g_heapAllocBytes += size;
    }
    return std::malloc(size);
}

void* operator new[](size_t size)
{
    if (g_trackHeap)
    {
        g_heapAllocCount++;
        g_heapAllocBytes += size;
    }
    return std::malloc(size);
}

void operator delete(void* p) noexcept
{
    std::free(p);
}

void operator delete[](void* p) noexcept
{
    std::free(p);
}

void operator delete(void* p, size_t) noexcept
{
    std::free(p);
}

void operator delete[](void* p, size_t) noexcept
{
    std::free(p);
}

// Include Arduino shim first
#include "Arduino.h"

// Include ASX NanoStream headers
#include "ASXNanoStreamEncoder.h"
#include "AsxEncoderV2.h"
#include "ASXProfileV2.h"
#include "AsxNanoStreamDecoderLight.h"

// ============================================================================
// 2. LoRaWAN Semtech SX1262 Time-on-Air & Energy Model
// ============================================================================
struct LoRaToaResult
{
    int spreadingFactor;
    double bandwidthHz;
    int payloadBytes;
    double preambleDurationMs;
    double payloadDurationMs;
    double timeOnAirMs;
    double txEnergyJoules;
    double cpuEnergyJoules;
    double netEnergySavedJoules;
};

// Semtech SX1262 LoRa Time-on-Air Calculation
// CR: 1 = 4/5, H: 0 = Explicit Header, DE: LowDataRateOptimize (1 if symbol time > 16ms, e.g. SF11/12 @ 125k)
static LoRaToaResult CalculateLoRaToa(int sf, double bwHz, int payloadBytes, double mcuActiveTimeMs, double mcuCurrentMa, double vddVolts = 3.3, double txCurrentMa = 22.0)
{
    double tSymMs = (std::pow(2.0, sf) / bwHz) * 1000.0;
    double tPreambleMs = (8.0 + 4.25) * tSymMs;

    int de = (tSymMs > 16.0) ? 1 : 0;
    int h = 0; // explicit header
    int cr = 1; // 4/5

    double numerator = 8.0 * payloadBytes - 4.0 * sf + 28.0 + 16.0 - 20.0 * h;
    double denominator = 4.0 * (sf - 2 * de);
    double ceilTerm = std::ceil(numerator / denominator);
    double nPayloadSymbols = 8.0 + std::max(ceilTerm * (cr + 4), 0.0);
    double tPayloadMs = nPayloadSymbols * tSymMs;
    double totalToaMs = tPreambleMs + tPayloadMs;

    double txEnergyJ = vddVolts * (txCurrentMa / 1000.0) * (totalToaMs / 1000.0);
    double cpuEnergyJ = vddVolts * (mcuCurrentMa / 1000.0) * (mcuActiveTimeMs / 1000.0);

    LoRaToaResult res;
    res.spreadingFactor = sf;
    res.bandwidthHz = bwHz;
    res.payloadBytes = payloadBytes;
    res.preambleDurationMs = tPreambleMs;
    res.payloadDurationMs = tPayloadMs;
    res.timeOnAirMs = totalToaMs;
    res.txEnergyJoules = txEnergyJ;
    res.cpuEnergyJoules = cpuEnergyJ;
    res.netEnergySavedJoules = 0.0;
    return res;
}

// ============================================================================
// 3. Main Profiler
// ============================================================================
int main(int argc, char** argv)
{
    bool allPassed = true;
    std::cout << "================================================================" << std::endl;
    std::cout << "ASXNanoStream C++ Hardware Resource & Energy Profiler (NS-07.01.03)" << std::endl;
    std::cout << "================================================================" << std::endl;

#if defined(__clang__)
    std::string compilerName = "Clang";
    std::string compilerVer = __clang_version__;
#elif defined(__GNUC__)
    std::string compilerName = "GCC";
    std::string compilerVer = std::to_string(__GNUC__) + "." + std::to_string(__GNUC_MINOR__) + "." + std::to_string(__GNUC_PATCHLEVEL__);
#else
    std::string compilerName = "Unknown";
    std::string compilerVer = "Unknown";
#endif

    std::cout << "Compiler: " << compilerName << " (" << compilerVer << ")" << std::endl;
    std::cout << "Standard: C++" << __cplusplus << std::endl;
    std::cout << "Host Architecture: " << (sizeof(void*) * 8) << "-bit" << std::endl;
    std::cout << "----------------------------------------------------------------" << std::endl;

    // ------------------------------------------------------------------------
    // SECTION 1: Host Object Sizes & Heap Allocation Check
    // ------------------------------------------------------------------------
    size_t sizeAsxV1 = sizeof(AsxNanoStream);
    size_t sizeAsxV2 = sizeof(AsxEncoderV2);
    size_t sizeDecoderLight = sizeof(AsxNanoStreamDecoder);
    size_t v2MtuBuffer = 128;
    size_t totalV2Footprint = sizeAsxV2 + v2MtuBuffer;

    std::cout << "\n[1] HOST OBJECT SIZES (not target MCU SRAM usage):" << std::endl;
    std::cout << "  - sizeof(AsxNanoStream V1):          " << sizeAsxV1 << " bytes" << std::endl;
    std::cout << "  - sizeof(AsxEncoderV2):              " << sizeAsxV2 << " bytes" << std::endl;
    std::cout << "  - sizeof(AsxNanoStreamDecoderLight): " << sizeDecoderLight << " bytes" << std::endl;
    std::cout << "  - V2 Static Frame Buffer:            " << v2MtuBuffer << " bytes" << std::endl;
    std::cout << "  - Host V2 Object + Buffer:          " << totalV2Footprint << " bytes (excludes samples, stack and runtime)" << std::endl;

    // Dynamic Heap Verification:
    g_trackHeap = true;
    g_heapAllocCount = 0;
    g_heapAllocBytes = 0;

    uint8_t staticBuffer[128];
    AsxEncoderV2 testEncoder(staticBuffer, sizeof(staticBuffer));
    const bool frameOk = testEncoder.BeginFrame(1, 1, 100);

    int64_t sampleBatch[60];
    for (int i = 0; i < 60; ++i) sampleBatch[i] = static_cast<int64_t>(250 + (i % 5));
    const bool blockOk = testEncoder.EncodeBlockAuto(0, AsxDataType::Int16, sampleBatch, 60);
    const bool endOk = testEncoder.EndFrame();

    g_trackHeap = false;

    std::cout << "  - Dynamic Heap Allocations during V2 Encode: " << g_heapAllocCount << " allocations (" << g_heapAllocBytes << " bytes)" << std::endl;
    const size_t encoderHeapAllocations = g_heapAllocCount;
    allPassed = frameOk && blockOk && endOk && encoderHeapAllocations == 0;
    if (!allPassed) std::cerr << "FAIL: V2 encoding or zero-allocation check failed." << std::endl;

    // Test decoder heap verification
    g_trackHeap = true;
    g_heapAllocCount = 0;
    g_heapAllocBytes = 0;
    AsxNanoStreamDecoder decoder([](String, int){});
    decoder.parse(String("KLED:1#1b"));
    g_trackHeap = false;
    const size_t decoderHeapAllocations = g_heapAllocCount;
    std::cout << "  - Dynamic Heap Allocations during Downlink Decode: " << g_heapAllocCount << " allocations" << std::endl;

    // ------------------------------------------------------------------------
    // SECTION 2: Encode Latency & Benchmarks (Per-sample & Per-mode)
    // ------------------------------------------------------------------------
    std::cout << "\n[2] ENCODE LATENCY & PERFORMANCE PROFILING (10,000 ITERATIONS):" << std::endl;

    struct ModeBenchmark
    {
        std::string modeName;
        int sampleCount;
        size_t outputBytes;
        double meanUs;
        double minUs;
        double maxUs;
        double p50Us;
        double p99Us;
        double usPerSample;
        // Cycles projections
        double cyclesAtmega16M;
        double cyclesStm32_72M;
        double cyclesEsp32_240M;
    };

    std::vector<ModeBenchmark> benchmarks;
    const int ITERATIONS = 10000;

    auto benchmarkMode = [&](const std::string& name, int sampleCount, auto fillSamples, auto encodeCall)
    {
        std::vector<int64_t> samples(sampleCount);
        fillSamples(samples);

        // Warm up
        for (int i = 0; i < 500; ++i)
        {
            uint8_t buf[256];
            AsxEncoderV2 enc(buf, sizeof(buf));
            if (!enc.BeginFrame(1, 1, 1) || !encodeCall(enc, samples.data(), sampleCount) || !enc.EndFrame())
            {
                allPassed = false;
                std::cerr << "FAIL: benchmark warmup for " << name << std::endl;
                return;
            }
        }

        std::vector<double> timings(ITERATIONS);
        size_t finalLen = 0;

        for (int i = 0; i < ITERATIONS; ++i)
        {
            uint8_t buf[256];
            AsxEncoderV2 enc(buf, sizeof(buf));
            const bool started = enc.BeginFrame(1, 1, 1);

            auto t0 = std::chrono::high_resolution_clock::now();
            const bool encoded = encodeCall(enc, samples.data(), sampleCount);
            const bool finalized = enc.EndFrame();
            auto t1 = std::chrono::high_resolution_clock::now();
            if (!started || !encoded || !finalized)
            {
                allPassed = false;
                std::cerr << "FAIL: benchmark encoding for " << name << std::endl;
                return;
            }

            timings[i] = std::chrono::duration<double, std::micro>(t1 - t0).count();
            if (i == 0) finalLen = enc.GetLength();
        }

        std::sort(timings.begin(), timings.end());
        double sum = 0;
        for (double t : timings) sum += t;
        double mean = sum / ITERATIONS;
        double minT = timings.front();
        double maxT = timings.back();
        double p50 = timings[ITERATIONS / 2];
        double p99 = timings[static_cast<size_t>(ITERATIONS * 0.99)];
        double perSample = mean / sampleCount;

        // Illustrative assumptions only; host timings do not predict MCU cycles.
        double estCyclesHost = mean * 3500.0; // @ 3.5 GHz
        double cyclesAtmega = estCyclesHost * 8.0;
        double cyclesStm32 = estCyclesHost * 2.5;
        double cyclesEsp32 = estCyclesHost * 1.5;

        ModeBenchmark bm;
        bm.modeName = name;
        bm.sampleCount = sampleCount;
        bm.outputBytes = finalLen;
        bm.meanUs = mean;
        bm.minUs = minT;
        bm.maxUs = maxT;
        bm.p50Us = p50;
        bm.p99Us = p99;
        bm.usPerSample = perSample;
        bm.cyclesAtmega16M = cyclesAtmega;
        bm.cyclesStm32_72M = cyclesStm32;
        bm.cyclesEsp32_240M = cyclesEsp32;
        benchmarks.push_back(bm);

        std::cout << "  " << std::left << std::setw(28) << (name + " (" + std::to_string(sampleCount) + " am)")
                  << " -> " << std::setw(4) << finalLen << " B"
                  << " | Lat: " << std::fixed << std::setprecision(2) << std::setw(6) << mean << " us"
                  << " (p99: " << std::setw(6) << p99 << " us)"
                  << " | " << std::setprecision(3) << perSample << " us/am"
                  << " | illustrative ATmega model: " << std::setprecision(2) << (cyclesAtmega / 16000.0) << " ms"
                  << " | illustrative STM32 model: " << (cyclesStm32 / 72000.0) << " ms"
                  << std::endl;
    };

    // 1. RAW Mode (60 samples)
    benchmarkMode("RAW_MODE", 60,
        [](std::vector<int64_t>& s) { for (size_t i = 0; i < s.size(); ++i) s[i] = static_cast<int64_t>(i * 5); },
        [](AsxEncoderV2& enc, const int64_t* s, int n) { return enc.EncodeBlockRAW(0, AsxDataType::Int16, s, n); });

    // 2. CONSTANT Mode (60 samples)
    benchmarkMode("CONSTANT_MODE", 60,
        [](std::vector<int64_t>& s) { for (size_t i = 0; i < s.size(); ++i) s[i] = 250; },
        [](AsxEncoderV2& enc, const int64_t* s, int n) { return enc.EncodeBlockCONSTANT(0, AsxDataType::Int16, s[0], n); });

    // 3. DELTA_VARINT Mode (60 samples)
    benchmarkMode("DELTA_VARINT_MODE", 60,
        [](std::vector<int64_t>& s) { for (size_t i = 0; i < s.size(); ++i) s[i] = static_cast<int64_t>(200 + i); },
        [](AsxEncoderV2& enc, const int64_t* s, int n) { return enc.EncodeBlockDELTA_VARINT(0, AsxDataType::Int16, s, n); });

    // 4. DELTA_BITPACK Mode (60 samples)
    benchmarkMode("DELTA_BITPACK_MODE", 60,
        [](std::vector<int64_t>& s) { for (size_t i = 0; i < s.size(); ++i) s[i] = static_cast<int64_t>(500 + (i % 3)); },
        [](AsxEncoderV2& enc, const int64_t* s, int n) { return enc.EncodeBlockDELTA_BITPACK(0, AsxDataType::Int16, s, n); });

    // 5. RANGE_BITPACK Mode (60 samples)
    benchmarkMode("RANGE_BITPACK_MODE", 60,
        [](std::vector<int64_t>& s) { for (size_t i = 0; i < s.size(); ++i) s[i] = static_cast<int64_t>(240 + (i % 6)); },
        [](AsxEncoderV2& enc, const int64_t* s, int n) { return enc.EncodeBlockRANGE_BITPACK(0, AsxDataType::Int16, s, n); });

    // 6. DELTA_RLE Mode (60 samples)
    benchmarkMode("DELTA_RLE_MODE", 60,
        [](std::vector<int64_t>& s) {
            for (size_t i = 0; i < s.size(); ++i) {
                if (i < 20) s[i] = 100;
                else if (i < 40) s[i] = 200;
                else s[i] = 300;
            }
        },
        [](AsxEncoderV2& enc, const int64_t* s, int n) { return enc.EncodeBlockDELTA_RLE(0, AsxDataType::Int16, s, n); });

    // 7. AUTO_SELECTOR (Sample scaling: 1, 2, 8, 16, 32, 60, 120 samples)
    int sampleCounts[] = { 1, 2, 8, 16, 32, 60, 120 };
    for (int count : sampleCounts)
    {
        benchmarkMode("AUTO_SELECTOR", count,
            [](std::vector<int64_t>& s) { for (size_t i = 0; i < s.size(); ++i) s[i] = static_cast<int64_t>(240 + (i % 6)); },
            [](AsxEncoderV2& enc, const int64_t* s, int n) { return enc.EncodeBlockAuto(0, AsxDataType::Int16, s, n); });
    }

    // ------------------------------------------------------------------------
    // SECTION 3: Semtech SX1262 LoRaWAN Time-on-Air & Battery Model
    // ------------------------------------------------------------------------
    std::cout << "\n[3] SEMTECH SX1262 LoRaWAN TIME-ON-AIR (ToA) & BATTERY PHYSICAL MODEL:" << std::endl;
    std::cout << "  Physical Parameters:" << std::endl;
    std::cout << "    - Bandwidth:        125 kHz" << std::endl;
    std::cout << "    - Preamble:         8 symbols" << std::endl;
    std::cout << "    - Coding Rate:      4/5 (CR=1)" << std::endl;
    std::cout << "    - Transceiver:      SX1262 @ +14 dBm (Itx = 22 mA, Vdd = 3.3 V)" << std::endl;
    std::cout << "    - ATmega328P CPU:   4.5 mA active @ 3.3V (16 MHz)" << std::endl;
    std::cout << "    - Battery:          LiSOCl2 2400 mAh (3.6 V, Derated 2000 mAh usable = 25,920 Joules)" << std::endl;
    std::cout << "    - Deep Sleep:       3.2 uA (Radio 1.2 uA + MCU 2.0 uA)" << std::endl;

    struct ComparisonScenario
    {
        std::string name;
        int payloadBytes;
        double cpuTimeMs;
    };

    std::vector<ComparisonScenario> scenarios = {
        { "Uncompressed RAW (60 am)", 132, 0.05 },
        { "V1 Textual Protocol",      78,  0.25 },
        { "V2 RANGE_BITPACK (Osc.)",   23,  0.42 },
        { "V2 CONSTANT (Flat)",        14,  0.15 },
        { "V2 DELTA_BITPACK (Smooth)", 30,  0.35 }
    };

    struct ToaEvaluation
    {
        std::string scenario;
        int bytes;
        double sf7ToaMs;
        double sf7TxJoules;
        double sf7CpuJoules;
        double sf7TotalJoules;
        double sf12ToaMs;
        double sf12TxJoules;
        double sf12CpuJoules;
        double sf12TotalJoules;
        double batteryYearsSf7_15m;
        double batteryYearsSf12_15m;
    };

    std::vector<ToaEvaluation> toaResults;

    const double USABLE_BATTERY_JOULES = 2000.0 * 3.6 * 3.6; // 25,920 Joules
    const double SLEEP_JOULES_PER_HOUR = 3.3 * (3.2e-6) * 3600.0; // ~0.038 J/hour
    const double TRANSMISSIONS_PER_HOUR = 4.0; // 1 every 15 mins (96/day)

    std::cout << "\n  Table: ToA & Energy Comparison across Scenarios (SF7 vs SF12):" << std::endl;
    std::cout << "  ------------------------------------------------------------------------------------------------------" << std::endl;
    std::cout << "  " << std::left << std::setw(26) << "Scenario"
              << std::setw(6) << "Bytes"
              << std::setw(12) << "SF7 ToA"
              << std::setw(14) << "SF7 Energy"
              << std::setw(14) << "SF12 ToA"
              << std::setw(14) << "SF12 Energy"
              << std::setw(12) << "SF12 Bat."
              << std::endl;
    std::cout << "  ------------------------------------------------------------------------------------------------------" << std::endl;

    for (const auto& sc : scenarios)
    {
        // SF7 evaluation
        LoRaToaResult r7 = CalculateLoRaToa(7, 125000.0, sc.payloadBytes, sc.cpuTimeMs, 4.5);
        double totalJ7 = r7.txEnergyJoules + r7.cpuEnergyJoules;

        // SF12 evaluation
        LoRaToaResult r12 = CalculateLoRaToa(12, 125000.0, sc.payloadBytes, sc.cpuTimeMs, 4.5);
        double totalJ12 = r12.txEnergyJoules + r12.cpuEnergyJoules;

        double energyPerHourSf7 = (TRANSMISSIONS_PER_HOUR * totalJ7) + SLEEP_JOULES_PER_HOUR;
        double hoursSf7 = USABLE_BATTERY_JOULES / energyPerHourSf7;
        double yearsSf7 = hoursSf7 / 8760.0;

        double energyPerHourSf12 = (TRANSMISSIONS_PER_HOUR * totalJ12) + SLEEP_JOULES_PER_HOUR;
        double hoursSf12 = USABLE_BATTERY_JOULES / energyPerHourSf12;
        double yearsSf12 = hoursSf12 / 8760.0;

        ToaEvaluation eval;
        eval.scenario = sc.name;
        eval.bytes = sc.payloadBytes;
        eval.sf7ToaMs = r7.timeOnAirMs;
        eval.sf7TxJoules = r7.txEnergyJoules;
        eval.sf7CpuJoules = r7.cpuEnergyJoules;
        eval.sf7TotalJoules = totalJ7;
        eval.sf12ToaMs = r12.timeOnAirMs;
        eval.sf12TxJoules = r12.txEnergyJoules;
        eval.sf12CpuJoules = r12.cpuEnergyJoules;
        eval.sf12TotalJoules = totalJ12;
        eval.batteryYearsSf7_15m = yearsSf7;
        eval.batteryYearsSf12_15m = yearsSf12;
        toaResults.push_back(eval);

        std::cout << "  " << std::left << std::setw(26) << sc.name
                  << std::setw(6) << sc.payloadBytes
                  << std::fixed << std::setprecision(1) << std::setw(8) << r7.timeOnAirMs << " ms"
                  << std::setprecision(3) << std::setw(10) << (totalJ7 * 1000.0) << " mJ"
                  << std::setprecision(0) << std::setw(8) << r12.timeOnAirMs << " ms"
                  << std::setprecision(1) << std::setw(10) << (totalJ12 * 1000.0) << " mJ"
                  << std::setprecision(2) << std::setw(8) << yearsSf12 << " yrs"
                  << std::endl;
    }
    std::cout << "  ------------------------------------------------------------------------------------------------------" << std::endl;

    // ------------------------------------------------------------------------
    // SECTION 4: Export JSON Evidence Artifact
    // ------------------------------------------------------------------------
    std::string jsonPath = argc > 1 ? argv[1] : "build/hardware-resource-profiling-results.json";
    std::ofstream out(jsonPath);

    if (out.is_open())
    {
        out << "{\n";
        out << "  \"metadata\": {\n";
        out << "    \"task\": \"NS-07.01.03\",\n";
        out << "    \"compiler\": \"" << compilerName << "\",\n";
        out << "    \"compiler_version\": \"" << compilerVer << "\",\n";
        out << "    \"cpp_standard\": \"" << __cplusplus << "\",\n";
        out << "    \"measurement_scope\": \"host objects and timings; MCU cycles and battery are illustrative models\",\n";
        out << "    \"all_passed\": " << (allPassed ? "true" : "false") << ",\n";
        out << "    \"iterations\": " << ITERATIONS << "\n";
        out << "  },\n";

        // Memory Envelopes
        out << "  \"host_object_sizes\": {\n";
        out << "    \"target_mcu_sram_measured\": false,\n";
        out << "    \"asx_v1_sizeof_bytes\": " << sizeAsxV1 << ",\n";
        out << "    \"asx_v2_sizeof_bytes\": " << sizeAsxV2 << ",\n";
        out << "    \"decoder_light_sizeof_bytes\": " << sizeDecoderLight << ",\n";
        out << "    \"v2_static_buffer_bytes\": " << v2MtuBuffer << ",\n";
        out << "    \"v2_object_and_buffer_bytes\": " << totalV2Footprint << ",\n";
        out << "    \"v2_dynamic_heap_allocations\": " << encoderHeapAllocations << ",\n";
        out << "    \"decoder_dynamic_heap_allocations\": " << decoderHeapAllocations << "\n";
        out << "  },\n";

        // Latency Benchmarks
        out << "  \"latency_benchmarks\": [\n";
        for (size_t i = 0; i < benchmarks.size(); ++i)
        {
            const auto& bm = benchmarks[i];
            out << "    {\n";
            out << "      \"mode\": \"" << bm.modeName << "\",\n";
            out << "      \"sample_count\": " << bm.sampleCount << ",\n";
            out << "      \"output_bytes\": " << bm.outputBytes << ",\n";
            out << "      \"mean_us\": " << bm.meanUs << ",\n";
            out << "      \"min_us\": " << bm.minUs << ",\n";
            out << "      \"max_us\": " << bm.maxUs << ",\n";
            out << "      \"p50_us\": " << bm.p50Us << ",\n";
            out << "      \"p99_us\": " << bm.p99Us << ",\n";
            out << "      \"us_per_sample\": " << bm.usPerSample << ",\n";
            out << "      \"projected_cycles_atmega16m\": " << bm.cyclesAtmega16M << ",\n";
            out << "      \"projected_cycles_stm32_72m\": " << bm.cyclesStm32_72M << ",\n";
            out << "      \"projected_cycles_esp32_240m\": " << bm.cyclesEsp32_240M << "\n";
            out << "    }" << (i + 1 < benchmarks.size() ? "," : "") << "\n";
        }
        out << "  ],\n";

        // LoRa Physical Battery Model
        out << "  \"lorawan_battery_model\": {\n";
        out << "    \"bandwidth_hz\": 125000,\n";
        out << "    \"transceiver\": \"Semtech SX1262\",\n";
        out << "    \"tx_current_ma\": 22.0,\n";
        out << "    \"tx_power_dbm\": 14.0,\n";
        out << "    \"battery_type\": \"LiSOCl2\",\n";
        out << "    \"battery_usable_capacity_mah\": 2000,\n";
        out << "    \"battery_usable_joules\": " << USABLE_BATTERY_JOULES << ",\n";
        out << "    \"deep_sleep_current_ua\": 3.2,\n";
        out << "    \"evaluations\": [\n";
        for (size_t i = 0; i < toaResults.size(); ++i)
        {
            const auto& te = toaResults[i];
            out << "      {\n";
            out << "        \"scenario\": \"" << te.scenario << "\",\n";
            out << "        \"payload_bytes\": " << te.bytes << ",\n";
            out << "        \"sf7_toa_ms\": " << te.sf7ToaMs << ",\n";
            out << "        \"sf7_total_joules\": " << te.sf7TotalJoules << ",\n";
            out << "        \"sf12_toa_ms\": " << te.sf12ToaMs << ",\n";
            out << "        \"sf12_total_joules\": " << te.sf12TotalJoules << ",\n";
            out << "        \"battery_lifetime_years_sf7_15m\": " << te.batteryYearsSf7_15m << ",\n";
            out << "        \"battery_lifetime_years_sf12_15m\": " << te.batteryYearsSf12_15m << "\n";
            out << "      }" << (i + 1 < toaResults.size() ? "," : "") << "\n";
        }
        out << "    ]\n";
        out << "  }\n";
        out << "}\n";
        out.close();
        if (!out)
        {
            std::cerr << "ERROR: Could not write JSON artifact: " << jsonPath << std::endl;
            return 1;
        }
        std::cout << "\n[4] Profiling JSON artifact generated successfully at: " << jsonPath << std::endl;
    }
    else
    {
        std::cerr << "ERROR: Could not open JSON artifact output path: " << jsonPath << std::endl;
        return 1;
    }

    std::cout << "================================================================" << std::endl;
    std::cout << (allPassed ? "Host profiling checks PASSED." : "Host profiling checks FAILED.") << std::endl;
    std::cout << "================================================================" << std::endl;
    return allPassed ? 0 : 1;
}
