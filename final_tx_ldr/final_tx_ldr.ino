#include <SPI.h>
#include <LoRa.h>
#include <Crypto.h>
#include <AES.h>
#include <CTR.h>
#include <DHT.h>

#define NODE_ID 0x44
#define SLOT_DELAY 1500 // TDMA Slot 2
#define LDRPIN PA1


AES128 aes;
CTR<AES128> ctr;
uint16_t counter = 1;
byte myKey[16];
byte baseKey[16] = {0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x61,0x62,0x63,0x64,0x65,0x66};


void printHex(const char* label, const byte* data, int len) {
  Serial.print(label);
  for (int i = 0; i < len; i++) {
    if (data[i] < 0x10) Serial.print("0");
    Serial.print(data[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
}


void setup() {
  Serial.begin(9600);

  uint32_t* uid = (uint32_t*)0x1FFF7A10;
  for (int i = 0; i < 3; i++) {
  Serial.print(uid[i], HEX);
  Serial.print(" ");
}
  for(int i=0; i<16; i++) myKey[i] = baseKey[i] ^ ((uint8_t*)uid)[i % 12];
  
  LoRa.setPins(PB6, PA9, PC7);
  if (!LoRa.begin(433E6)) while(1);
  LoRa.setSpreadingFactor(10);
  Serial.println("🛡️ Node 44 Online...");
  Serial.print("Device UID: ");
// uint32_t* uid = (uint32_t*)0x1FFF7A10;

Serial.println();

printHex("Derived AES Key: ", myKey, 16);


}

void loop() {
  if (LoRa.parsePacket() > 0 && LoRa.read() == 0xAA) {
    delay(SLOT_DELAY);
    
int ldr = analogRead(LDRPIN);

    byte plain[16] = {0};
    plain[0] = NODE_ID;
    plain[1] = highByte(ldr); // Map temp to byte 3
    plain[2] = lowByte(ldr); // Map hum to byte 4
    

    byte iv[16] = {0}; iv[0] = counter >> 8; iv[1] = counter & 0xFF;
    byte cipher[16];
    ctr.setKey(myKey, 16);
    ctr.setIV(iv, 16);
    ctr.encrypt(cipher, plain, 16);

Serial.println("---- SENSOR DATA ----");
Serial.print("Node ID: 0x"); Serial.println(NODE_ID, HEX);
Serial.print("Temperature: "); Serial.print(t); Serial.println(" °C");
Serial.print("Humidity: "); Serial.print(h); Serial.println(" %");

printHex("Plaintext: ", plain, 16);

Serial.print("CTR Counter: ");
Serial.println(counter);

printHex("IV: ", iv, 16);

printHex("Ciphertext: ", cipher, 16);

    LoRa.beginPacket();
    LoRa.write(cipher, 16);
    LoRa.write(iv[0]); LoRa.write(iv[1]);
    LoRa.endPacket();
Serial.println("📡 Encrypted packet sent");

    unsigned long start = millis();
    bool ack = false;
    while(millis() - start < 1000) {
      if(LoRa.parsePacket() >= 2 && LoRa.read() == 0xAC && LoRa.read() == NODE_ID) {
        ack = true; break;
      }
    }
    if (ack) { Serial.println("✅ ACK Received"); counter++; } 
    else Serial.println("❌ NO ACK");
    
    LoRa.sleep();
  }
}