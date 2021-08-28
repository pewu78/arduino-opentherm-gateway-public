#ifndef OT_CONTROLLER_H
#define OT_CONTROLLER_H

// #include "gateway.h"
#include <opentherm.h>
#include <state.h>

class OT_CONTROLLER
{
public:
  OT_CONTROLLER(STATE *state, byte pinMasterIn, byte pinMasterOut, byte pinSlaveIn, byte pinSlaveOut);
  void setup();
  void loop();

private:
  STATE &_state;

  OpenthermData _request;           // request data
  OpenthermData _requestOverride;   // overriden request data
  OpenthermData _response;          // response data
  bool _responseOverridden = false; // response from slave was overriden?
  bool _requestOverridden = false;  // request from master was overriden?

  byte _pinMasterIn;
  byte _pinMasterOut;
  byte _pinSlaveIn;
  byte _pinSlaveOut;

  byte _mode;
  byte _phase;

  void _interceptRequest();
  void _interceptResponse();
  void _onRequestTimeout();
  void _onResponseTimeout();
  
  template <class T> 
  void _setStateChg(T &state, bool &stateChg, const T &input);

  void _logRequestToSerial();
  void _logResponseToSerial();
};

#endif
