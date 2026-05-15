#pragma once

// ---------------------------------------------------------------------------
// OtaServer – HTTP firmware-upload via browser op http://<ip>/update
// ---------------------------------------------------------------------------
void otaServerStart();
void otaServerLoop();
void otaServerStop();
bool otaServerActive();
