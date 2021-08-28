#ifndef GATEWAY_H
#define GATEWAY_H
#include <Homie.h>

#define brandName "pewu78"
#define firmwareName "OT-Gateway"
#define firmwareVersion "0.3"

#if defined(ESP8266)
#pragma message "ESP8266 configuration"
// definitions of I/O pins
#define SCK_LED D5
// OpenTherm Boiler/Thermostat protocol
#define THERMOSTAT_IN D1  //=GPIO 5   2->D1
#define THERMOSTAT_OUT D6 //=GPIO 12  4->D6
#define BOILER_IN D2      //=GPIO 4   3->D2
#define BOILER_OUT D7     //=GPIO 13  5->D7
#elif defined(ESP32)
#pragma message "ESP32 configuration"
// definitions of I/O pins
#define SCK_LED LED_BUILTIN
// OpenTherm Boiler/Thermostat protocol
#define THERMOSTAT_IN 18  //=GPIO 18  2->18
#define THERMOSTAT_OUT 27 //=GPIO 27  4->27
#define BOILER_IN 19      //=GPIO 19  3->19
#define BOILER_OUT 14     //=GPIO 14  5->14
#else
#error "ESP8266 or ESP32 required"
#endif

#define LOGGING_LEVEL LOG_LEVEL_VERBOSE

// functions
// void homieLoopHandler();
// bool globalInputHandler(const HomieNode& node, const HomieRange& range, const String& property, const String& value);
bool thermostatInputHandler(const HomieRange &range, const String &property, const String &value);
void tOpenThermCallback();
void tSendStateCallback();
String toHomiePayload(bool value);
String toHomiePayload(float value);
String toHomiePayload(unsigned long value);
template <class T>
void sendState(HomieNode &node, const String &property, const T &state,
               bool &stateChg);
template <class T> void setStateChg(T &state, bool &stateChg, const T &input);
void sendState();
void onHomieEvent(const HomieEvent& event);

#endif
