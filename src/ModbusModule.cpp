#include "ModbusModule.h"
#include "HardwareConfig.h"
#include "ModBusMaster.h"
#include "LED_Statusanzeige.h"
#include "Device.h"
#include <cstring>


// uint32_t timer_time_between_Reg_Reads;
// uint32_t timer_time_between_Cycle_Reads;

bool run_cycle = true;
bool readyToSend = false; 

bool ModbusModule::idle_processing = false;
unsigned long ModbusModule::_timerCycleChannel = 0;

static void modbusPutWord(uint8_t *data, uint8_t &pos, uint16_t value)
{
    data[pos++] = (value >> 8) & 0xFF;
    data[pos++] = value & 0xFF;
}

static void modbusPutDWord(uint8_t *data, uint8_t &pos, uint32_t value)
{
    data[pos++] = (value >> 24) & 0xFF;
    data[pos++] = (value >> 16) & 0xFF;
    data[pos++] = (value >> 8) & 0xFF;
    data[pos++] = value & 0xFF;
}

ModbusModule::ModbusModule()
{
    idle(ModbusModule::idleCallback);
}

const std::string ModbusModule::name()
{
    return "modbus";
}

const std::string ModbusModule::version()
{
    // hides the module in the version output on the console, because the firmware version is sufficient.
    return "";
}

void ModbusModule::setup(bool configured)
{
    // delay(1000);
    logDebugP("Setup0");
    logIndentUp();

    // setup Pins
#if defined(DEVICE_SMARTMF_1TE_MODBUS) || defined(DEVICE_SMARTMF_MODBUS_AUSSEN)
    pinMode(SMARTMF_LED, OUTPUT);
    digitalWrite(SMARTMF_LED, LOW);
#endif
#ifdef DEVICE_SMARTMF_MODBUS_RTU_3BE
    Wire.setSDA(12);
    Wire.setSCL(13);
    Wire.begin();
    Wire.setClock(400000);
    initHW(get_HW_ID());
#endif
    pinMode(SMARTMF_MODBUS_DIR_PIN, OUTPUT);
    RS485_SERIAL.setRX(SMARTMF_MODBUS_RX_PIN);
    RS485_SERIAL.setTX(SMARTMF_MODBUS_TX_PIN);

    modbusInitSerial(RS485_SERIAL);

    if (configured)
    {
        // setupCustomFlash();                               // ********************************* anpassen wenn notwendig *********************
        setupChannels();
    }
    else
    {
#ifdef DEVICE_SMARTMF_MODBUS_RTU_3BE
        setLED_Modbus(false);
#endif
    }
    logIndentDown();
}

void ModbusModule::setupChannels()
{

    for (uint8_t i = 0; i < ParamMOD_VisibleChannels; i++)
    {
        _channels[i] = new ModbusChannel(i, 3, 3, RS485_SERIAL);
        _channels[i]->setup();
        _channels[i]->idle(idleCallback);
        _channels[i]->preTransmission(preTransmission);
        _channels[i]->postTransmission(postTransmission);
    }
#ifdef DEVICE_SMARTMF_MODBUS_RTU_3BE
    if (ParamMOD_VisibleChannels != 0)
        setLED_Modbus(true);
    else
        setLED_Modbus(false);
#endif
}

void ModbusModule::setupCustomFlash()
{
    logDebugP("initialize modbus flash");
    OpenKNX::Flash::Driver _modbusStorage;
#ifdef ARDUINO_ARCH_ESP32
    _modbusStorage.init("modbus");
#else
    _modbusStorage.init("modbus", modbus_FLASH_OFFSET, modbus_FLASH_SIZE);
#endif

    logTraceP("write modbus data");
    // _modbusStorage.writeByte(0, 0x11);
    // _modbusStorage.writeWord(1, 0xFFFF);
    // _modbusStorage.writeInt(3, 6666666);
    // for (size_t i = 0; i < 4095; i++)
    // {
    //     _modbusStorage.writeByte(i, i);
    // }
    // _modbusStorage.commit();

    logDebugP("read modbus data");
    logIndentUp();
    // logHexDebugP(_modbusStorage.flashAddress(), 4095);
    // logDebugP("byte: %02X", _modbusStorage.readByte(0)); // UINT8
    // logDebugP("word: %i", _modbusStorage.readWord(1));   // UINT16
    // logDebugP("int: %i", _modbusStorage.readInt(3));     // UINT32

    logIndentDown();
}

void ModbusModule::idleCallback()
{
    idle_processing = true;
    openknx.loop();
    idle_processing = false;
    _timerCycleChannel = millis();
}

void ModbusModule::preTransmission()
{
    digitalWrite(SMARTMF_MODBUS_DIR_PIN, 1);
}

void ModbusModule::postTransmission()
{
    digitalWrite(SMARTMF_MODBUS_DIR_PIN, 0);
}

uint8_t ModbusModule::findNextReadyToSend(int size)
{
    for (int i = 1; i <= size; i++) // i=1, damit wir die 0 als Rückgabewert haben, für kein CH is ready
    {
        if (readyToSendModbus[i])
        {
            readyToSendModbus[i] = false;
            return i;
        }
    }
    return 0;
}

int ModbusModule::findNextActive(int size, int currentIndex)
{
    for (int i = 1; i <= size; i++) // i=1, damit wir nicht wieder currentIndex selbst nehmen
    {
        int idx = (currentIndex + i) % size;
        if (_channels[idx]->isActiveCH())
        {
            return idx; // hier haben wir den nächsten activen CH gefunden
        }
    }
    return 0;
}

int ModbusModule::findNextReady(int size, int currentIndex)
{
    for (int i = currentIndex + 1; i < size; i++)
    {
        if (_channels[i]->isReadyCH())
            return i;
    }
    return size;
}

void ModbusModule::errorHandling()
{
    ErrorHandlingLED();
}

void ModbusModule::ErrorHandlingLED()
{
    bool error = false;
    for (int i = 0; i < ParamMOD_VisibleChannels; i++)
    {
        if (_error[i])
        {
            error = true;
        }
    }
    if (error)
    {
        // setLED_ERROR(HIGH);
#if defined(DEVICE_SMARTMF_1TE_MODBUS) || defined(DEVICE_SMARTMF_MODBUS_AUSSEN)
        digitalWrite(SMARTMF_LED, HIGH);
#endif
#ifdef DEVICE_SMARTMF_MODBUS_RTU_3BE
        setLED_ERROR(true);
#endif
    }
    else
    {
        // setLED_ERROR(LOW);
#if defined(DEVICE_SMARTMF_1TE_MODBUS) || defined(DEVICE_SMARTMF_MODBUS_AUSSEN)
        digitalWrite(SMARTMF_LED, LOW);
#endif
#ifdef DEVICE_SMARTMF_MODBUS_RTU_3BE
        setLED_ERROR(false);
#endif
    }
}

uint8_t ModbusModule::activeErrorCount()
{
    uint8_t count = 0;
    uint8_t visible = ParamMOD_VisibleChannels;
    if (visible > MOD_ChannelCount)
        visible = MOD_ChannelCount;

    for (uint8_t i = 0; i < visible; i++)
    {
        if (_error[i])
            count++;
    }
    return count;
}

void ModbusModule::clearErrorLog()
{
    memset(_errorLog, 0, sizeof(_errorLog));
    _errorLogNext = 0;
    _errorLogCount = 0;
    _errorLogSequence = 0;
    _errorEventCounter = 0;
}

bool ModbusModule::getErrorLogEntryByDisplayIndex(uint8_t displayIndex, ModbusErrorLogEntry &entry)
{
    if (displayIndex >= _errorLogCount)
        return false;

    int16_t physicalIndex = (int16_t)_errorLogNext - 1 - displayIndex;
    while (physicalIndex < 0)
        physicalIndex += MODBUS_ERRORLOG_SIZE;

    entry = _errorLog[physicalIndex % MODBUS_ERRORLOG_SIZE];
    return true;
}

void ModbusModule::updateErrorLogTimestamp(ModbusErrorLogEntry &entry)
{
    entry.uptimeSeconds = millis() / 1000;
    entry.year = 0;
    entry.month = 0;
    entry.day = 0;
    entry.hour = 0;
    entry.minute = 0;
    entry.second = 0;

    // Wenn die OpenKNX-Zeit bereits über KNX gesetzt wurde, wird sie zusätzlich
    // im Fehlerlog gespeichert. Ohne gültige KNX-Zeit bleibt nur die Laufzeit.
    if (openknx.time.isValid())
    {
        OpenKNX::DateTime now = openknx.time.getLocalTime();
        entry.year = now.year;
        entry.month = now.month;
        entry.day = now.day;
        entry.hour = now.hour;
        entry.minute = now.minute;
        entry.second = now.second;
    }
}

void ModbusModule::addErrorLogEntry(uint8_t channelIndex, uint8_t result, bool recovered)
{
    if (channelIndex >= ParamMOD_VisibleChannels || _channels[channelIndex] == nullptr)
        return;

    const uint8_t flags = recovered ? 0x01 : 0x00;
    const uint8_t channel = channelIndex + 1;

    // Nicht nur den physisch letzten Logeintrag prüfen. Bei mehreren dauerhaft
    // fehlerhaften Kanälen wechseln sich die Meldungen ab. Deshalb wird der
    // jeweils letzte Zustand dieses Kanals gesucht. Ist er unverändert, wird
    // lediglich der Wiederholungszähler und der Zeitstempel aktualisiert.
    for (uint8_t displayIndex = 0; displayIndex < _errorLogCount; displayIndex++)
    {
        int16_t physicalIndex = (int16_t)_errorLogNext - 1 - displayIndex;
        while (physicalIndex < 0)
            physicalIndex += MODBUS_ERRORLOG_SIZE;

        ModbusErrorLogEntry &lastForChannel = _errorLog[physicalIndex % MODBUS_ERRORLOG_SIZE];
        if (lastForChannel.channel != channel)
            continue;

        if (lastForChannel.errorCode == result &&
            lastForChannel.flags == flags)
        {
            if (lastForChannel.repeats < 0xFFFF)
                lastForChannel.repeats++;
            updateErrorLogTimestamp(lastForChannel);
            return;
        }

        // Der letzte Eintrag dieses Kanals beschreibt einen anderen Zustand.
        // Ältere Einträge desselben Kanals dürfen daher nicht zusammengefasst werden.
        break;
    }

    ModbusErrorLogEntry &entry = _errorLog[_errorLogNext];
    memset(&entry, 0, sizeof(entry));
    updateErrorLogTimestamp(entry);
    entry.repeats = 1;
    entry.sequence = ++_errorLogSequence;
    entry.channel = channel;
    entry.slaveId = _channels[channelIndex]->getModbusID();
    entry.errorCode = result;
    entry.functionCode = _channels[channelIndex]->getActiveFunction();
    entry.dpt = _channels[channelIndex]->getDpt();
    entry.registerAddress = _channels[channelIndex]->getRegisterAddress();
    entry.flags = flags;

    _errorLogNext = (_errorLogNext + 1) % MODBUS_ERRORLOG_SIZE;
    if (_errorLogCount < MODBUS_ERRORLOG_SIZE)
        _errorLogCount++;

    if (!recovered && _errorEventCounter < 0xFFFF)
        _errorEventCounter++;
}

void ModbusModule::handleChannelResult(uint8_t channelIndex, uint8_t result)
{
    if (channelIndex >= ParamMOD_VisibleChannels || _channels[channelIndex] == nullptr)
        return;

    if (_channels[channelIndex]->getDirection() != 1)
        return;

    if (result == result_old[channelIndex])
    {
        if (result != ku8MBSuccess)
            addErrorLogEntry(channelIndex, result, false);
        return;
    }

    if (result == ku8MBSuccess)
    {
        logInfoP("CH%i: run again", channelIndex + 1);
        if (_error[channelIndex])
            addErrorLogEntry(channelIndex, result, true);
        _error[channelIndex] = false;
    }
    else
    {
        logInfoP("CH%i: ERROR: %02X", channelIndex + 1, result);
        _error[channelIndex] = true;
        addErrorLogEntry(channelIndex, result, false);
    }

    uint16_t diag_register = ((uint16_t)result << 8) | (channelIndex + 1);
    KoMOD_DebugModbus.value(diag_register, DPT_Value_2_Ucount);
    result_old[channelIndex] = result;
}

void ModbusModule::fillErrorLogSummary(uint8_t *resultData, uint8_t &resultLength)
{
    uint8_t pos = 0;
    resultData[pos++] = 0;
    resultData[pos++] = _errorLogCount;
    resultData[pos++] = activeErrorCount();
    modbusPutWord(resultData, pos, _errorEventCounter);
    resultData[pos++] = ParamMOD_VisibleChannels;
    modbusPutDWord(resultData, pos, millis() / 1000);
    resultLength = pos;
}

void ModbusModule::fillErrorLogEntry(uint8_t displayIndex, uint8_t *resultData, uint8_t &resultLength)
{
    ModbusErrorLogEntry entry;
    if (!getErrorLogEntryByDisplayIndex(displayIndex, entry))
    {
        resultData[0] = 2;
        resultData[1] = 0;
        resultLength = 2;
        return;
    }

    uint8_t pos = 0;
    resultData[pos++] = 0;
    resultData[pos++] = entry.sequence;
    resultData[pos++] = entry.channel;
    resultData[pos++] = entry.slaveId;
    resultData[pos++] = entry.errorCode;
    resultData[pos++] = entry.functionCode;
    resultData[pos++] = entry.dpt;
    modbusPutWord(resultData, pos, entry.registerAddress);
    modbusPutWord(resultData, pos, entry.repeats);
    modbusPutDWord(resultData, pos, entry.uptimeSeconds);
    resultData[pos++] = entry.flags;
    modbusPutWord(resultData, pos, entry.year);
    resultData[pos++] = entry.month;
    resultData[pos++] = entry.day;
    resultData[pos++] = entry.hour;
    resultData[pos++] = entry.minute;
    resultData[pos++] = entry.second;
    resultLength = pos;
}

uint8_t ModbusModule::activeRegisterCount()
{
    uint8_t count = 0;
    for (uint16_t i = 0; i < ParamMOD_VisibleChannels; i++)
    {
        if (_channels[i] != nullptr && _channels[i]->isActiveCH())
            count++;
    }
    return count;
}

int16_t ModbusModule::channelIndexByActiveRegister(uint8_t displayIndex)
{
    uint8_t activeIndex = 0;
    for (uint16_t i = 0; i < ParamMOD_VisibleChannels; i++)
    {
        if (_channels[i] == nullptr || !_channels[i]->isActiveCH())
            continue;
        if (activeIndex == displayIndex)
            return (int16_t)i;
        activeIndex++;
    }
    return -1;
}

void ModbusModule::fillRegisterTableSummary(uint8_t *resultData, uint8_t &resultLength)
{
    uint8_t pos = 0;
    resultData[pos++] = 0;
    resultData[pos++] = activeRegisterCount();
    resultData[pos++] = ParamMOD_VisibleChannels;
    modbusPutDWord(resultData, pos, millis() / 1000);
    resultLength = pos;
}

void ModbusModule::fillRegisterTableEntry(uint8_t displayIndex, uint8_t *resultData, uint8_t &resultLength)
{
    int16_t channelIndex = channelIndexByActiveRegister(displayIndex);
    if (channelIndex < 0 || _channels[channelIndex] == nullptr)
    {
        resultData[0] = 2;
        resultData[1] = 0;
        resultLength = 2;
        return;
    }

    ModbusChannel *channel = _channels[channelIndex];
    char rawModbusText[80];
    char knxValueText[80];
    bool rawAvailable = channel->getRawModbusValueText(rawModbusText, sizeof(rawModbusText));
    bool knxAvailable = channel->getCurrentValueText(knxValueText, sizeof(knxValueText));

    uint8_t pos = 0;
    resultData[pos++] = 0;
    resultData[pos++] = (uint8_t)(channelIndex + 1);
    resultData[pos++] = channel->getSlaveSelection();
    resultData[pos++] = channel->getModbusID();
    resultData[pos++] = channel->getActiveFunction();
    resultData[pos++] = channel->getDpt();
    resultData[pos++] = channel->getDirection() ? 1 : 0;
    modbusPutWord(resultData, pos, channel->getConfiguredRegisterAddress());

    uint8_t flags = 0;
    if (rawAvailable)
        flags |= 0x01;
    if (channel->getDirection() == 1 && _error[channelIndex])
        flags |= 0x02;
    if (knxAvailable)
        flags |= 0x04;
    resultData[pos++] = flags;
    resultData[pos++] = (channel->getDirection() == 1) ? result_old[channelIndex] : 0xFF;

    size_t rawLength = rawAvailable ? strlen(rawModbusText) : 0;
    size_t knxLength = knxAvailable ? strlen(knxValueText) : 0;
    if (rawLength > sizeof(rawModbusText) - 1)
        rawLength = sizeof(rawModbusText) - 1;
    if (knxLength > sizeof(knxValueText) - 1)
        knxLength = sizeof(knxValueText) - 1;

    // Two lengths are sent before the two strings. Keep the complete response
    // below the KNX function-property payload limit.
    size_t maxTextBytes = 240 - (pos + 2);
    if (rawLength + knxLength > maxTextBytes)
    {
        if (rawLength > maxTextBytes)
        {
            rawLength = maxTextBytes;
            knxLength = 0;
        }
        else
        {
            knxLength = maxTextBytes - rawLength;
        }
    }

    resultData[pos++] = (uint8_t)rawLength;
    resultData[pos++] = (uint8_t)knxLength;
    if (rawLength > 0)
    {
        memcpy(resultData + pos, rawModbusText, rawLength);
        pos += (uint8_t)rawLength;
    }
    if (knxLength > 0)
    {
        memcpy(resultData + pos, knxValueText, knxLength);
        pos += (uint8_t)knxLength;
    }
    resultLength = pos;
}

void ModbusModule::loop(bool configured)
{

    // if (delayCheck(_timer1, 1000))
    //{
    //     logDebugP("LoopModule");
    //     _timer1 = millis();
    //     logDebugP("CH%i:", _channel);
    // }

    if (configured)
    {
        if (ParamMOD_VisibleChannels == 0)
            return;

        uint8_t processed = 0;
        uint8_t count = 0;
        uint8_t result;
        do
        {
            errorHandling();

            _channels[_currentChannel]->loop(readyToSend); // loop -> only for KNX send send cyclically
            _currentChannel = findNextActive(ParamMOD_VisibleChannels, _currentChannel);

            // if (!idle_processing)
            //{
            //     uint8_t ch = findNextReadyToSend(ParamMOD_VisibleChannels, _currentChannel);
            //
            //    if (ch != 0)
            //    {
            //        if (delayCheck(_timerCycleSendChannel, 10))
            //        {
            //            _channels[ch - 1]->knxToModbus();
            //            _timerCycleSendChannel;
            //        }
            //    }
            //}

            if (!idle_processing)
            {

                if (delayCheck(_timerCycleChannel, (ParamMOD_BusDelayRequest * 10)+5)) // Zeit zwischen zwei Modbus Register Abfragen +5ms default Wartezeit
                {
                    // prüft ob ein CH eine Modbus Botschaft senden will und gibt diese CH-nummer zurück
                    uint8_t ch = findNextReadyToSend(ParamMOD_VisibleChannels);
                    if (ch != 0) // KNX to MODBUS Abfrage
                    {
                        _channels[ch - 1]->knxToModbus();
                    }
                    else if (run_cycle) // MODBUS to KNX Abfrage
                    {
                        result = _channels[_channel]->readModbus(true); // read cyclically the Modbus-Channels
                        handleChannelResult(_channel, result);

                        // Sucht nächsten aktiven und wartenden Channel
                        _channel = findNextReady(ParamMOD_VisibleChannels, _channel);

                        // setzt _channel counter wieder zurück
                        if (_channel >= ParamMOD_VisibleChannels)
                        {
                            _channel = 0;
                            run_cycle = false;
                            _timerCycle = millis();
                        }
                    }
                    _timerCycleChannel = millis();
                }
            }

            // Wartet xsek bis der nächste komplette Abfragezyklus gestartet wird
            if (delayCheck(_timerCycle, (ParamMOD_BusDelayCycle * 1000)) && !run_cycle)
            {
                run_cycle = true;
                readyToSend = true; // Nachdem alle CH durchgelaufen sind, darf auf den Bus gesendet werden
            }
        } while (openknx.freeLoopIterate(ParamMOD_VisibleChannels, count, processed));
    }
}

#ifdef OPENKNX_DUALCORE

void ModbusModule::setup1(bool configured)
{
    delay(1000);
    // logDebugP("Setup1");
}

void ModbusModule::loop1(bool configured)
{
    if (delayCheck(_timer2, 7200))
    {
        // logDebugP("Loop1");
        _timer2 = millis();
    }
}
#endif

void ModbusModule::processInputKo(GroupObject &ko)
{
    logDebugP("processInputKo GA%04X", ko.asap());
    logHexDebugP(ko.valueRef(), ko.valueSize());

    // #define MOD_KoCalcNumber(index) (index + MOD_KoBlockOffset + _channelIndex * MOD_KoBlockSize)
    // #define MOD_KoCalcIndex(number) ((number >= MOD_KoCalcNumber(0) && number < MOD_KoCalcNumber(MOD_KoBlockSize)) ? (number - MOD_KoBlockOffset) % MOD_KoBlockSize : -1)
    // #define MOD_KoCalcChannel(number) ((number >= MOD_KoBlockOffset && number < MOD_KoBlockOffset + MOD_ChannelCount * MOD_KoBlockSize) ? (number - MOD_KoBlockOffset) / MOD_KoBlockSize : -1)

    // Compute modbus channel number
    // int channel = (iKo.asap() - MOD_KoOffset - MOD_KoGO_BASE_) / MOD_KoBlockSize;
    int channel = MOD_KoCalcChannel(ko.asap());
    if (channel >= 0 && channel < MOD_ChannelCount && _channels[channel]->getDirection() == 0)
    {
        logDebugP("->KO: %i", channel + 1);
        readyToSendModbus[channel + 1] = true;
        //_channels[_currentChannel]->knxToModbus();
    }
}

void ModbusModule::showHelp()
{
    openknx.console.printHelpLine("modbus", "Print the Modbus error log");
    openknx.console.printHelpLine("modbus log", "Print the Modbus error log");
    openknx.console.printHelpLine("modbus log clear", "Clear the Modbus error log");
}

bool ModbusModule::processFunctionProperty(uint8_t objectIndex, uint8_t propertyId, uint8_t length, uint8_t *data, uint8_t *resultData, uint8_t &resultLength)
{
    if (objectIndex != MODBUS_ERRORLOG_OBJECT_INDEX || propertyId != MODBUS_ERRORLOG_PROPERTY_ID)
        return false;

    openknx.common.skipLooptimeWarning();

    if (length < 1)
    {
        resultData[0] = 1;
        resultData[1] = 0;
        resultLength = 2;
        return true;
    }

    switch (data[0])
    {
    case 1: // summary
        fillErrorLogSummary(resultData, resultLength);
        return true;

    case 2: // entry by display index, 0 = newest
        if (length < 2)
        {
            resultData[0] = 1;
            resultData[1] = 0;
            resultLength = 2;
            return true;
        }
        fillErrorLogEntry(data[1], resultData, resultLength);
        return true;

    case 3: // clear log
        clearErrorLog();
        resultData[0] = 0;
        resultData[1] = 0;
        resultLength = 2;
        return true;

    case 4: // configured Modbus register table summary
        fillRegisterTableSummary(resultData, resultLength);
        return true;

    case 5: // configured Modbus register table entry
        if (length < 2)
        {
            resultData[0] = 1;
            resultData[1] = 0;
            resultLength = 2;
            return true;
        }
        fillRegisterTableEntry(data[1], resultData, resultLength);
        return true;

    default:
        resultData[0] = 1;
        resultData[1] = 0;
        resultLength = 2;
        return true;
    }
}

bool ModbusModule::processCommand(const std::string cmd, bool diagnoseKo)
{
    if (cmd == "modbus" || cmd == "modbus log")
    {
        logInfoP("Modbus Fehlerlog: Eintraege=%u, aktive Fehler=%u, Ereignisse=%u", _errorLogCount, activeErrorCount(), _errorEventCounter);
        logIndentUp();
        for (uint8_t i = 0; i < _errorLogCount; i++)
        {
            ModbusErrorLogEntry entry;
            if (getErrorLogEntryByDisplayIndex(i, entry))
            {
                logInfoP("#%u CH%u Slave%u Err=0x%02X Fn=0x%02X DPT=%u Reg=%u Rep=%u t=%lus%s",
                         entry.sequence,
                         entry.channel,
                         entry.slaveId,
                         entry.errorCode,
                         entry.functionCode,
                         entry.dpt,
                         entry.registerAddress,
                         entry.repeats,
                         entry.uptimeSeconds,
                         (entry.flags & 0x01) ? " RECOVERY" : "");
            }
        }
        logIndentDown();
        return true;
    }

    if (cmd == "modbus log clear")
    {
        clearErrorLog();
        logInfoP("Modbus Fehlerlog geloescht");
        return true;
    }

    return false;
}

#ifdef ARDUINO_ARCH_RP2040
#ifndef OPENKNX_USB_EXCHANGE_IGNORE
void ModbusModule::registerUsbExchangeCallbacks()
{
    // Sample
    openknxUsbExchangeModule.onLoad("modbus.txt", [](UsbExchangeFile *file) -> void
                                    { file->write("Demo"); });
    openknxUsbExchangeModule.onEject("modbus.txt", [](UsbExchangeFile *file) -> bool
                                     {
        // File is required
        if (file == nullptr)
        {
            logError("ModbusModule", "File modbus.txt was deleted but is mandatory");
            return false;
        }
        return true; });
}
#endif
#endif

bool ModbusModule::modbusParitySerial(uint32_t baud, HardwareSerial &serial)
{
    switch (ParamMOD_BusParitySelection_Slave1)
    {
    case 0: // Even (1 stop bit)
        serial.begin(baud, SERIAL_8E1);
        logInfoP("Parity: Even (1 stop bit)");
        return true;
        break;
    case 1: // Odd (1 stop bit)
        serial.begin(baud, SERIAL_8O1);
        logInfoP("Parity: Odd (1 stop bit)");
        return true;
        break;
    case 2: // None (2 stop bits)
        serial.begin(baud, SERIAL_8N2);
        logInfoP("Parity: None (2 stop bits)");
        return true;
        break;
    case 3: // None (1 stop bit)
        serial.begin(baud, SERIAL_8N1);
        logInfoP("Parity: None (1 stop bit)");
        return true;
        break;

    default:
        logInfoP("Parity: Error: %i", ParamMOD_BusParitySelection_Slave1);
        return false;
        break;
    }
}

bool ModbusModule::modbusInitSerial(HardwareSerial &serial)
{
    // Set Modbus communication baudrate
    switch (ParamMOD_BusBaudrateSelection_Slave1)
    {
    case 0:
        logInfoP("Baudrate: 1200kBit/s");
        return modbusParitySerial(1200, serial);

        break;
    case 1:
        logInfoP("Baudrate: 2400kBit/s");
        return modbusParitySerial(2400, serial);
        break;
    case 2:
        logInfoP("Baudrate: 4800kBit/s");
        return modbusParitySerial(4800, serial);
        break;
    case 3:
        logInfoP("Baudrate: 9600kBit/s");
        return modbusParitySerial(9600, serial);
        break;
    case 4:
        logInfoP("Baudrate: 19200kBit/s");
        return modbusParitySerial(19200, serial);
        break;
    case 5:
        logInfoP("Baudrate: 38400kBit/s");
        return modbusParitySerial(38400, serial);
        break;
    case 6:
        logInfoP("Baudrate: 56000kBit/s");
        return modbusParitySerial(56000, serial);
        break;
    case 7:
        logInfoP("Baudrate: 115200kBit/s");
        return modbusParitySerial(115200, serial);
        break;
    default:
        logInfoP("Baudrate: Error: %i", ParamMOD_BusBaudrateSelection_Slave1);
        return false;
        break;
    }
}

ModbusModule openknxModbusModule;