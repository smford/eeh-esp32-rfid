#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <WiFiUdp.h>
#include <Syslog.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ezTime.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <AsyncElegantOTA.h>
#include <SPIFFS.h>
#include "webpages.h"
#include "defaults.h"

// eztime library: https://github.com/ropg/ezTime v0.8.3
// esp async webserver library: https://github.com/me-no-dev/ESPAsyncWebServer v1.2.3
// syslog library: https://github.com/arcao/Syslog v2.0
// mfrc522 library: https://github.com/miguelbalboa/rfid  v1.4.6
// arduinojson library: https://github.com/bblanchon/ArduinoJson & https://arduinojson.org/ v6.15.2
// liquidcrystal_i2c library: https://github.com/johnrickman/LiquidCrystal_I2C
// asyncelegantota library https://github.com/ayushsharma82/AsyncElegantOTA

#define FIRMWARE_VERSION "v1.4-ota"

// configuration structure
struct Config {
  String hostname;         // hostname of device
  String device;           // device name
  String appname;          // application name
  String ssid;             // wifi ssid
  String wifipassword;     // wifi password
  int relaypin;            // relay pin number
  int ledpin;              // led pin number
  String httpuser;         // username to access web admin
  String httppassword;     // password to access web admin
  String apitoken;         // api token used to authenticate against the user management system
  String syslogserver;     // hostname or ip of the syslog server
  int syslogport;          // sylog port number
  bool inmaintenance;      // records whether the device is in maintenance mode between reboots
  String ntptimezone;      // ntp time zone to use, use the TZ database name from https://en.wikipedia.org/wiki/List_of_tz_database_time_zones
  int ntpsynctime;         // how frequently to schedule the regular syncing of ntp time
  int ntpwaitsynctime;     // upon boot, wait these many seconds to get ntp time from server
  String ntpserver;        // hostname or ip of the ntpserver
  int mfrcslaveselectpin;  // mfrc slave select pin number
  int mfrcresetpin;        // mfrc reset pin number
  int mfrccardwaittime;    // time to wait between checking whether a new card is present or not, some mfrcs balk at to high a check
  int lcdi2caddress;       // integer of the i2c address for the lcd screen, hex can be converted here: https://www.rapidtables.com/convert/number/hex-to-decimal.html
  int lcdwidth;            // width in characters for the lcd display
  int lcdheight;           // numler of lines for the lcd display
  int webserverporthttp;   // http port number for web admin
  int webserverporthttps;  // https port number for the web admin
  int webapiwaittime;      // forced delay in seconds between web api calls
  String serverurl;        // url of authentication server, e.g. "http://something.com/" or "https://192.168.20.60"
  String checkuserpage;    // check user webpage hosted on authentication server, e.g. "checkuser.php"
  String getuserpage;      // get user webpage hosted on authentication server, e.g. "getuser.php"
  String moduserpage;      // mod user webpage hosted on authentication server, e.g. "moduser.php"
  String overridecodes;    // list of rfid card numbers, seperated by commas, that have override access
};

String listFiles(bool ishtml=false);

// used for loading and saving configuration data
const char *filename = "/config.txt";
Config config;

// clean these up
const char* PARAM_INPUT_1 = "state";
const char* PARAM_INPUT_2 = "pin";

// keeps track of when the last webapicall was made to prevent hammering
unsigned long sinceLastRunTime = 0;

// used to track the status of card presented on the mfrc reader
uint8_t control = 0x00;

// possibly delete, maybe change this from global to local scope
String returnedJSON;

// currently presented card details
char* currentRFIDcard = "";
String currentRFIDUserIDStr = "";
String currentRFIDFirstNameStr = "";
String currentRFIDSurnameStr = "";
bool currentRFIDaccess = false;

// goto flags used within the two main loops (card present and card absent) to allow functions to be run
// these are sometimes neccessary to allow the lcd to update, as the asyncwebserver does not allow yeild or delay to be run within any function called by it
bool gotoToggleMaintenance = false;  // enter maintenance mode
bool gotoLogoutCurrentUser = false;  // log out current user
bool shouldReboot = false;           // schedule a reboot

// maintenance and override modes
bool inMaintenanceMode = false;
bool inOverrideMode = false;

// should i reset configuration to default?
bool resetConfigToDefault = false;

// setup MFRC522
MFRC522 mfrc522[1];

// setup udp connection
WiFiUDP udpClient;

// Syslog
Syslog syslog(udpClient, SYSLOG_PROTO_IETF);

// NTP
Timezone myTZ;
ezDebugLevel_t NTPDEBUG = INFO; // NONE, ERROR, INFO, DEBUG

// used to record when the device was booted
String bootTime;

// Setup LCD
LiquidCrystal_I2C *lcd;

// internal ESP32 temp sensor
#ifdef __cplusplus
extern "C" {
#endif
uint8_t temprature_sens_read();
#ifdef __cplusplus
}
#endif
uint8_t temprature_sens_read();

// represents the instance number of RFID action (discovery or removal)
int iteration = 0;

// initialise webserver
AsyncWebServer *server;

void setup() {
  Serial.begin(115200);

  Serial.print("Firmware: "); Serial.println(FIRMWARE_VERSION);

  Serial.println("Booting ...");

  Serial.println("Mounting SPIFFS ...");
  if (!SPIFFS.begin(true)) {
    Serial.println("ERROR: Cannot mount SPIFFS");
    //return;
  }

  if (resetConfigToDefault) {
    Serial.println("Resetting Configuration to Default");
    SPIFFS.remove(filename);
  }

  Serial.println(listFiles());

  Serial.println("Loading Configuration ...");
  loadConfiguration(filename, config);
  printConfig();

  // configure lcd using loaded configuration
  Serial.println("Configuring LCD ...");
  lcd = new LiquidCrystal_I2C(config.lcdi2caddress, config.lcdwidth, config.lcdheight);
  lcd->init();
  lcd->backlight();
  lcd->noCursor();

  lcd->print("Booting...");

  Serial.println("Configuring Syslog ...");
  // configure syslog server using loaded configuration
  syslog.server(config.syslogserver.c_str(), config.syslogport);
  syslog.deviceHostname(config.hostname.c_str());
  syslog.appName(config.appname.c_str());
  syslog.defaultPriority(LOG_INFO);

  // if no ssid set, change to AP mode and allow it to be configured
  if (config.ssid == "") {
    Serial.println("No Wifi Details Set, entering AP MODE");
    // need to implement this
  }

  Serial.println("Configuring RFID ...");
  SPI.begin(); // Init SPI bus
  mfrc522[0].PCD_Init(config.mfrcslaveselectpin, config.mfrcresetpin);
  delay(4); // delay needed to allow mfrc522 to spin up properly

  Serial.println("\nSystem Configuration:");
  Serial.println("---------------------");
  Serial.print("         Hostname: "); Serial.println(config.hostname);
  Serial.print("         App Name: "); Serial.println(config.appname);
  Serial.print("       EEH Device: "); Serial.println(config.device);
  if (config.inmaintenance) {
    Serial.println(" Maintenance Mode: true");
  } else {
    Serial.println(" Maintenance Mode: false");
  }
  Serial.print("    Syslog Server: "); Serial.print(config.syslogserver); Serial.print(":"); Serial.println(config.syslogport);
  Serial.print("Web API Wait Time: "); Serial.print(config.webapiwaittime); Serial.println(" seconds");
  Serial.print("  RFID Card Delay: "); Serial.print(config.mfrccardwaittime); Serial.println(" seconds");
  Serial.print("        Relay Pin: "); Serial.println(config.relaypin);
  Serial.print("          LED Pin: "); Serial.println(config.ledpin);
  Serial.print("    Web HTTP Port: "); Serial.println(config.webserverporthttp);
  Serial.print("   Web HTTPS Port: "); Serial.println(config.webserverporthttps);
  Serial.print("      ESP32 Flash: "); Serial.println(FIRMWARE_VERSION);
  Serial.print("   Flash Compiled: "); Serial.println(String(__DATE__) + " " + String(__TIME__));
  Serial.print("       ESP32 Temp: "); Serial.print((temprature_sens_read() - 32) / 1.8); Serial.println("C");
  Serial.print("  MFRC522 Version: "); Serial.println(getmfrcversion());
  Serial.print("       NTP Server: "); Serial.println(config.ntpserver);
  Serial.print("    NTP Time Sync: "); Serial.println(config.ntpsynctime);
  Serial.print("    NTP Time Zone: "); Serial.println(config.ntptimezone);

  lcd->clear();
  lcd->print("Connecting Wifi...");

  Serial.print("\nConnecting to Wifi: ");
  WiFi.begin(config.ssid.c_str(), config.wifipassword.c_str());
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n\nNetwork Configuration:");
  Serial.println("----------------------");
  Serial.print("         SSID: "); Serial.println(WiFi.SSID());
  Serial.print("  Wifi Status: "); Serial.println(WiFi.status());
  Serial.print("Wifi Strength: "); Serial.print(WiFi.RSSI()); Serial.println(" dBm");
  Serial.print("          MAC: "); Serial.println(WiFi.macAddress());
  Serial.print("           IP: "); Serial.println(WiFi.localIP());
  Serial.print("       Subnet: "); Serial.println(WiFi.subnetMask());
  Serial.print("      Gateway: "); Serial.println(WiFi.gatewayIP());
  Serial.print("        DNS 1: "); Serial.println(WiFi.dnsIP(0));
  Serial.print("        DNS 2: "); Serial.println(WiFi.dnsIP(1));
  Serial.print("        DNS 3: "); Serial.println(WiFi.dnsIP(2));
  Serial.println();

  Serial.println("Configuring LED ...");
  pinMode(config.ledpin, OUTPUT);
  disableLed("Automatically Disable LED upon boot");

  Serial.println("Configuring Relay ...");
  pinMode(config.relaypin, OUTPUT);
  disableRelay("Automatically Disable Relay upon boot");

  Serial.println();

  // configure time, wait config.ntpwaitsynctime seconds then progress, otherwise it can stall
  Serial.println("Configuring NTP ...");
  Serial.print("Attempting to NTP Sync time for "); Serial.print(config.ntpwaitsynctime); Serial.println(" seconds");
  lcd->clear();
  lcd->print("Syncing NTP...");
  waitForSync(config.ntpwaitsynctime);
  setInterval(config.ntpsynctime);
  setServer(config.ntpserver);
  setDebug(NTPDEBUG);
  myTZ.setLocation(config.ntptimezone);
  Serial.print(config.ntptimezone + ": "); Serial.println(printTime());

  // configure web server
  Serial.println("Configuring Webserver ...");
  server = new AsyncWebServer(config.webserverporthttp);
  configureWebServer();

  Serial.println("Configuring OTA Webserver ...");
  // configure ota web server
  AsyncElegantOTA.begin(server, config.httpuser.c_str(), config.httppassword.c_str());

  Serial.println("Starting Webserver ...");
  // startup web server
  server->begin();

  bootTime = printTime();
  Serial.print("Booted at: "); Serial.println(bootTime);
  syslog.logf("Booted");

  // since everything is now loaded, update lcd in the appropriate mode
  if (config.inmaintenance) {
    syslog.logf("Booting in to Maintenance Mode");
    lcd->clear();
    lcd->setCursor(0, 0); lcd->print(config.device);
    lcd->setCursor(0, 1); lcd->print("MAINTENANCE MODE");
    lcd->setCursor(0, 2); lcd->print("ALL ACCESS DENIED");
    lcd->setCursor(0, 3); lcd->print("");
  } else {
    lcd->clear();
    lcd->setCursor(0, 0); lcd->print(config.device);
    lcd->setCursor(0, 1); lcd->print("Present Access Card");
  }
}

String grantAccess(const char *myurl) {
  String grantaccessresult = httpGETRequest(myurl);
  return grantaccessresult;
}

String getUserDetails(const char *myurl) {
  String result = httpGETRequest(myurl);
  return result;
}

void dowebcall(const char *foundrfid) {
  Serial.print(iteration); Serial.println(" Starting dowebcall");
  unsigned long currentRunTime = millis();

  if ((currentRunTime - sinceLastRunTime) < (config.webapiwaittime * 1000)) {
    Serial.print(iteration); Serial.println(" Firing webcall too fast, ignoring");

    // probably need this
    delay((config.webapiwaittime * 1000) - (currentRunTime - sinceLastRunTime));
    //return;
  }

  if ((currentRunTime - sinceLastRunTime) > (config.webapiwaittime * 1000)) {
    if (WiFi.status() == WL_CONNECTED) {
      StaticJsonDocument<300> doc;

      String tempstring = config.serverurl + config.checkuserpage + "?device=" + config.device + "&rfid=" + String(currentRFIDcard) + "&api=" + config.apitoken;
      char checkURL[tempstring.length() + 1];
      tempstring.toCharArray(checkURL, tempstring.length() + 1);

      Serial.print(iteration); Serial.print(" dowebcall checkURL: "); Serial.println(checkURL);

      returnedJSON = httpGETRequest(checkURL);
      Serial.print(iteration); Serial.print(" ReturnedJSON:"); Serial.println(returnedJSON);

      DeserializationError error = deserializeJson(doc, returnedJSON);
      if (error) {
        Serial.print(iteration); Serial.print(F(" DeserializeJson() failed: ")); Serial.println(error.c_str());
        syslog.logf(LOG_ERR, "%d Error Decoding JSON: %s", iteration, error.c_str());
      }

      Serial.print(iteration); Serial.println(" Assigning Variables");
      const char* Timestamp = doc["Timestamp"];
      const char* RFID = doc["RFID"];
      const char* EEHDevice = doc["EEHDevice"];
      const char* UserID = doc["UserID"];
      const char* FirstName = doc["FirstName"];
      const char* Surname = doc["Surname"];
      const char* Grant = doc["Grant"];
      Serial.print(iteration); Serial.print(" Timestamp: "); Serial.println(Timestamp);
      Serial.print(iteration); Serial.print("      RFID: "); Serial.println(RFID);
      Serial.print(iteration); Serial.print(" EEHDevice: "); Serial.println(EEHDevice);
      Serial.print(iteration); Serial.print("    UserID: "); Serial.println(UserID);
      Serial.print(iteration); Serial.print("      Name: "); Serial.println(String(FirstName) + " " + String(Surname));
      Serial.print(iteration); Serial.print("     Grant: "); Serial.println(Grant);

      Serial.print(iteration); Serial.println(" Checking access");
      if (strcmp(RFID, foundrfid) == 0) {
        // presented rfid matches api returned rfid
        currentRFIDUserIDStr = doc["UserID"].as<String>();
        currentRFIDFirstNameStr = doc["FirstName"].as<String>();
        currentRFIDSurnameStr = doc["Surname"].as<String>();

        if (strcmp(Grant, "true") == 0) {
          // api says user has access

          if (strcmp(config.device.c_str(), EEHDevice) == 0) {
            // device matches api returned device

            if (!config.inmaintenance) {
              // grant access because not in maintenance mode; and rfid, grant and device match
              currentRFIDaccess = true;
              lcd->clear();
              lcd->setCursor(0, 0); lcd->print(config.device);
              lcd->setCursor(0, 1); lcd->print("ACCESS GRANTED");
              lcd->setCursor(0, 2); lcd->print("RFID: " + String(currentRFIDcard));
              lcd->setCursor(0, 3); lcd->print(currentRFIDFirstNameStr + " " + currentRFIDSurnameStr);
              enableLed(String(iteration) + " Access Granted: Enable LED: UserID:" + currentRFIDUserIDStr + " RFID:" + String(currentRFIDcard));
              enableRelay(String(iteration) + " Access Granted: Enable LED: UserID:" + currentRFIDUserIDStr + " RFID:" + String(currentRFIDcard));
            } else {
              // in maintenance mode, show message, deny access
              Serial.println(String(iteration) + " In maintenance mode, ignoring non-Override access");
              syslog.log(String(iteration) + "In maintenance mode, ignoring non-Override access");
              lcdPrint("ACCESS DENIED", "IN MAINTENANCE MODE", "Only Override User", "Allowed");
            }

          } else {
            // deny access, device does not match api returned device name
            disableLed(String(iteration) + " Device Mismatch: Disable LED: Expected:" + config.device + " Got:" + EEHDevice);
            disableRelay(String(iteration) + " Device Mismatch: Disable Relay: Expected:" + config.device + " Got:" + EEHDevice);
          }
        } else {
          // deny access, api says user does not have access
          lcd->clear();
          lcd->setCursor(0, 0); lcd->print(config.device);
          lcd->setCursor(0, 1); lcd->print("ACCESS DENIED");
          lcd->setCursor(0, 2); lcd->print("RFID: " + String(currentRFIDcard));
          lcd->setCursor(0, 3); lcd->print(currentRFIDFirstNameStr + " " + currentRFIDSurnameStr);
          disableLed(String(iteration) + " Access Denied: Disable LED: " + foundrfid);
          disableRelay(String(iteration) + " Access Denied: Disable Relay: " + foundrfid);
        }
      } else {
        // deny access, rfid matches does not match api returned rfid
        disableLed(String(iteration) + " RFID Mismatch: Disable LED: Expected:" + foundrfid + " Got:" + RFID);
        disableRelay(String(iteration) + " RFID Mismatch: Disable Relay: Expected:" + foundrfid + " Got:" + RFID);
      }

      sinceLastRunTime = millis();
    } else {
      Serial.println("WiFi Disconnected");
    }
  } else {
    Serial.print(iteration); Serial.println(" Not doing webcall, firing too fast");
  }
}

void loop() {
  loopBreakout("Card Absent");

  if (!mfrc522[0].PICC_IsNewCardPresent()) {
    // no new card found, re-loop
    return;
  }

  if (!mfrc522[0].PICC_ReadCardSerial()) {
    // empty ReadCardSerial means no real card found, re-loop
    return;
  }

  // if we have reached here it means a card is present
  char newcard[32] = "";
  array_to_string(mfrc522[0].uid.uidByte, 4, newcard);
  iteration++;
  Serial.print(iteration); Serial.print(" RFID Found: "); Serial.println(newcard);

  while (true) {
    control = 0;
    for (int i = 0; i < 3; i++) {
      if (!mfrc522[0].PICC_IsNewCardPresent()) {
        if (mfrc522[0].PICC_ReadCardSerial()) {
          //Serial.print('a');
          control |= 0x16;
        }
        if (mfrc522[0].PICC_ReadCardSerial()) {
          //Serial.print('b');
          control |= 0x16;
        }
        //Serial.print('c');
        control += 0x1;
      }
      //Serial.print('d');
      control += 0x4;
    }

    if (control == 13 || control == 14) {
      // a new card has been found
      if (strcmp(currentRFIDcard, newcard) != 0) {
        Serial.print(iteration); Serial.print(" New Card Found: "); Serial.println(newcard);
        syslog.logf("%d New Card Found: %s", iteration, newcard);
        currentRFIDcard = newcard;

        lcd->clear();
        lcd->setCursor(0, 0); lcd->print("Checking Access...");

        if (checkOverride(currentRFIDcard)) {
          // access override detected
          enableLed(String(iteration) + " Access Override Detected: Enable LED: " + String(currentRFIDcard));
          enableRelay(String(iteration) + " Access Override Detected: Enable Relay: " + String(currentRFIDcard));
          currentRFIDFirstNameStr = "Override";
          currentRFIDSurnameStr = "Mode";
          currentRFIDUserIDStr = "0";
          currentRFIDaccess = true;
          lcd->clear();
          lcd->setCursor(0, 0); lcd->print(config.device);
          lcd->setCursor(0, 1); lcd->print("ACCESS GRANTED");
          lcd->setCursor(0, 2); lcd->print("RFID: " + String(currentRFIDcard));
          lcd->setCursor(0, 3); lcd->print(currentRFIDFirstNameStr + " " + currentRFIDSurnameStr);
        } else {
          // normal user, do webcall
          dowebcall(newcard);
        }

      } else {
        // this "else" runs as regularly as a loop when a card is present
        //Serial.println("same card, not checking again");

        loopBreakout("Card Present");

      }
    } else {
      break;
    }
  }

  mfrc522[0].PICC_HaltA();
  mfrc522[0].PCD_StopCrypto1();
  if (!config.inmaintenance) {
    // not in maintenance mode, update LCD
    lcd->clear();
    lcd->setCursor(0, 0); lcd->print(config.device);
    lcd->setCursor(0, 1); lcd->print("Present Access Card");
  } else {
    // should be in maintenance mode, update LCD
    lcd->clear();
    lcd->setCursor(0, 0); lcd->print(config.device);
    lcd->setCursor(0, 1); lcd->print("MAINTENANCE MODE");
    lcd->setCursor(0, 2); lcd->print("ALL ACCESS DENIED");
    lcd->setCursor(0, 3); lcd->print("");
  }
  disableLed(String(iteration) + " " + "Access Revoked: Card Removed: Disable LED: " + String(newcard));
  disableRelay(String(iteration) + " " + "Access Revoked: Card Removed: Disable Relay: " + String(newcard));
  currentRFIDcard = "";
  currentRFIDaccess = false;
  currentRFIDUserIDStr = "";
  currentRFIDFirstNameStr = "";
  currentRFIDSurnameStr = "";
  inOverrideMode = false;
  delay((config.mfrccardwaittime * 1000));

  // Dump debug info about the card; PICC_HaltA() is automatically called
  //Serial.println("Starting picc_dumptoserial");
  //mfrc522[0].PICC_DumpToSerial(&(mfrc522[0].uid));
}

void enableRelay(String message) {
  digitalWrite(config.relaypin, LOW);
  Serial.println(message);
  syslog.log(message);
}

void disableRelay(String message) {
  digitalWrite(config.relaypin, HIGH);
  Serial.println(message);
  syslog.log(message);
}

void disableLed(String message) {
  digitalWrite(config.ledpin, LOW);
  Serial.println(message);
  syslog.log(message);
}

void enableLed(String message) {
  digitalWrite(config.ledpin, HIGH);
  Serial.println(message);
  syslog.log(message);
}

void rebootESP(String message) {
  Serial.print(iteration); Serial.print(" Rebooting ESP32: "); Serial.println(message);
  syslog.logf("%d Rebooting ESP32:%s", iteration, message);
  // wait 10 seconds to allow syslog to be sent
  delay(10000);
  ESP.restart();
}

String getFullStatus() {
  StaticJsonDocument<2200> fullStatusDoc;
  fullStatusDoc["Timestamp"] = printTime();
  fullStatusDoc["Hostname"] = config.hostname;
  fullStatusDoc["BootTime"] = bootTime;
  fullStatusDoc["AppName"] = config.appname;
  fullStatusDoc["EEHDevice"] = config.device;
  fullStatusDoc["OverrideUsers"] = config.overridecodes;
  fullStatusDoc["SyslogServer"] = config.syslogserver;
  fullStatusDoc["SyslogPort"] = config.syslogport;
  fullStatusDoc["ServerURL"] = config.serverurl;
  fullStatusDoc["CheckUserPage"] = config.checkuserpage;
  fullStatusDoc["GetUserPage"] = config.getuserpage;
  fullStatusDoc["ModUserPage"] = config.moduserpage;
  fullStatusDoc["Firmware"] = FIRMWARE_VERSION;
  fullStatusDoc["Temp"] = String((temprature_sens_read() - 32) / 1.8) + "C";
  fullStatusDoc["CompileTime"] = String(__DATE__) + " " + String(__TIME__);
  fullStatusDoc["ConfigFile"] = String(filename);
  fullStatusDoc["MFRC522SlaveSelect"] = config.mfrcslaveselectpin;
  fullStatusDoc["MFRC522ResetPin"] = config.mfrcresetpin;
  fullStatusDoc["MFRC522Firmware"] = getmfrcversion();
  fullStatusDoc["MFRC522CardWaitTime"] = config.mfrccardwaittime;
  fullStatusDoc["NTPServer"] = config.ntpserver;
  fullStatusDoc["NTPSyncTime"] = config.ntpsynctime;
  fullStatusDoc["NTPTimeZone"] = config.ntptimezone;
  fullStatusDoc["NTPWaitSynctime"] = config.ntpwaitsynctime;
  fullStatusDoc["NTPSyncStatus"] = getTimeStatus();
  fullStatusDoc["WebAPIWaitTime"] = config.webapiwaittime;
  fullStatusDoc["ShouldReboot"] = shouldReboot;
  fullStatusDoc["WebServerPortHTTP"] = config.webserverporthttp;
  fullStatusDoc["WebServerPortHTTPS"] = config.webserverporthttps;
  fullStatusDoc["SSID"] = WiFi.SSID();
  fullStatusDoc["WifiStatus"] = WiFi.status();
  fullStatusDoc["WifiSignalStrength"] = WiFi.RSSI();
  fullStatusDoc["MacAddress"] = WiFi.macAddress();
  fullStatusDoc["IPAddress"] = WiFi.localIP().toString();
  fullStatusDoc["Subnet"] = WiFi.subnetMask().toString();
  fullStatusDoc["Gateway"] = WiFi.gatewayIP().toString();
  fullStatusDoc["DNS1"] = WiFi.dnsIP(0).toString();
  fullStatusDoc["DNS2"] = WiFi.dnsIP(1).toString();
  fullStatusDoc["DNS3"] = WiFi.dnsIP(2).toString();
  fullStatusDoc["LCDI2CAddress"] = "0x" + String(config.lcdi2caddress, HEX);
  fullStatusDoc["LCDWidth"] = config.lcdwidth;
  fullStatusDoc["LCDHeight"] = config.lcdheight;

  fullStatusDoc["RelayPin"] = config.relaypin;
  if (digitalRead(config.relaypin)) {    // note this is the opposite of what is expected due to the way the relay works
    fullStatusDoc["RelayPinStatus"] = "off";
  } else {
    fullStatusDoc["RelayPinStatus"] = "on";
  }

  fullStatusDoc["LEDPin"] = config.ledpin;
  if (digitalRead(config.ledpin)) {
    fullStatusDoc["LEDPinStatus"] = "on";
  } else {
    fullStatusDoc["LEDPinStatus"] = "off";
  }

  if (strcmp(currentRFIDcard, "") == 0) {
    fullStatusDoc["CurrentRFID"] = "NONE";
  } else {
    fullStatusDoc["CurrentRFID"] = currentRFIDcard;
  }

  if (currentRFIDUserIDStr == "") {
    fullStatusDoc["CurrentRFIDUserID"] = "NONE";
  } else {
    fullStatusDoc["CurrentRFIDUserID"] = currentRFIDUserIDStr;
  }

  if (currentRFIDFirstNameStr == "") {
    fullStatusDoc["CurrentRFIDFirstName"] = "NONE";
  } else {
    fullStatusDoc["CurrentRFIDFirstName"] = currentRFIDFirstNameStr;
  }

  if (currentRFIDSurnameStr == "") {
    fullStatusDoc["CurrentRFIDSurname"] = "NONE";
  } else {
    fullStatusDoc["CurrentRFIDSurname"] = currentRFIDSurnameStr;
  }

  fullStatusDoc["CurrentRFIDAccess"] = currentRFIDaccess;

  if (config.inmaintenance) {
    fullStatusDoc["inMaintenanceMode"] = "true";
  } else {
    fullStatusDoc["inMaintenanceMode"] = "false";
  }

  if (inOverrideMode) {
    fullStatusDoc["inOverrideMode"] = "true";
  } else {
    fullStatusDoc["inOverrideMode"] = "false";
  }

  String fullStatus = "";
  serializeJson(fullStatusDoc, fullStatus);

  return fullStatus;
}

String getStatus() {
  Serial.println("getting un-authed status");
  StaticJsonDocument<200> shortStatusDoc;
  shortStatusDoc["Timestamp"] = printTime();
  shortStatusDoc["Hostname"] = config.hostname;

  // note this is the opposite of what is expected due to the way the relay works
  if (digitalRead(config.relaypin)) {
    shortStatusDoc[config.device] = "off";
  } else {
    shortStatusDoc[config.device] = "on";
  }

  String shortStatus = "";

  serializeJson(shortStatusDoc, shortStatus);
  return shortStatus;
}

String httpGETRequest(const char* serverURL) {
  HTTPClient http;

  http.begin(serverURL);

  int httpResponseCode = http.GET();

  String payload = "{}";

  if (httpResponseCode > 0) {
    Serial.print(iteration); Serial.print(" HTTP Response code: "); Serial.println(httpResponseCode);
    syslog.logf("%d HTTP Response Code:%d", iteration, httpResponseCode);
    payload = http.getString();
  } else {
    Serial.print(iteration); Serial.print(" ERROR: HTTP Response Code: "); Serial.println(httpResponseCode);
    syslog.logf(LOG_ERR, "%d ERROR: HTTP Response Code:%s", iteration, httpResponseCode);
  }
  // Free resources
  http.end();

  return payload;
}

// https://forum.arduino.cc/index.php?topic=639321.0
void array_to_string(byte array[], unsigned int len, char buffer[])
{
  for (unsigned int i = 0; i < len; i++)
  {
    byte nib1 = (array[i] >> 4) & 0x0F;
    byte nib2 = (array[i] >> 0) & 0x0F;
    buffer[i * 2 + 0] = nib1  < 0xA ? '0' + nib1  : 'A' + nib1  - 0xA;
    buffer[i * 2 + 1] = nib2  < 0xA ? '0' + nib2  : 'A' + nib2  - 0xA;
  }
  buffer[len * 2] = '\0';
}

String getmfrcversion() {
  String mfrcver;
  switch (mfrc522[0].PCD_ReadRegister(mfrc522[0].VersionReg)) {
    case 0x91:
      mfrcver = "v1.0";
      break;
    case 0x92:
      mfrcver = "v2.0";
      break;
    default:
      mfrcver = "0x" + String(mfrc522[0].PCD_ReadRegister(mfrc522[0].VersionReg), HEX) + ":counterfeit";
  }
  return mfrcver;
}

String printTime() {
  return myTZ.dateTime();
}

String getTimeStatus() {
  String myTimeStatus;
  switch (timeStatus()) {
    case 0:
      myTimeStatus = "Time Not Set";
      break;
    case 1:
      myTimeStatus = "Time Needs Syncing";
      break;
    case 2:
      myTimeStatus = "Time Set";
      break;
    default:
      myTimeStatus = "Unknown";
  }
  return myTimeStatus;
}

void lcdPrint(String line1, String line2, String line3, String line4) {
  lcd->clear();
  lcd->setCursor(0, 0); lcd->print(line1);
  lcd->setCursor(0, 1); lcd->print(line2);
  lcd->setCursor(0, 2); lcd->print(line3);
  lcd->setCursor(0, 3); lcd->print(line4);
}

void toggleMaintenance() {
  // only run toggle once
  gotoToggleMaintenance = false;

  // toggle maintenance mode
  config.inmaintenance = !config.inmaintenance;

  Serial.println(F("Storing inMaintenance configuration..."));
  saveConfiguration(filename, config);

  if (config.inmaintenance) {
    syslog.logf("Enabling Maintenance Mode");
    lcd->clear();
    lcd->setCursor(0, 0); lcd->print(config.device);
    lcd->setCursor(0, 1); lcd->print("MAINTENANCE MODE");
    lcd->setCursor(0, 2); lcd->print("ALL ACCESS DENIED");
    lcd->setCursor(0, 3); lcd->print("");
  } else {
    syslog.logf("Disabling Maintenance Mode");
    lcd->clear();
    lcd->setCursor(0, 0); lcd->print(config.device);
    lcd->setCursor(0, 1); lcd->print("Present Access Card");
  }
}

void logoutCurrentUser() {
  gotoLogoutCurrentUser = false;
  String logmessage = String(iteration) + " " + "Web Admin User Logout Initiated: RFID:" + String(currentRFIDcard) + " UserID:" + currentRFIDUserIDStr;
  Serial.println(logmessage);
  syslog.log(logmessage);

  currentRFIDaccess = false;
  disableLed(String(iteration) + " " + "Web Admin User Logout Initiated: Disable LED: RFID:" + String(currentRFIDcard) + " UserID:" + currentRFIDUserIDStr);
  disableRelay(String(iteration) + " " + "Web Admin User Logout Initiated: Disable Relay: RFID:" + String(currentRFIDcard) + " UserID:" + currentRFIDUserIDStr);

  lcd->clear();
  lcd->setCursor(0, 0); lcd->print(config.device);
  lcd->setCursor(0, 1); lcd->print("LOGGED OUT");
  lcd->setCursor(0, 2); lcd->print("RFID: " + String(currentRFIDcard));
  lcd->setCursor(0, 3); lcd->print(currentRFIDFirstNameStr + " " + currentRFIDSurnameStr);
}

// checks whether rfid is in the override list
bool checkOverride(const char *foundrfid) {
  if (config.overridecodes.indexOf(foundrfid) >= 0) {
    Serial.print(iteration); Serial.print(" "); Serial.print(foundrfid); Serial.println(" found in override list");
    return true;
  } else {
    Serial.print(iteration); Serial.print(" "); Serial.print(foundrfid); Serial.println(" not in override list");
    return false;
  }
}

// list all of the files, if ishtml=true, return html rather than simple text
String listFiles(bool ishtml) {
  String returnText = "";
  Serial.println("Listing files stored on SPIFFS");
  File root = SPIFFS.open("/");
  File foundfile = root.openNextFile();
  if (ishtml) { returnText += "<table><tr><th align='left'>Name</th><th align='left'>Size</th><th></th><th></th><th></th><th></th</tr>"; }
  while (foundfile) {
    if (ishtml) {
      returnText += "<tr align='left'><td>" + String(foundfile.name()) + "</td><td>" + humanReadableSize(foundfile.size()) + "</td><td><a href='/file?name=" + String(foundfile.name()) + "&action=download'>Download</a></td><td><a href='/file?name=" + String(foundfile.name()) + "&action=delete'>Delete</a></td>";
      returnText += "<td><button onclick=\"downloadDeleteButton(\'" + String(foundfile.name()) + "\', \'download\')\">Download</button>";
      returnText += "<td><button onclick=\"downloadDeleteButton(\'" + String(foundfile.name()) + "\', \'delete\')\">Delete</button></tr>";
    } else {
      returnText += "File: " + String(foundfile.name()) + "\n";
    }
    foundfile = root.openNextFile();
  }
  if (ishtml) { returnText += "</table>"; }
  return returnText;
}

// Make size of files human readable
// source: https://github.com/CelliesProjects/minimalUploadAuthESP32
String humanReadableSize(const size_t bytes) {
  if (bytes < 1024) return String(bytes) + " B";
  else if (bytes < (1024 * 1024)) return String(bytes / 1024.0) + " KB";
  else if (bytes < (1024 * 1024 * 1024)) return String(bytes / 1024.0 / 1024.0) + " MB";
  else return String(bytes / 1024.0 / 1024.0 / 1024.0) + " GB";
}
