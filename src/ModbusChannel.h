#pragma once

#include <cstddef>
#include "OpenKNX.h"
#include "ModBusMaster.h"

class ModbusChannel : public OpenKNX::Channel, public ModbusMaster
{
private:
  uint32_t sendDelay;
  uint32_t timer1sec;

  uint8_t _modbus_ID;
  uint16_t _registerAddr;
  uint8_t _baud_value;
  uint8_t _parity_value;
  uint8_t _readCyclecounter = 0;
  HardwareSerial &_serial;

  bool errorState[2];
  bool valueValid;
  bool _readyToSend;
  uint8_t _skipCounter;

  // Last raw value successfully transferred on Modbus. The ETS online table
  // uses this cache and therefore never triggers an additional Modbus request.
  bool _rawModbusValueValid = false;
  bool _rawModbusIsBit = false;
  bool _rawModbusBitValue = false;
  uint8_t _rawModbusWordCount = 0;
  uint16_t _rawModbusWords[4] = {0, 0, 0, 0};

  typedef union Values
  {
    uint8_t lValueUint8_t;
    uint16_t lValueUint16_t;
    int16_t lValueInt16_t;
    uint32_t lValueUint32_t;
    int lValueUint;
    int32_t lValueInt32_t;
    float lValue;
    bool lValueBool;
  } values_t;
  values_t lastSentValue;

  bool modbusParitySerial(uint32_t baud, HardwareSerial &serial);
  bool modbusInitSerial(HardwareSerial &serial);
  void sendKNX();
  void storeRawModbusBit(bool value);
  void storeRawModbusWords(const uint16_t *words, uint8_t count);
  void captureRawReadValue(uint8_t dpt);

public:
  ModbusChannel(uint8_t index, uint8_t baud_value, uint8_t parity_value, HardwareSerial &serial);
  bool isActiveCH();
  bool readDone();
  bool isReadyCH();
  uint8_t getModbusID();
  uint8_t getSlaveSelection();
  uint8_t getDpt();
  uint8_t getReadFunction();
  uint8_t getWriteFunction();
  uint8_t getActiveFunction();
  uint16_t getRegisterAddress();
  uint16_t getConfiguredRegisterAddress();
  bool getCurrentValueText(char *buffer, size_t bufferSize);
  bool getRawModbusValueText(char *buffer, size_t bufferSize);
  bool getDirection();
  inline uint16_t adjustRegisterAddress(uint16_t u16ReadAddress, uint8_t RegisterStart);
  uint8_t readModbus(bool readyToSend);
  bool sendModbus();
  uint8_t modbusToKnx(uint8_t dpt, bool readRequest);
  uint8_t knxToModbus();
  void printDebugResult(const char *dpt, uint16_t registerAddr, uint8_t result);
  uint8_t sendProtocol(uint16_t registerAddr, uint16_t u16value);
  const std::string name() override;
  void setup() override;
  void loop(bool readyToSend) override;
};