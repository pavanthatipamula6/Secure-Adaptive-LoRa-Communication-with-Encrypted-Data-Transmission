#include <SPI.h>
#include <LoRa.h>
#include <Crypto.h>
#include <AES.h>
#include <CTR.h>
#include <Wire.h>
#include <U8g2lib.h>


U8G2_SH1106_128X64_NONAME_F_HW_I2C
u8g2(U8G2_R0, U8X8_PIN_NONE);


AES128 aes;
CTR<AES128> ctr;


byte baseKey[16] = {
  0x30,0x31,0x32,0x33,
  0x34,0x35,0x36,0x37,
  0x38,0x39,0x61,0x62,
  0x63,0x64,0x65,0x66
};

uint32_t node44_UID[3] = {0x00240044, 0x34375118, 0x32383735};
uint32_t node45_UID[3] = {0x004D004F, 0x33375104, 0x36303437};

uint16_t lastCounters[256] = {0};


int valLDR  = 0;
int valTemp = 0;
int valHum  = 0;


int rssi44 = 0;
int rssi45 = 0;


void getDerivedKey(uint32_t* nodeUID, byte* outputKey) {
  for (int i = 0; i < 16; i++) {
    outputKey[i] = baseKey[i] ^ ((uint8_t*)nodeUID)[i % 12];
  }
}

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


  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.drawStr(0, 15, "SECURE GATEWAY");
  u8g2.drawStr(0, 35, "Waiting Nodes...");
  u8g2.sendBuffer();


  LoRa.setPins(PB6, PA9, PC7);
  if (!LoRa.begin(433E6)) {
    Serial.println(" LoRa Init Failed");
    while (1);
  }
  LoRa.setSpreadingFactor(10);

  Serial.println(" SECURE MASTER STARTED");
  Serial.println("Slot 0x44: 1–4s | Slot 0x45: 4.5–7.5s");
}

void loop() {

  Serial.println("\n--- TX BEACON ---");
  LoRa.beginPacket();
  LoRa.write(0xAA);
  LoRa.endPacket();

  unsigned long start = millis();

  while (millis() - start < 8000) {

    int packetSize = LoRa.parsePacket();
    if (packetSize >= 18) {

      unsigned long rxTime = millis() - start;
      uint32_t* activeUID = NULL;
      byte expectedID = 0;


      if (rxTime >= 1000 && rxTime <= 4000) {
        activeUID = node44_UID;
        expectedID = 0x44;
      }
      else if (rxTime >= 4500 && rxTime <= 7500) {
        activeUID = node45_UID;
        expectedID = 0x45;
      }

      Serial.print("[Time ");
      Serial.print(rxTime);
      Serial.print(" ms] ");

      if (activeUID == NULL) {
        Serial.println(" Slot Violation");
        continue;
      }


      byte cipher[16];
      for (int i = 0; i < 16; i++) {
        cipher[i] = LoRa.read();
      }

      uint16_t pCtr = (LoRa.read() << 8) | LoRa.read();


      byte key[16];
      getDerivedKey(activeUID, key);

      byte iv[16] = {0};
      iv[0] = pCtr >> 8;
      iv[1] = pCtr & 0xFF;

      byte plain[16];
      ctr.setKey(key, 16);
      ctr.setIV(iv, 16);
      ctr.decrypt(plain, cipher, 16);


      if (plain[0] != expectedID || pCtr <= lastCounters[expectedID]) {
        Serial.println("Crypto / Replay Error");
        continue;
      }

      lastCounters[expectedID] = pCtr;


      int rssi = LoRa.packetRssi();

      if (expectedID == 0x44) {
        valLDR = (plain[1] << 8) | plain[2];
        rssi44 = rssi;
      } else {
        valTemp = plain[3];
        valHum  = plain[4];
        rssi45  = rssi;
      }


      Serial.println("------------------------------------------------");
      Serial.print("AUTH NODE : 0x");
      Serial.println(expectedID, HEX);

      Serial.print("Counter     : ");
      Serial.println(pCtr);
printHex("IV: ", iv, 16);

printHex("Ciphertext: ", cipher, 16);
printHex("Plaintext: ", plain, 16);
      if (expectedID == 0x44) {
        Serial.print("LDR Value   : ");
        Serial.println(valLDR);
        Serial.print("RSSI Node44 : ");
        Serial.print(rssi44);
        Serial.println(" dBm");
      } else {
        Serial.print("Temperature : ");
        Serial.print(valTemp);
        Serial.println(" C");
        Serial.print("Humidity    : ");
        Serial.print(valHum);
        Serial.println(" %");
        Serial.print("RSSI Node45 : ");
        Serial.print(rssi45);
        Serial.println(" dBm");
      }
      Serial.println("------------------------------------------------");


      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_6x12_tr);


      u8g2.drawStr(0, 10, "SECURE GATEWAY");
      u8g2.drawHLine(0, 13, 128);


      u8g2.drawVLine(64, 14, 50);


      u8g2.setCursor(2, 28);
      u8g2.print("LDR (N44)");

      u8g2.setCursor(2, 42);
      u8g2.print("Val : ");
      u8g2.print(valLDR);

      u8g2.setCursor(2, 56);
      u8g2.print("RSSI: ");
      u8g2.print(rssi44);
      u8g2.print("dB");

      u8g2.setCursor(68, 28);
      u8g2.print("DHT (N45)");

      u8g2.setCursor(68, 40);
      u8g2.print("T : ");
      u8g2.print(valTemp);
      u8g2.print("C");

      u8g2.setCursor(68, 52);
      u8g2.print("H : ");
      u8g2.print(valHum);
      u8g2.print("%");

      u8g2.setCursor(68, 64);
      u8g2.print("RSSI:");
      u8g2.print(rssi45);
      u8g2.print("dB");

      u8g2.sendBuffer();

 
 
      delay(50);
      LoRa.beginPacket();
      LoRa.write(0xAC);
      LoRa.write(expectedID);
      LoRa.endPacket();
    }
  }
}