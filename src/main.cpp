#include <Arduino.h>
#include <FS.h>
#include <SPI.h>
#include <LittleFS.h>
#include <esp_task_wdt.h>
#include "includes.h"

#define SS_PIN  4
#define RST_PIN 15
#define SPK_PIN 38   // GPIO27 does not exist on ESP32-S3; use GPIO38

SPIClass rfidSPI(FSPI);
MFRC522 mfrc522(SS_PIN, RST_PIN, rfidSPI);
MFRC522::MIFARE_Key key;
MFRC522::MIFARE_Key ekey;
AES aes;

String spoolData = "AB1240276A210100100000FF016500000100000000000000";
bool encrypted = false;

// Forward declarations
void createKey();
void loadConfig();

void setup()
{
  Serial.begin(115200);
  // Wait up to 3 s for USB-CDC host to connect; continue anyway
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 3000) { delay(10); }
  Serial.println();
  Serial.println("[K2] === K2 RFID Writer booting ===");
  Serial.println("[K2] setup start");

  LittleFS.begin(true);
  Serial.println("[K2] LittleFS OK");
  loadConfig();
  Serial.println("[K2] config loaded");

  // Disable task watchdog during hardware init – some peripherals can be slow
  esp_task_wdt_deinit();

  // Start dedicated SPI bus for MFRC522 (FSPI/SPI2: SCK=5, MISO=7, MOSI=6)
  // TFT_eSPI uses USE_HSPI_PORT internally – no manual SPI.begin() needed for display
  rfidSPI.begin(5, 7, 6, -1);
  Serial.println("[K2] RFID SPI OK");

  // Initialise display (TFT_eSPI manages its own HSPI bus via USE_HSPI_PORT)
  Serial.println("[K2] display init...");
  displayInit();
  Serial.println("[K2] display OK");

  Serial.println("[K2] MFRC522 init...");
  mfrc522.PCD_Init();
  // Print firmware version to confirm communication
  byte v = mfrc522.PCD_ReadRegister(MFRC522::VersionReg);
  Serial.printf("[K2] MFRC522 version: 0x%02X %s\n", v,
                (v == 0x91 || v == 0x92) ? "(OK)" : "(not detected – check wiring)");
  key = {255, 255, 255, 255, 255, 255};
  pinMode(SPK_PIN, OUTPUT);
  Serial.println("[K2] === boot complete ===");
}


static unsigned long _lastRfidDbg = 0;

void loop()
{
  displayLoop();

  // Periodic MFRC522 health dump every 5 s
  if (millis() - _lastRfidDbg > 5000)
  {
    _lastRfidDbg = millis();
    byte v = mfrc522.PCD_ReadRegister(MFRC522::VersionReg);
    Serial.printf("[RFID] version=0x%02X IRQ=%d\n", v, mfrc522.PCD_ReadRegister(MFRC522::ComIrqReg));
  }

  if (!mfrc522.PICC_IsNewCardPresent())
    return;
  Serial.println("[RFID] card present");

  if (!mfrc522.PICC_ReadCardSerial())
  {
    Serial.println("[RFID] ReadCardSerial FAILED");
    return;
  }

  // Print UID
  Serial.print("[RFID] UID:");
  for (byte i = 0; i < mfrc522.uid.size; i++)
    Serial.printf(" %02X", mfrc522.uid.uidByte[i]);
  Serial.println();

  encrypted = false;

  MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
  Serial.printf("[RFID] PICC type: %s\n", MFRC522::PICC_GetTypeName(piccType) ? (const char*)MFRC522::PICC_GetTypeName(piccType) : "?");
  if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI && piccType != MFRC522::PICC_TYPE_MIFARE_1K && piccType != MFRC522::PICC_TYPE_MIFARE_4K)
  {
    Serial.println("[RFID] unsupported card type – skipping");
    tone(SPK_PIN, 400, 400);
    delay(2000);
    return;
  }

  createKey();

  MFRC522::StatusCode status;
  status = (MFRC522::StatusCode)mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, 7, &key, &(mfrc522.uid));
  Serial.printf("[RFID] auth key A: %s\n", MFRC522::GetStatusCodeName(status));
  if (status != MFRC522::STATUS_OK)
  {
    if (!mfrc522.PICC_IsNewCardPresent())
      return;
    if (!mfrc522.PICC_ReadCardSerial())
      return;
    status = (MFRC522::StatusCode)mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, 7, &ekey, &(mfrc522.uid));
    Serial.printf("[RFID] auth ekey: %s\n", MFRC522::GetStatusCodeName(status));
    if (status != MFRC522::STATUS_OK)
    {
      displaySetStatus(STATUS_ERROR);
      tone(SPK_PIN, 400, 150);
      delay(300);
      tone(SPK_PIN, 400, 150);
      delay(2000);
      return;
    }
    encrypted = true;
  }

  displaySetStatus(STATUS_WRITING);

  // ── Read blocks 4-6 from card and print to serial ─────────────────────
  String readBack = "";
  for (int blk = 4; blk <= 6; blk++)
  {
    byte buf[18]; byte len = sizeof(buf);
    if (mfrc522.MIFARE_Read(blk, buf, &len) == MFRC522::STATUS_OK)
    {
      byte dec[16];
      aes.encrypt(0, buf, dec);  // decrypt (mode 0)
      for (int j = 0; j < 16; j++) readBack += (char)dec[j];
    }
  }
  if (readBack.length() > 0)
  {
    Serial.println("[RFID] card data: " + readBack);
    displayUpdateSpool(readBack);
  }

  byte blockData[17];
  byte encData[16];
  int blockID = 4;
  for (int i = 0; i < spoolData.length(); i += 16)
  {
    spoolData.substring(i, i + 16).getBytes(blockData, 17);
    if (blockID >= 4 && blockID < 7)
    {
      aes.encrypt(1, blockData, encData);
      mfrc522.MIFARE_Write(blockID, encData, 16);
    }
    blockID++;
  }

  if (!encrypted)
  {
    byte buffer[18];
    byte byteCount = sizeof(buffer);
    byte block = 7;
    status = mfrc522.MIFARE_Read(block, buffer, &byteCount);
    int y = 0;
    for (int i = 10; i < 16; i++)
    {
      buffer[i] = ekey.keyByte[y];
      y++;
    }
    for (int i = 0; i < 6; i++)
    {
      buffer[i] = ekey.keyByte[i];
    }
    mfrc522.MIFARE_Write(7, buffer, 16);
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  Serial.println("[RFID] write done – SUCCESS");
  displaySetStatus(STATUS_SUCCESS);
  tone(SPK_PIN, 1000, 200);
  delay(2000);
}

void createKey()
{
  int x = 0;
  byte uid[16];
  byte bufOut[16];
  for (int i = 0; i < 16; i++)
  {
    if (x >= 4)
      x = 0;
    uid[i] = mfrc522.uid.uidByte[x];
    x++;
  }
  aes.encrypt(0, uid, bufOut);
  for (int i = 0; i < 6; i++)
  {
    ekey.keyByte[i] = bufOut[i];
  }
}

void loadConfig()
{
  if (LittleFS.exists("/spool.ini"))
  {
    File file = LittleFS.open("/spool.ini", "r");
    if (file)
    {
      String iniData;
      while (file.available()) iniData += (char)file.read();
      file.close();
      spoolData = iniData;
    }
  }
  else
  {
    File file = LittleFS.open("/spool.ini", "w");
    if (file) { file.print(spoolData); file.close(); }
  }
}