#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <WiFiUdp.h>
#include <Syslog.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// syslog library: https://github.com/arcao/Syslog v2.0
// mfrc522 library: https://github.com/miguelbalboa/rfid  v1.4.6
// arduinojson library: https://github.com/bblanchon/ArduinoJson & https://arduinojson.org/ v6.15.2


#define SYSLOG_SERVER "192.168.10.21"
#define SYSLOG_PORT 514
#define DEVICE_HOSTNAME "esp32-1.home.narco.tk"
#define APP_NAME "eeh-esp32-rfid-laser"
#define EEH_DEVICE "laser"
#define WEB_SERVER_PORT 80
#define FIRMWARE_VERSION "v1.0"

const char* ssid = "somessid";
const char* password = "xxxx";
//const char* serverURL1 = "https://mock-rfid-system.herokuapp.com/check?rfid=";
const char* serverURL1 = "http://192.168.10.21:56000/check?rfid=";
const char* serverURL2 = "&device=laser";
const int RST_PIN = 22; // Reset pin
const int SS_PIN = 21; // Slave select pin

const int RELAY = 26;
const int ONBOARD_LED = 2;

//=======
const char* http_username = "admin";
const char* http_password = "admin";

const char* PARAM_INPUT_1 = "state";
const char* PARAM_INPUT_2 = "pin";

const int output = 2;
//=======

char *accessOverrideCodes[] = {"90379632", "boss2", "boss3"};

char* serverURL;
unsigned long sinceLastRunTime = 0;
unsigned long waitTime = 2; // in seconds
unsigned long checkCardTime = 5; // in secondsE
String returnedJSON;
uint8_t control = 0x00;

char* currentRFIDcard = "";
bool currentRFIDaccess = false;

// should we reboot the server?
bool shouldReboot = false;

MFRC522 mfrc522(SS_PIN, RST_PIN); // Create MFRC522 instance

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udpClient;

Syslog syslog(udpClient, SYSLOG_SERVER, SYSLOG_PORT, DEVICE_HOSTNAME, APP_NAME, LOG_INFO);

int iteration = 0; // holds the MSGID number for syslog, also represents the instance number of RFID action (connection or removal)

AsyncWebServer server(WEB_SERVER_PORT);

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
  <button onclick="logoutButton()">Logout</button>
  <p>Firmware Version: %FIRMWARE%</p>
  <p>Current RFID Card: %PRESENTRFID%</p>
  <p>Current RFID Access: %RFIDACCESS%</p>
  %BUTTONPLACEHOLDER1%
  %BUTTONPLACEHOLDER2%
  <button onclick="rebootButton()">Reboot</button>
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
function rebootButton() {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/reboot", true);
  xhr.send();
  // i think _self needs to be changed to /index.html or something similar to prevent refreshes of the webpage and cached credentials constantly rebooting esp32
  setTimeout(function(){ window.open("/","_self"); }, 10000);
}
</script>
</body>
</html>
)rawliteral";

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

// Replaces placeholder with button section in your web page
String processor(const String& var) {
  if (var == "BUTTONPLACEHOLDER1") {
    String buttons = "";
    String outputStateValue = outputState(ONBOARD_LED);
    buttons+= "<p>LED</p><p><label class='switch'><input type='checkbox' onchange='toggleCheckbox(this, \"led\")' id='output' " + outputStateValue + "><span class='slider'></span></label></p>";
    return buttons;
  }

  if (var == "BUTTONPLACEHOLDER2") {
    String buttons = "";
    String outputStateValue = outputState(RELAY);
    
    // because HIGH = off for the relay, this needs to be reversed to make web button work
    if (outputStateValue == "checked") {
      outputStateValue = "";
    } else {
      outputStateValue = "checked";
    }

    buttons+= "<p>RELAY</p><p><label class='switch'><input type='checkbox' onchange='toggleCheckbox(this, \"relay\")' id='output' " + outputStateValue + "><span class='slider'></span></label></p>";
    return buttons;
  }

  if (var == "EEH_HOSTNAME") {
    String hostname = "default hostname";
    hostname = DEVICE_HOSTNAME;
    return hostname;
  }

  if (var == "PRESENTRFID") {
    if (strcmp(currentRFIDcard, "") == 0) {
      return "no one";
    } else {
      return currentRFIDcard;
    }
  }

  if (var == "RFIDACCESS") {
    if (currentRFIDaccess) {
      return "Granted";
    } else {
      return "Denied";
    }
  }

  if (var == "FIRMWARE") {
    return FIRMWARE_VERSION;
  }

  return String();
}

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

  SPI.begin(); // Init SPI bus
  mfrc522.PCD_Init(); // Init MFRC522
  delay(4); // delay needed to allow mfrc522 to spin up properly

  Serial.println("\nSystem Configuration:");
  Serial.println("---------------------");
  Serial.print("        Hostname: "); Serial.println(DEVICE_HOSTNAME);
  Serial.print("        App Name: "); Serial.println(APP_NAME);
  Serial.print("      EEH Device: "); Serial.println(EEH_DEVICE);
  Serial.print("   Syslog Server: "); Serial.print(SYSLOG_SERVER); Serial.print(":"); Serial.println(SYSLOG_PORT);
  Serial.print("   API Wait Time: "); Serial.print(waitTime); Serial.println(" seconds");
  Serial.print(" RFID Card Delay: "); Serial.print(checkCardTime); Serial.println(" seconds");
  Serial.print("       Relay Pin: "); Serial.println(RELAY);
  Serial.print("         LED Pin: "); Serial.println(ONBOARD_LED);
  Serial.print(" Web Server Port: "); Serial.println(WEB_SERVER_PORT);
  Serial.print("     ESP32 Flash: "); Serial.println(FIRMWARE_VERSION);
  mfrc522.PCD_DumpVersionToSerial(); // Show details of PCD - MFRC522 Card Reader details

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

  syslog.logf("Booted:%s", DEVICE_HOSTNAME);

  pinMode(ONBOARD_LED, OUTPUT);
  pinMode(RELAY, OUTPUT);
  disableLed();
  disableRelay("Disabled upon boot");

  //=============
  // https://randomnerdtutorials.com/esp32-esp8266-web-server-http-authentication/
  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!request->authenticate(http_username, http_password))
      return request->requestAuthentication();
    request->send_P(200, "text/html", index_html, processor);
  });

  server.on("/logout", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(401);
  });

  server.on("/logged-out", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", logout_html, processor);
  });

  server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!request->authenticate(http_username, http_password)) {
      return request->requestAuthentication();
    }
    shouldReboot = true;
  });

  server.on("/health", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("Healthcheck Fired");
    syslog.logf("Healthcheck Fired");
    request->send(200, "text/plain", "OK");
  });

  server.on("/fullstatus", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("Full Status Check Fired");
    syslog.logf("Full Status Check Fired");
    if (!request->authenticate(http_username, http_password)) {
      return request->requestAuthentication();
    }
    request->send(200, "application/json", getFullStatus());
  });

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("Status Check Fired");
    syslog.logf("Status Check Fired");
    request->send(200, "application/json", getStatus());
  });

  // Send a GET request to <ESP_IP>/update?state=<inputMessage>
  server.on("/update", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username, http_password)) {
      return request->requestAuthentication();
    }
    String inputMessage;
    String inputParam;
    String inputPin;
    // GET input1 value on <ESP_IP>/update?state=<inputMessage>
    if (request->hasParam(PARAM_INPUT_1) && request->hasParam(PARAM_INPUT_2)) {
      inputMessage = request->getParam(PARAM_INPUT_1)->value();
      inputParam = PARAM_INPUT_1; // check to remove
      inputPin = request->getParam(PARAM_INPUT_2)->value();

      if (inputPin == "relay") {
        if (inputMessage.toInt() == 1) {
          enableRelay("Web Admin");
          Serial.print(iteration); Serial.println(" Admin Web: Enable Relay");
          syslog.logf("%d Admin Web: Enable Relay", iteration);
        } else {
          disableRelay("Web Admin");
          Serial.print(iteration); Serial.println(" Admin Web: Disable Relay");
          syslog.logf("%d Admin Web: Disable Relay", iteration);
        }
      }

      if (inputPin == "led") {
        if (inputMessage.toInt() == 1) {
          enableLed();
          Serial.print(iteration); Serial.println(" Admin Web: Enable LED");
          syslog.logf("%d Admin Web: Enable LED", iteration);
        } else {
          disableLed();
          Serial.print(iteration); Serial.println(" Admin Web: Disable LED");
          syslog.logf("%d Admin Web: Disable LED", iteration);
        }
      }

    } else {
      inputMessage = "No message sent";
      inputParam = "none"; // check to remove
    }
    //Serial.println(inputMessage);
    request->send(200, "text/plain", "OK");
  });

  // Start server
  server.begin();
  //=============
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
      //syslog.logf("Wifi Connected: %s", DEVICE_HOSTNAME);
      StaticJsonDocument<200> doc;
      char serverURL[240];
      sprintf(serverURL, "%s%s%s", serverURL1, foundrfid, serverURL2);
      Serial.print(iteration); Serial.print(" dowebcall ServerURL: "); Serial.println(serverURL);

      returnedJSON = httpGETRequest(serverURL);
      Serial.print(iteration); Serial.print(" ReturnedJSON:"); Serial.println(returnedJSON);

      Serial.print(iteration); Serial.println(" JSON Deserialization");
      DeserializationError error = deserializeJson(doc, returnedJSON);
      if (error) {
        Serial.print(iteration); Serial.print(F(" DeserializeJson() failed: ")); Serial.println(error.c_str());
        syslog.logf(LOG_ERR,  "%d Error Decoding JSON: %s", iteration, error.c_str());
        //return false;
      }

      Serial.print(iteration); Serial.println(" Assigning Variables");
      const char* Timestamp = doc["Timestamp"];
      const char* RFID = doc["RFID"];
      const char* EEHDevice = doc["EEHDevice"];
      const char* Grant = doc["Grant"];
      Serial.print(iteration); Serial.print(" Timestamp: "); Serial.println(Timestamp);
      Serial.print(iteration); Serial.print("      RFID: "); Serial.println(RFID);
      Serial.print(iteration); Serial.print(" EEHDevice: "); Serial.println(EEHDevice);
      Serial.print(iteration); Serial.print("     Grant: "); Serial.println(Grant);

      Serial.print(iteration); Serial.println(" Checking access");
      if (strcmp(RFID, foundrfid) == 0) {
        Serial.print(iteration); Serial.println(" RFID Matches");
        if (strcmp(Grant, "true") == 0) {
          if (strcmp(EEH_DEVICE, EEHDevice) == 0) {
            Serial.print(iteration); Serial.println(" Devices Match");
            syslog.logf("%d ACCESS GRANTED:%s for %s", iteration, foundrfid, EEHDevice);
            currentRFIDaccess = true;
            enableLed();
            enableRelay(currentRFIDcard);
          } else {
            Serial.print(iteration); Serial.print(" ERROR: Device Mismatch: DetectedDevice: "); Serial.print(EEH_DEVICE); Serial.print(" JSONDevice:"); Serial.println(EEHDevice);
            syslog.logf(LOG_ERR, "%d ERROR: Device Mismatch: DetectedDevice:%s JSONDEevice:%s", iteration, EEH_DEVICE, EEHDevice);
          }
        } else {
          Serial.print(iteration); Serial.print(" ERROR: Access Denied: "); Serial.print(foundrfid); Serial.print(" for "); Serial.println(EEHDevice);
          syslog.logf(LOG_ERR, "%d ERROR: Access Denied: %s for %s", iteration, foundrfid, EEHDevice);
        }
      } else {
        Serial.print(iteration); Serial.print(" ERROR: RFID Mismatch: DetectedRFID: "); Serial.print(foundrfid); Serial.print(" JSONRFID:"); Serial.println(RFID);
        syslog.logf(LOG_ERR, "%d ERROR: Access Denied DetectedRFID:%s JSONRFID:%s for %s", iteration, foundrfid, RFID, EEHDevice);
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
  // reboot from web admin set
  if (shouldReboot) {
    rebootESP("Web Admin");
  }

  if ( !mfrc522.PICC_IsNewCardPresent()) {
    // no new card found, re-loop
    return;
  }

  if ( !mfrc522.PICC_ReadCardSerial()) {
    // no serial means no real card found, re-loop
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

    //Serial.print("control:");Serial.println(control);
    if (control == 13 || control == 14) {
      //Serial.println("Card present");

      if (strcmp(currentRFIDcard, newcard) != 0) {
        Serial.print(iteration); Serial.print(" New Card Found: "); Serial.println(newcard);
        syslog.logf("%d New Card Found:%s", iteration, newcard);
        currentRFIDcard = newcard;

        // check accessOverrideCodes
        bool overRideActive = false;
        for (byte i = 0; i < (sizeof(accessOverrideCodes) / sizeof(accessOverrideCodes[0])); i++) {
          if (strcmp(currentRFIDcard, accessOverrideCodes[i]) == 0) {
            Serial.print(iteration); Serial.print(" Access Override Detected: "); Serial.println(accessOverrideCodes[i]);
            syslog.logf("%d Access Override Detected for %s on %s", iteration, currentRFIDcard, EEH_DEVICE);
            overRideActive = true;
          }
        }

        if (overRideActive) {
          // boss detected!
          enableLed();
          enableRelay(currentRFIDcard);
          currentRFIDaccess = true;
        } else {
          // normal user, do webcall
          dowebcall(newcard);
        }
        //===========


      } else {
        //Serial.println("same card, not checking again");
      }
    } else {
      break;
    }
  }


  Serial.print(iteration); Serial.print(" Card Removed: "); Serial.println(newcard);
  syslog.logf("%d Card Removed:%s", iteration, newcard);

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  disableLed();
  disableRelay(currentRFIDcard);
  currentRFIDcard = "";
  currentRFIDaccess = false;
  delay((checkCardTime * 1000));

  // Dump debug info about the card; PICC_HaltA() is automatically called
  //Serial.println("Starting picc_dumptoserial");
  //mfrc522.PICC_DumpToSerial(&(mfrc522.uid));
}

void disableRelay(char* message) {
  digitalWrite(RELAY, HIGH);
  Serial.print(iteration); Serial.print(" Disable relay: "); Serial.println(message);
  syslog.logf("%d Relay Disabled:%s", iteration, message);
}

void enableRelay(char* message) {
  digitalWrite(RELAY, LOW);
  Serial.print(iteration); Serial.print(" Enable relay: "); Serial.println(message);
  syslog.logf("%d Relay Enabled:%s", iteration, message);
}

void disableLed() {
  digitalWrite(ONBOARD_LED, LOW);
}

void enableLed() {
  digitalWrite(ONBOARD_LED, HIGH);
}

void rebootESP(char* message) {
  Serial.print(iteration); Serial.print(" Rebooting ESP32: "); Serial.println(message);
  syslog.logf("%d Rebooting ESP32:%s", iteration, message);
  // wait 5 seconds to allow syslog to be sent
  delay(5000);
  ESP.restart();
}

String getFullStatus() {
  Serial.println("getting status");
  // {"Timestamp":"2020-06-12 01:24:29.979233047 +0100 BST m=+204897.028579088","RFID":"7AA5E03F","EEHDevice":"laser","Grant":"true"}
  //{"Timestamp":"2020-06-12 01:24:29.979233047 +0100 BST m=+204897.028579088","Hostname":"", "AppName":"", "EEHDevice":"laser", "SyslogServer":"", "SyslogPort":"", "Firmware":"", "APIWait":"", "RFIDDelay":"", "WebServerPort":"", "SSID":"", "WifiStatus":"", "MacAddress":"", "IPAddress":"", "Subnet":"", "Gateway":"", "DNS1":"", "DNS2":"", "DNS3":"", "RelayPin":"", "RelayPinStatus":"", "LEDPin":"", "LEDPinStatus":"", "RFID":"7AA5E03F","Grant":"true"}
  String mockFullStatus = "{\"Timestamp\":\"2020-06-12 01:24:29.979233047 +0100 BST m=+204897.028579088\",\"Hostname\":\"\", \"AppName\":\"\", \"EEHDevice\":\"laser\", \"SyslogServer\":\"\", \"SyslogPort\":\"\", \"Firmware\":\"\", \"APIWait\":\"\", \"RFIDDelay\":\"\", \"WebServerPort\":\"\", \"SSID\":\"\", \"WifiStatus\":\"\", \"MacAddress\":\"\", \"IPAddress\":\"\", \"Subnet\":\"\", \"Gateway\":\"\", \"DNS1\":\"\", \"DNS2\":\"\", \"DNS3\":\"\", \"RelayPin\":\"\", \"RelayPinStatus\":\"\", \"LEDPin\":\"\", \"LEDPinStatus\":\"\", \"RFID\":\"7AA5E03F\",\"Grant\":\"true\"}";

  return mockFullStatus;
}

String getStatus() {
  Serial.println("getting un-authed status");
  return "getting un-authed status";
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
