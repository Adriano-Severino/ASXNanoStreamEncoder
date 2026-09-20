/*
 * ASXNanoStream Protocol V2 - Encoder Library (C++)
 * Version: 2.0.0
 * License: MIT
 * Author: Adriano Xavier
 *
 * ZERO alocação dinâmica (no `std::string` or `new/malloc`).
 */

#ifndef ASX_ENCODER_V2_H
#define ASX_ENCODER_V2_H

#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>
#include "ASXProfileV2.h"

// Modos de empacotamento V2
enum class AsxModeId : uint8_t
{
    RAW = 0x00,
    CONSTANT = 0x01,
    DELTA_VARINT = 0x02,
    DELTA_BITPACK = 0x03,
    RANGE_BITPACK = 0x04,
    DELTA_RLE = 0x05
};

// Flags de Frame V2
enum class AsxFrameFlags : uint8_t
{
    None = 0x00,
    HasTimestamp = 0x01,
    HasCRC = 0x02
};

class AsxEncoderV2
{
private:
    uint8_t* _buffer;
    size_t _capacity;
    size_t _length;
    bool _overflowed;
    size_t _blockCountPatchIdx;
    uint32_t _blockCount;
    bool _frameStarted = false;
    size_t _activeBlockPatchIdx = 0;
    bool _frameFinalized = false;
    uint8_t _frameFlags = 0;
    uint32_t _sampleCount = 0;
    uint32_t _activeBlockSamples = 0;

    static bool IsValidValue(AsxDataType type, int64_t value)
    {
        switch (type)
        {
            case AsxDataType::Boolean: return value == 0 || value == 1;
            case AsxDataType::Int16: return value >= INT16_MIN && value <= INT16_MAX;
            case AsxDataType::UInt16: return value >= 0 && value <= UINT16_MAX;
            case AsxDataType::Int32: return value >= INT32_MIN && value <= INT32_MAX;
            case AsxDataType::UInt32: return value >= 0 && value <= UINT32_MAX;
            // This integer API cannot encode IEEE-754 float values faithfully.
            default: return false;
        }
    }

    static bool ValidateSamples(AsxDataType type, const int64_t* samples, size_t sampleCount)
    {
        // An array cannot exceed the addressable object size (notably on AVR).
        if (samples == nullptr || sampleCount == 0 || sampleCount > 65536UL ||
            sampleCount > SIZE_MAX / sizeof(*samples)) return false;
        for (size_t index = 0; index < sampleCount; ++index)
            if (!IsValidValue(type, samples[index])) return false;
        return true;
    }

    static size_t BoundedCost(uint64_t cost)
    {
        return cost >= SIZE_MAX ? SIZE_MAX : static_cast<size_t>(cost);
    }

    inline bool checkCapacity(size_t needed)
    {
        if (_frameFinalized || _overflowed) return false;
        if (_buffer == nullptr || _length > _capacity || needed > _capacity - _length)
        {
            _overflowed = true;
            return false;
        }
        return true;
    }

public:
    // Construtor: requer um buffer pre-alocado para evitar heap fragmentation no uC.
    AsxEncoderV2(uint8_t* buffer, size_t capacity)
        : _buffer(buffer), _capacity(capacity), _length(0), _overflowed(false), _blockCountPatchIdx(0), _blockCount(0)
    {
    }

    void Reset()
    {
        _length = 0;
        _overflowed = false;
        _blockCountPatchIdx = 0;
        _blockCount = 0;
        _frameStarted = false;
        _activeBlockPatchIdx = 0;
        _frameFinalized = false;
        _frameFlags = 0;
        _sampleCount = 0;
        _activeBlockSamples = 0;
    }

    size_t GetLength() const { return _length; }
    size_t GetCapacity() const { return _capacity; }
    size_t GetRemainingCapacity() const { return _capacity > _length ? _capacity - _length : 0; }
    bool CanFit(size_t bytes) const { return _buffer != nullptr && _length <= _capacity && bytes <= _capacity - _length; }
    bool HasOverflowed() const { return _overflowed; }
    const uint8_t* GetBuffer() const { return _buffer; }
    uint32_t GetBlockCount() const { return _blockCount; }

    // --- MÉTODOS DE FORMAÇÃO DE FRAME (NS-03.04.03) ---

    bool BeginFrame(uint32_t profileId, uint32_t profileVersion, uint32_t sequenceNumber, uint8_t flags = 0, uint64_t timestamp = 0)
    {
        Reset();
        if ((flags & ~uint8_t(3)) != 0) return false;
        _frameFlags = flags;
        if (!WriteByte(0xA2)) return false; // Magic Byte
        if (!WriteULEB128(profileId)) return false;
        if (!WriteULEB128(profileVersion)) return false;
        if (!WriteULEB128(sequenceNumber)) return false;
        if (!WriteByte(flags)) return false;

        if (flags & static_cast<uint8_t>(AsxFrameFlags::HasTimestamp))
        {
            if (!WriteULEB128(timestamp)) return false;
        }

        _blockCountPatchIdx = _length;
        // Reserva 2 bytes fixos com padding para contador de blocos (até 16383 blocos)
        if (!checkCapacity(2)) return false;
        _buffer[_length++] = 0x80;
        _buffer[_length++] = 0x00;
        _frameStarted = true;
        return true;
    }

    bool EndFrame()
    {
        if (_frameFinalized) return true;
        if (!_frameStarted || _overflowed || _activeBlockPatchIdx != 0) return false;
        if (_blockCount > 16383) return false;
        const bool hasCrc = (_frameFlags & static_cast<uint8_t>(AsxFrameFlags::HasCRC)) != 0;
        if (hasCrc && !checkCapacity(2)) return false;
        _buffer[_blockCountPatchIdx] = (_blockCount & 0x7F) | 0x80;
        _buffer[_blockCountPatchIdx + 1] = (_blockCount >> 7) & 0x7F;
        if (hasCrc)
        {
            uint16_t checksum = 0xFFFF;
            for (size_t index = 0; index < _length; ++index)
            {
                checksum ^= static_cast<uint16_t>(_buffer[index]) << 8;
                for (uint8_t bit = 0; bit < 8; ++bit)
                    checksum = static_cast<uint16_t>((checksum << 1) ^ ((checksum & 0x8000) ? 0x1021 : 0));
            }
            _buffer[_length++] = static_cast<uint8_t>(checksum);
            _buffer[_length++] = static_cast<uint8_t>(checksum >> 8);
        }
        _frameFinalized = true;
        return true;
    }

    // Utilitário estático: Codifica ULEB128 em um buffer arbitrário e retorna bytes escritos
    static size_t EncodeULEB128(uint64_t value, uint8_t* out_buffer, size_t max_len)
    {
        if (out_buffer == nullptr) return 0;
        size_t count = 0;
        do
        {
            if (count >= max_len) return 0; // Previne overflow no buffer de saída
            uint8_t byte = value & 0x7F;
            value >>= 7;
            if (value != 0)
                byte |= 0x80;
            out_buffer[count++] = byte;
        } while (value != 0);
        return count;
    }

    // Utilitário estático: Mapeamento ZigZag
    static uint64_t EncodeZigZag(int64_t value)
    {
        return (static_cast<uint64_t>(value) << 1) ^ (uint64_t(0) - static_cast<uint64_t>(value < 0));
    }

    // Utilitário estático: Tamanho (bytes) de um valor ULEB128 sem escrevê-lo
    static size_t GetULEB128Size(uint64_t value)
    {
        size_t count = 0;
        do
        {
            count++;
            value >>= 7;
        } while (value != 0);
        return count;
    }

    // Utilitário para pegar largura em bytes de um AsxDataType
    static uint8_t GetDataTypeByteWidth(AsxDataType type)
    {
        switch (type)
        {
            case AsxDataType::Boolean: return 1;
            case AsxDataType::Int16:
            case AsxDataType::UInt16:  return 2;
            case AsxDataType::Int32:
            case AsxDataType::UInt32:
            case AsxDataType::Float32: return 4;
            default: return 1;
        }
    }

    // Escreve um byte diretamente no buffer
    bool WriteByte(uint8_t value)
    {
        if (!checkCapacity(1)) return false;
        _buffer[_length++] = value;
        return true;
    }

    // Escreve um ULEB128 no buffer
    bool WriteULEB128(uint64_t value)
    {
        uint8_t temp[10];
        size_t size = EncodeULEB128(value, temp, sizeof(temp));
        if (size == 0 || !checkCapacity(size)) return false;
        for (size_t i = 0; i < size; i++)
        {
            _buffer[_length++] = temp[i];
        }
        return true;
    }

    // Escreve array de bytes no buffer
    bool WriteBytes(const uint8_t* data, size_t len)
    {
        if (data == nullptr && len != 0) return false;
        if (!checkCapacity(len)) return false;
        for (size_t i = 0; i < len; i++)
        {
            _buffer[_length++] = data[i];
        }
        return true;
    }

    // Escreve Little-Endian baseado no tipo
    bool WriteValueLE(int64_t value, AsxDataType type)
    {
        if (!IsValidValue(type, value)) return false;
        uint8_t width = GetDataTypeByteWidth(type);
        if (!checkCapacity(width)) return false;

        uint64_t v = static_cast<uint64_t>(value);
        for (uint8_t i = 0; i < width; i++)
        {
            _buffer[_length++] = static_cast<uint8_t>((v >> (i * 8)) & 0xFF);
        }
        return true;
    }

    // --- MÉTODOS DE CÁLCULO DE CUSTO (Para Feature NS-03.02.03 e Decisões de Buffer) ---

    static size_t CalculateCostRAW(AsxDataType type, size_t sampleCount)
    {
        if (!IsValidValue(type, 0) || sampleCount == 0 || sampleCount > 65536UL) return SIZE_MAX;
        return BoundedCost(static_cast<uint64_t>(GetDataTypeByteWidth(type)) * sampleCount);
    }

    static size_t CalculateCostCONSTANT(AsxDataType type)
    {
        if (!IsValidValue(type, 0)) return SIZE_MAX;
        return GetDataTypeByteWidth(type);
        // O Bloco já define o sample count no Header. O Body do CONSTANT possui APENAS o valor (e N implícito).
    }

    static size_t CalculateCostDELTA_VARINT(AsxDataType type, int64_t firstValue, const int64_t* samples, size_t sampleCount)
    {
        if (!ValidateSamples(type, samples, sampleCount) || !IsValidValue(type, firstValue)) return SIZE_MAX;
        if (sampleCount == 0) return 0;
        uint64_t cost = GetDataTypeByteWidth(type); // O primeiro valor é armazenado bruto (RAW) no início

        int64_t lastVal = firstValue;
        for (size_t i = 1; i < sampleCount; i++)
        {
            int64_t delta = samples[i] - lastVal;
            uint64_t zz = EncodeZigZag(delta);
            cost += GetULEB128Size(zz);
            lastVal = samples[i];
        }
        return BoundedCost(cost);
    }

    static size_t CalculateCostRANGE_BITPACK(AsxDataType type, const int64_t* samples, size_t sampleCount)
    {
        if (!ValidateSamples(type, samples, sampleCount)) return SIZE_MAX;
        if (sampleCount == 0) return 0;
        int64_t minVal = samples[0];
        int64_t maxVal = samples[0];
        for (size_t i = 1; i < sampleCount; i++) {
            if (samples[i] < minVal) minVal = samples[i];
            if (samples[i] > maxVal) maxVal = samples[i];
        }
        uint64_t range = maxVal - minVal;
        if (range == 0) return 0; // Invalido, deve usar CONSTANT

        uint8_t bitWidth = 0;
        while (range > 0) {
            bitWidth++;
            range >>= 1;
        }

        uint64_t totalBits = static_cast<uint64_t>(sampleCount) * bitWidth;
        uint64_t bytesForBits = (totalBits + 7) / 8;
        return BoundedCost(GetDataTypeByteWidth(type) + 1 + bytesForBits);
    }

    static size_t CalculateCostDELTA_BITPACK(AsxDataType type, const int64_t* samples, size_t sampleCount)
    {
        if (!ValidateSamples(type, samples, sampleCount)) return SIZE_MAX;
        if (sampleCount <= 1) return 0;
        uint64_t maxZigZag = 0;
        for (size_t i = 1; i < sampleCount; i++) {
            int64_t delta = samples[i] - samples[i-1];
            uint64_t zz = EncodeZigZag(delta);
            if (zz > maxZigZag) maxZigZag = zz;
        }
        if (maxZigZag == 0) return 0; // Invalido, deve usar CONSTANT

        uint8_t bitWidth = 0;
        while (maxZigZag > 0) {
            bitWidth++;
            maxZigZag >>= 1;
        }

        uint64_t totalBits = static_cast<uint64_t>(sampleCount - 1) * bitWidth;
        uint64_t bytesForBits = (totalBits + 7) / 8;
        return BoundedCost(GetDataTypeByteWidth(type) + 1 + bytesForBits);
    }

    static size_t CalculateCostDELTA_RLE(AsxDataType type, const int64_t* samples, size_t sampleCount)
    {
        if (!ValidateSamples(type, samples, sampleCount)) return SIZE_MAX;
        if (sampleCount <= 1) return 0; // Invalido

        uint64_t cost = GetDataTypeByteWidth(type); // O primeiro valor entra bruto (RAW)

        int64_t lastVal = samples[0];
        int64_t currentDelta = samples[1] - lastVal;
        uint32_t currentCount = 1;

        for (size_t i = 2; i < sampleCount; i++) {
            lastVal = samples[i-1];
            int64_t delta = samples[i] - lastVal;
            if (delta == currentDelta) {
                currentCount++;
            } else {
                cost += GetULEB128Size(currentCount);
                cost += GetULEB128Size(EncodeZigZag(currentDelta));
                currentDelta = delta;
                currentCount = 1;
            }
        }

        cost += GetULEB128Size(currentCount);
        cost += GetULEB128Size(EncodeZigZag(currentDelta));

        return BoundedCost(cost);
    }

    // --- MÉTODOS DE ENCODING DOS MODOS ---

    // Inicializa o Header do Bloco e retorna o índice onde o BodyLength foi escrito
    // (para fazer patch após a gravação do body).
    size_t BeginBlock(uint32_t channelId, uint32_t sampleCount, AsxModeId modeId)
    {
        if (!_frameStarted || _frameFinalized || _overflowed || _activeBlockPatchIdx != 0 || _blockCount >= 256 ||
            sampleCount == 0 || sampleCount > 65536UL - _sampleCount || static_cast<uint8_t>(modeId) > 5) return 0;
        if (!WriteULEB128(channelId)) return 0;
        if (!WriteULEB128(sampleCount)) return 0;

        // Espaço para o Body Length (vamos assumir 2 bytes no pior caso para um bloco normal num uC)
        // No entanto, para ser seguro em varint, precisamos conhecer o length exato.
        // Como o design requer varint e não temos length ainda, no uC é melhor gravar o body em um
        // sub-buffer ou gravar o length com padding fixo. Vamos usar padding fixo de 2 bytes (até 16383 bytes)
        // para facilitar a inserção in-place.
        size_t lengthPatchIndex = _length;
        if (!checkCapacity(2 + 1)) return 0; // 2 bytes padding length + 1 byte mode

        _buffer[_length++] = 0x80; // Placeholder byte 0
        _buffer[_length++] = 0x00; // Placeholder byte 1

        _buffer[_length++] = static_cast<uint8_t>(modeId);

        _activeBlockPatchIdx = lengthPatchIndex;
        _activeBlockSamples = sampleCount;
        return lengthPatchIndex;
    }

    bool EndBlock(size_t lengthPatchIndex, size_t bodyLength)
    {
        if (!_frameStarted || _overflowed || lengthPatchIndex == 0 || lengthPatchIndex != _activeBlockPatchIdx ||
            lengthPatchIndex > _length || _length - lengthPatchIndex < 3 ||
            bodyLength != _length - lengthPatchIndex - 3) return false;
        if (bodyLength > 16383) return false; // Limite suportado no padding fixo de 2 bytes
        // Preenche o ULEB128 de 2 bytes fixos com o valor do bodyLength
        _buffer[lengthPatchIndex] = (bodyLength & 0x7F) | 0x80;
        _buffer[lengthPatchIndex + 1] = (bodyLength >> 7) & 0x7F;
        _blockCount++;
        _sampleCount += _activeBlockSamples;
        _activeBlockPatchIdx = 0;
        return true;
    }

    // 0x00: RAW
    bool EncodeBlockRAW(uint32_t channelId, AsxDataType type, const int64_t* samples, size_t sampleCount)
    {
        if (!ValidateSamples(type, samples, sampleCount)) return false;
        size_t patchIdx = BeginBlock(channelId, sampleCount, AsxModeId::RAW);
        if (patchIdx == 0) return false;

        size_t startBody = _length;
        for (size_t i = 0; i < sampleCount; i++)
        {
            if (!WriteValueLE(samples[i], type)) return false;
        }

        return EndBlock(patchIdx, _length - startBody);
    }

    // 0x01: CONSTANT
    bool EncodeBlockCONSTANT(uint32_t channelId, AsxDataType type, int64_t constantValue, size_t sampleCount)
    {
        if (!IsValidValue(type, constantValue) || sampleCount == 0 || sampleCount > 65536UL) return false;
        size_t patchIdx = BeginBlock(channelId, sampleCount, AsxModeId::CONSTANT);
        if (patchIdx == 0) return false;

        size_t startBody = _length;
        if (!WriteValueLE(constantValue, type)) return false;

        return EndBlock(patchIdx, _length - startBody);
    }

    // 0x02: DELTA_VARINT
    bool EncodeBlockDELTA_VARINT(uint32_t channelId, AsxDataType type, const int64_t* samples, size_t sampleCount)
    {
        if (!ValidateSamples(type, samples, sampleCount)) return false;
        if (sampleCount == 0) return false;

        size_t patchIdx = BeginBlock(channelId, sampleCount, AsxModeId::DELTA_VARINT);
        if (patchIdx == 0) return false;

        size_t startBody = _length;

        // Primeiro valor armazenado como RAW (largura do DataType)
        if (!WriteValueLE(samples[0], type)) return false;

        int64_t lastVal = samples[0];
        for (size_t i = 1; i < sampleCount; i++)
        {
            int64_t delta = samples[i] - lastVal;
            uint64_t zz = EncodeZigZag(delta);
            if (!WriteULEB128(zz)) return false;
            lastVal = samples[i];
        }

        return EndBlock(patchIdx, _length - startBody);
    }

    // 0x04: RANGE_BITPACK
    bool EncodeBlockRANGE_BITPACK(uint32_t channelId, AsxDataType type, const int64_t* samples, size_t sampleCount)
    {
        if (!ValidateSamples(type, samples, sampleCount)) return false;
        if (sampleCount == 0) return false;
        int64_t minVal = samples[0];
        int64_t maxVal = samples[0];
        for (size_t i = 1; i < sampleCount; i++) {
            if (samples[i] < minVal) minVal = samples[i];
            if (samples[i] > maxVal) maxVal = samples[i];
        }
        uint64_t range = maxVal - minVal;
        if (range == 0) return false; // Width 0 requires CONSTANT mode

        uint8_t bitWidth = 0;
        while (range > 0) {
            bitWidth++;
            range >>= 1;
        }

        size_t patchIdx = BeginBlock(channelId, sampleCount, AsxModeId::RANGE_BITPACK);
        if (patchIdx == 0) return false;
        size_t startBody = _length;

        if (!WriteValueLE(minVal, type)) return false;
        if (!WriteByte(bitWidth)) return false;

        uint64_t bitBuffer = 0;
        uint8_t bitsInBuffer = 0;

        for (size_t i = 0; i < sampleCount; i++) {
            uint64_t offset = samples[i] - minVal;
            bitBuffer |= (offset << bitsInBuffer);
            bitsInBuffer += bitWidth;

            while (bitsInBuffer >= 8) {
                if (!WriteByte(bitBuffer & 0xFF)) return false;
                bitBuffer >>= 8;
                bitsInBuffer -= 8;
            }
        }
        if (bitsInBuffer > 0) {
            if (!WriteByte(bitBuffer & 0xFF)) return false;
        }

        return EndBlock(patchIdx, _length - startBody);
    }

    // 0x03: DELTA_BITPACK
    bool EncodeBlockDELTA_BITPACK(uint32_t channelId, AsxDataType type, const int64_t* samples, size_t sampleCount)
    {
        if (!ValidateSamples(type, samples, sampleCount)) return false;
        if (sampleCount <= 1) return false;

        uint64_t maxZigZag = 0;
        for (size_t i = 1; i < sampleCount; i++) {
            int64_t delta = samples[i] - samples[i-1];
            uint64_t zz = EncodeZigZag(delta);
            if (zz > maxZigZag) maxZigZag = zz;
        }
        if (maxZigZag == 0) return false; // Width 0 requires CONSTANT mode

        uint8_t bitWidth = 0;
        while (maxZigZag > 0) {
            bitWidth++;
            maxZigZag >>= 1;
        }

        size_t patchIdx = BeginBlock(channelId, sampleCount, AsxModeId::DELTA_BITPACK);
        if (patchIdx == 0) return false;
        size_t startBody = _length;

        if (!WriteValueLE(samples[0], type)) return false;
        if (!WriteByte(bitWidth)) return false;

        uint64_t bitBuffer = 0;
        uint8_t bitsInBuffer = 0;

        for (size_t i = 1; i < sampleCount; i++) {
            int64_t delta = samples[i] - samples[i-1];
            uint64_t zz = EncodeZigZag(delta);

            bitBuffer |= (zz << bitsInBuffer);
            bitsInBuffer += bitWidth;

            while (bitsInBuffer >= 8) {
                if (!WriteByte(bitBuffer & 0xFF)) return false;
                bitBuffer >>= 8;
                bitsInBuffer -= 8;
            }
        }
        if (bitsInBuffer > 0) {
            if (!WriteByte(bitBuffer & 0xFF)) return false;
        }

        return EndBlock(patchIdx, _length - startBody);
    }

    // 0x05: DELTA_RLE
    bool EncodeBlockDELTA_RLE(uint32_t channelId, AsxDataType type, const int64_t* samples, size_t sampleCount)
    {
        if (!ValidateSamples(type, samples, sampleCount)) return false;
        if (sampleCount <= 1) return false;

        size_t patchIdx = BeginBlock(channelId, sampleCount, AsxModeId::DELTA_RLE);
        if (patchIdx == 0) return false;
        size_t startBody = _length;

        if (!WriteValueLE(samples[0], type)) return false;

        int64_t lastVal = samples[0];
        int64_t currentDelta = samples[1] - lastVal;
        uint32_t currentCount = 1;

        for (size_t i = 2; i < sampleCount; i++) {
            lastVal = samples[i-1];
            int64_t delta = samples[i] - lastVal;

            if (delta == currentDelta) {
                currentCount++;
            } else {
                if (!WriteULEB128(currentCount)) return false;
                if (!WriteULEB128(EncodeZigZag(currentDelta))) return false;
                currentDelta = delta;
                currentCount = 1;
            }
        }

        if (!WriteULEB128(currentCount)) return false;
        if (!WriteULEB128(EncodeZigZag(currentDelta))) return false;

        return EndBlock(patchIdx, _length - startBody);
    }

    // Auto Selector
    bool EncodeBlockAuto(uint32_t channelId, AsxDataType type, const int64_t* samples, size_t sampleCount)
    {
        if (!ValidateSamples(type, samples, sampleCount)) return false;
        if (sampleCount == 0) return false;

        // Calcula os custos dos modos aplicáveis
        size_t costRAW = CalculateCostRAW(type, sampleCount);
        size_t costCONSTANT = SIZE_MAX;
        size_t costDELTA_VARINT = SIZE_MAX;
        size_t costDELTA_BITPACK = SIZE_MAX;
        size_t costRANGE_BITPACK = SIZE_MAX;
        size_t costDELTA_RLE = SIZE_MAX;

        bool allEqual = true;
        for (size_t i = 1; i < sampleCount; i++) {
            if (samples[i] != samples[0]) {
                allEqual = false;
                break;
            }
        }

        if (allEqual) {
            costCONSTANT = CalculateCostCONSTANT(type);
        } else {
            costDELTA_VARINT = CalculateCostDELTA_VARINT(type, samples[0], samples, sampleCount);
            size_t c_rpack = CalculateCostRANGE_BITPACK(type, samples, sampleCount);
            if (c_rpack > 0) costRANGE_BITPACK = c_rpack;

            size_t c_dpack = CalculateCostDELTA_BITPACK(type, samples, sampleCount);
            if (c_dpack > 0) costDELTA_BITPACK = c_dpack;

            size_t c_rle = CalculateCostDELTA_RLE(type, samples, sampleCount);
            if (c_rle > 0) costDELTA_RLE = c_rle;
        }

        // Seleção de mínimo custo com tie-breaker
        // CONSTANT > RAW > RANGE_BITPACK > DELTA_BITPACK > DELTA_VARINT > DELTA_RLE
        AsxModeId bestMode = AsxModeId::RAW;
        size_t minCost = costRAW;

        if (allEqual && costCONSTANT != SIZE_MAX && costCONSTANT <= minCost) {
            bestMode = AsxModeId::CONSTANT;
            minCost = costCONSTANT;
        }

        if (costRANGE_BITPACK < minCost) {
            bestMode = AsxModeId::RANGE_BITPACK;
            minCost = costRANGE_BITPACK;
        }

        if (costDELTA_BITPACK < minCost) {
            bestMode = AsxModeId::DELTA_BITPACK;
            minCost = costDELTA_BITPACK;
        }

        if (costDELTA_VARINT < minCost) {
            bestMode = AsxModeId::DELTA_VARINT;
            minCost = costDELTA_VARINT;
        }

        if (costDELTA_RLE < minCost) {
            bestMode = AsxModeId::DELTA_RLE;
            minCost = costDELTA_RLE;
        }

        // The fixed two-byte body length cannot represent larger blocks.
        // Reject before starting a block, including saturated cost estimates.
        if (minCost > 16383) return false;

        switch (bestMode) {
            case AsxModeId::CONSTANT: return EncodeBlockCONSTANT(channelId, type, samples[0], sampleCount);
            case AsxModeId::RAW: return EncodeBlockRAW(channelId, type, samples, sampleCount);
            case AsxModeId::RANGE_BITPACK: return EncodeBlockRANGE_BITPACK(channelId, type, samples, sampleCount);
            case AsxModeId::DELTA_BITPACK: return EncodeBlockDELTA_BITPACK(channelId, type, samples, sampleCount);
            case AsxModeId::DELTA_VARINT: return EncodeBlockDELTA_VARINT(channelId, type, samples, sampleCount);
            case AsxModeId::DELTA_RLE: return EncodeBlockDELTA_RLE(channelId, type, samples, sampleCount);
        }

        return false;
    }
};

#endif // ASX_ENCODER_V2_H
