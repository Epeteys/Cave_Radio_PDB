// I2C_Programming.ino
// Arduino sketch to program a CAT24C256 EEPROM over I2C

// Tested with ESP32 Dev kit (ESP32-WROOM-32E)
// Works with a modified export from the Ti TPS25751 GUI,
// Export a full flash binary as .c and modify to match 
// example files (see QDX_PDB_2026-05-03_R2.h)

#include <Wire.h>
// #include <QDX_PDB_2026-04-30_V1.h>
// #include <QDX_PDB_2026-05-01_R1.h>
#include <QDX_PDB_2026-05-03_R2.h>

// CAT24C256 I2C address (A2=A1=A0=GND → 0x50)
// Confirm against your schematic's A0/A1/A2 connections
#define EEPROM_ADDR 0x50

// Page size for CAT24C256
#define PAGE_SIZE   64

// Maximum write cycle time (ms)
#define WRITE_CYCLE_MS 5

// ---------------------------------------------------------------
// Paste your TI-exported binary payload here as a byte array.
// Example shows a placeholder — replace with your actual .bin data.
// ---------------------------------------------------------------
// const uint8_t eepromData[] PROGMEM = {
//   // TODO: paste exported bytes from TPS25751 GUI here
//   // e.g., 0x4C, 0x49, 0x4E, 0x45, ...
// };

const uint16_t dataLen = sizeof(eepromData);

// Write a single page (up to PAGE_SIZE bytes) to the EEPROM
bool writePage(uint16_t address, const uint8_t* data, uint8_t len) {
  Wire.beginTransmission(EEPROM_ADDR);
  Wire.write((uint8_t)(address >> 8));    // High address byte
  Wire.write((uint8_t)(address & 0xFF));  // Low address byte
  for (uint8_t i = 0; i < len; i++) {
    Wire.write(pgm_read_byte(&data[i]));
  }
  return Wire.endTransmission() == 0;
}

// Wait for EEPROM write cycle to complete (ACK polling)
void waitForWrite() {
  uint32_t start = millis();
  while (millis() - start < 50) {
    Wire.beginTransmission(EEPROM_ADDR);
    if (Wire.endTransmission() == 0) return;
    delay(1);
  }
  Serial.println(F("WARNING: EEPROM write timeout"));
}

void programEEPROM() {
  Serial.print(F("Programming "));
  Serial.print(dataLen);
  Serial.println(F(" bytes..."));

  uint16_t written = 0;
  while (written < dataLen) {
    // Clamp to page boundary
    uint16_t pageOffset = written % PAGE_SIZE;
    uint8_t chunk = min((uint16_t)(PAGE_SIZE - pageOffset),
                        (uint16_t)(dataLen - written));

    if (!writePage(written, &eepromData[written], chunk)) {
      Serial.print(F("Write failed at address 0x"));
      Serial.println(written, HEX);
      return;
    }

    waitForWrite();
    written += chunk;

    // Progress indicator every 512 bytes
    if (written % 512 == 0 || written == dataLen) {
      Serial.print(F("  Written: "));
      Serial.print(written);
      Serial.print(F(" / "));
      Serial.println(dataLen);
    }
  }

  Serial.println(F("Done."));
}

void verifyEEPROM() {
  Serial.println(F("Verifying..."));
  uint16_t errors = 0;

  for (uint16_t addr = 0; addr < dataLen; addr += 32) {
    uint8_t chunk = min((uint16_t)32, (uint16_t)(dataLen - addr));

    Wire.beginTransmission(EEPROM_ADDR);
    Wire.write((uint8_t)(addr >> 8));
    Wire.write((uint8_t)(addr & 0xFF));
    Wire.endTransmission(false);  // Repeated start

    Wire.requestFrom((uint8_t)EEPROM_ADDR, chunk);
    for (uint8_t i = 0; i < chunk; i++) {
      uint8_t expected = pgm_read_byte(&eepromData[addr + i]);
      uint8_t actual   = Wire.read();
      if (actual != expected) {
        Serial.print(F("Mismatch @ 0x"));
        Serial.print(addr + i, HEX);
        Serial.print(F(": expected 0x"));
        Serial.print(expected, HEX);
        Serial.print(F(" got 0x"));
        Serial.println(actual, HEX);
        errors++;
        if (errors > 10) { Serial.println(F("Too many errors, aborting.")); return; }
      }
    }
  }

  if (errors == 0) Serial.println(F("Verify OK."));
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(100000);  // 100 kHz — safe for long wires

  delay(500);
  Serial.println(F("TPS25751 EEPROM Programmer"));

  programEEPROM();
  verifyEEPROM();
}

void loop() {}