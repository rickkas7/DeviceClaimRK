#include "DeviceClaimRK.h"

DeviceClaim::DeviceClaim() {

}

DeviceClaim::~DeviceClaim() {

}

void DeviceClaim::setup() {
    bool reinitData = true;

    if (readConfigData(&configData,sizeof(configData))) {
        if (configData.magic == CONFIG_MAGIC) {
            reinitData = false;
        }
    }
    
    if (reinitData) {
        Log.info("reinitializing eeprom");
        memset(&configData, 0, sizeof(configData));
        configData.magic = CONFIG_MAGIC;
        writeConfigData(&configData, sizeof(configData));
    }

    Log.info("flags=%04x", configData.flags);

    if (isClaimed()) {
        // Already claimed, jump do DONE state as there's nothing to do
        state = State::DONE;
    }
}
 

void DeviceClaim::loop() {
    switch(state) {
        case State::START:
            if (Particle.connected()) {
                state = State::CONNECT_WAIT;
                stateTime = millis();
            }
            break;

        case State::CONNECT_WAIT:
            if ((millis() - stateTime) >= connectWaitMs) {
                // Post the claiming event
                // Log.info("publishing %s", claimEventName.c_str());
                Particle.publish(claimEventName, "", PRIVATE);

                state = State::REQUEST_WAIT;
                stateTime = millis();
            }
            break;

        case State::REQUEST_WAIT:
            if ((millis() - stateTime) >= claimRequestWaitMs) {
                // As of 2020-01-18 there is a bug where the cloud does not notice the device has been
                // claimed and private events will not flow until reconnected. For now, force a 
                // disconnect so private events will work. 
                // Once this is fixed, we can skip this and just check that it worked.
                // Log.info("disconnected from the cloud");
                Particle.publish("spark/device/session/end", "", PRIVATE);
                state = State::DISCONNECT_WAIT;
                stateTime = millis();
            }
            break;

        case State::DISCONNECT_WAIT:
            if (!Particle.connected()) {
                // Log.trace("disconnected");
                state = State::RECONNECT_WAIT;
                stateTime = millis();
            }
            break;

        case State::RECONNECT_WAIT: 
            if (Particle.connected()) {
                // Log.info("reconnected");
                state = State::CHECK_CLAIM;
                stateTime = millis();
            }
            break;

         case State::CHECK_CLAIM:
            // Log.info("CHECK_CLAIM");
            {
                char eventName[64];
                snprintf(eventName, sizeof(eventName), "%s/hook-response/%s", System.deviceID().c_str(), testEventName.c_str());
                Particle.subscribe(eventName, &DeviceClaim::subscriptionHandler, this, MY_DEVICES);
            }
            Particle.publish(testEventName, PRIVATE);

            state = State::CHECK_WAIT;
            stateTime = millis();
            break;

        case State::CHECK_WAIT:
            if ((millis() - stateTime) >= testWaitMs) {
                Log.info("claiming failed");
                state = State::FAILURE;
            }
            break;

        case State::FAILURE:
            break;

        case State::DONE:
            break;
    }

}

void DeviceClaim::forceClaim() {
    setFlag(CONFIG_FLAG_CLAIMED, false);
    writeConfigData(&configData, sizeof(configData));

    if (state == State::DONE || state == State::FAILURE) {
        state = State::START;
    }
}

DeviceClaim::Progress DeviceClaim::getProgress() const {
    switch(state) {
        case State::DONE:
            return Progress::DONE;
        
        case State::FAILURE:
            return Progress::FAILURE;

        default:
            return Progress::IN_PROGRESS;
    }
}

bool DeviceClaim::readConfigData(DeviceClaimConfigData *data, size_t /* dataSize */) {
    EEPROM.get(eepromAddr, *data);
    return true;
}

bool DeviceClaim::writeConfigData(const DeviceClaimConfigData *data, size_t /* dataSize */) {
    EEPROM.put(eepromAddr, *data);
    return true;
}

void DeviceClaim::setFlag(uint16_t mask, bool value) {
    if (value) {
        configData.flags |= mask;
    }
    else {
        configData.flags &= ~mask;
    }
}

bool DeviceClaim::getFlag(uint16_t mask) const {
    return (configData.flags & mask) != 0;
}

void DeviceClaim::subscriptionHandler(const char *eventName, const char *data) {
    // This is used to handle the response from the device name request.
    // If you want it, it's in data. The only reason we do this is so we know
    // that claiming worked.
    size_t len = strlen(eventName);
    if (len >= 2 && eventName[len - 2] == '/' && eventName[len - 1] == '0') {

        // Log.info("got response eventName=%s", eventName);
        // Log.print(data);

        setFlag(CONFIG_FLAG_CLAIMED, true);
        writeConfigData(&configData, sizeof(configData));

        Log.info("successfully claimed");
        state = State::DONE;
    }
}
