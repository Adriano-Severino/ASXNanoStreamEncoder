#include <cassert>
#include <cstdint>
#include <iostream>
#include <Arduino.h>
#include <limits.h>
#include <string.h>
// Seed counters at their boundary without processing billions of samples.
#define private public
#include "../src/ASXNanoStreamEncoder.h"
#undef private

static void TestNumericBoundaries()
{
    AsxNanoStream encoder;
    assert(encoder.setBaseline(INT32_MIN));
    assert(encoder.addAnalog(INT32_MAX, UINT32_MAX));
    assert(encoder.addAnalog(INT32_MIN));
    assert(encoder.getPayload() == "B-2147483648#+4294967295v4294967295ms-4294967295v");
    encoder.reset();
    assert(encoder.setBaseline(INT32_MAX));
    assert(encoder.addAnalog(INT32_MAX));
    assert(encoder.getPayload() == "B2147483647#+0v");
}

static void TestCapacityAndBaselineAtomicity()
{
    AsxNanoStream encoder(0, 6);
    assert(encoder.addAnalog(10));
    assert(!encoder.addAnalog(100));
    assert(!encoder.setBaseline(300));
    assert(encoder.getSampleCount() == 1);
    assert(encoder.length() == 4);
    encoder.setMaxPayloadCapacity(20);
    assert(encoder.addAnalog(11));
    assert(encoder.getPayload() == "+10v+1v");
    encoder.setMaxPayloadCapacity(1);
    assert(encoder.getPayload() == "+10v+1v");
    encoder.reset();
    encoder.setMaxPayloadCapacity(SIZE_MAX);
    assert(encoder.addBinary(true));
    assert(!encoder.isFlushRequired());
    assert(!encoder.canAccept(""));
}

static void TestRleBoundariesAndLength()
{
    AsxNanoStream encoder(0, 5);
    assert(encoder.addBinary(true));
    assert(encoder.length() == 2);
    assert(encoder.addBinary(true));
    assert(encoder.length() == 4);
    assert(encoder.addBinary(true));
    assert(encoder.length() == 5);
    assert(encoder.getPayload() == "x3-1b");
    encoder.reset();
    encoder.setMaxPayloadCapacity(0);
    encoder.setMaxRepeatCount(3);
    for (int index = 0; index < 7; ++index) assert(encoder.addBinary(true));
    assert(encoder.getPayload() == "x3-1bx3-1b1b");
    encoder.reset();
    encoder.setMaxSampleCount(2);
    assert(encoder.addBinary(false));
    assert(encoder.addBinaryWithStatus(true) == AsxEncoderStatus::FlushRequired);
    assert(encoder.addBinaryWithStatus(true) == AsxEncoderStatus::BufferFull);
    assert(encoder.getSampleCount() == 2);
    assert(encoder.getPayload() == "0b1b");
}

static void TestLargePayload()
{
    AsxNanoStream encoder(0, 40000);
    encoder.setMaxSampleCount(0);
    for (uint32_t index = 0; index < 20000; ++index)
        assert(encoder.addBinary((index % 2) != 0));
    assert(encoder.length() == 40000);
    assert(encoder.getPayload().length() == 40000);
    assert(!encoder.addBinary(false));
    assert(encoder.getSampleCount() == 20000);
    assert(encoder.isFlushRequired());
}

static void TestAllocationFailures()
{
    AsxNanoStream encoder;
    assert(encoder.addAnalog(10));
    String::failNextReserve();
    assert(encoder.addAnalogWithStatus(100) == AsxEncoderStatus::BufferFull);
    assert(encoder.hasOverflowed());
    assert(encoder.getSampleCount() == 1);
    assert(encoder.length() == 4);
    assert(encoder.addAnalog(11));
    assert(encoder.getPayload() == "+10v+1v");
    String::failNextReserve();
    assert(!encoder.setBaseline(999));
    assert(encoder.addAnalog(12));
    assert(encoder.getPayload() == "+10v+1v+1v");
    encoder.reset();
    assert(encoder.addBinary(true, 65535));
    String::failNextCopy();
    assert(encoder.getPayload().length() == 0);
    assert(encoder.hasOverflowed());
    assert(encoder.getPayload() == "1b65535ms");
    assert(encoder.getSampleCount() == 1);
    encoder.reset();
    String::failNextReserve();
    assert(!encoder.addBinary(false));
    assert(encoder.getSampleCount() == 0);
    assert(encoder.length() == 0);
    String::failNextReserve();
    AsxNanoStream initialAllocationFailure(128);
    assert(initialAllocationFailure.hasOverflowed());
    String::resetAllocationFailures();
}

static void TestCounterAndPhysicalLimits()
{
    AsxNanoStream encoder;
    encoder.setMaxSampleCount(0);
    encoder._sampleCount = UINT32_MAX;
    assert(encoder.addBinaryWithStatus(true) == AsxEncoderStatus::BufferFull);
    assert(encoder.getSampleCount() == UINT32_MAX);
    assert(!encoder.canAccept("1b"));
    encoder.reset();
    encoder.setMaxRepeatCount(0);
    assert(encoder.addBinary(true));
    encoder._repeatCount = UINT32_MAX;
    assert(encoder.addBinary(true));
    assert(encoder.getPayload() == "x4294967295-1b1b");
    assert(!encoder.fitsCapacity(static_cast<uint64_t>(UINT_MAX)));
    assert(encoder.fitsCapacity(static_cast<uint64_t>(UINT_MAX) - 1));
}

int main()
{
    TestNumericBoundaries();
    TestCapacityAndBaselineAtomicity();
    TestRleBoundariesAndLength();
    TestLargePayload();
    TestAllocationFailures();
    TestCounterAndPhysicalLimits();
    std::cout << "Encoder V1 safety tests passed\n";
}
