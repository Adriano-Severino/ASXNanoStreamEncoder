/*
 * ASXNanoStream Protocol - Encoder Library (C++)
 * Version: 1.0.4
 * License: MIT
 * Author: Adriano Xavier
 */
#ifndef ASX_NANO_STREAM_H
#define ASX_NANO_STREAM_H

#include <Arduino.h>
#include <stdint.h>
#include <limits.h>
#include <string.h>

enum class AsxEncoderStatus : uint8_t
{
    Success = 0,
    FlushRequired = 1,
    BufferFull = 2,
    InvalidInput = 3
};

class AsxNanoStream
{
public:
    static const uint32_t MAX_V1_DURATION_MS = 65535U;
    static const int32_t MAX_ANALOG_VALUE = 2147483647;
    static const int32_t MIN_ANALOG_VALUE = -2147483647 - 1;

private:
    String _payload;
    // Every generated command fits: sign, ten decimal digits, 'v', NUL.
    // Fixed storage makes accepting a pending command independent of heap allocation.
    char _pendingCommand[13];
    uint32_t _pendingDuration;
    uint32_t _repeatCount;
    int32_t _lastAnalogValue;
    size_t _maxPayloadCapacity;
    uint32_t _maxRepeatCount;
    uint32_t _maxSampleCount;
    uint32_t _sampleCount;
    bool _overflowed;

    static char* writeUnsigned(char* output, uint64_t value)
    {
        char reversed[20];
        uint8_t count = 0;
        do
        {
            reversed[count++] = static_cast<char>('0' + value % 10);
            value /= 10;
        } while (value != 0);
        while (count != 0) *output++ = reversed[--count];
        *output = '\0';
        return output;
    }

    void appendUnsigned(uint32_t value)
    {
        char digits[11];
        writeUnsigned(digits, value);
        _payload += digits;
    }

    static uint64_t encodedCost(size_t commandLength, uint32_t duration, uint32_t repeats)
    {
        if (repeats == 0) return 0;
        const uint64_t unitLength = static_cast<uint64_t>(commandLength) +
            (duration > 0 ? digitCount(duration) + 2 : 0);
        const uint64_t literalCost = static_cast<uint64_t>(repeats) * unitLength;
        const uint64_t rleCost = 2 + digitCount(repeats) + unitLength;
        return repeats > 1 && rleCost <= literalCost ? rleCost : literalCost;
    }

    bool continuesPending(const char* command, uint32_t duration) const
    {
        return _repeatCount > 0 && _repeatCount < UINT32_MAX &&
            (_maxRepeatCount == 0 || _repeatCount < _maxRepeatCount) &&
            duration == _pendingDuration && strcmp(command, _pendingCommand) == 0;
    }

    uint64_t calculateProspectiveLength(const char* command, uint32_t duration) const
    {
        if (continuesPending(command, duration))
            return static_cast<uint64_t>(_payload.length()) +
                encodedCost(strlen(command), duration, _repeatCount + 1);
        return static_cast<uint64_t>(length()) + encodedCost(strlen(command), duration, 1);
    }

    bool fitsCapacity(uint64_t bytes) const
    {
        // AVR String lengths are unsigned int; leave room for the terminating NUL.
        // Apply this bound even when the caller disables the configured limit.
        return bytes < static_cast<uint64_t>(UINT_MAX) &&
            bytes < static_cast<uint64_t>(SIZE_MAX) &&
            (_maxPayloadCapacity == 0 || bytes <= _maxPayloadCapacity);
    }

    bool ensureStorage(uint64_t bytes)
    {
        if (!fitsCapacity(bytes) || !_payload.reserve(static_cast<unsigned int>(bytes)))
        {
            _overflowed = true;
            return false;
        }
        return true;
    }

    bool sampleLimitReached() const
    {
        return _sampleCount == UINT32_MAX ||
            (_maxSampleCount > 0 && _sampleCount >= _maxSampleCount);
    }

    void appendPendingUnit()
    {
        _payload += _pendingCommand;
        if (_pendingDuration > 0)
        {
            appendUnsigned(_pendingDuration);
            _payload += "ms";
        }
    }

    // The caller reserves the complete resulting payload before any mutation.
    void flushPending()
    {
        if (_repeatCount == 0) return;
        const uint64_t unitLength = encodedCost(strlen(_pendingCommand), _pendingDuration, 1);
        const uint64_t rleCost = 2 + digitCount(_repeatCount) + unitLength;
        if (_repeatCount > 1 && rleCost <= static_cast<uint64_t>(_repeatCount) * unitLength)
        {
            _payload += 'x';
            appendUnsigned(_repeatCount);
            _payload += '-';
            appendPendingUnit();
        }
        else
        {
            for (uint32_t index = 0; index < _repeatCount; ++index) appendPendingUnit();
        }
        _repeatCount = 0;
        _pendingCommand[0] = '\0';
        _pendingDuration = 0;
    }

    AsxEncoderStatus addCommand(const char* command, uint32_t duration)
    {
        const size_t commandLength = strlen(command);
        if (commandLength == 0 || commandLength >= sizeof(_pendingCommand))
            return AsxEncoderStatus::InvalidInput;
        if (sampleLimitReached() || !ensureStorage(calculateProspectiveLength(command, duration)))
        {
            _overflowed = true;
            return AsxEncoderStatus::BufferFull;
        }
        if (continuesPending(command, duration))
            ++_repeatCount;
        else
        {
            flushPending();
            memcpy(_pendingCommand, command, commandLength + 1);
            _pendingDuration = duration;
            _repeatCount = 1;
        }
        ++_sampleCount;
        return isFlushRequired() ? AsxEncoderStatus::FlushRequired : AsxEncoderStatus::Success;
    }

public:
    AsxNanoStream(unsigned int initialCapacity = 0, size_t maxPayloadCapacity = 0)
        : _maxPayloadCapacity(maxPayloadCapacity), _maxRepeatCount(9999U),
          _maxSampleCount(50000U)
    {
        reset();
        if (initialCapacity > 0) reserve(initialCapacity);
    }

    void setMaxPayloadCapacity(size_t maximum) { _maxPayloadCapacity = maximum; }
    size_t getMaxPayloadCapacity() const { return _maxPayloadCapacity; }
    void setMaxRepeatCount(uint32_t maximum) { _maxRepeatCount = maximum; }
    uint32_t getMaxRepeatCount() const { return _maxRepeatCount; }
    void setMaxSampleCount(uint32_t maximum) { _maxSampleCount = maximum; }
    uint32_t getMaxSampleCount() const { return _maxSampleCount; }
    uint32_t getSampleCount() const { return _sampleCount; }
    bool hasOverflowed() const { return _overflowed; }

    void reserve(unsigned int bytes)
    {
        if (bytes == UINT_MAX || !_payload.reserve(bytes)) _overflowed = true;
    }

    bool isFlushRequired() const
    {
        if (_overflowed || sampleLimitReached()) return true;
        // Division before multiplication prevents wraparound on 16-bit targets.
        const size_t threshold = (_maxPayloadCapacity / 10) * 9 +
            ((_maxPayloadCapacity % 10) * 9) / 10;
        return _maxPayloadCapacity > 0 && length() >= threshold;
    }

    bool canAccept(const String& command, uint32_t durationMs = 0) const
    {
        // An empty String can signal allocation failure in a caller-built command.
        return command.length() > 0 && command.length() < UINT_MAX &&
            !sampleLimitReached() && fitsCapacity(calculateProspectiveLength(command.c_str(), durationMs));
    }

    void reset()
    {
        _payload = "";
        _pendingCommand[0] = '\0';
        _pendingDuration = 0;
        _repeatCount = 0;
        _lastAnalogValue = 0;
        _sampleCount = 0;
        _overflowed = false;
    }

    bool setBaseline(int32_t value)
    {
        char token[14];
        char* output = token;
        *output++ = 'B';
        if (value < 0) *output++ = '-';
        output = writeUnsigned(output, value < 0 ? static_cast<uint64_t>(-static_cast<int64_t>(value)) : value);
        *output++ = '#';
        *output = '\0';
        if (!ensureStorage(static_cast<uint64_t>(length()) + strlen(token))) return false;
        flushPending();
        _payload += token;
        _lastAnalogValue = value;
        return true;
    }

    AsxEncoderStatus addBinaryWithStatus(bool state, uint32_t durationMs = 0)
    {
        return addCommand(state ? "1b" : "0b", durationMs);
    }

    bool addBinary(bool state, uint32_t durationMs = 0)
    {
        const AsxEncoderStatus status = addBinaryWithStatus(state, durationMs);
        return status == AsxEncoderStatus::Success || status == AsxEncoderStatus::FlushRequired;
    }

    AsxEncoderStatus addAnalogWithStatus(int32_t value, uint32_t durationMs = 0)
    {
        const int64_t delta = static_cast<int64_t>(value) - _lastAnalogValue;
        char command[13];
        command[0] = delta < 0 ? '-' : '+';
        char* end = writeUnsigned(command + 1, static_cast<uint64_t>(delta < 0 ? -delta : delta));
        *end++ = 'v';
        *end = '\0';
        const AsxEncoderStatus status = addCommand(command, durationMs);
        if (status == AsxEncoderStatus::Success || status == AsxEncoderStatus::FlushRequired)
            _lastAnalogValue = value;
        return status;
    }

    bool addAnalog(int32_t value, uint32_t durationMs = 0)
    {
        const AsxEncoderStatus status = addAnalogWithStatus(value, durationMs);
        return status == AsxEncoderStatus::Success || status == AsxEncoderStatus::FlushRequired;
    }

    String getPayload()
    {
        // Existing accepted samples remain retrievable after lowering the configured cap.
        flushPending();
        String result(_payload);
        if (result.length() != _payload.length()) _overflowed = true;
        return result;
    }

    static int digitCount(uint64_t value)
    {
        int digits = 1;
        while (value >= 10) { ++digits; value /= 10; }
        return digits;
    }

    // size_t preserves exact lengths beyond INT_MAX on AVR.
    size_t length() const
    {
        return static_cast<size_t>(static_cast<uint64_t>(_payload.length()) +
            encodedCost(strlen(_pendingCommand), _pendingDuration, _repeatCount));
    }
};
#endif
