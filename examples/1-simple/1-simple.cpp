#include "DeviceClaimRK.h"

PRODUCT_ID(8761);
PRODUCT_VERSION(1);

SYSTEM_THREAD(ENABLED);

SerialLogHandler logHandler;

DeviceClaim deviceClaim;

void setup() {
    // Wait for a USB serial connection for up to 15 seconds
    waitFor(Serial.isConnected, 15000);

    deviceClaim.withEepromAddr(1990);
    deviceClaim.setup();
}

void loop() {
    deviceClaim.loop();
}

