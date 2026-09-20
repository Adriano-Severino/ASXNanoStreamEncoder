#include "Arduino.h"
#include "ASXNanoStreamEncoder.h"
#include "AsxEncoderV2.h"
#include "ASXProfileV2.h"
#include "AsxNanoStreamDecoderLight.h"

volatile size_t g_v1_size = 0;
volatile size_t g_v2_size = 0;
volatile int g_dec_count = 0;

void ProbeCallback(String, int)
{
    g_dec_count++;
}

void TestV1Probe()
{
    AsxNanoStream enc;
    enc.setBaseline(100);
    enc.addAnalog(120);
    enc.addBinary(true);
    enc.addBinary(true);
    g_v1_size = enc.length();
}

void TestV2Probe(const int64_t* samples, size_t count)
{
    uint8_t buf[128];
    AsxEncoderV2 enc(buf, sizeof(buf));
    enc.BeginFrame(1, 1, 100);
    enc.EncodeBlockAuto(0, AsxDataType::Int16, samples, count);
    enc.EndFrame();
    g_v2_size = enc.GetLength();
}

void TestDecoderProbe(const char* token)
{
    AsxNanoStreamDecoder dec(ProbeCallback);
    dec.parse(String(token));
}
