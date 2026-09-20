#include <AsxEncoderV2.h>

uint8_t frameBuffer[128];
AsxEncoderV2 encoder(frameBuffer, sizeof(frameBuffer));
uint32_t sequenceNumber = 0;

void setup() {
  Serial.begin(115200);
}

void loop() {
  const int64_t temperatures[] = {250, 251, 250, 251};
  // Profile 1, channel 0: temperature in tenths of a degree Celsius.
  if (encoder.BeginFrame(1, 1, sequenceNumber++,
                         static_cast<uint8_t>(AsxFrameFlags::HasCRC)) &&
      encoder.EncodeBlockAuto(0, AsxDataType::Int16, temperatures, 4) &&
      encoder.EndFrame()) {
    Serial.write(encoder.GetBuffer(), encoder.GetLength());
  }
  delay(5000);
}
