# Plan: K2_RFID migratie → RPi Zero 2W + 8.8" HDMI touch (1920×480)

**Aanbevolen aanpak:** hybride architectuur — RPi Zero 2W voor display/UI/RFID, kleine co-processor (ESP32-C3 mini) voor real-time IR PWM via USB-serieel.

---

## Fase 1 — Omgeving opzetten

1. Raspberry Pi OS Lite op RPi Zero 2W
2. Resolutie 1920×480 instellen in `/boot/config.txt`
3. Python pakketten: `pygame`, `spidev`, `mfrc522`, `RPi.GPIO` of `pigpio`, `requests`, `pycryptodome`, `pyserial`
4. SPI inschakelen via `raspi-config`

---

## Fase 2 — Display & UI *(grootste winst)*

1. Pygame venster 1920×480, fullscreen
2. Kleurschema overnemen uit `src/Display/display.h` (`CLR_HEADER_BG`, `CLR_BODY_BG`, etc.)
3. 6 pagina's herbouwen: RFID, Lamp, Audio, Airco, Macro's, Instellingen
4. Touch via USB HID = standaard pygame muisevents → swipe detectie zelfde logica als huidige `display.h`
5. Toast notifications, screensaver/sleep timer en NTP klok in header overnemen

---

## Fase 3 — RFID (MFRC522)

1. Aansluiten op RPi SPI0 (MOSI=GPIO10, MISO=GPIO9, SCK=GPIO11, SS=GPIO8, RST=GPIO25)
2. Python `mfrc522` of `pi-rc522` library
3. AES encryptie → `pycryptodome` (`AES.MODE_ECB`) vervangt `src/AES/AES.h`
4. Materiaaldb: `src/DB/matdb.h` is gzip JSON → laden met Python `gzip` + `json`

---

## Fase 4 — Rotary encoders

1. Encoders op vrije RPi GPIO-pinnen (geen input-only beperking zoals ESP32 GPIO34/36/39)
2. `pigpio` voor betrouwbare debouncing via hardware interrupts
3. Eigen encoder klasse die `delta` bijhoudt (vervangt `RotaryEncoder` + `OneButton`)

---

## Fase 5 — IR zenden *(co-processor)*

1. ESP32-C3 mini als dedicated IR slave via USB-serieel (`pyserial`)
2. Simpel tekstprotocol per regel:
   - `IR NEC 0x8E7629D`
   - `IR AC TEMP=21 FAN=0 MODE=0 ON`
3. ESP32-C3 firmware: minimale Arduino sketch met `IRremoteESP8266`, luistert op `Serial`
4. Alle NEC-codes uit `src/main.cpp` (`IR_ONOFF`, `IR_VOLUP`, etc.) en Mitsubishi Heavy AC 152-bit blijven zo cycle-exact correct
5. Alternatief (minder betrouwbaar): `pigpio` hardware PWM direct op RPi

---

## Fase 6 — WLED & WiFi

1. `requests` voor HTTP naar `192.168.10.24` / `192.168.10.232` — zelfde `/win&...` commands als `_wledGet()` in `src/main.cpp`
2. WiFi ingebouwd in RPi OS, geen extra configuratie nodig
3. OTA updates → SSH + `git pull` in plaats van ArduinoOTA

---

## Fase 7 — Persistente instellingen

1. JSON bestand vervangt `Preferences` (NVS namespace)
2. Touch kalibratie niet nodig (USB HID scherm)
3. Opslaan: sleep timer, WLED brightness, RFID veldkeuze, airco toestand

---

## Verificatie

1. Alle 6 pagina's bereikbaar via swipe
2. RFID tag lezen en schrijven met AES encryptie
3. IR NEC code sturen naar HiFi versterker
4. IR Mitsubishi Heavy AC 152-bit sturen — meest kritisch, valideer met oscilloscoop of IR analyzer
5. WLED scenes en brightness aansturen via WiFi
6. Encoder delta correct verwerkt (temperatuur, volume)
7. Screensaver activeert na 5 minuten inactiviteit

---

## Architectuurbeslissingen

| Onderwerp | Keuze | Reden |
|---|---|---|
| IR timing | Co-processor (ESP32-C3) | Linux niet real-time, jitter bij 152-bit AC codes |
| UI stack | Python + pygame | Laagste leercurve, snelste iteratie |
| RPi model | Zero **2W** verplicht | Zero 1 te traag voor pygame bij 1920×480 |
| Display modus | Pygame fullscreen | Geen X11/Wayland overhead |
| OTA | SSH + git | Vervangt ArduinoOTA |
| Encryptie | pycryptodome AES ECB | 1-op-1 vervanging van `src/AES/AES.h` |
