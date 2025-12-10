/*
 * ASXNanoStream Protocol - Decoder Library (Edge/Arduino)
 * Version: 1.0.0
 * License: MIT
 * Author: Adriano Severino
 * 
 * Use this library to decode DOWNLINK commands sent from the ASX Cloud.
 * It converts compressed streams (e.g., "KLED:1#") into C++ Callbacks.
 */

#ifndef ASX_NANO_STREAM_DECODER_H
#define ASX_NANO_STREAM_DECODER_H

#include <Arduino.h>

// Definição do tipo de função de Callback
// O usuário define uma função que recebe (Chave, Valor)
typedef void (*AsxCommandCallback)(String key, int value);

class AsxNanoStreamDecoder {
private:
    AsxCommandCallback _callback;

public:
    // Construtor: Recebe a função que executará as ordens
    AsxNanoStreamDecoder(AsxCommandCallback callback) {
        _callback = callback;
    }

    // Processa o pacote recebido do Satélite/LoRa
    void parse(String payload) {
        int cursor = 0;
        int len = payload.length();

        while (cursor < len) {
            // Detecta comando Chave-Valor (K key : value #)
            if (payload[cursor] == 'K') {
                int separator = payload.indexOf(':', cursor);
                int end = payload.indexOf('#', cursor);

                if (separator != -1 && end != -1) {
                    // Extrai a Chave
                    String key = payload.substring(cursor + 1, separator);
                    
                    // Extrai o Valor
                    String valStr = payload.substring(separator + 1, end);
                    int value = valStr.toInt();

                    // Executa a ação no hardware do cliente
                    if (_callback) {
                        _callback(key, value);
                    }

                    cursor = end + 1; // Avança para o próximo comando
                    continue;
                }
            }
            
            // Convenção: Chave será "BIN"
            if (isDigit(payload[cursor]) && payload[cursor+1] == 'b') {
                 int val = (payload[cursor] == '1') ? 1 : 0;
                 if (_callback) {
                     _callback("BIN", val);
                 }
                 cursor += 2;
                 continue;
            }
            cursor++;
        }
    }
};

#endif
