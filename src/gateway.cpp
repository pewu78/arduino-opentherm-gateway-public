#include <Arduino.h>
#include <Homie.h>
#include <TaskScheduler.h>
#include <ArduinoLog.h>
#include "gateway.h"
#include "state.h"
#include "otController.h"

Scheduler ts;

Task tOpenTherm(TASK_IMMEDIATE, TASK_FOREVER, &tOpenThermCallback, &ts, true);
Task tSendState(TASK_SECOND, TASK_FOREVER, &tSendStateCallback, &ts, false);

STATE state; // current state of thermostat / boiler

OT_CONTROLLER openthermCtrl(&state, THERMOSTAT_IN, THERMOSTAT_OUT, BOILER_IN, BOILER_OUT);

HomieNode thermostat("thermostat", "Thermostat", "thermostat", false, 0, 0, thermostatInputHandler);
HomieNode boiler("boiler", "Boiler", "boiler");

byte disconnected_count;

void setup()
{
  // Setup serial
  Serial.begin(115200);
  Serial << endl
         << endl;
  // Serial.setDebugOutput(true); // ESP debug output

  disconnected_count = 0;

  Log.begin(LOGGING_LEVEL, &Serial, false);
  Log.notice("Firmware: %s Version: %s" CR, firmwareName, firmwareVersion);
  Homie_setFirmware(firmwareName, firmwareVersion);
  Homie_setBrand(brandName);
  // Homie.setHomieBootMode(HomieBootMode::STANDALONE);
  Homie.setLedPin(SCK_LED, HIGH);
  // Homie.disableLogging();

  // Homie.setGlobalInputHandler(globalInputHandler);
  // Homie.setLoopFunction(homieLoopHandler);

  thermostat.advertise("ch").setName("Central Heating").setDatatype("boolean").settable();
  thermostat.advertise("choverride").setName("Central Heating Override").setDatatype("boolean").settable();
  thermostat.advertise("dhw").setName("Domestic Hot Water").setDatatype("boolean").settable();
  thermostat.advertise("dhwoverride").setName("Domestic Hot Water Override").setDatatype("boolean").settable();
  thermostat.advertise("chsetpoint").setName("Central Heating setpoint").setDatatype("float").setUnit("°C").settable().setFormat("10:90");
  thermostat.advertise("dhwsetpoint").setName("Domestic Hot Water setpoint").setDatatype("float").setUnit("°C").settable().setFormat("0:60");
  thermostat.advertise("roomtemp").setName("Room Temperature").setDatatype("float").setUnit("°C");
  thermostat.advertise("online").setName("Online").setDatatype("boolean");

  boiler.advertise("fault").setName("Fault").setDatatype("boolean");
  boiler.advertise("ch").setName("Central Heating").setDatatype("boolean");
  boiler.advertise("dhw").setName("Domestic Hot Water").setDatatype("boolean");
  boiler.advertise("flame").setName("Flame").setDatatype("boolean");
  boiler.advertise("modulation").setName("Modulation Level").setDatatype("float").setUnit("%").setFormat("0:100");
  boiler.advertise("feedtemp").setName("Feed Temperature").setDatatype("float").setUnit("°C");
  // boiler.advertise("return-temp").setName("Return Temperature").setDatatype("float").setUnit("°C");
  boiler.advertise("dhwtemp").setName("Domestic Hot Water Temperature").setDatatype("float").setUnit("°C");
  boiler.advertise("outsidetemp").setName("Outside Temperature").setDatatype("float").setUnit("°C");
  boiler.advertise("online").setName("Online").setDatatype("boolean");

  Homie.onEvent(onHomieEvent); // before Homie.setup()
  Homie.setup();
  openthermCtrl.setup();
}

void loop()
{
  ts.execute(); // execute task scheduler
  Homie.loop();
}

// void homieLoopHandler()
// {
//   ts.execute(); // execute task scheduler
// }

template <class T>
void setStateChg(T &state, bool &stateChg, const T &input)
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

// bool globalInputHandler(const HomieNode& node, const HomieRange& range, const String& property, const String& value) {
//   // Homie.getLogger() << "Global input handler. Received on node " << node.getId() << ": " << property << " = " << value << endl;
//   Log.trace("[GIH][%s][%s][%s]" CR, node.getId(), property.c_str(), value.c_str());
//   return false;
// }

bool thermostatInputHandler(const HomieRange &range, const String &property, const String &value)
{
  Log.trace("[MQTT IN][%s][%s]" CR, property.c_str(), value.c_str());
  if (property.equals("choverride"))
  {
    setStateChg(state.thermostat.chOverride, state.thermostatChg.chOverride, (value.equals("true")) ? true : false);
  }
  else if (property.equals("dhwoverride"))
  {
    setStateChg(state.thermostat.dhwOverride, state.thermostatChg.dhwOverride, (value.equals("true")) ? true : false);
  }
  else if (property.equals("ch"))
  {
    setStateChg(state.thermostat.chEnable, state.thermostatChg.chEnable, (value.equals("true")) ? true : false);
  }
  else if (property.equals("dhw"))
  {
    setStateChg(state.thermostat.dhwEnable, state.thermostatChg.dhwEnable, (value.equals("true")) ? true : false);
  }
  else if (property.equals("chsetpoint"))
  {
    setStateChg(state.thermostat.chSetpoint, state.thermostatChg.chSetpoint, value.toFloat());
  }
  else if (property.equals("dhwsetpoint"))
  {
    setStateChg(state.thermostat.dhwSetpoint, state.thermostatChg.dhwSetpoint, value.toFloat());
  }
  else
  {
    Log.warning("Property %s is not valid" CR, property.c_str());
  }

  return true;
}

void tOpenThermCallback()
{
  openthermCtrl.loop();
}

void tSendStateCallback()
{
  // every second ticks
  // Log.trace("EverySecond" CR);
  sendState();
}

String toHomiePayload(bool value)
{
  return value ? "true" : "false";
}

String toHomiePayload(float value)
{
  return String(value, 2);
}

String toHomiePayload(unsigned long value)
{
  return String(value);
}

// TODO: refactor (same in otController)

template <class T>
void sendState(HomieNode &node, const String &property, const T &state, bool &stateChg)
{
  if (stateChg)
  {
    Log.trace("[MQTT OUT][%s][%s][%s] ... ", strcmp(node.getName(), "Boiler") == 0 ? "B" : "T", property.c_str(), toHomiePayload(state));
    if (node.setProperty(property).send(toHomiePayload(state)) != 0)
    {
      stateChg = false; // clear changed flag on successfull send
      Log.trace("[OK]" CR);
    }
    else
    {
      Log.trace("[ERROR]" CR);
    }
  }
}

void sendState()
{
  // Log.trace("sendState()" CR);
  if (Homie.isConnected() && Homie.getMqttClient().connected())
  {
    // boiler
    sendState(boiler, "fault", state.boiler.fault, state.boilerChg.fault);
    sendState(boiler, "ch", state.boiler.chOn, state.boilerChg.chOn);
    sendState(boiler, "dhw", state.boiler.dhwOn, state.boilerChg.dhwOn);
    sendState(boiler, "flame", state.boiler.flameOn, state.boilerChg.flameOn);
    sendState(boiler, "modulation", state.boiler.modulationLvl, state.boilerChg.modulationLvl);
    sendState(boiler, "feedtemp", state.boiler.feedTemp, state.boilerChg.feedTemp);
    sendState(boiler, "dhwtemp", state.boiler.dhwTemp, state.boilerChg.dhwTemp);
    sendState(boiler, "outsidetemp", state.boiler.outsideTemp, state.boilerChg.outsideTemp);
    sendState(boiler, "online", state.boiler.online, state.boilerChg.online);
    // thermostat
    sendState(thermostat, "ch", state.thermostat.chEnable, state.thermostatChg.chEnable);
    sendState(thermostat, "choverride", state.thermostat.chOverride, state.thermostatChg.chOverride);
    sendState(thermostat, "dhw", state.thermostat.dhwEnable, state.thermostatChg.dhwEnable);
    sendState(thermostat, "dhwoverride", state.thermostat.dhwOverride, state.thermostatChg.dhwOverride);
    sendState(thermostat, "chsetpoint", state.thermostat.chSetpoint, state.thermostatChg.chSetpoint);
    sendState(thermostat, "dhwsetpoint", state.thermostat.dhwSetpoint, state.thermostatChg.dhwSetpoint);
    sendState(thermostat, "roomtemp", state.thermostat.roomTemp, state.thermostatChg.roomTemp);
    sendState(thermostat, "online", state.thermostat.online, state.thermostatChg.online);
  }
  else
  {
    Log.trace("Not connected." CR);
  }
}

void onHomieEvent(const HomieEvent &event)
{
  switch (event.type)
  {
  case HomieEventType::STANDALONE_MODE:
    // Do whatever you want when standalone mode is started
    Log.trace("HomieEvent:STANDALONE_MODE" CR);
    break;
  case HomieEventType::CONFIGURATION_MODE:
    // Do whatever you want when configuration mode is started
    Log.trace("HomieEvent:CONFIGURATION_MODE" CR);
    break;
  case HomieEventType::NORMAL_MODE:
    // Do whatever you want when normal mode is started
    Log.trace("HomieEvent:NORMAL_MODE" CR);
    break;
  case HomieEventType::OTA_STARTED:
    // Do whatever you want when OTA is started
    Log.trace("HomieEvent:OTA_STARTED" CR);
    tOpenTherm.disable();
    tSendState.disable();
    break;
  case HomieEventType::OTA_PROGRESS:
    // Do whatever you want when OTA is in progress
    // You can use event.sizeDone and event.sizeTotal
    break;
  case HomieEventType::OTA_FAILED:
    // Do whatever you want when OTA is failed
    Log.trace("HomieEvent:OTA_FAILED" CR);
    tOpenTherm.enable();
    tSendState.enable();
    break;
  case HomieEventType::OTA_SUCCESSFUL:
    // Do whatever you want when OTA is successful
    Log.trace("HomieEvent:OTA_SUCCESSFUL" CR);
    tOpenTherm.enable();
    tSendState.enable();
    break;
  case HomieEventType::ABOUT_TO_RESET:
    // Do whatever you want when the device is about to reset
    Log.trace("HomieEvent:ABOUT_TO_RESET" CR);
    break;
  case HomieEventType::WIFI_CONNECTED:
    // Do whatever you want when Wi-Fi is connected in normal mode
    // You can use event.ip, event.gateway, event.mask
    Log.trace("HomieEvent:WIFI_CONNECTED" CR);
    disconnected_count = 0;
    break;
  case HomieEventType::WIFI_DISCONNECTED:
    // Do whatever you want when Wi-Fi is disconnected in normal mode
    // You can use event.wifiReason
    /*
      WIFI_REASON_UNSPECIFIED              = 1,
      WIFI_REASON_AUTH_EXPIRE              = 2,
      WIFI_REASON_AUTH_LEAVE               = 3,
      WIFI_REASON_ASSOC_EXPIRE             = 4,
      WIFI_REASON_ASSOC_TOOMANY            = 5,
      WIFI_REASON_NOT_AUTHED               = 6,
      WIFI_REASON_NOT_ASSOCED              = 7,
      WIFI_REASON_ASSOC_LEAVE              = 8,
      WIFI_REASON_ASSOC_NOT_AUTHED         = 9,
      WIFI_REASON_DISASSOC_PWRCAP_BAD      = 10,
      WIFI_REASON_DISASSOC_SUPCHAN_BAD     = 11,
      WIFI_REASON_IE_INVALID               = 13,
      WIFI_REASON_MIC_FAILURE              = 14,
      WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT   = 15,
      WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT = 16,
      WIFI_REASON_IE_IN_4WAY_DIFFERS       = 17,
      WIFI_REASON_GROUP_CIPHER_INVALID     = 18,
      WIFI_REASON_PAIRWISE_CIPHER_INVALID  = 19,
      WIFI_REASON_AKMP_INVALID             = 20,
      WIFI_REASON_UNSUPP_RSN_IE_VERSION    = 21,
      WIFI_REASON_INVALID_RSN_IE_CAP       = 22,
      WIFI_REASON_802_1X_AUTH_FAILED       = 23,
      WIFI_REASON_CIPHER_SUITE_REJECTED    = 24,

      WIFI_REASON_BEACON_TIMEOUT           = 200,
      WIFI_REASON_NO_AP_FOUND              = 201,
      WIFI_REASON_AUTH_FAIL                = 202,
      WIFI_REASON_ASSOC_FAIL               = 203,
      WIFI_REASON_HANDSHAKE_TIMEOUT        = 204,
    */
    Log.trace("HomieEvent:WIFI_DISCONNECTED Reason:%i" CR, event.wifiReason);
    disconnected_count++;
    Log.trace("Disconnected count:%i" CR, disconnected_count);
    if (disconnected_count == 1)
    {
      state.thermostat.chOverride = false;
      state.thermostat.dhwOverride = false;
    }
    if (disconnected_count == 10)
    {
      ESP.restart();
    }
    break;
  case HomieEventType::MQTT_READY:
    // Do whatever you want when MQTT is connected in normal mode
    Log.trace("HomieEvent:MQTT_READY" CR);
    tSendState.enable();
    break;
  case HomieEventType::MQTT_DISCONNECTED:
    // Do whatever you want when MQTT is disconnected in normal mode
    // You can use event.mqttReason
    /*
      MQTT Reason (source: https://github.com/marvinroger/async-mqtt-client/blob/master/src/AsyncMqttClient/DisconnectReasons.hpp)
      0 TCP_DISCONNECTED
      1 MQTT_UNACCEPTABLE_PROTOCOL_VERSION
      2 MQTT_IDENTIFIER_REJECTED
      3 MQTT_SERVER_UNAVAILABLE
      4 MQTT_MALFORMED_CREDENTIALS
      5 MQTT_NOT_AUTHORIZED
      6 ESP8266_NOT_ENOUGH_SPACE
      7 TLS_BAD_FINGERPRINT
    */
    Log.trace("HomieEvent:MQTT_DISCONNECTED Reason:%i" CR, event.mqttReason);
    tSendState.disable();
    break;
  case HomieEventType::MQTT_PACKET_ACKNOWLEDGED:
    // Do whatever you want when an MQTT packet with QoS > 0 is acknowledged by the broker
    // You can use event.packetId
    break;
  case HomieEventType::READY_TO_SLEEP:
    // After you've called `prepareToSleep()`, the event is triggered when MQTT is disconnected
    Log.trace("HomieEvent:READY_TO_SLEEP" CR);
    break;
  case HomieEventType::SENDING_STATISTICS:
    // Do whatever you want when statistics are sent in normal mode
    Log.trace("HomieEvent:SENDING_STATISTICS" CR);
    break;
  }
}