#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <WiFiUdp.h>
#include <Syslog.h>

// syslog library: https://github.com/arcao/Syslog v2.0
// mfrc522 library: https://github.com/miguelbalboa/rfid  v1.4.6
// arduinojson library: https://github.com/bblanchon/ArduinoJson & https://arduinojson.org/ v6.15.2


#define SYSLOG_SERVER "192.168.10.21"
#define SYSLOG_PORT 514
#define DEVICE_HOSTNAME "esp32-1.home.narco.tk"
#define APP_NAME "eeh-esp-rfid-laser"
#define EEH_DEVICE "laser"
#define ONBOARD_LED 2

const char* ssid = "somessid";
const char* password = "xxxx";
//const char* serverURL1 = "https://mock-rfid-system.herokuapp.com/check?rfid=";
const char* serverURL1 = "http://192.168.10.21:56000/check?rfid=";
const char* serverURL2 = "&device=laser";
const int RST_PIN = 22; // Reset pin
const int SS_PIN = 21; // Slave select pin

const int RELAY = 26;

char *accessOverrideCodes[] = {"90379632", "boss2", "boss3"};

char* serverURL;
unsigned long sinceLastRunTime = 0;
unsigned long waitTime = 2; // in seconds
unsigned long checkCardTime = 5; // in secondsE
String returnedJSON;
int loopnumber = 0;
uint8_t control = 0x00;

char* currentRFIDcard = "";

MFRC522 mfrc522(SS_PIN, RST_PIN); // Create MFRC522 instance

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udpClient;

Syslog syslog(udpClient, SYSLOG_SERVER, SYSLOG_PORT, DEVICE_HOSTNAME, APP_NAME, LOG_INFO);

int iteration = 0; // holds the MSGID number for syslog, also represents the instance number of RFID action (connection or removal)

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
  Serial.print(" RFID Card Delay: "); Serial.print(checkCardTime); Serial.println(" seconds");
  mfrc522.PCD_DumpVersionToSerial(); // Show details of PCD - MFRC522 Card Reader details

  Serial.print("\nConnecting to Wifi: ");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  //syslog.logf("Booted: %s", DEVICE_HOSTNAME);

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
  Serial.print("API Wait Time: "); Serial.print(waitTime); Serial.println(" seconds");
  Serial.println();

  // configure led
  pinMode(ONBOARD_LED, OUTPUT);
  pinMode(RELAY, OUTPUT);
  disableRelay();
}

void dowebcall(const char *foundrfid) {
  Serial.print(iteration); Serial.println(" Starting dowebcall");
  //StaticJsonDocument<200> doc;
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

      //Serial.println("before 1");
      returnedJSON = httpGETRequest(serverURL);
      Serial.print(iteration); Serial.print(" ReturnedJSON:"); Serial.println(returnedJSON);
      //Serial.println("after 1");

      //Serial.println("before 2");
      Serial.print(iteration); Serial.println(" JSON Deserialization");
      DeserializationError error = deserializeJson(doc, returnedJSON);
      //Serial.println("after 2");
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
            enableRelay();
          } else {
            Serial.print(iteration); Serial.print(" ERROR: Device Mismatch: DetectedDevice:"); Serial.print(EEH_DEVICE); Serial.print(" JSONDevice:"); Serial.println(EEHDevice);
            syslog.logf(LOG_ERR, "%d ERROR: Device Mismatch: DetectedDevice:%s JSONDEevice:%s", iteration, EEH_DEVICE, EEHDevice);
          }
        } else {
          Serial.print(iteration); Serial.print(" ERROR: Access Denied:"); Serial.print(foundrfid); Serial.print(" for "); Serial.println(EEHDevice);
          syslog.logf(LOG_ERR, "%d ERROR: Access Denied: %s for %s", iteration, foundrfid, EEHDevice);
        }
      } else {
        Serial.print(iteration); Serial.print(" ERROR: RFID Mismatch: DetectedRFID:"); Serial.print(foundrfid); Serial.print(" JSONRFID:"); Serial.println(RFID);
        syslog.logf(LOG_ERR, "%d ERROR: Access Denied DetectedRFID:%s JSONRFID:%s for %s", iteration, foundrfid, RFID, EEHDevice);
      }

      sinceLastRunTime = millis();
      //return true;
    } else {
      Serial.println("WiFi Disconnected");
      //return false;
    }
    //Serial.print("            Loop: "); Serial.println(loopnumber);
    //Serial.print("  currentRunTime: "); Serial.println(currentRunTime);
    //Serial.print("sinceLastRunTime: "); Serial.println(sinceLastRunTime);
  } else {
    Serial.print(iteration); Serial.println(" Not doing webcall, firing too fast");
  }
  //return false;
}

void loop() {


  if ( !mfrc522.PICC_IsNewCardPresent()) {
    // no new card found, re-loop
    return;
  }

  if ( !mfrc522.PICC_ReadCardSerial()) {
    // no serial means no real card found, re-loop
    return;
  }

  // new card detected
  char str[32] = "";
  array_to_string(mfrc522.uid.uidByte, 4, str);
  iteration++;
  Serial.print(iteration); Serial.print(" RFID Found: "); Serial.println(str);

  char serverURL[80];
  sprintf(serverURL, "%s%s%s", serverURL1, str, serverURL2);
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

      // Log message can be formated like with printf function.
      //syslog.logf(LOG_ERR,  "This is error message no. %d", iteration);

      if (strcmp(currentRFIDcard, str) != 0) {
        //Serial.print("old currentRFIDcard:"); Serial.println(currentRFIDcard);
        //Serial.print("        strRFIDcard:"); Serial.println(str);
        Serial.print(iteration); Serial.print(" New Card Found:"); Serial.println(str);
        syslog.logf("%d New Card Found:%s", iteration, str);
        //iteration++;
        currentRFIDcard = str;
        //Serial.print("new currentRFIDcard:"); Serial.println(currentRFIDcard);
        //Serial.print("        strRFIDcard:"); Serial.println(str);

        //===========
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
          enableRelay();
        } else {
          // normal user, do webcall
          dowebcall(str);
        }
        //===========


      } else {
        //Serial.println("same card, not checking again");
      }
    } else {
      break;
    }
  }


  Serial.print(iteration); Serial.println(" Card Removed");
  syslog.logf("%d Card Removed:%s", iteration, str);

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  disableRelay();
  currentRFIDcard = "";
  delay((checkCardTime * 1000));

  // Dump debug info about the card; PICC_HaltA() is automatically called
  //Serial.println("Starting picc_dumptoserial");
  //mfrc522.PICC_DumpToSerial(&(mfrc522.uid));
}

void disableRelay() {
  digitalWrite(ONBOARD_LED, LOW);
  digitalWrite(RELAY, HIGH);
  Serial.print(iteration); Serial.println(" Disable relay");
  syslog.logf("%d Relay Disabled:%s", iteration, currentRFIDcard);
}

void enableRelay() {
  digitalWrite(ONBOARD_LED, HIGH);
  digitalWrite(RELAY, LOW);
  Serial.print(iteration); Serial.println(" Enable relay");
  syslog.logf("%d Relay Enabled:%s", iteration, currentRFIDcard);
}

String httpGETRequest(const char* serverURL) {
  //Serial.println("before 3");
  HTTPClient http;

  http.begin(serverURL);
  //Serial.println("after 3");

  //Serial.println("before 4");
  int httpResponseCode = http.GET();
  //Serial.println("after 4");

  String payload = "{}";

  if (httpResponseCode > 0) {
    //Serial.println("before 5");
    Serial.print(iteration); Serial.print(" HTTP Response code:"); Serial.println(httpResponseCode);
    syslog.logf("%d HTTP Response Code:%d", iteration, httpResponseCode);
    //Serial.println("after 5");
    //Serial.println("before 7");
    payload = http.getString();
    //Serial.println("after 7");
  } else {
    //Serial.println("before 6");
    Serial.print(iteration); Serial.print(" ERROR: HTTP Response Code:"); Serial.println(httpResponseCode);
    syslog.logf(LOG_ERR, "%d ERROR: HTTP Response Code:%s", iteration, httpResponseCode);
    //Serial.println("after 6");
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
