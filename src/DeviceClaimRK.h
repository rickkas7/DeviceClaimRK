#ifndef __DEVICECLAIMRK_H
#define __DEVICECLAIMRK_H

#include "Particle.h"

/**
 * @brief Structure stored in EEPROM to hold claimed flag
 */
typedef struct {
    uint32_t magic;         //!< Set to CONFIG_MAGIC (0x750ca339)
    uint16_t flags;         //!< Flags such as CONFIG_FLAG_CLAIMED.
    uint16_t reserved;      //!< Currently 0 
} DeviceClaimConfigData; 

/**
 * @brief Class for device claim library
 * 
 * You typically allocate one of these as a global variable
 */
class DeviceClaim {
public:
    /**
     * @brief Constructor. Typically this object is created as a global object.
     */
    DeviceClaim();

    /**
     * @brief Destructor. Typically this object is created as a global object and thus never deleted.
     */
    virtual ~DeviceClaim();

    /**
     * @brief You must call this method from setup()
     */
    void setup();

    /**
     * @brief You must call this method from loop();
     */
    void loop();

    /**
     * @brief Force claiming again
     * 
     * If the device is already claimed it will succeed, however it uses 3K to 6K of data, so you probably
     * don't want to do this unnecessarily. This will also retry after failure.
     */
    void forceClaim();

    /**
     * @brief Sets the address in virtual EEPROM where data is stored. The default is 1998.
     * 
     * @param eepromAddr The address to save the data at. There must be 8 bytes available at this address.
     * 
     * On Gen 2 devices (Electron and E Series), there are 2047 bytes of EEPROM. 
     * On Gen 3 devices (Boron), there are 4096 bytes of EEPROM.
     * 
     * You can use any available addresses that you are not already using in your application.
     * 
     * It's also possible to subclass this class to store the data elsewhere, such as in your own
     * EEPROM, Flash, FRAM, etc.. See readConfigData.
     * 
     * The function getEepromDataSize() return the value of 8.
     */
    DeviceClaim &withEepromAddr(size_t eepromAddr) { this->eepromAddr = eepromAddr; return *this; };

    /**
     * @brief Sets the event name to use to trigger claiming
     * 
     * This must match the webhook that you set up to handle claiming. The default is "deviceClaim".
     */
    DeviceClaim &withClaimEventName(const char *claimEventName) { this->claimEventName = claimEventName; return *this; };

    /**
     * @brief Sets the test name to use to test whether claiming worked
     * 
     * This must match the webhook that you set up to handle claiming. The default is "deviceTest".
     * Maxmium string length is 24 ASCII characters.
     */
    DeviceClaim &withTestEventName(const char *testEventName) { this->testEventName = testEventName; return *this; };


    /**
     * @brief Sets the amount of time after connecting to the cloud to send the claim event. Default: 2000 ms
     */
    DeviceClaim &withConnectWaitMs(unsigned long ms) { connectWaitMs = ms; return *this; };

    /**
     * @brief Sets the amount of time after sending the claim event before disconnecting. Default: 3000 ms
     */
    DeviceClaim &waitClaimRequestWaitMs(unsigned long ms) { claimRequestWaitMs = ms; return *this; };
 
    /**
     * @brief Amount of time to wait for a test event response before declaring failure. Default: 10000 ms.
     * 
     * This is a timeout value. Under normal circumstances the response is returned quicky indicating
     * success and the end of this time limit is never reached. 
     */
    DeviceClaim &withTestWaitMs(unsigned long ms) { testWaitMs = ms; return *this; };
 
    

    /**
     * @brief Result codes from getProgress()
     */
    enum class Progress {
        IN_PROGRESS,    //!< Claiming is in progress
        FAILURE,        //!< Claiming failed (the claiming event was sent, and the test event did not succeed)
        DONE            //!< The device was either previously claimed, or it was just succeessfully claimed
    };

    /**
     * @brief Gets the progress of the claiming
     * 
     * Returns one of the constants:
     * 
     * - `DeviceClaim::Progress::IN_PROGRESS` Claiming is in progress
     * - `DeviceClaim::Progress::FAILURE` Claiming failed (the claiming event was sent, and the test event did not succeed)
     * - `DeviceClaim::Progress::DONE` The device was either previously claimed, or it was just succeessfully claimed
     */
    Progress getProgress() const;

    /**
     * @brief Gets the size of the data stored at the specified EEPROM location
     */ 
    size_t getEepromDataSize() const { return sizeof(DeviceClaimConfigData); };
    

    /**
     * @brief Set a flag value in the configuration data
     * 
     * @param mask The mask bit to set. For example CONFIG_FLAG_CLAIMED. Mask should be a uint16_t with one bit set.
     * 
     * @param value The value to set (true to set, or false to clear)
     * 
     */
    void setFlag(uint16_t mask, bool value = true);

    /**
     * @brief Get a flag value in the configuration data
     * 
     * @param mask The mask bit to get. For example CONFIG_FLAG_CLAIMED. Mask should be a uint16_t with one bit set.
     * 
     * @return true if the flag is set (1) or false if the flag is clear (0),
     */
    bool getFlag(uint16_t mask) const;

    /**
     * @brief Returns true if CONFIG_FLAG_CLAIMED flag bit is set in the configuration data
     */
    bool isClaimed() const { return getFlag(CONFIG_FLAG_CLAIMED); };

    /**
     * @brief Reads the configuration data out of EEPROM at address eepromAddr
     * 
     * This method is virtual so you can subclass this class and store the configuration somewhere else,
     * for example your own EEPROM, Flash, FRAM, etc.. For custom storage methods you don't have to use
     * eepromAddr and you can use your own logic instead.
     */
    virtual bool readConfigData(DeviceClaimConfigData *data, size_t dataSize);

    /**
     * @brief Writes the configuration data to EEPROM at address eepromAddr
     * 
     * This method is virtual so you can subclass this class and store the configuration somewhere else,
     * for example your own EEPROM, Flash, FRAM, etc.. For custom storage methods you don't have to use
     * eepromAddr and you can use your own logic instead.
     */
    virtual bool writeConfigData(const DeviceClaimConfigData *data, size_t dataSize);


    static const uint32_t CONFIG_MAGIC = 0x750ca339;        //!< Magic value stored in the DeviceClaimConfigData in EEPROM
    static const uint16_t CONFIG_FLAG_CLAIMED   = 0x0001;   //!< Bit in to indicate that claiming has been done.


protected:
    /**
     * @brief Finite state machine states
     */
    enum class State { 
        START,              //!< Initial state
        CONNECT_WAIT,       //!< Waiting to connect to the Particle cloud, then sends claim event
        REQUEST_WAIT,       //!< Waiting after sending the claim event, then sends cloud disconnect event
        DISCONNECT_WAIT,    //!< Waiting for the cloud to disconnect
        RECONNECT_WAIT,     //!< Waiting for the cloud to connect again after disconnecting
        CHECK_CLAIM,        //!< Sends the test event
        CHECK_WAIT,         //!< Waits for the test event response. Moves into DONE state from subscriptionHandler on success.
        FAILURE,            //!< Claiming failed
        DONE                //!< Claiming completed (successfully)
    };

    /**
     * @brief Subscription handler for test event
     * 
     * Note: This function will transition state from CHECK_WAIT to DONE when called.
     */
    void subscriptionHandler(const char *eventName, const char *data);

    size_t eepromAddr = 1998;                   //!< Address to store the saved data. Must have 8 bytes available at this location see also withEepromAddr(), getEepromDataSize().
    String claimEventName = "deviceClaim";      //!< Name of the event used to claim a device. See also withClaimEventName().
    String testEventName = "deviceTest";        //!< Name of the event used to test claiming. See also withTestEventName().
    DeviceClaimConfigData configData;           //!< Data stored in the EEPROM (8 bytes)
    State state = State::START;                 //!< Current state of the finite state machine.
    unsigned long stateTime = 0;                //!< Timing information for some state transitions.
    unsigned long connectWaitMs = 2000;         //!< The amount of time after connecting to the cloud to send the claim event. See also withConnectWaitMs().
    unsigned long claimRequestWaitMs = 3000;    //!< Sets the amount of time after sending the claim event before disconnecting. See also withClaimRequestWaitMs().
    unsigned long testWaitMs = 10000;           //!< Amount of time to wait for a test event response before declaring failure. See also withTestWaitMs().
};

#endif /* __DEVICECLAIMRK_H */
