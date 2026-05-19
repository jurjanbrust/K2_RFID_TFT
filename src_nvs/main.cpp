#include <Arduino.h>
#include <Preferences.h>

void setup() {
    Serial.begin(115200);
    delay(500);

    Preferences p;
    p.begin("spotify", false);
    p.putString("client_id",     "e088f8b7f0ad488e952a0d461a904016");
    p.putString("client_secret", "23840a6ae0764ea787f7fec0298ab80f");
    p.putString("refresh_token", "AQDO0tCiK7wZgR3raS71_FYNZW3l_Py4C1htuNjEOf9piZoiyJbD69fkpA25kDvaypBQfk3SaQeU3M90qJ0dtF4RUx-On1NhibjbPy0Dta7YoomssMrET4Gl0eqnwCA4CBk");
    p.end();

    Serial.println("Spotify credentials opgeslagen in NVS.");
}

void loop() {}
