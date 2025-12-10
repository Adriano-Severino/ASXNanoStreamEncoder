#include "AsxNanoStreamDecoderLight.h"

// Pinos
#define RELE_IRRIGACAO 5
#define LED_STATUS 13

void executarOrdem(String comando, int valor) {
    Serial.print("Comando Recebido: "); 
    Serial.print(comando);
    Serial.print(" -> Valor: ");
    Serial.println(valor);

    if (comando == "IRRIG") {
        if (valor == 1) {
            digitalWrite(RELE_IRRIGACAO, HIGH);
            Serial.println(">> Irrigacao LIGADA");
        } else {
            digitalWrite(RELE_IRRIGACAO, LOW);
            Serial.println(">> Irrigacao DESLIGADA");
        }
    }
    
    if (comando == "LED") {
        digitalWrite(LED_STATUS, valor);
    }
}

AsxNanoStreamDecoder decoder(executarOrdem);

void setup() {
    Serial.begin(115200);
    pinMode(RELE_IRRIGACAO, OUTPUT);
    pinMode(LED_STATUS, OUTPUT);
}

void loop() {
    // Simula recebimento de dados do Satélite/LoRa
    // Na prática viria de: lora.readString()
    if (Serial.available()) {
        String msgSatelite = Serial.readStringUntil('\n');
        decoder.parse(msgSatelite);
    }
}
