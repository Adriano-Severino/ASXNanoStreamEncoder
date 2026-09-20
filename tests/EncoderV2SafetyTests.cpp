#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>
#include "../src/AsxEncoderV2.h"

using BlockEncoder = bool (AsxEncoderV2::*)(uint32_t, AsxDataType, const int64_t*, size_t);
static const BlockEncoder modes[] = {
    &AsxEncoderV2::EncodeBlockRAW, &AsxEncoderV2::EncodeBlockDELTA_VARINT,
    &AsxEncoderV2::EncodeBlockRANGE_BITPACK, &AsxEncoderV2::EncodeBlockDELTA_BITPACK,
    &AsxEncoderV2::EncodeBlockDELTA_RLE, &AsxEncoderV2::EncodeBlockAuto
};

static void TestInvalidInputs()
{
    uint8_t storage[128] = {};
    AsxEncoderV2 encoder(storage, sizeof(storage));
    const int64_t invalidSamples[] = { INT64_MIN, INT64_MAX };
    const int64_t validSamples[] = { 0, 1 };
    for (auto mode : modes)
    {
        assert(encoder.BeginFrame(1, 1, 1));
        const size_t originalLength = encoder.GetLength();
        assert(!(encoder.*mode)(0, AsxDataType::Int32, nullptr, 2));
        assert(!(encoder.*mode)(0, AsxDataType::Int32, validSamples, SIZE_MAX));
        assert(!(encoder.*mode)(0, AsxDataType::Int32, validSamples, 0));
        assert(!(encoder.*mode)(0, AsxDataType::Int32, invalidSamples, 2));
        assert(!(encoder.*mode)(0, AsxDataType::Float32, validSamples, 2));
        assert(!(encoder.*mode)(0, static_cast<AsxDataType>(255), validSamples, 2));
        assert(encoder.GetLength() == originalLength);
        assert((encoder.*mode)(0, AsxDataType::Int32, validSamples, 2));
        assert(encoder.EndFrame());
    }
    assert(AsxEncoderV2::CalculateCostRAW(AsxDataType::Int32, SIZE_MAX) == SIZE_MAX);
    assert(AsxEncoderV2::CalculateCostDELTA_VARINT(AsxDataType::Int32, 0, nullptr, 2) == SIZE_MAX);
    assert(AsxEncoderV2::CalculateCostDELTA_VARINT(AsxDataType::Int32, INT64_MIN, validSamples, 2) == SIZE_MAX);
    assert(AsxEncoderV2::CalculateCostRANGE_BITPACK(AsxDataType::Int32, invalidSamples, 2) == SIZE_MAX);
    assert(AsxEncoderV2::CalculateCostDELTA_BITPACK(AsxDataType::Int32, invalidSamples, 2) == SIZE_MAX);
    assert(AsxEncoderV2::CalculateCostDELTA_RLE(AsxDataType::Int32, invalidSamples, 2) == SIZE_MAX);
    assert(encoder.BeginFrame(1, 1, 1));
    assert(!encoder.EncodeBlockCONSTANT(0, AsxDataType::UInt16, -1, 1));
    assert(!encoder.EncodeBlockCONSTANT(0, AsxDataType::Int16, 32768, 1));
    assert(!encoder.EncodeBlockCONSTANT(0, AsxDataType::Boolean, 2, 1));
    assert(!encoder.EncodeBlockCONSTANT(0, AsxDataType::Int32, 1, SIZE_MAX));
    assert(encoder.EncodeBlockCONSTANT(0, AsxDataType::UInt32, UINT32_MAX, 1));
    assert(encoder.EndFrame());
}

static void TestCrcAndLifecycle()
{
    uint8_t storage[32] = {};
    const int64_t sample = 250;
    // Independent golden checksum: Python binascii.crc_hqx(frameWithoutCrc, 0xffff).
    const uint8_t expected[] = { 0xA2, 1, 1, 1, 2, 0x81, 0, 0, 1, 0x82, 0, 0, 0xFA, 0, 0xA4, 0x5E };
    AsxEncoderV2 encoder(storage, sizeof(storage));
    assert(encoder.BeginFrame(1, 1, 1, 2));
    assert(encoder.EncodeBlockRAW(0, AsxDataType::Int16, &sample, 1));
    assert(encoder.EndFrame());
    assert(encoder.GetLength() == sizeof(expected));
    assert(std::memcmp(storage, expected, sizeof(expected)) == 0);
    assert(encoder.EndFrame()); // No second trailer.
    assert(!encoder.WriteByte(0));
    assert(!encoder.EncodeBlockRAW(0, AsxDataType::Int16, &sample, 1));
    assert(encoder.GetLength() == sizeof(expected));
    assert(!encoder.BeginFrame(1, 1, 1, 4));
    assert(!encoder.EndFrame());

    for (size_t capacity = 0; capacity <= sizeof(expected); ++capacity)
    {
        std::memset(storage, 0xCC, sizeof(storage));
        AsxEncoderV2 limited(storage, capacity);
        const bool success = limited.BeginFrame(1, 1, 1, 2) &&
            limited.EncodeBlockRAW(0, AsxDataType::Int16, &sample, 1) && limited.EndFrame();
        assert(success == (capacity == sizeof(expected)));
        for (size_t index = capacity; index < sizeof(storage); ++index) assert(storage[index] == 0xCC);
    }
}

static void TestBudgetsAndBoundaryValues()
{
    uint8_t storage[4096] = {};
    AsxEncoderV2 encoder(storage, sizeof(storage));
    assert(encoder.BeginFrame(1, 1, 1));
    assert(encoder.EncodeBlockCONSTANT(0, AsxDataType::Int16, 1, 65536));
    assert(!encoder.EncodeBlockCONSTANT(0, AsxDataType::Int16, 1, 1));
    assert(encoder.EndFrame());
    assert(encoder.BeginFrame(1, 1, 1));
    for (int index = 0; index < 256; ++index)
        assert(encoder.EncodeBlockCONSTANT(0, AsxDataType::Int16, 1, 1));
    assert(!encoder.EncodeBlockCONSTANT(0, AsxDataType::Int16, 1, 1));
    assert(encoder.EndFrame());

    const int64_t signedSamples[] = { INT32_MIN, INT32_MAX, INT32_MIN, INT32_MAX };
    const int64_t unsignedSamples[] = { 0, UINT32_MAX, 0, UINT32_MAX };
    for (auto mode : modes)
    {
        assert(encoder.BeginFrame(1, 1, 1));
        assert((encoder.*mode)(0, AsxDataType::Int32, signedSamples, 4));
        assert(encoder.EndFrame());
        assert(encoder.BeginFrame(1, 1, 1));
        assert((encoder.*mode)(0, AsxDataType::UInt32, unsignedSamples, 4));
        assert(encoder.EndFrame());
    }
}

static uint64_t ReadULEB128(const uint8_t* storage, size_t length, size_t& offset)
{
    uint64_t value = 0;
    for (unsigned shift = 0; shift < 64; shift += 7)
    {
        assert(offset < length);
        const uint8_t byte = storage[offset++];
        value |= uint64_t(byte & 0x7F) << shift;
        if ((byte & 0x80) == 0) return value;
    }
    assert(false);
    return 0;
}

static void TestBitpackRoundTrip()
{
    // Alternating extremes require 32-bit offsets and 33-bit ZigZag deltas.
    // Read one bit at a time, independently of the encoder's accumulator.
    const int64_t samples[] = { 0, UINT32_MAX, 1, UINT32_MAX - 1, 2, UINT32_MAX };
    for (bool delta : { false, true })
    {
        uint8_t storage[128] = {};
        AsxEncoderV2 encoder(storage, sizeof(storage));
        assert(encoder.BeginFrame(1, 1, 1));
        assert(delta ? encoder.EncodeBlockDELTA_BITPACK(0, AsxDataType::UInt32, samples, 6)
                     : encoder.EncodeBlockRANGE_BITPACK(0, AsxDataType::UInt32, samples, 6));
        assert(encoder.EndFrame());
        size_t offset = 5;
        assert(ReadULEB128(storage, encoder.GetLength(), offset) == 1);
        assert(ReadULEB128(storage, encoder.GetLength(), offset) == 0);
        assert(ReadULEB128(storage, encoder.GetLength(), offset) == 6);
        const size_t bodyLength = static_cast<size_t>(ReadULEB128(storage, encoder.GetLength(), offset));
        assert(storage[offset++] == static_cast<uint8_t>(delta ? AsxModeId::DELTA_BITPACK : AsxModeId::RANGE_BITPACK));
        assert(bodyLength == encoder.GetLength() - offset);
        uint32_t base = 0;
        for (unsigned byte = 0; byte < 4; ++byte) base |= uint32_t(storage[offset++]) << (8 * byte);
        const unsigned width = storage[offset++];
        assert(width == (delta ? 33u : 32u));
        size_t bitOffset = offset * 8;
        int64_t previous = base;
        for (size_t index = delta ? 1 : 0; index < 6; ++index)
        {
            uint64_t packed = 0;
            for (unsigned bit = 0; bit < width; ++bit, ++bitOffset)
            {
                assert(bitOffset / 8 < encoder.GetLength());
                packed |= uint64_t((storage[bitOffset / 8] >> (bitOffset % 8)) & 1) << bit;
            }
            const int64_t value = delta
                ? previous + static_cast<int64_t>(packed >> 1) * ((packed & 1) ? -1 : 1) - static_cast<int64_t>(packed & 1)
                : static_cast<int64_t>(base) + static_cast<int64_t>(packed);
            assert(value == samples[index]);
            previous = value;
        }
        while (bitOffset < encoder.GetLength() * 8)
        {
            assert(((storage[bitOffset / 8] >> (bitOffset % 8)) & 1) == 0);
            ++bitOffset;
        }
    }
}

static void TestCapacityForAllModes()
{
    const int64_t samples[] = { INT32_MIN, INT32_MAX, 0, -1, INT32_MIN };
    uint8_t storage[128];
    for (auto mode : modes)
    {
        AsxEncoderV2 full(storage, sizeof(storage));
        assert(full.BeginFrame(UINT32_MAX, UINT32_MAX, UINT32_MAX, 3, UINT64_MAX));
        assert((full.*mode)(UINT32_MAX, AsxDataType::Int32, samples, 5));
        assert(full.EndFrame());
        const size_t required = full.GetLength();
        for (size_t capacity = 0; capacity <= required; ++capacity)
        {
            std::memset(storage, 0xCC, sizeof(storage));
            AsxEncoderV2 limited(storage, capacity);
            const bool success = limited.BeginFrame(UINT32_MAX, UINT32_MAX, UINT32_MAX, 3, UINT64_MAX) &&
                (limited.*mode)(UINT32_MAX, AsxDataType::Int32, samples, 5) && limited.EndFrame();
            assert(success == (capacity == required));
            assert(limited.GetLength() <= capacity);
            for (size_t index = capacity; index < sizeof(storage); ++index) assert(storage[index] == 0xCC);
        }
    }
}

static void TestOversizedAutoBlock()
{
    std::vector<int64_t> samples(20000);
    for (size_t index = 0; index < samples.size(); ++index) samples[index] = (index % 2) ? UINT32_MAX : 0;
    uint8_t storage[32] = {};
    AsxEncoderV2 encoder(storage, sizeof(storage));
    assert(encoder.BeginFrame(1, 1, 1));
    const size_t originalLength = encoder.GetLength();
    assert(!encoder.EncodeBlockAuto(0, AsxDataType::UInt32, samples.data(), samples.size()));
    assert(encoder.GetLength() == originalLength);
    assert(!encoder.HasOverflowed());
    assert(encoder.EncodeBlockCONSTANT(0, AsxDataType::UInt32, 1, 1));
    assert(encoder.EndFrame());
}

int main()
{
    TestInvalidInputs();
    TestCrcAndLifecycle();
    TestBudgetsAndBoundaryValues();
    TestBitpackRoundTrip();
    TestCapacityForAllModes();
    TestOversizedAutoBlock();
    // Failed headers must never patch beyond the caller's declared capacity.
    for (size_t capacity = 0; capacity < 7; ++capacity)
    {
        uint8_t storage[16];
        for (auto& byte : storage) byte = 0xCC;
        AsxEncoderV2 encoder(storage, capacity);
        assert(!encoder.BeginFrame(1, 1, 1));
        assert(!encoder.EndFrame());
        for (size_t index = capacity; index < sizeof(storage); ++index)
            assert(storage[index] == 0xCC);
    }

    uint8_t storage[32] = {};
    AsxEncoderV2 encoder(storage, sizeof(storage));
    assert(encoder.BeginFrame(1, 1, 1));
    assert(!encoder.CanFit(SIZE_MAX));
    assert(!encoder.WriteBytes(storage, SIZE_MAX));
    assert(!encoder.EndFrame());
    assert(encoder.BeginFrame(1, 1, 1));
    assert(!encoder.EndBlock(SIZE_MAX, 0));
    assert(!encoder.EndBlock(0, 0));
    const size_t patch = encoder.BeginBlock(0, 1, AsxModeId::RAW);
    assert(patch != 0);
    assert(!encoder.EndFrame());
    assert(encoder.WriteValueLE(250, AsxDataType::Int16));
    assert(!encoder.EndBlock(patch, 1));
    assert(encoder.EndBlock(patch, 2));
    assert(!encoder.EndBlock(patch, 2));
    assert(encoder.EndFrame());
    assert(encoder.GetBlockCount() == 1);

    AsxEncoderV2 missingBuffer(nullptr, 32);
    assert(!missingBuffer.BeginFrame(1, 1, 1));
    assert(!missingBuffer.EndFrame());
    assert(AsxEncoderV2::EncodeULEB128(1, nullptr, 10) == 0);
    assert(AsxEncoderV2::EncodeZigZag(-1) == 1);
    assert(AsxEncoderV2::EncodeZigZag(INT64_MIN) == UINT64_MAX);
    assert(AsxEncoderV2::EncodeZigZag(INT64_MAX) == UINT64_MAX - 1);
}
