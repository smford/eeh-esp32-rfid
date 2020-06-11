# eeh-esp32-rfid

A simple ESP32 Based RFID Access Control System for tools

## Components
- ESP32 Dev Board
- MFRC522 RFID Board
- Generic Relay Board

## Features
- Upon RFID being presented eeh-esp32-rfid will check whether that card has been granted access to the device and fire the relay
- Users can be baked in to the firmware or checked against a server
- Logging via syslog
- Session tracking in logs
- Multiple types of user are supported: admin, trainer, user, + more
- Support tls web api calls using JSON
- Unfire relay upon rfid card removal


## Things to do
- Add status light to signify when it is checking access, in trainer mode, locked, unlocked, etc
- OTA updating of firmware
- Use wifimanager or IotWebConf to make configuration easier
- Enable active checking of access, regularly poll and check whether card still has access
- NTP time, to enforce windows of operation
- Enable heartbeat capability, to be used with a canary to alert upon device failure
- Allow remote checking of current status of relay, to see if device is in use and by whom
- Add syslog bootup time to capture when the device was rebooted
- Figure out sizing for JSON doc
- Figure out sizing of variable for url
- Convert l240-l247 in to a function
- Lockdown mode / Device disabled except for admin users
- Scheduled reboots
- API tokens
- Add ability to add users: trainer beeps card, then beeps newly trained users card, eeh-esp32-rfid then posts to API and updates user database

## Done
- Add syslogs for web stuff
- Enable remote firing of relay via a web interface or api call - a remote unlock-and-lock ability
- Make variable str in line 193 have a better name
