/*
 * ASXNanoStream Protocol - Encoder Library (C++)
 * Version: 1.0.0
 * License: MIT
 * Author: Adriano Severino
 */

#ifndef ASX_NANO_STREAM_H
#define ASX_NANO_STREAM_H

#include <Arduino.h>

class AsxNanoStream {
private:
    String _payload;
    
    // Estado para RLE (Compressão de repetição)
    String _pendingCommand;
    uint16_t _pendingDuration;
    int _repeatCount;
    
    // Estado para Delta Encoding (Analógico)
    int _lastAnalogValue;

    // Função interna para "despejar" o comando pendente no payload
    void flushPending() {
        if (_repeatCount == 0) return;

        // Se repetiu mais de 1 vez, adiciona o prefixo de loop "xN-"
        if (_repeatCount > 1) {
            _payload += "x";
            _payload += _repeatCount;
            _payload += "-";
        }

        // Adiciona o comando (ex: "1b" ou "+10v")
        _payload += _pendingCommand;

        // Adiciona o tempo se houver (ex: "500ms")
        if (_pendingDuration > 0) {
            _payload += _pendingDuration;
            _payload += "ms";
        }
        
        // Fecha o bloco de repetição se necessário
        if (_repeatCount > 1) {
            // No protocolo ASX simples, o hífen já conecta, 
            // mas se for um bloco complexo, usaria '&'. 
            // Para comandos simples, o prefixo basta. 
        }

        // Reseta o estado
        _repeatCount = 0;
        _pendingCommand = "";
        _pendingDuration = 0;
    }

public:
    AsxNanoStream() {
        reset();
    }

    // Limpa o buffer para começar uma nova mensagem
    void reset() {
        _payload = "";
        _pendingCommand = "";
        _pendingDuration = 0;
        _repeatCount = 0;
        _lastAnalogValue = 0; // Valor inicial padrão
    }

    // Define o valor base inicial (opcional, para ECG/Sensores)
    void setBaseline(int value) {
        _lastAnalogValue = value;
        _payload += "B";
        _payload += value;
        _payload += "#";
    }

    // Adiciona Estado Binário (Ligar/Desligar)
    // Ex: addBinary(true, 500) -> gera "1b500ms"
    void addBinary(bool state, uint16_t durationMs = 0) {
        String cmd = state ? "1b" : "0b";

        // Verifica se é igual ao anterior (para comprimir)
        if (cmd == _pendingCommand && durationMs == _pendingDuration) {
            _repeatCount++;
        } else {
            flushPending(); // Salva o anterior
            _pendingCommand = cmd;
            _pendingDuration = durationMs;
            _repeatCount = 1;
        }
    }

    // Adiciona Valor Analógico (Calcula Delta automaticamente)
    // Ex: addAnalog(25, 100) se o anterior era 20 -> gera "+5v100ms"
    void addAnalog(int value, uint16_t durationMs = 0) {
        int delta = value - _lastAnalogValue;
        _lastAnalogValue = value; // Atualiza a referência

        String cmd = "";
        if (delta >= 0) cmd += "+";
        cmd += delta;
        cmd += "v";

        // Verifica compressão
        if (cmd == _pendingCommand && durationMs == _pendingDuration) {
            _repeatCount++;
        } else {
            flushPending();
            _pendingCommand = cmd;
            _pendingDuration = durationMs;
            _repeatCount = 1;
        }
    }

    // Finaliza e retorna a string pronta para envio
    String getPayload() {
        flushPending(); // Garante que o último comando seja gravado
        return _payload;
    }
    
    // Retorna o tamanho atual em bytes
    int length() {
        return _payload.length(); // Nota: é aproximado se houver pending
    }
};

#endif
