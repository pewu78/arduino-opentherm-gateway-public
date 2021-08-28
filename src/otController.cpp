#include <Homie.h>
#include <ArduinoLog.h>
#include "gateway.h"
#include "otController.h"

#define PHASE_LISTEN_MASTER 1
#define PHASE_REQUEST_SLAVE 2
#define PHASE_RESPOND_MASTER 3

#define MODE_GATEWAY 0
#define MODE_MASTER 1

OT_CONTROLLER::OT_CONTROLLER(STATE *state, byte pinMasterIn, byte pinMasterOut, byte pinSlaveIn, byte pinSlaveOut)
    : _state(*state)
{
  _pinMasterIn = pinMasterIn;
  _pinMasterOut = pinMasterOut;
  _pinSlaveIn = pinSlaveIn;
  _pinSlaveOut = pinSlaveOut;
}

void OT_CONTROLLER::setup()
{
  pinMode(_pinMasterIn, INPUT);
  digitalWrite(_pinMasterOut, HIGH);
  pinMode(_pinMasterOut, OUTPUT); // low output = high current, high output = low current
  pinMode(_pinSlaveIn, INPUT);
  digitalWrite(_pinSlaveOut, HIGH);
  pinMode(_pinSlaveOut, OUTPUT); // low output = high voltage, high output = low voltage

  _mode = MODE_GATEWAY;
  _phase = PHASE_LISTEN_MASTER;
}

void OT_CONTROLLER::loop()
{
  // Listen to request from thermostat
  if (_phase == PHASE_LISTEN_MASTER)
  {
    if (OPENTHERM::isIdle() || OPENTHERM::isError() || OPENTHERM::isSent())
    {
      if (OPENTHERM::isError())
      { // no response from master or error in it
        OPENTHERM::stop();
        _onRequestTimeout();
      }
      // start listening when not listening
      OPENTHERM::listen(_pinMasterIn, 800);
      Homie.setIdle(true);
    }
    else if (OPENTHERM::getMessage(_request))
    {
      Homie.setIdle(false);
      // request received
      OPENTHERM::stop();
      _phase = PHASE_REQUEST_SLAVE;
      _requestOverridden = false;
      _responseOverridden = false;
      _interceptRequest();
      _logRequestToSerial();
    }
  }
  // Send request from thermostat to boiler, get response
  else if (_phase == PHASE_REQUEST_SLAVE)
  {
    if (OPENTHERM::isIdle())
    { // send request to slave
      OPENTHERM::send(_pinSlaveOut, _requestOverridden ? _requestOverride : _request);
    }
    else if (OPENTHERM::isSent())
    { // await response from slave with timeout of 800ms
      OPENTHERM::listen(_pinSlaveIn, 800);
    }
    else if (OPENTHERM::hasMessage())
    { // response received
      if (!_responseOverridden)
      {
        OPENTHERM::getMessage(_response);
        _interceptResponse();
      }
      OPENTHERM::stop();
      _phase = (_mode == MODE_GATEWAY) ? PHASE_RESPOND_MASTER : PHASE_LISTEN_MASTER;
      _logResponseToSerial();
    }
    else if (OPENTHERM::isError())
    { // no response from slave or error in it, lets skip it and listen to next request from master
      OPENTHERM::stop();
      _phase = PHASE_LISTEN_MASTER;
      _onResponseTimeout();
    }
  }
  // Send response from boiler to thermostat
  else if (_phase == PHASE_RESPOND_MASTER)
  {
    if (OPENTHERM::isIdle())
    { // send response to master
      OPENTHERM::send(_pinMasterOut, _response);
    }
    else if (OPENTHERM::isSent())
    { // response sent, listen to next request from master
      OPENTHERM::stop();
      _phase = PHASE_LISTEN_MASTER;
    }
  }
}

void OT_CONTROLLER::_interceptRequest()
{
  if (_state.thermostat.chOverride || _state.thermostat.dhwOverride)
  {
    _requestOverridden = true;
    _requestOverride = _request;
  }
  else
  {
    _requestOverridden = false;
  }

  if (_request.id == OT_MSGID_STATUS)
  {

    if (_state.thermostat.chOverride)
    {
      bitWrite(_requestOverride.valueHB, 0, _state.thermostat.chEnable);
    }
    else
    {
      bool chEnable = bitRead(_request.valueHB, 0);
      _setStateChg(_state.thermostat.chEnable, _state.thermostatChg.chEnable, chEnable);
    }

    if (_state.thermostat.dhwOverride)
    {
      bitWrite(_requestOverride.valueHB, 1, _state.thermostat.dhwEnable);
    }
    else
    {
      bool dhwEnable = bitRead(_request.valueHB, 1);
      _setStateChg(_state.thermostat.dhwEnable, _state.thermostatChg.dhwEnable, dhwEnable);
    }
  }
  else if (_request.id == OT_MSGID_CH_SETPOINT)
  {
    if (_state.thermostat.chOverride)
    {
      _requestOverride.f88(_state.thermostat.chSetpoint);
    }
    else
    {
      _setStateChg(_state.thermostat.chSetpoint, _state.thermostatChg.chSetpoint, _request.f88());
    }
  }
  else if (_request.id == OT_MSGID_DHW_SETPOINT)
  {
    if (_state.thermostat.dhwOverride)
    {
      _requestOverride.f88(_state.thermostat.dhwSetpoint);
    }
    else
    {
      _setStateChg(_state.thermostat.dhwSetpoint, _state.thermostatChg.dhwSetpoint, _request.f88());
    }
  }
  // In my case boiler returns alway 0 so I capture this data from thermostat request here
  // Unfortunately thermostat seems to send this only once after it's startup
  else if (_request.id == OT_MSGID_ROOM_TEMP)
  {
    _setStateChg(_state.thermostat.roomTemp, _state.thermostatChg.roomTemp, _request.f88());
  }
  // Set thermostat state to online.
  _setStateChg(_state.thermostat.online, _state.thermostatChg.online, true);
  // Reset consecutive request timeout count
  _state.thermostat.timeoutCnt = 0;
}

void OT_CONTROLLER::_interceptResponse()
{
  if (_response.id == OT_MSGID_STATUS)
  {
    bool fault = bitRead(_response.valueLB, 0);
    bool chOn = bitRead(_response.valueLB, 1);
    bool dhwOn = bitRead(_response.valueLB, 2);
    bool flameOn = bitRead(_response.valueLB, 3);

    _setStateChg(_state.boiler.fault, _state.boilerChg.fault, fault);
    _setStateChg(_state.boiler.chOn, _state.boilerChg.chOn, chOn);
    _setStateChg(_state.boiler.dhwOn, _state.boilerChg.dhwOn, dhwOn);
    _setStateChg(_state.boiler.flameOn, _state.boilerChg.flameOn, flameOn);
  }
  else if (_response.id == OT_MSGID_MODULATION_LEVEL)
  {
    _setStateChg(_state.boiler.modulationLvl, _state.boilerChg.modulationLvl, _response.f88());
  }
  // Boiler returns always 0 - obtaining from request above instead
  // else if (_response.id == OT_MSGID_ROOM_TEMP) {
  //   _state.thermostat.roomTemp = _response.f88();
  // }
  else if (_response.id == OT_MSGID_FEED_TEMP)
  {
    _setStateChg(_state.boiler.feedTemp, _state.boilerChg.feedTemp, _response.f88());
  }
  else if (_response.id == OT_MSGID_DHW_TEMP)
  {
    _setStateChg(_state.boiler.dhwTemp, _state.boilerChg.dhwTemp, _response.f88());
  }
  else if (_response.id == OT_MSGID_OUTSIDE_TEMP)
  {
    _setStateChg(_state.boiler.outsideTemp, _state.boilerChg.outsideTemp, _response.f88());
  }
  // In my case boiler returns alway 0 - skipping...
  /*   else if (_response.id == OT_MSGID_RETURN_WATER_TEMP)
  {
    _state.boiler.returnTemp = _response.f88();
  }
 */
  // Set boiler state to online.
  _setStateChg(_state.boiler.online, _state.boilerChg.online, true);
  // Reset consecutive response timeout count
  _state.boiler.timeoutCnt = 0;
}

void OT_CONTROLLER::_onRequestTimeout()
{
  if (_state.thermostat.timeoutCnt > 1)
  {
    _setStateChg(_state.thermostat.online, _state.thermostatChg.online, false);
  }
  Log.warning("[T] Timeout" CR);
}

void OT_CONTROLLER::_onResponseTimeout()
{
  if (_state.boiler.timeoutCnt > 1)
  {
    _setStateChg(_state.boiler.online, _state.boilerChg.online, false);
  }
  Log.warning("[T] ");
  OPENTHERM::printToSerial(_request);
  Log.warning(" -> [B] Timeout" CR);
}

template <class T>
void OT_CONTROLLER::_setStateChg(T &state, bool &stateChg, const T &input)
{
  if (state != input)
  {
    state = input;
    stateChg = true;
  }
  // Do not clear, it is cleared only on successfull MQTT send
  // else
  // {
  //   stateChg = false;
  // }
}

void OT_CONTROLLER::_logRequestToSerial()
{
  // #ifdef DEBUG
  if (_request.id == OT_MSGID_STATUS)
  {
    Log.trace("[T] Master status: %b" CR, _request.valueHB);
    Log.trace("[T] CH enable: %T" CR, bitRead(_request.valueHB, 0));
    if (_state.thermostat.chOverride)
      Log.trace("[T] CH enable (overriden): %T" CR, bitRead(_requestOverride.valueHB, 0));
    Log.trace("[T] DHW enable: %T" CR, bitRead(_request.valueHB, 1));
    if (_state.thermostat.dhwOverride)
      Log.trace("[T] DHW enable (overriden): %T" CR, bitRead(_requestOverride.valueHB, 1));
  }
  else if (_request.id == OT_MSGID_SLAVE_CONFIG)
  {
    Log.trace("[T] Slave config?" CR);
  }
  else if (_request.id == OT_MSGID_CH_SETPOINT)
  {
    Log.trace("[T] CH setpoint: %F °C" CR, _request.f88());
    if (_state.thermostat.chOverride)
      Log.trace("[T] CH setpoint (overriden): %F °C" CR, _requestOverride.f88());
  }
  else if (_request.id == OT_MSGID_DHW_SETPOINT)
  {
    Log.trace("[T] DHW setpoint: %F °C" CR, _request.f88());
    if (_state.thermostat.dhwOverride)
      Log.trace("[T] DHW setpoint (overriden): %F °C" CR, _requestOverride.f88());
  }
  else if (_request.id == OT_MSGID_ROOM_SETPOINT)
  {
    Log.trace("[T] Room setpoint: %F °C" CR, _request.f88());
  }
  else if (_request.id == OT_MSGID_ROOM_TEMP)
  {
    Log.trace("[T] Room temp: %F °C" CR, _request.f88());
  }
  else if (_request.id == OT_MSGID_FAULT_FLAGS)
  {
    Log.trace("[T] Faults?" CR);
  }
  else if (_request.id == OT_MSGID_FEED_TEMP)
  {
    Log.trace("[T] Feed temp?" CR);
  }
  else if (_request.id == OT_MSGID_RETURN_WATER_TEMP)
  {
    Log.trace("[T] Return temp?" CR);
  }
  else if (_request.id == OT_MSGID_OUTSIDE_TEMP)
  {
    Log.trace("[T] Outside temp?" CR);
  }
  else if (_request.id == OT_MSGID_DHW_TEMP)
  {
    Log.trace("[T] DHW temp?" CR);
  }
  else if (_request.id == OT_MSGID_DHW_BOUNDS)
  {
    Log.trace("[T] DHW setpoint bounds?" CR);
  }
  else if (_request.id == OT_MSGID_CH_BOUNDS)
  {
    Log.trace("[T] CH setpoint bounds?" CR);
  }
  else if (_request.id == OT_MSGID_MODULATION_LEVEL)
  {
    Log.trace("[T] Modulation lvl?" CR);
  }
  else if (_request.id == OT_MSGID_DHW_FLOW_RATE)
  {
    Log.trace("[T] DHW flow rate?" CR);
  }
  else if (_request.id == OT_MSGID_EXHAUST_TEMP)
  {
    Log.trace("[T] Exhaust temp?" CR);
  }
  else if (_request.id == OT_MSGID_MAX_CH_SETPOINT)
  {
    Log.trace("[T] CH setpoint max: %F °C" CR, _request.f88());
  }
  else if (_request.id == OT_MSGID_MAX_MODULATION_LEVEL)
  {
    Log.trace("[T] Max modulation lvl: %F %%" CR, _request.f88());
  }
  else
  {
    Log.trace("[T] ");
    OPENTHERM::printToSerial(_request);
    Log.trace(CR);
  }
  // #endif
}

void OT_CONTROLLER::_logResponseToSerial()
{
  // #ifdef DEBUG
  if (_response.id == OT_MSGID_STATUS)
  {
    Log.trace("[B] Slave status: %b" CR, _response.valueLB);
    Log.trace("[B] Fault: %T" CR, bitRead(_response.valueLB, 0));
    Log.trace("[B] CH: %T" CR, bitRead(_response.valueLB, 1));
    Log.trace("[B] DHW: %T" CR, bitRead(_response.valueLB, 2));
    Log.trace("[B] Flame: %T" CR, bitRead(_response.valueLB, 3));
  }
  else if (_response.id == OT_MSGID_SLAVE_CONFIG)
  {
    Log.trace("[B] Slave config: %b" CR, _response.valueHB);
    Log.trace("[B] DHW present: %T" CR, bitRead(_response.valueHB, 0));
    Log.trace("[B] Control type: %T" CR, bitRead(_response.valueHB, 1));
    Log.trace("[B] Cooling config: %T" CR, bitRead(_response.valueHB, 2));
    Log.trace("[B] DHW config: %T" CR, bitRead(_response.valueHB, 3));
  }
  else if (_response.id == OT_MSGID_MODULATION_LEVEL)
  {
    Log.trace("[B] Modulation lvl: %F %%" CR, _response.f88());
  }
  else if (_response.id == OT_MSGID_ROOM_TEMP)
  {
    Log.trace("[B] Room temp: %F °C" CR, _response.f88());
  }
  else if (_response.id == OT_MSGID_FEED_TEMP)
  {
    Log.trace("[B] Feed temp: %F °C" CR, _response.f88());
  }
  else if (_response.id == OT_MSGID_DHW_TEMP)
  {
    Log.trace("[B] DHW temp: %F °C" CR, _response.f88());
  }
  else if (_response.id == OT_MSGID_OUTSIDE_TEMP)
  {
    Log.trace("[B] Outside temp: %F °C" CR, _response.f88());
  }
  else if (_response.id == OT_MSGID_RETURN_WATER_TEMP)
  {
    Log.trace("[B] Return temp: %F °C" CR, _response.f88());
  }
  else if (_response.id == OT_MSGID_EXHAUST_TEMP)
  {
    Log.trace("[B] Exhaust temp: %F °C" CR, _response.f88());
  }
  else if (_response.id == OT_MSGID_DHW_FLOW_RATE)
  {
    Log.trace("[B] DHW flow rate: %F l/m" CR, _response.f88());
  }
  else if (_response.id == OT_MSGID_FAULT_FLAGS)
  {
    Log.trace("[B] Faults: %b" CR, _response.valueHB);
  }
  else if (_response.id == OT_MSGID_CH_SETPOINT)
  {
    Log.trace("[B] CH setpoint: %F °C" CR, _response.f88());
  }
  else if (_response.id == OT_MSGID_DHW_SETPOINT)
  {
    Log.trace("[B] DHW setpoint: %F °C" CR, _response.f88());
  }
  else if (_response.id == OT_MSGID_ROOM_SETPOINT)
  {
    Log.trace("[B] Room setpoint: %F °C" CR, _response.f88());
  }
  else if (_response.id == OT_MSGID_DHW_BOUNDS)
  {
    Log.trace("[B] DHW setpoint lower/upper bound: %d / %d °C" CR, _response.valueLB, _response.valueHB);
  }
  else if (_response.id == OT_MSGID_CH_BOUNDS)
  {
    Log.trace("[B] CH setpoint lower/upper bound: %d / %d °C" CR, _response.valueLB, _response.valueHB);
  }
  else if (_response.id == OT_MSGID_MAX_MODULATION_LEVEL)
  {
    Log.trace("[B] Max modulation lvl: %F %%" CR, _response.f88());
  }
  else if (_response.id == OT_MSGID_MAX_CH_SETPOINT)
  {
    Log.trace("[B] CH setpoint ACK" CR);
  }
  else
  {
    Log.trace("[B] ");
    OPENTHERM::printToSerial(_response);
    Log.trace(CR);
  }
  // #endif
}
