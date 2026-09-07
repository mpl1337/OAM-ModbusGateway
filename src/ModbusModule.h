#pragma once
#include "ModbusChannel.h"
#include "OpenKNX.h"
#ifdef ARDUINO_ARCH_RP2040
#ifndef OPENKNX_USB_EXCHANGE_IGNORE
#include "UsbExchangeModule.h"
#endif
#endif

static constexpr uint8_t MODBUS_ERRORLOG_SIZE = 32;
static constexpr uint8_t MODBUS_ERRORLOG_OBJECT_INDEX = 161;
static constexpr uint8_t MODBUS_ERRORLOG_PROPERTY_ID = 5;

struct ModbusErrorLogEntry
{
    uint32_t uptimeSeconds = 0;
    uint16_t repeats = 0;
    uint16_t year = 0;
    uint8_t month = 0;
    uint8_t day = 0;
    uint8_t hour = 0;
    uint8_t minute = 0;
    uint8_t second = 0;
    uint8_t sequence = 0;
    uint8_t channel = 0;
    uint8_t slaveId = 0;
    uint8_t errorCode = 0;
    uint8_t functionCode = 0;
    uint8_t dpt = 0;
    uint16_t registerAddress = 0;
    uint8_t flags = 0;
};

class ModbusModule : public OpenKNX::Module, public ModbusMaster
{
private:
    bool _error[255] = {false};
    bool readyToSendModbus[255] = {0};
    uint8_t result_old[255] = {0x01};

    ModbusErrorLogEntry _errorLog[MODBUS_ERRORLOG_SIZE] = {};
    uint8_t _errorLogNext = 0;
    uint8_t _errorLogCount = 0;
    uint8_t _errorLogSequence = 0;
    uint16_t _errorEventCounter = 0;
    uint32_t _timer1 = 0;
    uint32_t _timer2 = 0;
    uint32_t _timerCycle = 0;

    uint32_t _timerCycleSendChannel = 0;
    uint8_t _currentChannel = 0;
    uint8_t _channel = 0;

    ModbusChannel *_channels[MOD_ChannelCount];
    OpenKNX::Flash::Driver *_modbusStorage = nullptr;
    static bool idle_processing;
    static uint32_t _timerCycleChannel;

    void setupCustomFlash();
    void setupChannels();
    int findNextActive(int size, int currentIndex);
    int findNextReady(int size, int currentIndex);
    uint8_t findNextReadyToSend(int size);
    void errorHandling();
    void ErrorHandlingLED();
    void handleChannelResult(uint8_t channelIndex, uint8_t result);
    void addErrorLogEntry(uint8_t channelIndex, uint8_t result, bool recovered);
    void updateErrorLogTimestamp(ModbusErrorLogEntry &entry);
    bool getErrorLogEntryByDisplayIndex(uint8_t displayIndex, ModbusErrorLogEntry &entry);
    void clearErrorLog();
    uint8_t activeErrorCount();
    void fillErrorLogSummary(uint8_t *resultData, uint8_t &resultLength);
    void fillErrorLogEntry(uint8_t displayIndex, uint8_t *resultData, uint8_t &resultLength);
    uint8_t activeRegisterCount();
    int16_t channelIndexByActiveRegister(uint8_t displayIndex);
    void fillRegisterTableSummary(uint8_t *resultData, uint8_t &resultLength);
    void fillRegisterTableEntry(uint8_t displayIndex, uint8_t *resultData, uint8_t &resultLength);
#ifdef ARDUINO_ARCH_RP2040
#ifndef OPENKNX_USB_EXCHANGE_IGNORE
    void registerUsbExchangeCallbacks();
#endif
#endif
    static void idleCallback();
    static void preTransmission();
    static void postTransmission();

public:
    ModbusModule();
    void loop(bool configured) override;
    void setup(bool configured) override;
#ifdef OPENKNX_DUALCORE
    void loop1(bool configured) override;
    void setup1(bool configured) override;
#endif
    const std::string name() override;
    const std::string version() override;
    void processInputKo(GroupObject &ko) override;
    bool processFunctionProperty(uint8_t objectIndex, uint8_t propertyId, uint8_t length, uint8_t *data, uint8_t *resultData, uint8_t &resultLength) override;
    bool processCommand(const std::string cmd, bool diagnoseKo);
    void showHelp() override;

    bool modbusInitSerial(HardwareSerial &serial);
    bool modbusParitySerial(uint32_t baud, HardwareSerial &serial);
};

extern ModbusModule openknxModbusModule;