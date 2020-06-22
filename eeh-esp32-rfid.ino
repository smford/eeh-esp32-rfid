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

// eztime library: https://github.com/ropg/ezTime v0.8.3
// esp async webserver library: https://github.com/me-no-dev/ESPAsyncWebServer v1.2.3
// syslog library: https://github.com/arcao/Syslog v2.0
// mfrc522 library: https://github.com/miguelbalboa/rfid  v1.4.6
// arduinojson library: https://github.com/bblanchon/ArduinoJson & https://arduinojson.org/ v6.15.2
// liquidcrystal_i2c library: https://github.com/johnrickman/LiquidCrystal_I2C

#define SYSLOG_SERVER "192.168.10.21"
#define SYSLOG_PORT 514
#define DEVICE_HOSTNAME "esp32-1.home.narco.tk"
#define APP_NAME "eeh-esp32-rfid-laser"
#define EEH_DEVICE "laser"
#define WEB_SERVER_PORT 80
#define FIRMWARE_VERSION "v1.0"
#define ADMIN_SERVER "http://192.168.10.21:8180/"

// Provide official timezone names
// https://en.wikipedia.org/wiki/List_of_tz_database_time_zones
#define NTPTIMEZONE "Europe/London"
#define NTPSYNCTIME 60
#define NTPWAITSYNCTIME 5
#define NTPSERVER "192.168.10.21"
//#define NTPSERVER "europe.pool.ntp.org"

// lcd configuration
const int LCD_I2C = 0x27;
const int LCD_WIDTH = 20;
const int LCD_HEIGHT = 4;
//const uint8_t LCD_HEIGHT = 4;

//const char* ssid = "ssid";
//const char* password = "password";
//const char* serverURL1 = "https://mock-rfid-system.herokuapp.com/check?rfid=";
//const char* serverURL1 = "http://192.168.10.21:56000/check?rfid=";
const char* serverURL1 = "http://192.168.10.21:8180/check.php?rfid=";
const char* serverURL2 = "&device=laser&api=abcde";

// mfrc522 is in spi mode
const int RST_PIN = 33; // Reset pin
const int SS_PIN = 32; // Slave select pin

const int RELAY = 26;
const int ONBOARD_LED = 2;

//=======
const char* http_username = "admin";
const char* http_password = "admin";

const char* PARAM_INPUT_1 = "state";
const char* PARAM_INPUT_2 = "pin";

// possibly delete
//const int output = 2;
//=======

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

String APITOKEN = "abcde";

// should we reboot the server?
bool shouldReboot = false;

// maintenance and override modes
bool gotoMaintenanceMode = false;
bool inMaintenanceMode = false;
bool inOverrideMode = false;

// MFRC522
MFRC522 mfrc522(SS_PIN, RST_PIN); // Create MFRC522 instance

// Syslog
WiFiUDP udpClient;
Syslog syslog(udpClient, SYSLOG_SERVER, SYSLOG_PORT, DEVICE_HOSTNAME, APP_NAME, LOG_INFO);

// NTP
Timezone myTZ;
String bootTime;
ezDebugLevel_t NTPDEBUG = INFO; // NONE, ERROR, INFO, DEBUG

// Setup LCD
//LiquidCrystal_I2C lcd(0x27,20,4);
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
<!DOCTYPE HTML><html>
<head>
  <title>%EEH_HOSTNAME%</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    h2 {font-size: 2.6rem;}
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
  <p>Device Time: <span id="ntptime">%DEVICETIME%</span> | Firmware Version: %FIRMWARE%</p>
  <button onclick="logoutButton()">Logout Web Admin</button>
  <button onclick="getUserDetailsButton()">Refresh Current Card User Details</button>
  <button onclick="grantAccessButton()" %GRANTBUTTONENABLE%>Grant Access to Current Card</button>
  <button onclick="revokeAccessButton()" %GRANTBUTTONENABLE%>Revoke Access to Current Card</button>
  <button onclick="displayConfig()">Display Config</button>
  <button onclick="refreshNTP()">Refresh NTP</button>
  <button onclick="maintenanceButton()">Maintenance Mode</button>
  <button onclick="rebootButton()">Reboot</button>
  <p>Status: <span id="statusdetails"></span></p>
  <p>System State:   <span id="currentaccess">%CURRENTSYSTEMSTATE%</span></p>
  <p><hr></p>
  <div id="userdetails">%USERDETAILS%</div>
  <p><hr></p>
  %LEDSLIDER%
  %RELAYSLIDER%
  <p id="configheader"></p>
  <p id="configdetails"></p>
<script>function toggleCheckbox(element, pin) {
  var xhr = new XMLHttpRequest();
  if(element.checked){ xhr.open("GET", "/update?state=1&pin="+pin, true); }
  else { xhr.open("GET", "/update?state=0&pin="+pin, true); }
  xhr.send();
}
function logoutButton() {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/logout", true);
  xhr.send();
  setTimeout(function(){ window.open("/logged-out","_self"); }, 1000);
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
//======
function maintenanceButton() {
  document.getElementById("statusdetails").innerHTML = "Entering Maintenance Mode";
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/maintenance?haveaccess=true", true);
  xhr.send();
  setTimeout(function(){
    document.getElementById("statusdetails").innerHTML = "Maintenance Mode Entered: " + xhr.responseText;
  },5000);
}
//======
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
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
</head>
<body>
  <p>Logged out or <a href="/">return to homepage</a>.</p>
  <p><strong>Note:</strong> close all web browser tabs to complete the logout process.</p>
</body>
</html>
)rawliteral";

// reboot.html base upon https://gist.github.com/Joel-James/62d98e8cb3a1b6b05102
const char reboot_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<h3>
  Rebooting, returning to main page in <span id="countdown">30</span> seconds
</h3>
<script type="text/javascript">
  var seconds = 30;
  function countdown() {
    seconds = seconds - 1;
    if (seconds < 0) {
      // Chnage your redirection link here
      window.location = "/";
    } else {
      // Update remaining seconds
      document.getElementById("countdown").innerHTML = seconds;
      // Count down using javascript
      window.setTimeout("countdown()", 1000);
    }
  }
  // Run countdown function
  countdown();
</script>
</html>
)rawliteral";


// parses and processes index.html
String processor(const String& var) {
  if (var == "LEDSLIDER") {
    String buttons = "";
    String outputStateValue = outputState(ONBOARD_LED);
    buttons+= "<p>LED</p><p><label class='switch'><input type='checkbox' onchange='toggleCheckbox(this, \"led\")' id='output' " + outputStateValue + "><span class='slider'></span></label></p>";
    return buttons;
  }

  if (var == "RELAYSLIDER") {
    String buttons = "";
    String outputStateValue = outputState(RELAY);
    
    // because HIGH = off for the relay, this needs to be reversed to make web button work
    if (outputStateValue == "checked") {
      outputStateValue = "";
    } else {
      outputStateValue = "checked";
    }

    buttons += "<p>RELAY</p><p><label class='switch'><input type='checkbox' onchange='toggleCheckbox(this, \"relay\")' id='output' " + outputStateValue + "><span class='slider'></span></label></p>";
    return buttons;
  }

  if (var == "EEH_HOSTNAME") {
    return DEVICE_HOSTNAME;
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
      returnText += "<tr><td align='left'><b>Device:</b></td><td align='left'>" + String(EEH_DEVICE) + "</td></tr>";
      returnText += "<tr><td align='left'><b>Access:</b></td><td align='left'>" + currentAccess + "</td></tr>";
      returnText += "</table>";

      // working but ugly
      //returnText = "<table><tr><td align='left'><b>RFID</b></td><td align='left'>" + String(currentRFIDcard) + "</td></tr>" + "<tr><td align='left'><b>UserID</b></td><td align='left'>" + currentRFIDUserIDStr + "<tr><td align='left'><b>Name</b></td><td align='left'>" + currentRFIDFirstNameStr + " " + currentRFIDSurnameStr + "</td></tr>" + "<tr><td align='left'><b>Device</b></td><td align='left'>" + String(EEH_DEVICE) + "</td></tr>" + "<tr><td align='left'><b>Grant</b></td><td align='left'>" + currentAccess + "</td></tr></table>";

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

void setup() {
  Serial.begin(115200);

  lcd.init();
  lcd.backlight();
  lcd.noCursor();

  lcd.print("Booting...");

  SPI.begin(); // Init SPI bus
  mfrc522.PCD_Init(); // Init MFRC522
  delay(4); // delay needed to allow mfrc522 to spin up properly

  Serial.println("\nSystem Configuration:");
  Serial.println("---------------------");
  Serial.print("        Hostname: "); Serial.println(DEVICE_HOSTNAME);
  Serial.print("        App Name: "); Serial.println(APP_NAME);
  Serial.print("      EEH Device: "); Serial.println(EEH_DEVICE);
  Serial.print("Maintenance Mode: "); Serial.println(inMaintenanceMode);
  Serial.print("   Syslog Server: "); Serial.print(SYSLOG_SERVER); Serial.print(":"); Serial.println(SYSLOG_PORT);
  Serial.print("   API Wait Time: "); Serial.print(waitTime); Serial.println(" seconds");
  Serial.print(" RFID Card Delay: "); Serial.print(checkCardTime); Serial.println(" seconds");
  Serial.print("       Relay Pin: "); Serial.println(RELAY);
  Serial.print("         LED Pin: "); Serial.println(ONBOARD_LED);
  Serial.print(" Web Server Port: "); Serial.println(WEB_SERVER_PORT);
  Serial.print("     ESP32 Flash: "); Serial.println(FIRMWARE_VERSION);
  Serial.print("  Flash Compiled: "); Serial.println(String(__DATE__) + " " + String(__TIME__));
  Serial.print("      ESP32 Temp: "); Serial.print((temprature_sens_read() - 32) / 1.8); Serial.println("C");

  Serial.print(" MFRC522 Version: "); Serial.println(getmfrcversion());
  Serial.print("      NTP Server: "); Serial.println(NTPSERVER);
  Serial.print("   NTP Time Sync: "); Serial.println(NTPSYNCTIME);
  Serial.print("   NTP Time Zone: "); Serial.println(NTPTIMEZONE);

  lcd.clear();
  lcd.print("Connecting to Wifi..");

  Serial.print("\nConnecting to Wifi: ");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n\nNetwork Configuration:");
  Serial.println("----------------------");
  Serial.print("         SSID: "); Serial.println(WiFi.SSID());
  Serial.print("  Wifi Status: "); Serial.println(WiFi.status());
  Serial.print("          MAC: "); Serial.println(WiFi.macAddress());
  Serial.print("           IP: "); Serial.println(WiFi.localIP());
  Serial.print("       Subnet: "); Serial.println(WiFi.subnetMask());
  Serial.print("      Gateway: "); Serial.println(WiFi.gatewayIP());
  Serial.print("        DNS 1: "); Serial.println(WiFi.dnsIP(0));
  Serial.print("        DNS 2: "); Serial.println(WiFi.dnsIP(1));
  Serial.print("        DNS 3: "); Serial.println(WiFi.dnsIP(2));
  Serial.println();

  pinMode(ONBOARD_LED, OUTPUT);
  pinMode(RELAY, OUTPUT);
  disableLed("Automatically Disable LED upon boot");
  disableRelay("Automatically Disable Relay upon boot");

  Serial.println();

  // configure time, wait 10 seconds then progress, otherwise it can stall
  Serial.print("Attempting to NTP Sync time for "); Serial.print(NTPWAITSYNCTIME); Serial.println(" seconds");
  lcd.clear();
  lcd.print("Syncing NTP...");
  waitForSync(NTPWAITSYNCTIME);
  setInterval(NTPSYNCTIME);
  setServer(NTPSERVER);
  setDebug(NTPDEBUG);
  myTZ.setLocation(F(NTPTIMEZONE));
  Serial.print(String(NTPTIMEZONE) + ": "); Serial.println(printTime());

  bootTime = printTime();
  Serial.print("Booted at: "); Serial.println(bootTime);
  syslog.logf("Booted");
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(String(EEH_DEVICE));
  lcd.setCursor(0, 1); lcd.print("Present Access Card");

  // https://randomnerdtutorials.com/esp32-esp8266-web-server-http-authentication/
  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!request->authenticate(http_username, http_password)) {
      return request->requestAuthentication();
    }

    /*
    //----------
    int headers = request->headers();
    int i;
    for(i=0;i<headers;i++){
      AsyncWebHeader* h = request->getHeader(i);
      Serial.printf("HEADER[%s]: %s\n", h->name().c_str(), h->value().c_str());
    }
    //----------
    */

    String logmessage = "Client:" + request->client()->remoteIP().toString() + " /";
    Serial.println(logmessage);
    syslog.log(logmessage);
    request->send_P(200, "text/html", index_html, processor);
  });

  server.on("/logout", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(401);
  });

  server.on("/maintenance", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!request->authenticate(http_username, http_password)) {
      return request->requestAuthentication();
    }
    //String haveaccess = request->getParam("haveaccess")->value();
    const char* haveaccess = request->getParam("mode")->value().c_str();
    Serial.println("Entering maintenance mode");
    //enterMaintenance();
    gotoMaintenanceMode = true;
    request->send(200, "text/plain", "maintenance mode");
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
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " /logged-out";
    Serial.println(logmessage);
    syslog.log(logmessage);
    request->send_P(200, "text/html", logout_html, processor);
  });

  server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!request->authenticate(http_username, http_password)) {
      return request->requestAuthentication();
    }
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " /reboot";
    Serial.println(logmessage);
    syslog.log(logmessage);
    request->send(200, "text/html", reboot_html);
    shouldReboot = true;
  });

server.on("/getuser", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!request->authenticate(http_username, http_password)) {
      return request->requestAuthentication();
    }
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " RFID:" + String(currentRFIDcard) + " /getuser";
    Serial.println(logmessage);
    syslog.log(logmessage);
    char getUserURL[240];
    sprintf(getUserURL, "%s%s%s%s%s%s%s", ADMIN_SERVER, "getuser.php?device=", EEH_DEVICE, "&rfid=", String(currentRFIDcard), "&api=", APITOKEN);
    //Serial.print("GetUserURL: "); Serial.println(getUserURL);
    request->send(200, "text/html", getUserDetails(getUserURL));
  });

  server.on("/grant", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!request->authenticate(http_username, http_password)) {
      return request->requestAuthentication();
    }
    //String haveaccess = request->getParam("haveaccess")->value();
    const char* haveaccess = request->getParam("haveaccess")->value().c_str();
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " RFID:" + String(currentRFIDcard) + " /grant?haveaccess=" + haveaccess;
    Serial.println(logmessage);
    syslog.log(logmessage);
    //String grantAccess(String myurl)
    char grantURL[240];
    //String haveaccess;
    //haveaccess = request->getParam("haveaccess")->value();
    sprintf(grantURL, "%s%s%s%s%s%s%s%s%s", ADMIN_SERVER, "moduser.php?device=", EEH_DEVICE, "&modrfid=", String(currentRFIDcard), "&api=", APITOKEN, "&haveaccess=", haveaccess);
    //Serial.print("GrantURL: "); Serial.println(grantURL);
    //request->send(200, "text/html", grantAccess("http://192.168.10.21:8180/moduser.php?device=" + EEH_DEVICE + "&modrfid=" + String(currentRFIDcard) + "&api=" + APITOKEN + "&haveaccess=true");
    //logmessage = 
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
    if (!request->authenticate(http_username, http_password)) {
      return request->requestAuthentication();
    }
    //request->send(200, "text/html", "ok");
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " /ntprefresh";
    Serial.println(logmessage);
    syslog.log(logmessage);
    updateNTP();
    request->send(200, "text/html", printTime());
  });

  server.on("/health", HTTP_GET, [](AsyncWebServerRequest *request){
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " /health";
    Serial.println(logmessage);
    syslog.log(logmessage);
    request->send(200, "text/plain", "OK");
  });

  server.on("/fullstatus", HTTP_GET, [](AsyncWebServerRequest *request){
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " /fullstatus";
    Serial.println(logmessage);
    syslog.log(logmessage);
    if (!request->authenticate(http_username, http_password)) {
      return request->requestAuthentication();
    }
    request->send(200, "application/json", getFullStatus());
  });

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " /status";
    Serial.println(logmessage);
    syslog.log(logmessage);
    request->send(200, "application/json", getStatus());
  });

  // used for checking whether time is sync
  server.on("/time", HTTP_GET, [](AsyncWebServerRequest *request){
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " /time";
    Serial.println(logmessage);
    syslog.log(logmessage);
    request->send(200, "text/plain", printTime());
  });

  // called when slider has been toggled
  server.on("/update", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username, http_password)) {
      return request->requestAuthentication();
    }
    String inputMessage;
    String inputPin;
    if (request->hasParam(PARAM_INPUT_1) && request->hasParam(PARAM_INPUT_2)) {
      inputMessage = request->getParam(PARAM_INPUT_1)->value();
      inputPin = request->getParam(PARAM_INPUT_2)->value();

      String logmessage = "Client:" + request->client()->remoteIP().toString() + " Toggle Slider" + inputMessage + ":" + inputPin;
      Serial.println(logmessage);
      syslog.log(logmessage);
      logmessage = "";

      if (inputPin == "relay") {
        if (inputMessage.toInt() == 1) {
          logmessage = "Client:" + request->client()->remoteIP().toString() + " Enable Relay";
          enableRelay(logmessage);
        } else {
          logmessage = "Client:" + request->client()->remoteIP().toString() + " Disable Relay";
          disableRelay(logmessage);
        }
      }

      if (inputPin == "led") {
        if (inputMessage.toInt() == 1) {
          logmessage = "Client:" + request->client()->remoteIP().toString() + " Enable LED";
          enableLed(logmessage);
        } else {
          logmessage = "Client:" + request->client()->remoteIP().toString() + " Disable LED";
          disableLed(logmessage);
        }
      }

    } else {
      inputMessage = "No message sent";
    }
    request->send(200, "text/plain", "OK");
  });

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

      //Serial.print(iteration); Serial.println(" JSON Deserialization");
      DeserializationError error = deserializeJson(doc, returnedJSON);
      if (error) {
        Serial.print(iteration); Serial.print(F(" DeserializeJson() failed: ")); Serial.println(error.c_str());
        syslog.logf(LOG_ERR, "%d Error Decoding JSON: %s", iteration, error.c_str());
        //return false;
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
        // Serial.print(iteration); Serial.println(" RFID Matches");

        currentRFIDUserIDStr = doc["UserID"].as<String>();
        currentRFIDFirstNameStr = doc["FirstName"].as<String>();
        currentRFIDSurnameStr = doc["Surname"].as<String>();

        if (strcmp(Grant, "true") == 0) {
          // api says user has access

          if (strcmp(EEH_DEVICE, EEHDevice) == 0) {
            // device matches api returned device

            if (!inMaintenanceMode) {
              // grant access because not in maintenance mode; and rfid, grant and device match
              currentRFIDaccess = true;
              lcd.clear();
              lcd.setCursor(0, 0); lcd.print(String(EEH_DEVICE));
              lcd.setCursor(0, 1); lcd.print("ACCESS GRANTED");
              lcd.setCursor(0, 2); lcd.print("RFID: " + String(currentRFIDcard));
              lcd.setCursor(0, 3); lcd.print(currentRFIDFirstNameStr + " " + currentRFIDSurnameStr);
              enableLed(String(iteration) + " Access Granted: Enable LED: UserID:" + currentRFIDUserIDStr + " RFID:" + String(currentRFIDcard));
              enableRelay(String(iteration) + " Access Granted: Enable LED: UserID:" + currentRFIDUserIDStr + " RFID:" + String(currentRFIDcard));
            } else {
              // in maintenance mode, show message, deny access
              Serial.println(String(iteration) + "In maintenance mode, ignoring non-Override access");
              syslog.log(String(iteration) + "In maintenance mode, ignoring non-Override access");
              lcdPrint("ACCESS DENIED", "IN MAINTENANCE MODE", "Only Override User", "Allowed");
              delay(10000);
            }

          } else {
            // deny access, device does not match api returned device name
            disableLed(String(iteration) + " Device Mismatch: Disable LED: Expected:" + String(EEH_DEVICE) + " Got:" + EEHDevice);
            disableRelay(String(iteration) + " Device Mismatch: Disable Relay: Expected:" + String(EEH_DEVICE) + " Got:" + EEHDevice);
          }
        } else {
          // deny access, api says user does not have access
          lcd.clear();
          lcd.setCursor(0, 0); lcd.print(String(EEH_DEVICE));
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
      //return true;
    } else {
      Serial.println("WiFi Disconnected");
      //return false;
    }
  } else {
    Serial.print(iteration); Serial.println(" Not doing webcall, firing too fast");
  }
  //return false;
}

void loop() {
  // when no card present, display ntp sync events on serial
  events();

  // when no card present, reboot if we've told it to reboot
  if (shouldReboot) {
    rebootESP("Web Admin - Card Absent");
  }

  if (gotoMaintenanceMode) {
    Serial.println("Card Absent - Enable Maintenance");
    enterMaintenance();
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
          lcd.setCursor(0, 0); lcd.print(String(EEH_DEVICE));
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

        if (gotoMaintenanceMode) {
          Serial.println("Card Absent - Enable Maintenance");
          enterMaintenance();
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
  if (!inMaintenanceMode) {
    // not in maintenance mode, update LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Present Access Card");
  } else {
    // should be in maintenance mode, update LCD
    enterMaintenance();
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
  digitalWrite(RELAY, LOW);
  Serial.println(message);
  syslog.log(message);
}

void disableRelay(String message) {
  digitalWrite(RELAY, HIGH);
  Serial.println(message);
  syslog.log(message);
}

void disableLed(String message) {
  digitalWrite(ONBOARD_LED, LOW);
  Serial.println(message);
  syslog.log(message);
}

void enableLed(String message) {
  digitalWrite(ONBOARD_LED, HIGH);
  Serial.println(message);
  syslog.log(message);
}

void rebootESP(char* message) {
  Serial.print(iteration); Serial.print(" Rebooting ESP32: "); Serial.println(message);
  syslog.logf("%d Rebooting ESP32:%s", iteration, message);
  // wait 5 seconds to allow syslog to be sent
  delay(10000);
  ESP.restart();
}

String getFullStatus() {
  StaticJsonDocument<1600> fullStatusDoc;
  fullStatusDoc["Timestamp"] = printTime();
  fullStatusDoc["Hostname"] = DEVICE_HOSTNAME;
  fullStatusDoc["BootTime"] = bootTime;
  fullStatusDoc["AppName"] = APP_NAME;
  fullStatusDoc["EEHDevice"] = EEH_DEVICE;
  fullStatusDoc["OverrideUsers"] = getAccessOverrideCodes();
  fullStatusDoc["SyslogServer"] = SYSLOG_SERVER;
  fullStatusDoc["SyslogPort"] = SYSLOG_PORT;
  fullStatusDoc["ServerURL1"] = serverURL1;
  fullStatusDoc["ServerURL2"] = serverURL2;
  fullStatusDoc["Firmware"] = FIRMWARE_VERSION;
  fullStatusDoc["Temp"] = String((temprature_sens_read() - 32) / 1.8) + "C";
  fullStatusDoc["CompileTime"] = String(__DATE__) + " " + String(__TIME__);
  fullStatusDoc["MFRC522SlaveSelect"] = SS_PIN;
  fullStatusDoc["MFRC522ResetPin"] = RST_PIN;
  fullStatusDoc["MFRC522Firmware"] = getmfrcversion();
  fullStatusDoc["NTPServer"] = NTPSERVER;
  fullStatusDoc["NTPSyncTime"] = NTPSYNCTIME;
  fullStatusDoc["NTPTimeZone"] = NTPTIMEZONE;
  fullStatusDoc["NTPWaitSynctime"] = NTPWAITSYNCTIME;
  fullStatusDoc["NTPSyncStatus"] = getTimeStatus();
  fullStatusDoc["APIWait"] = waitTime;
  fullStatusDoc["RFIDDelay"] = checkCardTime;
  fullStatusDoc["ShouldReboot"] = shouldReboot;
  fullStatusDoc["WebServerPort"] = WEB_SERVER_PORT;
  fullStatusDoc["SSID"] = WiFi.SSID();
  fullStatusDoc["WifiStatus"] = WiFi.status();
  fullStatusDoc["MacAddress"] = WiFi.macAddress();
  fullStatusDoc["IPAddress"] = WiFi.localIP().toString();
  fullStatusDoc["Subnet"] = WiFi.subnetMask().toString();
  fullStatusDoc["Gateway"] = WiFi.gatewayIP().toString();
  fullStatusDoc["DNS1"] = WiFi.dnsIP(0).toString();
  fullStatusDoc["DNS2"] = WiFi.dnsIP(1).toString();
  fullStatusDoc["DNS3"] = WiFi.dnsIP(2).toString();
  fullStatusDoc["RelayPin"] = RELAY;
  fullStatusDoc["LCDI2C"] = "0x" + String(LCD_I2C, HEX);
  fullStatusDoc["LCDWidth"] = LCD_WIDTH;
  fullStatusDoc["LCDHeight"] = LCD_HEIGHT;

  // note this is the opposite of what is expected due to the way the relay works
  if (digitalRead(RELAY)) {
    fullStatusDoc["RelayPinStatus"] = "off";
  } else {
    fullStatusDoc["RelayPinStatus"] = "on";
  }

  fullStatusDoc["LEDPin"] = ONBOARD_LED;
  if (digitalRead(ONBOARD_LED)) {
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
  fullStatusDoc["inMaintenanceMode"] = inMaintenanceMode;
  fullStatusDoc["inOverrideMode"] = inOverrideMode;

  String fullStatus = "";
  serializeJson(fullStatusDoc, fullStatus);

  return fullStatus;
}

String getStatus() {
  Serial.println("getting un-authed status");
  StaticJsonDocument<200> shortStatusDoc;
  shortStatusDoc["Timestamp"] = printTime();
  shortStatusDoc["Hostname"] = DEVICE_HOSTNAME;

  // note this is the opposite of what is expected due to the way the relay works
  if (digitalRead(RELAY)) {
    shortStatusDoc[EEH_DEVICE] = "off";
  } else {
    shortStatusDoc[EEH_DEVICE] = "on";
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

void enterMaintenance() {
  // only run maintance mode once
  gotoMaintenanceMode = false;

  // flag that we are in maintenance mode
  inMaintenanceMode = true;

  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(String(EEH_DEVICE));
  lcd.setCursor(0, 1); lcd.print("MAINTENANCE MODE");
  lcd.setCursor(0, 2); lcd.print("ALL ACCESS DENIED");
  lcd.setCursor(0, 3); lcd.print("");
}
