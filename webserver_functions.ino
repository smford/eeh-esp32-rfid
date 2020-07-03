// checks state of a pin, used when writing button slider position
String outputState(int PINCHECK) {
  if (digitalRead(PINCHECK)) {
    return "checked";
  }
  else {
    return "";
  }
  return "";
}

// used by server.on functions to discern whether a user has the correct httpapitoken OR is authenticated by username and password
bool checkUserWebAuth(AsyncWebServerRequest * request) {
  bool isAuthenticated = false;

  if (request->hasParam("api") && (strcmp(request->getParam("api")->value().c_str(), config.httpapitoken.c_str()) == 0)) {
    isAuthenticated = true;
  }

  if (request->authenticate(config.httpuser.c_str(), config.httppassword.c_str())) {
    Serial.println("is authenticated via username and password");
    isAuthenticated = true;
  }
  return isAuthenticated;
}


// handles uploads to the filserver
void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  // make sure authenticated before allowing upload
  if (!request->authenticate(config.httpuser.c_str(), config.httppassword.c_str())) {
    return request->requestAuthentication();
  }

  String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
  Serial.println(logmessage);
  syslog.log(logmessage);

  if (!index) {
    logmessage = "Upload Start: " + String(filename);
    // open the file on first call and store the file handle in the request object
    request->_tempFile = SPIFFS.open("/" + filename, "w");
    Serial.println(logmessage);
    syslog.log(logmessage);
  }

  if (len) {
    // stream the incoming chunk to the opened file
    request->_tempFile.write(data, len);
    logmessage = "Writing file: " + String(filename) + " index=" + String(index) + " len=" + String(len);
    Serial.println(logmessage);
    syslog.log(logmessage);
  }

  if (final) {
    logmessage = "Upload Complete: " + String(filename) + ",size: " + String(index + len);
    // close the file handle as the upload is now done
    request->_tempFile.close();
    Serial.println(logmessage);
    syslog.log(logmessage);
    request->redirect("/");
  }
}

// checks whether in maintenance mode, used when writing out button slider position
String isInMaintenance() {
  if (config.inmaintenance) {
    return "checked";
  } else {
    return "";
  }
  return "";
}

// parses and processes index.html
String processor(const String& var) {

  if (var == "MAINTENANCEMODESLIDER") {
    String buttons = "";
    String outputStateValue = isInMaintenance();
    buttons += "<p>MAINTENANCE MODE</p><p><label class='switch'><input type='checkbox' onchange='toggleMaintenance(this)' id='mmslider' " + outputStateValue + "><span class='slider'></span></label></p>";
    return buttons;
  }

  if (var == "LEDSLIDER") {
    String buttons = "";
    String outputStateValue = outputState(config.ledpin);
    buttons += "<p>LED</p><p><label class='switch'><input type='checkbox' onchange='toggleCheckbox(this, \"led\")' id='ledslider' " + outputStateValue + "><span class='slider'></span></label></p>";
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

void notFound(AsyncWebServerRequest *request) {
  String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
  Serial.println(logmessage);
  syslog.log(logmessage);
  request->send(404, "text/plain", "Not found");
}

void configureWebServer() {
  // configure web server

  // if url isn't found
  server->onNotFound(notFound);

  // https://randomnerdtutorials.com/esp32-esp8266-web-server-http-authentication/
  // Route for root / web page
  server->on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
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

  server->on("/logout", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(401);
  });

  server->on("/file", HTTP_GET, [](AsyncWebServerRequest * request) {
    if (!request->authenticate(config.httpuser.c_str(), config.httppassword.c_str())) {
      return request->requestAuthentication();
    }
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
    Serial.println(logmessage);
    syslog.log(logmessage);

    int args = request->args();
    for (int i = 0; i < args; i++) {
      Serial.printf("ARG[%s]: %s\n", request->argName(i).c_str(), request->arg(i).c_str());
    }

    const char *fileName = request->getParam("name")->value().c_str();
    const char *fileAction = request->getParam("action")->value().c_str();

    if (!SPIFFS.exists(fileName)) {
      request->send(200, "text/plain", "ERROR");
    } else {
      if (strcmp(fileAction, "download") == 0) {
        request->send(SPIFFS, fileName, "application/octet-stream");
      }
      if (strcmp(fileAction, "delete") == 0) {
        SPIFFS.remove(fileName);
        request->send(200, "text/plain", "Deleted File: " + String(fileName));
      }
    }
  });

  server->on("/listfiles", HTTP_GET, [](AsyncWebServerRequest * request)
  {
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
    if (checkUserWebAuth(request)) {
      logmessage += " Success";
      Serial.println(logmessage);
      syslog.log(logmessage);
      request->send(200, "text/plain", listFiles(true));
    } else {
      logmessage += " Failed Auth";
      Serial.println(logmessage);
      syslog.log(logmessage);
      return request->requestAuthentication();
    }
  });

  // run handleUpload function when any file is uploaded
  server->onFileUpload(handleUpload);

  server->on("/maintenance", HTTP_GET, [](AsyncWebServerRequest * request) {

    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();

    if (checkUserWebAuth(request)) {
      logmessage += " Success";
      Serial.println(logmessage);
      syslog.log(logmessage);

      logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();

      if (request->hasParam("state")) {
        String returnText;

        const char* selectState = request->getParam("state")->value().c_str();

        if (strcmp(selectState, "enable") == 0) {
          logmessage += " Enabling maintenance mode";
          returnText = "MAINTENANCE MODE";

          // if currently NOT in maintenance mode, toggle it on
          if (!config.inmaintenance) {
            gotoToggleMaintenance = true;
          }

        } else if (strcmp(selectState, "disable") == 0) {
          logmessage += " Disabling maintenance mode";
          returnText = "";

          // if currently in maintenance mode, toggle it off
          if (config.inmaintenance) {
            gotoToggleMaintenance = true;
          }
        } else {
          logmessage += " ERROR: invalid state sent to maintenance mode, ignoring: " + String(selectState);
          returnText = "ERROR: invalid state sent to maintenance mode, ignoring: " + String(selectState);
          gotoToggleMaintenance = false;
        }

        Serial.println(logmessage);
        syslog.log(logmessage);
        request->send(200, "text/html", returnText);
      } else {
        request->send(200, "text/html", "ERROR: state not defined");
      }

    } else {
      logmessage += " Failed Auth";
      Serial.println(logmessage);
      syslog.log(logmessage);
      return request->requestAuthentication();
    }
  });

  server->on("/backlighton", HTTP_GET, [](AsyncWebServerRequest * request) {
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
    if (checkUserWebAuth(request)) {
      logmessage += " Success";
      Serial.println(logmessage);
      syslog.log(logmessage);
      lcd->backlight();
      request->send(200, "text/html", "LCD Backlight On");
    } else {
      logmessage += " Failed Auth";
      Serial.println(logmessage);
      syslog.log(logmessage);
      return request->requestAuthentication();
    }
  });


  server->on("/backlightoff", HTTP_GET, [](AsyncWebServerRequest * request) {
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
    if (checkUserWebAuth(request)) {
      logmessage += " Success";
      Serial.println(logmessage);
      syslog.log(logmessage);
      lcd->noBacklight();
      request->send(200, "text/html", "LCD Backlight Off");
    } else {
      logmessage += " Failed Auth";
      Serial.println(logmessage);
      syslog.log(logmessage);
      return request->requestAuthentication();
    }
  });

  server->on("/logged-out", HTTP_GET, [](AsyncWebServerRequest * request) {
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
    Serial.println(logmessage);
    syslog.log(logmessage);
    request->send_P(200, "text/html", logout_html, processor);
  });

  server->on("/reboot", HTTP_GET, [](AsyncWebServerRequest * request) {
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();

    if (checkUserWebAuth(request)) {
      request->send(200, "text/html", reboot_html);
      logmessage += " Success";
      Serial.println(logmessage);
      syslog.log(logmessage);
      shouldReboot = true;
    } else {
      logmessage += " Failed Auth";
      Serial.println(logmessage);
      syslog.log(logmessage);
      return request->requestAuthentication();
    }
  });

  server->on("/getuser", HTTP_GET, [](AsyncWebServerRequest * request) {
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
    if (checkUserWebAuth(request)) {
      logmessage += " Success";
      Serial.println(logmessage);
      syslog.log(logmessage);
      String getUserURL = config.serverurl + config.getuserpage + "?device=" + config.device + "&rfid=" + String(currentRFIDcard) + "&api=" + config.serverapitoken;
      request->send(200, "text/html", getUserDetails(getUserURL.c_str()));
    } else {
      logmessage += " Failed Auth";
      Serial.println(logmessage);
      syslog.log(logmessage);
      return request->requestAuthentication();
    }
  });

  server->on("/grant", HTTP_GET, [](AsyncWebServerRequest * request) {
    if (!request->authenticate(config.httpuser.c_str(), config.httppassword.c_str())) {
      return request->requestAuthentication();
    }
    const char* access = request->getParam("access")->value().c_str();
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " RFID:" + String(currentRFIDcard) + " " + request->url() + "?access=" + access;
    Serial.println(logmessage);
    syslog.log(logmessage);
    String grantURL;
    //tempstring.toCharArray(grantURL, tempstring.length() + 1);
    Serial.print("GrantURL: "); Serial.println(grantURL);
    if (strcmp(access, "grant") == 0) {
      // granting access
      grantURL = config.serverurl + config.moduserpage + "?device=" + config.device + "&modrfid=" + String(currentRFIDcard) + "&api=" + config.serverapitoken + "&access=true";
      logmessage = "Web Admin: Granting access for " + String(currentRFIDcard);
    } else {
      // default fall back to revoking access
      grantURL = config.serverurl + config.moduserpage + "?device=" + config.device + "&modrfid=" + String(currentRFIDcard) + "&api=" + config.serverapitoken + "&access=false";
      logmessage = "Web Admin: Revoking access for " + String(currentRFIDcard);
    }
    Serial.println(logmessage);
    syslog.log(logmessage);
    request->send(200, "text/html", grantAccess(grantURL.c_str()));
  });

  server->on("/ntprefresh", HTTP_GET, [](AsyncWebServerRequest * request) {
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
    if (checkUserWebAuth(request)) {
      logmessage += " Success";
      Serial.println(logmessage);
      syslog.log(logmessage);
      updateNTP();
      request->send(200, "text/html", printTime());
    } else {
      logmessage += " Failed Auth";
      Serial.println(logmessage);
      syslog.log(logmessage);
      return request->requestAuthentication();
    }
  });

  server->on("/logout-current-user", HTTP_GET, [](AsyncWebServerRequest * request) {
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
    if (checkUserWebAuth(request)) {
      logmessage += " Success";
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
    } else {
      logmessage += " Failed Auth";
      Serial.println(logmessage);
      syslog.log(logmessage);
      return request->requestAuthentication();
    }
  });

  server->on("/health", HTTP_GET, [](AsyncWebServerRequest * request) {
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
    Serial.println(logmessage);
    syslog.log(logmessage);
    request->send(200, "text/plain", "OK");
  });

  server->on("/fullstatus", HTTP_GET, [](AsyncWebServerRequest * request) {
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
    if (checkUserWebAuth(request)) {
      logmessage += " Success";
      Serial.println(logmessage);
      syslog.log(logmessage);
      request->send(200, "application/json", getFullStatus());
    } else {
      logmessage += " Failed Auth";
      Serial.println(logmessage);
      syslog.log(logmessage);
      return request->requestAuthentication();
    }
  });

  server->on("/status", HTTP_GET, [](AsyncWebServerRequest * request) {
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
    Serial.println(logmessage);
    syslog.log(logmessage);
    request->send(200, "application/json", getStatus());
  });

  // used for checking whether time is sync
  server->on("/time", HTTP_GET, [](AsyncWebServerRequest * request) {
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
    Serial.println(logmessage);
    syslog.log(logmessage);
    request->send(200, "text/plain", printTime());
  });

  // called when slider has been toggled
  server->on("/toggle", HTTP_GET, [] (AsyncWebServerRequest * request) {
    if (!request->authenticate(config.httpuser.c_str(), config.httppassword.c_str())) {
      return request->requestAuthentication();
    }
    String inputMessage;
    String inputPin;
    if (request->hasParam("state") && request->hasParam("pin")) {
      inputMessage = request->getParam("state")->value();
      inputPin = request->getParam("pin")->value();

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
}
