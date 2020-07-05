# eeh-esp32-rfid

A simple ESP32 Based RFID Access Control System for tools or door.

## Components
- ESP32 Dev Board
- MFRC522 RFID Board
- Generic Relay Board
- I2C 2004 LCD

## Features
- Authenticated Web Admin interface
- Users can be from an internally baked list (in case of network connectivity problems), or checked against an external user managment system via api queries
- Multiple types of user are supported: admin, trainer, user, + more
- Forced user presence, the relay will unfired when a card is removed
- User access management possible via Web Admin, useful after training a user to give immediate access
- User session tracking
- Short and Full Status information, for use with monitoring systems supporting json
- OTA Updating of device
- Configuration stored as code
- Can track exact device usage, for example, how long a laser was actually firing for a particular user
- Full remote and automated management possible by use of the api
- Informative LCD Display
- Maintenance Mode where only specific users can get override access
- Logging via syslog
- Metrics collected in influxdb/telegraf: system temp, access granted, wifi signal strength, and whether actual device being used (still to come)
- NTP Time synchronisation
- Support tls web api calls using json

## Pin Out


## API Calls Supported

| Done | API Endpoint | Description | Auth:API | Auth:User/Pass | Parameters | Example |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| yes | /backlightoff | Turns LCD Backlight Off | yes | yes | - | /backlightoff?api=xxx |
| yes | /backlighton | Turns LCD Backlight On | yes | yes | - | /backlighton?api=xxx |
| no | /file | Delete or download a file from spiffs | | | | |
| yes | /fullstatus | Display full running configuration and data | yes | yes | - | /fullstatus?api=xxx |
| yes | /getuser | Get currently presented cards user details | yes | yes | - | /getuser?api=xxx |
| no | /grant | Grant/Revoke a users access | | | | |
| yes | /health | Simple health response | - | - | - | /health |
| yes | /listfiles | List files on spiffs | yes | yes | - | /listfiles?api=xxx |
| yes | /logout-current-user | Log out current user of device | yes | yes | - | /logout-current-user?api=xxx |
| yes | /maintenance | Enable or disable maintenance mode | yes | yes | state=enable state=disable | /maintenance?api=xxx&state=enable |
| yes | /ntprefresh | Initiate a ntp time refresh | yes | yes | - | /ntprefresh?api=xxx |
| yes | /reboot | Initiate a reboot | yes | yes | - | /reboot?api=xxx |
| yes | /scanwifi | Display available wifi networks | yes | yes | - | /scanwifi?api=xxx |
| yes | /status | Simple status page | - | - | - | /status |
| yes | /time | Display current time on device | - | - | - | /time |
| yes | /toggle | Turn LED or Relay on/off | yes | yes | pin=led&state=on  pin=relay&state=off| /toggle?api=xxx&pin=led&state=off |

## Electrical Stuff to do
- the relay fires when writing the firmware
- power relay via 5v to optically isolate from the ESP32 - toggle jumper
- add reboot and flash buttons
- add current transformer/detector device to monitor usage
- figure out how to get the 3v from the esp32 working safely with the 5v on the lcd

## Coding Cleanup
- Convert MFRC522 mfrc522[1]; to being MFRC522 *mfrc522; and mfrc522 = new MFRC522(config.mfrcslaveselectpin, config.mfrcresetpin) style
- Convert to a function: Serial.print(iteration); Serial.println(" Checking access");
- Change button and slider code generation to sit within processor function
- change grantUser() and getUserDetails() in to a generic function
- Adjust the timeout on setTimeout(function(){, 5000 might be too generous and it makes web interface seen a bit unresponsive.  WebSockets will superceed this if implemented.
- Make while (true) loop better and more logical, while (true) loop + break is for when an already existing card is still present
- Creat a generic shipMetric(String metricname, String metricvalue) function rather than individual ship* functions
- Make the api call responses cleaner, maybe json or plain text, some are currently html
- Change from using Strings library to standard strings
- Make parsing of json data presented in to the web interface safer: https://www.w3schools.com/js/js_json_parse.asp
- Merge /backlighton and /backlightoff into /backlight?state=on/off

## Things to do
- If wifi is disconnected, update LCD to alert user and put in to maintenance mode
- Make syslog optional
- Change web admin password to be a hash
- Change the api token to be a hash
- Change password for OTA webpage to be a hash
- Convert Web Admin to using websockets
- If no settings file, set default, and go in to programming mode
- Add status light to signify when it is checking access, in trainer mode, locked, unlocked, etc
- Enable active checking of access, regularly poll and check whether card still has access
- Figure out sizing for JSON doc
- Figure out sizing of variable for url
- Regularly pull down user list from server and store in spiffs
- Add a sensor to detect whether the device is actually firing and ship somewhere
- API token implementation for laptop to esp32
- If no card present, grant and revoke access buttons are disabled, but when a card is presented and card details are refreshed, if a card is found the buttons should be enabled
- Standardise time format: https://github.com/ropg/ezTime#built-in-date-and-time-formats
- Upon boot, pull time from server, then start using ntp
- If ntp sync fails 10 times, force a reboot to address bug with ESP32s

## Bugs
- Bad/odd http response codes can cause a crash - often seen when having trouble doing web calls, do a check after httpGETRequest
- NTP sync sometimes doesnt change time to correct zone, likely problem querying eztime server
- If bootTime = Thursday, 01-Jan-1970 00:00:16 UTC, refresh it for the most current time

## Nice to have
- Convert from using String to standard string library to keep memory clean and device more stable
- Change from LiquidCrystal_I2C.h to LiquidCrystalIO.h
- Rather than lcdi2cadderss being an int, convert to a string ("0x27" for example) to allow easier configuration
- make lcdPrint() adaptable for varying sizes of display (autoscroll perhaps)
- Cleanup the OTA webpage
- Allow all settings to be updated via web admin
- Allow flashing from default firmware, and then configuration via web admin
- Use wifimanager or IotWebConf to make configuration easier
- Enforce windows of operation
- Add a debugging mode
- Scheduled reboots
- When a card is removed or presented, auto refresh the web admin page
- Sort out logging levels info or info+error
- Enable https on device
- Web Admin: Scan i2c devices and print out

## Abandoned
- Add ability to add users: trainer beeps card, then beeps newly trained users card, eeh-esp32-rfid then posts to API and updates user database
- When revoking access, disable led and relay, access in web admin, and in full status.  To do the same effect, revoke access then log out user.
- Make override codes be stored as a nested array within the config struct and in json.  Hard to arrange, instead used simple csv method

## Done
- Clean up Authentication success or failed messages
- Display available wifi networks: https://github.com/me-no-dev/ESPAsyncWebServer#scanning-for-available-wifi-networks
- Make function to print web admin args for debugging
- Rename influxdb* variable names to telegraf because that is more accurate
- Make web admin web page delay time configurable
- After OTA update, reboot
- Change returnedJSON from global to local scope
- Change haveaccess to being: &access=grant or &access=revoke
- Cleanup config.webapiwaittime
- Added Wifi signal strength metric logging
- Convert "if (!mfrc522.PICC_IsNewCardPresent()) {" to a function
- Implement lcdPrint(l1, l2, l3, l4)
- Auth protect /backlighton and /backlightoff
- Mask out secrets from all output
- Cleanup PARAM_INPUT_1 and PARAM_INPUT_2
- Make shipping metrics optional
- Regularly send "in use data" back to somewhere
- Send stats back to influxdb
- Upload with error messages
- Clean up download and delete links
- Upload with progress bar
- Upload any file
- Upload settings file
- Web Admin: List files on spiffs
- Web Admin: View/Download and delete files on spiffs
- Download settings file
- Fix the default settings
- Store configuration to spiffs
- Make defining of serverURL and its handling less gross, and add output to fullstatus
- Move parts of the code to seperate files
- Figure out a way to nicely handle to the two loops - loop1=card present  loop2=no card present
- Make maintenance mode persist between reboots
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
