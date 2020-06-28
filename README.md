# eeh-esp32-rfid

A simple ESP32 Based RFID Access Control System for tools

## Components
- ESP32 Dev Board
- MFRC522 RFID Board
- Generic Relay Board
- I2C 2004 LCD

## Features
- Web Admin interface
- Short and Full Status information
- Webapi: reboot, status check, time check, full status check, refresh ntp, + more
- Upon RFID being presented eeh-esp32-rfid will check whether that card has been granted access to the device and fire the relay
- Users can be baked in to the firmware (super boss access, incase of network connectivity problems) or checked against a server (normal user access)
- Logging via syslog
- Session tracking in logs
- NTP Time synchronisation
- Multiple types of user are supported: admin, trainer, user, + more
- Support tls web api calls using JSON
- Unfire relay upon rfid card removal

## Pin Out


## Electrical Stuff to do
- the relay fires when writing the firmware
- power relay via 5v to optically isolate from the ESP32 - toggle jumper
- add reboot and flash buttons
- add current transformer/detector device to monitor usage
- figure out how to get the 3v from the esp32 working safely with the 5v on the lcd

## Coding Cleanup
- change from LiquidCrystal_I2C.h to LiquidCrystalIO.h

## Things to do
- Fix the default settings
- Upload settings file
- Download settings file
- If no settings file, set default, and go in to programming mode
- Cleanup the OTA webpage
- Make maintenance mode persist between reboots
- Add status light to signify when it is checking access, in trainer mode, locked, unlocked, etc
- Make json output of boss's be a struct
- Enable active checking of access, regularly poll and check whether card still has access
- Make defining of serverURL and its handling less gross, and add output to fullstatus
- Figure out sizing for JSON doc
- Figure out sizing of variable for url
- Convert to a function: Serial.print(iteration); Serial.println(" Checking access");
- Store configuration to spiffs
- Change haveaccess to being: &access=grant or &access=revoke
- Regularly pull down user last from server and store in spifs
- Regularly send "in use data" back to somewhere
- Add a sensor to detect whether the laser is actually firing and ship somewhere
- Convert "if (!mfrc522.PICC_IsNewCardPresent()) {" to a function
- Change button and slider code generation to sit within processor function
- API token implementation for laptop to esp32
- If no card present, grant and revoke access buttons are disabled, but when a card is presented and card details are refreshed, if a card is found the buttons should be enabled
- Standardise time format: https://github.com/ropg/ezTime#built-in-date-and-time-formats
- Upon boot, pull time from server, then start using utp
- If ntp sync fails 10 times, force a reboot
- change grantUser() and getUserDetails() in to a generic function
- Implement lcdPrint(l1, l2, l3, l4) and make adaptable for varying sizes of display (autoscroll perhaps)
- Figure out a way to nicely handle to the two loops - loop1=card present  loop2=no card present

## Bugs
- Bad/odd http response codes can cause a crash - often seen when having trouble doing web calls
- NTP sync sometimes doesnt change time to correct zone, likely problem querying eztime server
- If bootTime = Thursday, 01-Jan-1970 00:00:16 UTC, refresh it for the most current time

## Nice to have
- Send stats back to influxdb
- Allow all settings to be updated via web admin
- Allow flashing from default firmware, and then configuration via web admin
- Use wifimanager or IotWebConf to make configuration easier
- Enforce windows of operation
- Display available wifi networks: https://github.com/me-no-dev/ESPAsyncWebServer#scanning-for-available-wifi-networks
- Add a debugging mode
- Scheduled reboots
- When a card is removed or presented, auto refresh the web admin page
- Sort out logging levels info or info+error

## Abandoned
- Add ability to add users: trainer beeps card, then beeps newly trained users card, eeh-esp32-rfid then posts to API and updates user database
- When revoking access, disable led and relay, access in web admin, and in full status.  To do the same effect, revoke access then log out user.

## Done
- Log off a user via the web admin
- Added wifi signal strength to full status
- OTA updating of firmware
- Convert URL logs to:  String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
- Reboot button doesnt always work
- Clean up html
- Set html language
- Fix encoding of html doc on all pages
- Return 404 on bad urls
- Maintenance mode = Device disabled except for admin users
- Update current? details when in Override Mode to allow correct display of information on web admin
- API token implementation for accessing moduser.php
- Display username on main web admin
- Add output to LCD
- Add I2C LCD
- Display full user details from button on web admin
- Clean up moduser.php result when displayed on web admin after granting or revoking access
- Add syslogs for web stuff
- Enable NTP
- Ban/revoke a user via the web admin
- Add ability to add users: trainer logs on to web interface and can then grant access to currently presented card
- Clean up logging and debug output around granting and revoking access via web admin
- Add unknown card found to logs
- Force ntp sync via api and web admin
- Fix reboot function
- Enable remote firing of relay via a web interface or api call - a remote unlock-and-lock ability
- Add requester ip details to web admin logs
- Added NTP Sync Status to fullstatus
- Remote reboot command via web interface and api
- Add syslog bootup time to capture when the device was rebooted
- Enable heartbeat capability, to be used with a canary to alert upon device failure /health
- After remote reboot, change the url to be index rather than /reboot (can cause looping reboots)
- Make variable str in line 193 have a better name
- Enable status capability, to see what the current status of the system is (whos logged in, whether leds or relay on, etc) /status
- Add fullstatus link to web admin
- Standardise logging style and mechanism
- Added internal ESP32 temp to full status
- Allow remote checking of current status of relay, to see if device is in use and by whom
