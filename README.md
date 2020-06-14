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
- Add requester ip details to web admin logs
- Add unknown card found to logs
- Add status light to signify when it is checking access, in trainer mode, locked, unlocked, etc
- OTA updating of firmware
- Make json output of boss's be a struct
- After remote reboot, change the url to be index rather than /reboot (can cause looping reboots)
- Use wifimanager or IotWebConf to make configuration easier
- Add fullstatus link to web admin
- Enable active checking of access, regularly poll and check whether card still has access
- Enforce windows of operation
- Figure out sizing for JSON doc
- Figure out sizing of variable for url
- Log off a user via the web admin
- Ban a user via the web admin
- Convert l240-l247 in to a function
- Lockdown mode / Device disabled except for admin users
- Scheduled reboots
- Regularly send "in use data" back to somewhere
- Add a sensor to detect whether the laser is actually firing and ship somewhere
- API tokens
- Add ability to add users: trainer beeps card, then beeps newly trained users card, eeh-esp32-rfid then posts to API and updates user database
- Force ntp sync via api and web admin
- If bootTime = Thursday, 01-Jan-1970 00:00:16 UTC, refresh it for the most current time
- Upon boot, pull time from server, then start using utp
- If ntp sync fails 10 times, force a reboot

## Done
- Add syslogs for web stuff
- Enable NTP
- Fix reboot function
- Enable remote firing of relay via a web interface or api call - a remote unlock-and-lock ability
- Remote reboot command via web interface and api
- Add syslog bootup time to capture when the device was rebooted
- Enable heartbeat capability, to be used with a canary to alert upon device failure /health
- Make variable str in line 193 have a better name
- Enable status capability, to see what the current status of the system is (whos logged in, whether leds or relay on, etc) /status
- Allow remote checking of current status of relay, to see if device is in use and by whom
