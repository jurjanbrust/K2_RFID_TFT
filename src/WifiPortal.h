#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// WiFi Captive Portal
//
// Gebruik:
//   wifiPortalStart()  – start AP + DNS + HTTP server
//   wifiPortalLoop()   – aanroepen vanuit loop() zolang portal actief is
//   wifiPortalStop()   – sluit AP en servers
//   wifiPortalActive() – true als de portal draait
//
// AP naam: "K2-RFID-Setup"  (geen wachtwoord)
// Alle DNS verzoeken → 192.168.4.1
// GET  /          → HTML-formulier
// POST /save      → sla SSID + pass op in NVS, herstart ESP
// GET  /status    → JSON {connected, ip}
//
// De portal start automatisch als wifiPortalStart() wordt aangeroepen,
// en sluit zichzelf na een succesvolle opslag (ESP herstart).
// ---------------------------------------------------------------------------

void wifiPortalStart(const char* apName = "K2-RFID-Setup");
void wifiPortalLoop();
void wifiPortalStop();
bool wifiPortalActive();
