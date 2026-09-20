/*
 * ASXNanoStream Protocol - Decoder Library (Edge/Arduino)
 * Version: 1.0.4
 * License: MIT
 * Author: Adriano Xavier
 *
 * Use this library to decode DOWNLINK commands sent from the ASX Cloud.
 * It converts compressed streams (e.g., "KLED:1#") into C++ Callbacks.
 */

#ifndef ASX_NANO_STREAM_DECODER_H
#define ASX_NANO_STREAM_DECODER_H

#include <Arduino.h>
#include <limits.h>

// Definição do tipo de função de Callback
// O usuário define uma função que recebe (Chave, Valor)
typedef void (*AsxCommandCallback)(String key, int value);

class AsxNanoStreamDecoder {
private:
    AsxCommandCallback _callback;

    static bool parseInteger(const String& payload, size_t begin, size_t end, int& value) {
        if (begin == end) return false;
        const bool negative = payload[begin] == '-';
        if (negative || payload[begin] == '+') ++begin;
        if (begin == end) return false;

        // Use the target's int range (16 bits on AVR, often 32 elsewhere).
        const unsigned int limit = negative
            ? static_cast<unsigned int>(INT_MAX) + 1U
            : static_cast<unsigned int>(INT_MAX);
        unsigned int magnitude = 0;
        for (; begin < end; ++begin) {
            const char c = payload[begin];
            if (c < '0' || c > '9') return false;
            const unsigned int digit = static_cast<unsigned int>(c - '0');
            if (magnitude > (limit - digit) / 10U) return false;
            magnitude = magnitude * 10U + digit;
        }
        value = negative
            ? (magnitude == limit ? INT_MIN : -static_cast<int>(magnitude))
            : static_cast<int>(magnitude);
        return true;
    }

public:
    // Construtor: Recebe a função que executará as ordens
    AsxNanoStreamDecoder(AsxCommandCallback callback) {
        _callback = callback;
    }

    // Processa o pacote recebido do Satélite/LoRa
    void parse(const String& payload) {
        size_t cursor = 0;
        const size_t len = payload.length();

        while (cursor < len) {
            // Detecta comando Chave-Valor (K key : value #)
            if (payload[cursor] == 'K') {
                size_t end = cursor + 1;
                size_t separator = len;
                bool validKey = true;
                while (end < len && payload[end] != '#') {
                    const unsigned char c = static_cast<unsigned char>(payload[end]);
                    if (separator == len) {
                        if (c == ':') separator = end;
                        else if (c < 33 || c > 126) validKey = false;
                    }
                    ++end;
                }
                // Never reinterpret bytes inside a malformed/truncated command
                // as a second actuator command, or search beyond its delimiter.
                if (end == len) return;
                int value = 0;
                if (validKey && separator != len && separator > cursor + 1 &&
                    parseInteger(payload, separator + 1, end, value) && _callback) {
                    String key = payload.substring(cursor + 1, separator);
                    // Arduino String may be empty after an allocation failure.
                    if (key.length() == separator - cursor - 1) _callback(key, value);
                }
                cursor = end + 1;
                continue;
            }

            // Convenção: Chave será "BIN"
            if (payload[cursor] >= '0' && payload[cursor] <= '9') {
                size_t end = cursor + 1;
                while (end < len && payload[end] >= '0' && payload[end] <= '9') ++end;
                if (end < len && payload[end] == 'b') {
                    if (end - cursor == 1 && (payload[cursor] == '0' || payload[cursor] == '1') &&
                        (cursor == 0 || (payload[cursor - 1] != '-' && payload[cursor - 1] != '+')) && _callback) {
                        String key("BIN");
                        if (key.length() == 3) _callback(key, payload[cursor] - '0');
                    }
                    ++end;
                }
                cursor = end;
                continue;
            }
            cursor++;
        }
    }
};

#endif
