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
#include "secrets.h"

// eztime library: https://github.com/ropg/ezTime v0.8.3
// esp async webserver library: https://github.com/me-no-dev/ESPAsyncWebServer v1.2.3
// syslog library: https://github.com/arcao/Syslog v2.0
// mfrc522 library: https://github.com/miguelbalboa/rfid  v1.4.6
// arduinojson library: https://github.com/bblanchon/ArduinoJson & https://arduinojson.org/ v6.15.2
// liquidcrystal_i2c library: https://github.com/johnrickman/LiquidCrystal_I2C

#define WEB_SERVER_PORT 80
#define FIRMWARE_VERSION "v1.1-ota"
#define ADMIN_SERVER "http://192.168.10.21:8180/"

// lcd configuration
const int LCD_I2C = 0x27;
const int LCD_WIDTH = 20;
const int LCD_HEIGHT = 4;

//const char* serverURL1 = "https://mock-rfid-system.herokuapp.com/check?rfid=";
//const char* serverURL1 = "http://192.168.10.21:56000/check?rfid=";
const char* serverURL1 = "http://192.168.10.21:8180/check.php?rfid=";
const char* serverURL2 = "&device=laser&api=abcde";

// configuration structure
struct Config {
  String hostname;
  String device;
  String appname;
  String ssid;
  String wifipassword;
  int relaypin;
  int ledpin;
  String httpuser;
  String httppassword;
  String overridecodes;
  String apitoken;
  String syslogserver;
  int syslogport;
  bool inmaintenance;
  String ntptimezone;
  int ntpsynctime;
  int ntpwaitsynctime;
  String ntpserver;
};

// use for loading and saving configuration data
const char *filename = "/config5.txt";
Config config;

// mfrc522 is in spi mode
const int RST_PIN = 33; // Reset pin
const int SS_PIN = 32; // Slave select pin

const char* PARAM_INPUT_1 = "state";
const char* PARAM_INPUT_2 = "pin";

char *accessOverrideCodes[] = {"90379632", "boss2aaa", "boss3bbb"};

char* serverURL;
unsigned long sinceLastRunTime = 0;
unsigned long waitTime = 2; // in seconds
unsigned long checkCardTime = 5; // in secondsE
String returnedJSON;
uint8_t control = 0x00;

char* currentRFIDcard = "";
String currentRFIDUserIDStr = "";
String currentRFIDFirstNameStr = "";
String currentRFIDSurnameStr = "";
bool currentRFIDaccess = false;

// should we reboot the server?
bool shouldReboot = false;

// flags used within main loop to allow functions to be run
// these are sometimes neccessary to allow the lcd to update, as the asyncwebserver does
// not allow yeild or delay to be run within a sub function which the lcd library used
bool gotoToggleMaintenance = false;
bool gotoLogoutCurrentUser = false;

// maintenance and override modes
bool inMaintenanceMode = false;
bool inOverrideMode = false;

// MFRC522
MFRC522 mfrc522(SS_PIN, RST_PIN); // Create MFRC522 instance

// setup udp connection
WiFiUDP udpClient;

// Syslog
Syslog syslog(udpClient, SYSLOG_PROTO_IETF);

// NTP
Timezone myTZ;
String bootTime;
ezDebugLevel_t NTPDEBUG = INFO; // NONE, ERROR, INFO, DEBUG

// Setup LCD
LiquidCrystal_I2C lcd(LCD_I2C, LCD_WIDTH, LCD_HEIGHT);

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
AsyncWebServer server(WEB_SERVER_PORT);

// index.html
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html lang="en">
<head>
  <title>%EEH_HOSTNAME%</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta charset="UTF-8">
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    h2 {font-size: 2.6rem;}
    h3 {color: white; font-weight: normal; background-color: red;}
    body {max-width: 600px; margin:0px auto; padding-bottom: 10px;}
    .switch {position: relative; display: inline-block; width: 120px; height: 68px} 
    .switch input {display: none}
    .slider {position: absolute; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; border-radius: 34px}
    .slider:before {position: absolute; content: ""; height: 52px; width: 52px; left: 8px; bottom: 8px; background-color: #fff; -webkit-transition: .4s; transition: .4s; border-radius: 68px}
    input:checked+.slider {background-color: #2196F3}
    input:checked+.slider:before {-webkit-transform: translateX(52px); -ms-transform: translateX(52px); transform: translateX(52px)}
  </style>
</head>
<body>
  <h2>%EEH_HOSTNAME%</h2>
  <h3 id="maintenancemode">%MAINTENANCEMODE%</h3>
  <p>Device Time: <span id="ntptime">%DEVICETIME%</span> | Firmware Version: %FIRMWARE%</p>
  <button onclick="logoutButton()">Logout Web Admin</button>
  <button onclick="getUserDetailsButton()">Refresh Current Card User Details</button>
  <button onclick="grantAccessButton()" %GRANTBUTTONENABLE%>Grant Access to Current Card</button>
  <button onclick="revokeAccessButton()" %GRANTBUTTONENABLE%>Revoke Access to Current Card</button>
  <button onclick="displayConfig()">Display Config</button>
  <button onclick="refreshNTP()">Refresh NTP</button>
  <button onclick="logoutCurrentUserButton()">Logout Current User</button>
  <button onclick="rebootButton()">Reboot</button>
  <input type="button" onclick="location.href='/update';" value="OTA Update" />
  <p>Status: <span id="statusdetails"></span></p>
  <p>System State: <span id="currentaccess">%CURRENTSYSTEMSTATE%</span></p>
  <hr>
  <div id="userdetails">%USERDETAILS%</div>
  <hr>
  %LEDSLIDER%
  %RELAYSLIDER%
  %MAINTENANCEMODESLIDER%
  <p id="configheader"></p>
  <p id="configdetails"></p>
<script>
function toggleCheckbox(element, pin) {
  var xhr = new XMLHttpRequest();
  if(element.checked){ xhr.open("GET", "/toggle?state=1&pin="+pin, true); }
  else { xhr.open("GET", "/toggle?state=0&pin="+pin, true); }
  xhr.send();
}
function toggleMaintenance(element) {
  var xhr = new XMLHttpRequest();
  var newState = "";
  if (element.checked) {
    document.getElementById("statusdetails").innerHTML = "Enabling Maintenance Mode";
    newState = "enable";
  } else {
    document.getElementById("statusdetails").innerHTML = "Disabling Maintenance Mode";
    newState = "disable";
  }
  xhr.open("GET", "/maintenance?state="+newState, true);
  xhr.send();
  setTimeout(function(){
    document.getElementById("maintenancemode").innerHTML = xhr.responseText;
    document.getElementById("statusdetails").innerHTML = "Toggled Maintenance Mode";
  },5000);
}
function logoutButton() {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/logout", true);
  xhr.send();
  setTimeout(function(){ window.open("/logged-out","_self"); }, 1000);
}
function logoutCurrentUserButton() {
  document.getElementById("statusdetails").innerHTML = "Logging Out Current User ...";
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/logout-current-user", true);
  xhr.send();
  setTimeout(function(){
    document.getElementById("relayslider").checked = false;
    document.getElementById("ledslider").checked = false;
    document.getElementById("statusdetails").innerHTML = "Logged Out User";
    document.getElementById("userdetails").innerHTML = xhr.responseText;
  },5000);
}
function grantAccessButton() {
  document.getElementById("statusdetails").innerHTML = "Granting Access ...";
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/grant?haveaccess=true", true);
  xhr.send();
  setTimeout(function(){
    document.getElementById("statusdetails").innerHTML = "Access Granted";
    document.getElementById("userdetails").innerHTML = xhr.responseText;
  },5000);
}
function getUserDetailsButton() {
  document.getElementById("statusdetails").innerHTML = "Getting User Details ...";
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/getuser", true);
  xhr.send();
  setTimeout(function(){
    document.getElementById("statusdetails").innerHTML = "Refreshed User Details";
    document.getElementById("userdetails").innerHTML = xhr.responseText;
  },5000);
}
function revokeAccessButton() {
  document.getElementById("statusdetails").innerHTML = "Revoking access ...";
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/grant?haveaccess=false", true);
  xhr.send();
  setTimeout(function(){
    document.getElementById("statusdetails").innerHTML = "Access Revoked";
    document.getElementById("userdetails").innerHTML = xhr.responseText;
  },5000);
}
function rebootButton() {
  document.getElementById("statusdetails").innerHTML = "Invoking Reboot ...";
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/reboot", true);
  xhr.send();
  window.open("/reboot","_self");
  // setTimeout(function(){ window.open("/reboot","_self"); }, 5);
}
function refreshNTP() {
  document.getElementById("statusdetails").innerHTML = "Refreshing NTP ...";
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/ntprefresh", true);
  xhr.send();
  setTimeout(function(){
    document.getElementById("statusdetails").innerHTML = "Refreshed NTP";
    document.getElementById("ntptime").innerHTML = xhr.responseText;
  },5000);
}
function displayConfig() {
  document.getElementById("statusdetails").innerHTML = "Loading Configuration ...";
  document.getElementById("configheader").innerHTML = "<h3>Configuration<h3>";
  xmlhttp=new XMLHttpRequest();
  xmlhttp.open("GET", "/fullstatus", false);
  xmlhttp.send();
  var mydata = JSON.parse(xmlhttp.responseText);
  var displaydata = "<table><tr><th align='left'>Setting</th><th align='left'>Value</th></tr>";
  for (var key of Object.keys(mydata)) {
    displaydata = displaydata + "<tr><td align='left'>" + key + "</td><td align='left'>" + mydata[key] + "</td></tr>";
  }
  displaydata = displaydata + "</table>";
  document.getElementById("statusdetails").innerHTML = "Configuration Loaded";
  document.getElementById("configdetails").innerHTML = displaydata;
}
</script>
</body>
</html>
)rawliteral";

// logout.html
const char logout_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html lang="en">
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta charset="UTF-8">
</head>
<body>
  <p>Logged out or <a href="/">return to homepage</a>.</p>
  <p><strong>Note:</strong> close all web browser tabs to complete the logout process.</p>
</body>
</html>
)rawliteral";

// reboot.html base upon https://gist.github.com/Joel-James/62d98e8cb3a1b6b05102
const char reboot_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html lang="en">
<head>
  <meta charset="UTF-8">
</head>
<body>
<h3>
  Rebooting, returning to main page in <span id="countdown">30</span> seconds
</h3>
<script type="text/javascript">
  var seconds = 30;
  function countdown() {
    seconds = seconds - 1;
    if (seconds < 0) {
      window.location = "/";
    } else {
      document.getElementById("countdown").innerHTML = seconds;
      window.setTimeout("countdown()", 1000);
    }
  }
  countdown();
</script>
</body>
</html>
)rawliteral";


// parses and processes index.html
String processor(const String& var) {

  if (var == "MAINTENANCEMODESLIDER") {
    String buttons = "";
    String outputStateValue = isInMaintenance();
    buttons+= "<p>MAINTENANCE MODE</p><p><label class='switch'><input type='checkbox' onchange='toggleMaintenance(this)' id='mmslider' " + outputStateValue + "><span class='slider'></span></label></p>";
    return buttons;
  }

  if (var == "LEDSLIDER") {
    String buttons = "";
    String outputStateValue = outputState(config.ledpin);
    buttons+= "<p>LED</p><p><label class='switch'><input type='checkbox' onchange='toggleCheckbox(this, \"led\")' id='ledslider' " + outputStateValue + "><span class='slider'></span></label></p>";
    return buttons;
  }

  if (var == "RELAYSLIDER") {
    String buttons = "";
    String outputStateValue = outputState(config.relaypin);
    
    // because HIGH = off for the relay, this needs to be reversed to make web button work
    if (outputStateValue == "checked") {
      outputStateValue = "";
    } else {
      outputStateValue = "checked";
    }

    buttons += "<p>RELAY</p><p><label class='switch'><input type='checkbox' onchange='toggleCheckbox(this, \"relay\")' id='relayslider' " + outputStateValue + "><span class='slider'></span></label></p>";
    return buttons;
  }

  if (var == "EEH_HOSTNAME") {
    return config.hostname;
  }

  if (var == "MAINTENANCEMODE") {
    if (config.inmaintenance) {
      return "MAINTENANCE MODE";
    } else {
      return "";
    }
  }

  if (var == "USERDETAILS") {
    if (strcmp(currentRFIDcard, "") == 0) {
      return "NO CARD PRESENT";
    } else {
      String returnText = "";
      String currentFullName = currentRFIDFirstNameStr + " " + currentRFIDSurnameStr;
      String currentUserID = currentRFIDUserIDStr;
      String currentAccess = "";

      if (currentFullName == "") currentFullName = "ERROR: User Not Found";
      if (currentUserID == "") currentUserID = "NONE";

      if (currentRFIDaccess) {
        currentAccess = "Allow";
      } else {
        currentAccess = "Deny";
      }

      returnText = "<table>";
      returnText += "<tr><td align='left'><b>Name:</b></td><td align='left'>" + currentFullName + "</td></tr>";
      returnText += "<tr><td align='left'><b>User ID:</b></td><td align='left'>" + currentUserID + "</td></tr>";
      returnText += "<tr><td align='left'><b>RFID:</b></td><td align='left'>" + String(currentRFIDcard) + "</td></tr>";
      returnText += "<tr><td align='left'><b>Device:</b></td><td align='left'>" + config.device + "</td></tr>";
      returnText += "<tr><td align='left'><b>Access:</b></td><td align='left'>" + currentAccess + "</td></tr>";
      returnText += "</table>";

      return returnText;
    }
  }

  if (var == "GRANTBUTTONENABLE") {
    if (strcmp(currentRFIDcard, "") == 0) {
      return "DISABLED";
    }
  }

  if (var == "CURRENTSYSTEMSTATE") {
    if (currentRFIDaccess) {
      return "Granted";
    } else {
      return "Denied";
    }
  }

  if (var == "FIRMWARE") {
    return FIRMWARE_VERSION;
  }

  if (var == "DEVICETIME") {
    return printTime();
  }

  return String();
}

// checks state of a pin, used when writing button slider position
String outputState(int PINCHECK){
  if(digitalRead(PINCHECK)){
    return "checked";
  }
  else {
    return "";
  }
  return "";
}

String isInMaintenance() {
  if (config.inmaintenance) {
    return "checked";
  } else {
    return "";
  }
  return "";
}

void setup() {
  Serial.begin(115200);

  lcd.init();
  lcd.backlight();
  lcd.noCursor();

  lcd.print("Booting...");

  // loading configuration
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  Serial.println("Removing old config files");
  SPIFFS.remove("/config.txt");
  SPIFFS.remove("/config2.txt");
  SPIFFS.remove("/config3.txt");
  SPIFFS.remove("/config4.txt");

  Serial.println("=============");
  Serial.println(F("Print config file before..."));
  printFile(filename);
  Serial.println(F("Loading configuration..."));
  loadConfiguration(filename, config);
  Serial.println(F("Saving configuration..."));
  saveConfiguration(filename, config);
  Serial.println(F("Print config file after..."));
  printFile(filename);
  Serial.println("=============");
  printConfig();
  Serial.println("=============");

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

  SPI.begin(); // Init SPI bus
  mfrc522.PCD_Init(); // Init MFRC522
  delay(4); // delay needed to allow mfrc522 to spin up properly

  Serial.println("\nSystem Configuration:");
  Serial.println("---------------------");
  Serial.print("        Hostname: "); Serial.println(config.hostname);
  Serial.print("        App Name: "); Serial.println(config.appname);
  Serial.print("      EEH Device: "); Serial.println(config.device);
  if (config.inmaintenance) {
    Serial.println("Maintenance Mode: true");
  } else {
    Serial.println("Maintenance Mode: false");
  }
  Serial.print("   Syslog Server: "); Serial.print(config.syslogserver); Serial.print(":"); Serial.println(config.syslogport);
  Serial.print("   API Wait Time: "); Serial.print(waitTime); Serial.println(" seconds");
  Serial.print(" RFID Card Delay: "); Serial.print(checkCardTime); Serial.println(" seconds");
  Serial.print("       Relay Pin: "); Serial.println(config.relaypin);
  Serial.print("         LED Pin: "); Serial.println(config.ledpin);
  Serial.print(" Web Server Port: "); Serial.println(WEB_SERVER_PORT);
  Serial.print("     ESP32 Flash: "); Serial.println(FIRMWARE_VERSION);
  Serial.print("  Flash Compiled: "); Serial.println(String(__DATE__) + " " + String(__TIME__));
  Serial.print("      ESP32 Temp: "); Serial.print((temprature_sens_read() - 32) / 1.8); Serial.println("C");

  Serial.print(" MFRC522 Version: "); Serial.println(getmfrcversion());
  Serial.print("      NTP Server: "); Serial.println(config.ntpserver);
  Serial.print("   NTP Time Sync: "); Serial.println(config.ntpsynctime);
  Serial.print("   NTP Time Zone: "); Serial.println(config.ntptimezone);

//===========
  File root = SPIFFS.open("/");
 
  File file3 = root.openNextFile();
 
  while(file3){
 
      Serial.print("FILE: ");
      Serial.println(file3.name());
 
      file3 = root.openNextFile();
  }
//===========
  Serial.print("checking nonexistent file: "); Serial.println(SPIFFS.exists("/nonexisting.txt"));

  if (SPIFFS.exists("/test.txt")) {
    Serial.println("test.txt exists");
    readafile();
  } else {
    Serial.println("test.txt does not exist, creating");
    writeafile();
  }

  SPIFFS.remove("/test.txt");
//===============
  
  lcd.clear();
  lcd.print("Connecting to Wifi..");

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

  pinMode(config.ledpin, OUTPUT);
  pinMode(config.relaypin, OUTPUT);
  disableLed("Automatically Disable LED upon boot");
  disableRelay("Automatically Disable Relay upon boot");

  Serial.println();

  // configure time, wait config.ntpwaitsynctime seconds then progress, otherwise it can stall
  Serial.print("Attempting to NTP Sync time for "); Serial.print(config.ntpwaitsynctime); Serial.println(" seconds");
  lcd.clear();
  lcd.print("Syncing NTP...");
  waitForSync(config.ntpwaitsynctime);
  setInterval(config.ntpsynctime);
  setServer(config.ntpserver);
  setDebug(NTPDEBUG);
  myTZ.setLocation(config.ntptimezone);
  Serial.print(config.ntptimezone + ": "); Serial.println(printTime());

  bootTime = printTime();
  Serial.print("Booted at: "); Serial.println(bootTime);
  syslog.logf("Booted");

  // Break all of these server.on in to a seperate file
  // https://randomnerdtutorials.com/esp32-esp8266-web-server-http-authentication/
  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!request->authenticate(config.httpuser.c_str(), config.httppassword.c_str())) {
      return request->requestAuthentication();
    }

    /*
    int headers = request->headers();
    int i;
    for(i=0;i<headers;i++){
      AsyncWebHeader* h = request->getHeader(i);
      Serial.printf("HEADER[%s]: %s\n", h->name().c_str(), h->value().c_str());
    }
    */

    String logmessage = "Client:" + request->client()->remoteIP().toString() + + " " + request->url();
    Serial.println(logmessage);
    syslog.log(logmessage);
    request->send_P(200, "text/html", index_html, processor);
  });

  server.on("/logout", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(401);
  });

  server.on("/maintenance", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!request->authenticate(config.httpuser.c_str(), config.httppassword.c_str())) {
      return request->requestAuthentication();
    }

    String returnText = "";
    String logmessage = "";

    const char* selectState = request->getParam("state")->value().c_str();

    if (strcmp(selectState, "enable") == 0) {
      logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url() + " Enabling maintenance mode";
      returnText = "MAINTENANCE MODE";
      gotoToggleMaintenance = true;
    } else if (strcmp(selectState, "disable") == 0) {
      logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url() + " Disabling maintenance mode";
      returnText = "";
      gotoToggleMaintenance = true;
    } else {
      logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url() + " ERROR: invalid state sent to maintenance mode, ignoring: " + String(selectState);
      returnText = "ERROR: invalid state sent to maintenance mode, ignoring: " + String(selectState);
      gotoToggleMaintenance = false;
    }
    request->send(200, "text/html", returnText);
    Serial.println(logmessage);
    syslog.log(logmessage);
  });

  server.on("/backlighton", HTTP_GET, [](AsyncWebServerRequest *request){
    lcd.backlight();
    request->send(200, "text/html", "backlight on");
  });

  server.on("/backlightoff", HTTP_GET, [](AsyncWebServerRequest *request){
    lcd.noBacklight();
    request->send(200, "text/html", "backlight off");
  });

  server.on("/logged-out", HTTP_GET, [](AsyncWebServerRequest *request){
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
    Serial.println(logmessage);
    syslog.log(logmessage);
    request->send_P(200, "text/html", logout_html, processor);
  });

  server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!request->authenticate(config.httpuser.c_str(), config.httppassword.c_str())) {
      return request->requestAuthentication();
    }
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
    Serial.println(logmessage);
    syslog.log(logmessage);
    request->send(200, "text/html", reboot_html);
    shouldReboot = true;
  });

  server.on("/getuser", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!request->authenticate(config.httpuser.c_str(), config.httppassword.c_str())) {
      return request->requestAuthentication();
    }
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " RFID:" + String(currentRFIDcard) + " " + request->url();
    Serial.println(logmessage);
    syslog.log(logmessage);
    char getUserURL[240];
    sprintf(getUserURL, "%s%s%s%s%s%s%s", ADMIN_SERVER, "getuser.php?device=", config.device, "&rfid=", String(currentRFIDcard), "&api=", config.apitoken);

    //Serial.print("GetUserURL: "); Serial.println(getUserURL);
    request->send(200, "text/html", getUserDetails(getUserURL));
  });

  server.on("/grant", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!request->authenticate(config.httpuser.c_str(), config.httppassword.c_str())) {
      return request->requestAuthentication();
    }
    const char* haveaccess = request->getParam("haveaccess")->value().c_str();
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " RFID:" + String(currentRFIDcard) + " " + request->url() + "?haveaccess=" + haveaccess;
    Serial.println(logmessage);
    syslog.log(logmessage);
    char grantURL[240];
    sprintf(grantURL, "%s%s%s%s%s%s%s%s%s", ADMIN_SERVER, "moduser.php?device=", config.device, "&modrfid=", String(currentRFIDcard), "&api=", config.apitoken, "&haveaccess=", haveaccess);
    if (strcmp(haveaccess, "true") == 0) {
      // granting access
      logmessage = "Web Admin: Granting access for " + String(currentRFIDcard);
    } else {
      // revoking access
      logmessage = "Web Admin: Revoking access for " + String(currentRFIDcard);
    }
    Serial.println(logmessage);
    syslog.log(logmessage);
    request->send(200, "text/html", grantAccess(grantURL));
  });

  server.on("/ntprefresh", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!request->authenticate(config.httpuser.c_str(), config.httppassword.c_str())) {
      return request->requestAuthentication();
    }
    //request->send(200, "text/html", "ok");
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
    Serial.println(logmessage);
    syslog.log(logmessage);
    updateNTP();
    request->send(200, "text/html", printTime());
  });

  server.on("/logout-current-user", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!request->authenticate(config.httpuser.c_str(), config.httppassword.c_str())) {
      return request->requestAuthentication();
    }
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
    Serial.println(logmessage);
    syslog.log(logmessage);
    gotoLogoutCurrentUser = true;

    String returnText = "<table>";
    returnText += "<tr><td align='left'><b>Name:</b></td><td align='left'>" + currentRFIDFirstNameStr + " " + currentRFIDSurnameStr + "</td></tr>";
    returnText += "<tr><td align='left'><b>User ID:</b></td><td align='left'>" + currentRFIDUserIDStr + "</td></tr>";
    returnText += "<tr><td align='left'><b>RFID:</b></td><td align='left'>" + String(currentRFIDcard) + "</td></tr>";
    returnText += "<tr><td align='left'><b>Device:</b></td><td align='left'>" + config.device + "</td></tr>";
    returnText += "<tr><td align='left'><b>Access:</b></td><td align='left'>Web Admin Logged Out</td></tr>";
    returnText += "</table>";
    
    request->send(200, "text/html", returnText);
  });

  server.on("/health", HTTP_GET, [](AsyncWebServerRequest *request){
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
    Serial.println(logmessage);
    syslog.log(logmessage);
    request->send(200, "text/plain", "OK");
  });

  server.on("/fullstatus", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!request->authenticate(config.httpuser.c_str(), config.httppassword.c_str())) {
      return request->requestAuthentication();
    }
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
    Serial.println(logmessage);
    syslog.log(logmessage);
    request->send(200, "application/json", getFullStatus());
  });

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
    Serial.println(logmessage);
    syslog.log(logmessage);
    request->send(200, "application/json", getStatus());
  });

  // used for checking whether time is sync
  server.on("/time", HTTP_GET, [](AsyncWebServerRequest *request){
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
    Serial.println(logmessage);
    syslog.log(logmessage);
    request->send(200, "text/plain", printTime());
  });

  // called when slider has been toggled
  server.on("/toggle", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if (!request->authenticate(config.httpuser.c_str(), config.httppassword.c_str())) {
      return request->requestAuthentication();
    }
    String inputMessage;
    String inputPin;
    if (request->hasParam(PARAM_INPUT_1) && request->hasParam(PARAM_INPUT_2)) {
      inputMessage = request->getParam(PARAM_INPUT_1)->value();
      inputPin = request->getParam(PARAM_INPUT_2)->value();

      String logmessage = "Client:" + request->client()->remoteIP().toString() + " Toggle Slider" + inputMessage + ":" + inputPin + " " + request->url();
      Serial.println(logmessage);
      syslog.log(logmessage);
      logmessage = "";

      if (inputPin == "relay") {
        if (inputMessage.toInt() == 1) {
          logmessage = "Client:" + request->client()->remoteIP().toString() + " Enable Relay" + " " + request->url();
          enableRelay(logmessage);
        } else {
          logmessage = "Client:" + request->client()->remoteIP().toString() + " Disable Relay" + " " + request->url();
          disableRelay(logmessage);
        }
      }

      if (inputPin == "led") {
        if (inputMessage.toInt() == 1) {
          logmessage = "Client:" + request->client()->remoteIP().toString() + " Enable LED" + " " + request->url();
          enableLed(logmessage);
        } else {
          logmessage = "Client:" + request->client()->remoteIP().toString() + " Disable LED" + " " + request->url();
          disableLed(logmessage);
        }
      }

    } else {
      inputMessage = "No message sent";
    }
    request->send(200, "text/plain", "OK");
  });

  if (config.inmaintenance) {
    syslog.logf("Booting in to Maintenance Mode");
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print(config.device);
    lcd.setCursor(0, 1); lcd.print("MAINTENANCE MODE");
    lcd.setCursor(0, 2); lcd.print("ALL ACCESS DENIED");
    lcd.setCursor(0, 3); lcd.print("");
  } else {
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print(config.device);
    lcd.setCursor(0, 1); lcd.print("Present Access Card");
  }

  // if url isn't found
  server.onNotFound(notFound);

  // configure ota webserver
  AsyncElegantOTA.begin(&server, config.httpuser.c_str(), config.httppassword.c_str());

  // startup webserver
  server.begin();
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

  if ((currentRunTime - sinceLastRunTime) < (waitTime * 1000)) {
    Serial.print(iteration); Serial.println(" Firing webcall too fast, ignoring");

    // probably need this
    delay((waitTime * 1000) - (currentRunTime - sinceLastRunTime));
    //return;
  }

  if ((currentRunTime - sinceLastRunTime) > (waitTime * 1000)) {
    if (WiFi.status() == WL_CONNECTED) {
      StaticJsonDocument<300> doc;
      char serverURL[240];
      sprintf(serverURL, "%s%s%s", serverURL1, foundrfid, serverURL2);

      String logmessage = "";

      Serial.print(iteration); Serial.print(" dowebcall ServerURL: "); Serial.println(serverURL);

      returnedJSON = httpGETRequest(serverURL);
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
              lcd.clear();
              lcd.setCursor(0, 0); lcd.print(config.device);
              lcd.setCursor(0, 1); lcd.print("ACCESS GRANTED");
              lcd.setCursor(0, 2); lcd.print("RFID: " + String(currentRFIDcard));
              lcd.setCursor(0, 3); lcd.print(currentRFIDFirstNameStr + " " + currentRFIDSurnameStr);
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
          lcd.clear();
          lcd.setCursor(0, 0); lcd.print(config.device);
          lcd.setCursor(0, 1); lcd.print("ACCESS DENIED");
          lcd.setCursor(0, 2); lcd.print("RFID: " + String(currentRFIDcard));
          lcd.setCursor(0, 3); lcd.print(currentRFIDFirstNameStr + " " + currentRFIDSurnameStr);
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
  // when no card present, display ntp sync events on serial
  events();

  // when no card present, reboot if we've told it to reboot
  if (shouldReboot) {
    rebootESP("Web Admin - Card Absent");
  }

  if (gotoToggleMaintenance) {
    toggleMaintenance();
  }

  if (gotoLogoutCurrentUser) {
    logoutCurrentUser();
  }

  if (!mfrc522.PICC_IsNewCardPresent()) {
    // no new card found, re-loop
    //Serial.println("x");
    return;
  }

  if (!mfrc522.PICC_ReadCardSerial()) {
    // no serial means no real card found, re-loop
    //Serial.println("y");
    return;
  }

  // new card detected
  char newcard[32] = "";
  array_to_string(mfrc522.uid.uidByte, 4, newcard);
  iteration++;
  Serial.print(iteration); Serial.print(" RFID Found: "); Serial.println(newcard);

  char serverURL[80];
  sprintf(serverURL, "%s%s%s", serverURL1, newcard, serverURL2);
  Serial.print(iteration); Serial.print(" ServerURL: "); Serial.println(serverURL);

  while (true) {
    control = 0;
    for (int i = 0; i < 3; i++) {
      if (!mfrc522.PICC_IsNewCardPresent()) {
        if (mfrc522.PICC_ReadCardSerial()) {
          //Serial.print('a');
          control |= 0x16;
        }
        if (mfrc522.PICC_ReadCardSerial()) {
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

        lcd.clear();
        lcd.setCursor(0, 0); lcd.print("Checking Access...");

        // check accessOverrideCodes
        bool overRideActive = false;
        for (byte i = 0; i < (sizeof(accessOverrideCodes) / sizeof(accessOverrideCodes[0])); i++) {
          if (strcmp(currentRFIDcard, accessOverrideCodes[i]) == 0) {
            overRideActive = true;
            inOverrideMode = true;
          }
        }

        if (overRideActive) {
          // access override detected
          enableLed(String(iteration) + " Access Override Detected: Enable LED: " + String(currentRFIDcard));
          enableRelay(String(iteration) + " Access Override Detected: Enable Relay: " + String(currentRFIDcard));
          currentRFIDFirstNameStr = "Override";
          currentRFIDSurnameStr = "Mode";
          currentRFIDUserIDStr = "0";
          currentRFIDaccess = true;
          lcd.clear();
          lcd.setCursor(0, 0); lcd.print(config.device);
          lcd.setCursor(0, 1); lcd.print("ACCESS GRANTED");
          lcd.setCursor(0, 2); lcd.print("RFID: " + String(currentRFIDcard));
          lcd.setCursor(0, 3); lcd.print(currentRFIDFirstNameStr + " " + currentRFIDSurnameStr);
        } else {
          // normal user, do webcall
          dowebcall(newcard);
        }

      } else {
        // this "else" runs as regularly as a loop when a card is present
        //Serial.println("same card, not checking again");

        //===
        // if card present, reboot if we've told it to reboot
        if (shouldReboot) {
          rebootESP("Web Admin - Card Present");
        }

        if (gotoToggleMaintenance) {
          toggleMaintenance();
        }

        if (gotoLogoutCurrentUser) {
          logoutCurrentUser();
        }

        // when card present, display ntp sync events on serial
        events();
      }
    } else {
      break;
    }
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  if (!config.inmaintenance) {
    // not in maintenance mode, update LCD
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print(config.device);
    lcd.setCursor(0, 1); lcd.print("Present Access Card");
  } else {
    // should be in maintenance mode, update LCD
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print(config.device);
    lcd.setCursor(0, 1); lcd.print("MAINTENANCE MODE");
    lcd.setCursor(0, 2); lcd.print("ALL ACCESS DENIED");
    lcd.setCursor(0, 3); lcd.print("");
  }
  disableLed(String(iteration) + " " + "Access Revoked: Card Removed: Disable LED: " + String(newcard));
  disableRelay(String(iteration) + " " + "Access Revoked: Card Removed: Disable Relay: " + String(newcard));
  currentRFIDcard = "";
  currentRFIDaccess = false;
  currentRFIDUserIDStr = "";
  currentRFIDFirstNameStr = "";
  currentRFIDSurnameStr = "";
  inOverrideMode = false;
  delay((checkCardTime * 1000));

  // Dump debug info about the card; PICC_HaltA() is automatically called
  //Serial.println("Starting picc_dumptoserial");
  //mfrc522.PICC_DumpToSerial(&(mfrc522.uid));
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

void rebootESP(char* message) {
  Serial.print(iteration); Serial.print(" Rebooting ESP32: "); Serial.println(message);
  syslog.logf("%d Rebooting ESP32:%s", iteration, message);
  // wait 10 seconds to allow syslog to be sent
  delay(10000);
  ESP.restart();
}

String getFullStatus() {
  StaticJsonDocument<2000> fullStatusDoc;
  fullStatusDoc["Timestamp"] = printTime();
  fullStatusDoc["Hostname"] = config.hostname;
  fullStatusDoc["BootTime"] = bootTime;
  fullStatusDoc["AppName"] = config.appname;
  fullStatusDoc["EEHDevice"] = config.device;
  fullStatusDoc["OverrideUsers"] = getAccessOverrideCodes();
  fullStatusDoc["SyslogServer"] = config.syslogserver;
  fullStatusDoc["SyslogPort"] = config.syslogport;
  fullStatusDoc["ServerURL1"] = serverURL1;
  fullStatusDoc["ServerURL2"] = serverURL2;
  fullStatusDoc["Firmware"] = FIRMWARE_VERSION;
  fullStatusDoc["Temp"] = String((temprature_sens_read() - 32) / 1.8) + "C";
  fullStatusDoc["CompileTime"] = String(__DATE__) + " " + String(__TIME__);
  fullStatusDoc["ConfigFile"] = String(filename);
  fullStatusDoc["MFRC522SlaveSelect"] = SS_PIN;
  fullStatusDoc["MFRC522ResetPin"] = RST_PIN;
  fullStatusDoc["MFRC522Firmware"] = getmfrcversion();
  fullStatusDoc["NTPServer"] = config.ntpserver;
  fullStatusDoc["NTPSyncTime"] = config.ntpsynctime;
  fullStatusDoc["NTPTimeZone"] = config.ntptimezone;
  fullStatusDoc["NTPWaitSynctime"] = config.ntpwaitsynctime;
  fullStatusDoc["NTPSyncStatus"] = getTimeStatus();
  fullStatusDoc["APIWait"] = waitTime;
  fullStatusDoc["RFIDDelay"] = checkCardTime;
  fullStatusDoc["ShouldReboot"] = shouldReboot;
  fullStatusDoc["WebServerPort"] = WEB_SERVER_PORT;
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
  fullStatusDoc["RelayPin"] = config.relaypin;
  fullStatusDoc["LCDI2C"] = "0x" + String(LCD_I2C, HEX);
  fullStatusDoc["LCDWidth"] = LCD_WIDTH;
  fullStatusDoc["LCDHeight"] = LCD_HEIGHT;

  // note this is the opposite of what is expected due to the way the relay works
  if (digitalRead(config.relaypin)) {
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
  switch (mfrc522.PCD_ReadRegister(mfrc522.VersionReg)) {
    case 0x91:
      mfrcver = "v1.0";
      break;
    case 0x92:
      mfrcver = "v2.0";
      break;
    default:
      mfrcver = "0x" + String(mfrc522.PCD_ReadRegister(mfrc522.VersionReg), HEX) + ":counterfeit";
  }
  return mfrcver;
}

String getAccessOverrideCodes() {
  String nicelist = "";
  String tempcomma = "";
  for (int i = 0; i < (sizeof(accessOverrideCodes) / sizeof(accessOverrideCodes[0])); i++) {
    if (i == 0) {
      tempcomma = "";
    } else {
      tempcomma = ", ";
    }
    nicelist += tempcomma + String(accessOverrideCodes[i]);
  }
  return nicelist;
}

void notFound(AsyncWebServerRequest *request) {
  String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
  Serial.println(logmessage);
  syslog.log(logmessage);
  request->send(404, "text/plain", "Not found");
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
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(line1);
  lcd.setCursor(0, 1); lcd.print(line2);
  lcd.setCursor(0, 2); lcd.print(line3);
  lcd.setCursor(0, 3); lcd.print(line4);
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
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print(config.device);
    lcd.setCursor(0, 1); lcd.print("MAINTENANCE MODE");
    lcd.setCursor(0, 2); lcd.print("ALL ACCESS DENIED");
    lcd.setCursor(0, 3); lcd.print("");
  } else {
    syslog.logf("Disabling Maintenance Mode");
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print(config.device);
    lcd.setCursor(0, 1); lcd.print("Present Access Card");
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

  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(config.device);
  lcd.setCursor(0, 1); lcd.print("LOGGED OUT");
  lcd.setCursor(0, 2); lcd.print("RFID: " + String(currentRFIDcard));
  lcd.setCursor(0, 3); lcd.print(currentRFIDFirstNameStr + " " + currentRFIDSurnameStr);
}

void writeafile() {
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
  File file = SPIFFS.open("/test.txt", FILE_WRITE);
  if(!file){
     Serial.println("There was an error opening the file for writing");
     return;
   }

  if (file.print("TEST steve")) {
    Serial.println("File was written");
  } else {
    Serial.println("File write failed");
  }
 
  file.close();
}

void readafile() {
  File file2 = SPIFFS.open("/test.txt");
  if (!file2){
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.println("File Content:");
  while(file2.available()){
    Serial.write(file2.read());
  }
  file2.close();
}

// based upon https://arduinojson.org/v6/example/config/
void loadConfiguration(const char *filename, Config &config) {
  // Open file for reading
  File file = SPIFFS.open(filename);

  if (!file){
    Serial.println("Failed to open file for reading");
    return;
  }

  if (!SPIFFS.exists(filename)) {
    Serial.println(String(filename) + " does not exist");
  } else {
    Serial.println(String(filename) + " exists");
  }

  StaticJsonDocument<1000> doc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, file);
  if (error) {
    Serial.println(F("Failed to read file, using default configuration"));
  }

  // Copy values from the JsonDocument to the Config
  config.hostname = doc["hostname"].as<String>();
  if (config.hostname == "null") { config.hostname = "esp32-1.home.narco.tk"; }

  config.device = doc["device"].as<String>();
  if (config.device == "null") { config.device = "laser"; }

  config.appname = doc["appname"].as<String>();
  if (config.appname == "null") { config.appname = "eeh-esp32-rfid-laser"; }

  config.ssid = doc["ssid"].as<String>();
  if (config.ssid == "null") { config.ssid = "narcotkshedtp"; }

  config.wifipassword = doc["wifipassword"].as<String>();
  //if (config.wifipassword == "null") { config.wifipassword = "junk"; }
  if (config.wifipassword == "null") { config.wifipassword = String(password); }

  config.relaypin = doc["relaypin"] | 26;
  config.ledpin = doc["ledpin"] | 2;

  config.httpuser = doc["httpuser"].as<String>();
  if (config.httpuser == "null") { config.httpuser = "admin"; }

  config.httppassword = doc["httppassword"].as<String>();
  if (config.httppassword == "null") { config.httppassword = "admin"; }

  config.overridecodes = doc["overridecodes"].as<String>();
  if (config.overridecodes == "null") { config.overridecodes = "defaultoverridecodes"; }

  config.apitoken = doc["apitoken"].as<String>();
  if (config.apitoken == "null") { config.apitoken = "abcde"; }

  config.syslogserver = doc["syslogserver"].as<String>();
  if (config.syslogserver == "null") { config.syslogserver = "192.168.10.21"; }

  config.syslogport = doc["syslogport"] | 514;

  config.inmaintenance = doc["inmaintenance"] | false;

  config.ntptimezone = doc["ntptimezone"].as<String>();
  if (config.ntptimezone == "null") { config.ntptimezone = "Europe/London"; }

  config.ntpsynctime = doc["ntpsynctime"] | 60;

  config.ntpwaitsynctime = doc["ntpwaitsynctime"] | 5;

  config.ntpserver = doc["ntpserver"].as<String>();
  if (config.ntpserver == "null") { config.ntpserver = "192.168.10.21"; }

  file.close();
}

void saveConfiguration(const char *filename, const Config &config) {
  // Delete existing file, otherwise the configuration is appended to the file
  SPIFFS.remove(filename);

  // Open file for writing
  File file = SPIFFS.open(filename, FILE_WRITE);
  if (!file) {
    Serial.println(F("Failed to create file"));
    return;
  }

  StaticJsonDocument<1000> doc;

  // Set the values in the document
  doc["hostname"] = config.hostname;
  doc["ssid"] = config.ssid;
  doc["wifipassword"] = config.wifipassword;
  doc["relaypin"] = config.relaypin;
  doc["ledpin"] = config.ledpin;
  doc["httpuser"] = config.httpuser;
  doc["httppassword"] = config.httppassword;
  doc["overridecodes"] = config.overridecodes;
  doc["apitoken"] = config.apitoken;
  doc["syslogserver"] = config.syslogserver;
  doc["syslogport"] = config.syslogport;
  doc["inmaintenance"] = config.inmaintenance;
  doc["ntptimezone"] = config.ntptimezone;
  doc["ntpsynctime"] = config.ntpsynctime;
  doc["ntpwaitsynctime"] = config.ntpwaitsynctime;
  doc["ntpserver"] = config.ntpserver;

  // Serialize JSON to file
  if (serializeJson(doc, file) == 0) {
    Serial.println(F("Failed to write to file"));
  }

  // Close the file
  file.close();
}


// Prints the content of a file to the Serial
void printFile(const char *filename) {
  // Open file for reading
  File file = SPIFFS.open(filename);
  if (!file) {
    Serial.println(F("Failed to read file"));
    return;
  }
  // Extract each characters by one by one
  while (file.available()) {
    Serial.print((char)file.read());
  }
  Serial.println();
  // Close the file
  file.close();
}

void printConfig() {
  Serial.print("       hostname: "); Serial.println(config.hostname);
  Serial.print("           ssid: "); Serial.println(config.ssid);
  Serial.print("   wifipassword: "); Serial.println(config.wifipassword);
  Serial.print("       relaypin: "); Serial.println(config.relaypin);
  Serial.print("         ledpin: "); Serial.println(config.ledpin);
  Serial.print("       httpuser: "); Serial.println(config.httpuser);
  Serial.print("   httppassword: "); Serial.println(config.httppassword);
  Serial.print("  overridecodes: "); Serial.println(config.overridecodes);
  Serial.print("       apitoken: "); Serial.println(config.apitoken);
  Serial.print("   syslogserver: "); Serial.println(config.syslogserver);
  Serial.print("     syslogport: "); Serial.println(config.syslogport);
  Serial.print("  inmaintenance: "); Serial.println(config.inmaintenance);
  Serial.print("    ntptimezone: "); Serial.println(config.ntptimezone);
  Serial.print("    ntpsynctime: "); Serial.println(config.ntpsynctime);
  Serial.print("ntpwaitsynctime: "); Serial.println(config.ntpwaitsynctime);
  Serial.print("      ntpserver: "); Serial.println(config.ntpserver);
}
