#include <Arduino.h>
#include "AsxNanoStream.h"

AsxNanoStream asx;

void setup() {
  Serial.begin(115200);
  
  // --- EXEMPLO 1: SENSOR DE TEMPERATURA ---
  Serial.println("--- Teste Sensor Temperatura ---");
  
  asx.reset();
  asx.setBaseline(250); // Começa em 25.0 graus (x10)
  
  // Simula leituras estáveis (Compressão RLE vai agir aqui)
  asx.addAnalog(250, 6000); // 25.0°C, espera 6s
  asx.addAnalog(250, 6000); // Igual (vai virar x2)
  asx.addAnalog(250, 6000); // Igual (vai virar x3)
  
  // Simula uma subida
  asx.addAnalog(251, 6000); // +1v
  asx.addAnalog(252, 6000); // +1v (deltas iguais, pode comprimir se a lógica permitir)
  
  String payload = asx.getPayload();
  Serial.print("Payload Gerado: ");
  Serial.println(payload);
  // Saída esperada: B250#x3-+0v6000ms+1v6000ms+1v6000ms
  
  
  // --- EXEMPLO 2: MATRIX LED (O "Caso dos 99%") ---
  Serial.println("\n--- Teste Matrix LED ---");
  asx.reset();
  
  // 512 pixels desligados
  for(int i=0; i<512; i++) {
    asx.addBinary(0); 
  }
  
  // 32 pixels ligados (Linha do meio)
  for(int i=0; i<32; i++) {
    asx.addBinary(1);
  }
  
  // 480 pixels desligados
  for(int i=0; i<480; i++) {
    asx.addBinary(0);
  }
  
  // Adiciona comando de fim de linha 'z' manualmente se quiser,
  // ou simplesmente envia o stream.
  
  String matrixPayload = asx.getPayload();
  Serial.print("Tamanho Original (JSON estimado): ~2000 bytes\n");
  Serial.print("Tamanho ASX: ");
  Serial.print(matrixPayload.length());
  Serial.println(" bytes");
  Serial.println("Payload: " + matrixPayload);
  // Saída esperada: x512-0bx32-1bx480-0b
}

void loop() {
}
