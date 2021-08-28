#ifndef STATE_H
#define STATE_H
#include <Arduino.h>

struct BoilerState
{
  bool fault = false;
  bool chOn = false;
  bool dhwOn = false;
  bool flameOn = false;
  float modulationLvl = 0;
  float feedTemp = 0;
  //float returnTemp = 0;
  float dhwTemp = 0;
  float outsideTemp = 0;
  bool online = false;
  unsigned int timeoutCnt = 0; // count of how many consecutive times boiler failed to respond
};

struct BoilerStateChanged
{
  bool fault = false;
  bool chOn = false;
  bool dhwOn = false;
  bool flameOn = false;
  bool modulationLvl = false;
  bool feedTemp = false;
  //bool returnTemp = false;
  bool dhwTemp = false;
  bool outsideTemp = false;
  bool online = true;
};

struct ThermostatState
{
  bool chEnable = false;
  bool chOverride = false;
  bool dhwEnable = false;
  bool dhwOverride = false;
  float roomTemp = 0;
  float chSetpoint = 0;
  float dhwSetpoint = 0;
  bool online = false;
  unsigned int timeoutCnt = 0; // count of how many consecutive times thermostat failed to request
};

struct ThermostatStateChanged
{
  bool chEnable = false;
  bool chOverride = false;
  bool dhwEnable = false;
  bool dhwOverride = false;
  bool roomTemp = false;
  bool chSetpoint = false;
  bool dhwSetpoint = false;
  bool online = true;
};

class STATE
{
public:
  BoilerState boiler; // read-only
  BoilerStateChanged boilerChg;
  ThermostatState thermostat; // read-write
  ThermostatStateChanged thermostatChg;
};

#endif
