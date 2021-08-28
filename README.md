# Opentherm Gateway for ESP32/ESP8266
OpenTherm Gateway using [Opentherm IO library for Arduino and Shield](https://github.com/jpraus/arduino-opentherm) made by Jiří Praus.
Using [Homie library for ESP8266 / ESP32](https://github.com/homieiot/homie-esp8266) for MQTT communication.
Based on Jiří's code of [OpenTherm regulator](https://hackaday.io/project/162819-opentherm-regulator).

## What is it good for? ##
Place it between OpenTherm boiler and thermostat to intercept communication and override it if needed to control CH/DHW (**man-in-the-middle** device).
Data is sent over MQTT using Homie convention, so it's automatically detected by OpenHAB but can be used by any home automation system supporting MQTT (currently using with Home Assistant).

## What can you do with it? ##
Thermostat:
- read and control (override) central heating operation and setpoint
- read and control (override) domestic hot water heating operation and setpoint
- read room temperature sensor
- read online status

Boiler:
- read central heating operation status
- read domestic hot water heating operation status
- read flame status
- read fault status
- read modulation level
- read feed temperature
- read return temperature
- read domestic hot water temperature
- read outside sensor temperature
- read online status

## How to use it? ##
- Have OTG shield connected to ESP - you can't connect the shield directly due to ESP pin [restrictions](https://randomnerdtutorials.com/esp8266-pinout-reference-gpios). My advice is to solder angular connectors to these 4 pins used for communication and straight-through for the rest and use jumper wires to connect them to the chosen/valid ones [(ref1)](https://raw.githubusercontent.com/pewu78/arduino-opentherm-gateway-public/master/doc/ESP8266_SHIELD_BOTTOM.jpg) [(ref2)](https://raw.githubusercontent.com/pewu78/arduino-opentherm-gateway-public/master/doc/ESP8266_SHIELD_TOP.jpg). See gateway.h for possible choices.
- Set your platform (ESP32/ESP8266) as current in project
- Make necessary changes in gateway.h (I/O pins used) and config.json (WIFI/MQTT)
- Build and upload (also filesystem image!)
- Monitor communication with serial monitor
- Use with OH/HA (below) or any other MQTT-based automation
### OpenHAB ###
OH supports Homie convention, so it should detect MQTT entities automatically (it did for me). You're good to go, but on your own - I don't use OH anymore :)
### Home Assistant ###
HA does not support directly Homie devices, you need to add MQTT entities manually. I provided mine configuration in ha directory, you can use these files as packages (it assumes device_id is "OT-Gateway" as in default config). Update notifiers for alerts in ot_gateway.yaml to your devices.
Example dashboard:
![Home Assistant Dashboard](https://raw.githubusercontent.com/pewu78/arduino-opentherm-gateway-public/master/doc/OTG_HA.png)

## Known issues ##
- In case of my thermostat room temperature is only reported (sent to boiler) once after thermostat startup, so it's not usable, but you may have better luck
- My boiler doesn't support return temperature reporting, but again your mileage may vary (it depends on boiler, see [this](http://otgw.tclcode.com/matrix.cgi#boilers))
- Modulation level is not reported precisely by boiler, especially for short durations of heating
- Homie OTA updates currently result in 'CPU panic' most likely due to OT library interrupts, need to check if they can be disabled for the duration of OTA.
- For me the ESP32 loses WIFI connection once every few weeks with reason 2 ("ESP32 station start" ?) and fails to reconnect unless I connect laptop to USB port to see debug messages and restart ESP (restarting ESP without it doesn't help, maybe some power supply issue)

## Backstory ##
I looked for means to control boiler with OT, but didn't want to use existing [OT Gateway](https://otgw.tclcode.com).
Found Jiří's library / shield and ordered the latter for ESP (3.3V variant).
Unfortunately the pinout of the shield is not very good for ESP8266, as the used pins (2-5) have some restrictions, so that required a bit of rewiring. Solving this I managed to get it running in monitoring mode (intercept & output thermostat-boiler communication).
For communication I've chosen Homie library as at that time I was using OpenHAB, and again I hit some issues with OH implementation of Homie which stalled the progress again.
After some time it was fixed I could connect, but found out the WIFI/MQTT connection very unstable, possibly due to bad copy of ESP...
Then I switched to ESP32 and got it running fine for quite some time now.
