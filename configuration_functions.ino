// based upon https://arduinojson.org/v6/example/config/
void loadConfiguration(const char *filename, Config &config) {
  Serial.println("Loading configuration from " + String(filename));

  // flag used to detect if a default value is loaded, if default value loaded initiate a save after load
  bool initiatesave = false;

  if (!SPIFFS.exists(filename)) {
    Serial.println(String(filename) + " not found");
    initiatesave = true;
  } else {
    Serial.println(String(filename) + " found");
  }

  // Open file for reading
  Serial.println("Opening " + String(filename));
  File file = SPIFFS.open(filename);

  if (!file) {
    Serial.println("ERROR: Failed to open file" + String(filename));
    return;
  }

  StaticJsonDocument<2000> doc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, file);
  if (error) {
    Serial.println(F("Failed to process configuration file, will load default configuration"));
    Serial.println("===ERROR===");
    Serial.println(error.c_str());
    Serial.println("===========");
  }

  // Copy values from the JsonDocument to the Config
  config.hostname = doc["hostname"].as<String>();
  if (config.hostname == "null") {
    initiatesave = true;
    config.hostname = default_hostname;
  }

  config.device = doc["device"].as<String>();
  if (config.device == "null") {
    initiatesave = true;
    config.device = default_device;
  }

  config.appname = doc["appname"].as<String>();
  if (config.appname == "null") {
    initiatesave = true;
    config.appname = default_appname;
  }

  config.ssid = doc["ssid"].as<String>();
  if (config.ssid == "null") {
    initiatesave = true;
    config.ssid = default_ssid;
  }

  config.wifipassword = doc["wifipassword"].as<String>();
  if (config.wifipassword == "null") {
    initiatesave = true;
    config.wifipassword = default_wifipassword;
  }

  config.relaypin = doc["relaypin"];
  if (config.relaypin == 0) {
    initiatesave = true;
    config.relaypin = default_relaypin;
  }

  config.ledpin = doc["ledpin"];
  if (config.ledpin == 0) {
    initiatesave = true;
    config.ledpin = default_ledpin;
  }

  config.httpuser = doc["httpuser"].as<String>();
  if (config.httpuser == "null") {
    initiatesave = true;
    config.httpuser = default_httpuser;
  }

  config.httppassword = doc["httppassword"].as<String>();
  if (config.httppassword == "null") {
    initiatesave = true;
    config.httppassword = default_httppassword;
  }

  config.httpapitoken = doc["httpapitoken"].as<String>();
  if (config.httpapitoken == "null") {
    initiatesave = true;
    config.httpapitoken = default_httpapitoken;
  }

  config.syslogserver = doc["syslogserver"].as<String>();
  if (config.syslogserver == "null") {
    initiatesave = true;
    config.syslogserver = default_syslogserver;
  }

  config.syslogport = doc["syslogport"];
  if (config.syslogport == 0) {
    initiatesave = true;
    config.syslogport = default_syslogport;
  }

  if (doc.containsKey("inmaintenance")) {
    config.inmaintenance = doc["inmaintenance"].as<bool>();
  } else {
    initiatesave = true;
    config.inmaintenance = default_inmaintenance;
  }

  config.ntptimezone = doc["ntptimezone"].as<String>();
  if (config.ntptimezone == "null") {
    initiatesave = true;
    config.ntptimezone = default_ntptimezone;
  }

  config.ntpsynctime = doc["ntpsynctime"];
  if (config.ntpsynctime == 0) {
    initiatesave = true;
    config.ntpsynctime = default_ntpsynctime;
  }

  config.ntpwaitsynctime = doc["ntpwaitsynctime"];
  if (config.ntpwaitsynctime == 0) {
    initiatesave = true;
    config.ntpwaitsynctime = default_ntpwaitsynctime;
  }

  config.ntpserver = doc["ntpserver"].as<String>();
  if (config.ntpserver == "null") {
    initiatesave = true;
    config.ntpserver = default_ntpserver;
  }

  config.mfrcslaveselectpin = doc["mfrcslaveselectpin"];
  if (config.mfrcslaveselectpin == 0) {
    initiatesave = true;
    config.mfrcslaveselectpin = default_mfrcslaveselectpin;
  }

  config.mfrcresetpin = doc["mfrcresetpin"];
  if (config.mfrcresetpin == 0) {
    initiatesave = true;
    config.mfrcresetpin = default_mfrcresetpin;
  }

  config.mfrccardwaittime = doc["mfrccardwaittime"];
  if (config.mfrccardwaittime == 0) {
    initiatesave = true;
    config.mfrccardwaittime = default_mfrccardwaittime;
  }

  config.lcdi2caddress = doc["lcdi2caddress"];
  if (config.lcdi2caddress == 0) {
    initiatesave = true;
    config.lcdi2caddress = default_lcdi2caddress;
  }

  config.lcdwidth = doc["lcdwidth"];
  if (config.lcdwidth == 0) {
    initiatesave = true;
    config.lcdwidth = default_lcdwidth;
  }

  config.lcdheight = doc["lcdheight"];
  if (config.lcdheight == 0) {
    initiatesave = true;
    config.lcdheight = default_lcdheight;
  }

  config.webserverporthttp = doc["webserverporthttp"];
  if (config.webserverporthttp == 0) {
    initiatesave = true;
    config.webserverporthttp = default_webserverporthttp;
  }

  config.webserverporthttps = doc["webserverporthttps"];
  if (config.webserverporthttps == 0) {
    initiatesave = true;
    config.webserverporthttps = default_webserverporthttps;
  }

  if (doc.containsKey("webapiwaittime")) {
    config.webapiwaittime = doc["webapiwaittime"].as<int>();
  } else {
    initiatesave = true;
    config.webapiwaittime = default_webapiwaittime;
  }

  if (doc.containsKey("webpagedelay")) {
    config.webpagedelay = doc["webpagedelay"].as<int>();
  } else {
    initiatesave = true;
    config.webpagedelay = default_webpagedelay;
  }

  config.serverurl = doc["serverurl"].as<String>();
  if (config.serverurl == "null") {
    initiatesave = true;
    config.serverurl = default_serverurl;
  }

  config.serverapitoken = doc["serverapitoken"].as<String>();
  if (config.serverapitoken == "null") {
    initiatesave = true;
    config.serverapitoken = default_serverapitoken;
  }

  config.checkuserpage = doc["checkuserpage"].as<String>();
  if (config.checkuserpage == "null") {
    initiatesave = true;
    config.checkuserpage = default_checkuserpage;
  }

  config.getuserpage = doc["getuserpage"].as<String>();
  if (config.getuserpage == "null") {
    initiatesave = true;
    config.getuserpage = default_getuserpage;
  }

  config.moduserpage = doc["moduserpage"].as<String>();
  if (config.moduserpage == "null") {
    initiatesave = true;
    config.moduserpage = default_moduserpage;
  }

  config.overridecodes = doc["overridecodes"].as<String>();
  if (config.overridecodes == "null") {
    initiatesave = true;
    config.overridecodes = default_overridecodes;
  }

  if (doc.containsKey("influxdbenable")) {
    config.influxdbenable = doc["influxdbenable"].as<bool>();
  } else {
    initiatesave = true;
    config.influxdbenable = default_influxdbenable;
  }

  config.influxdbserver = doc["influxdbserver"].as<String>();
  if (config.influxdbserver == "null") {
    initiatesave = true;
    config.influxdbserver = default_influxdbserver;
  }

  config.influxdbserverport = doc["influxdbserverport"];
  if (config.influxdbserverport == 0) {
    initiatesave = true;
    config.influxdbserverport = default_influxdbserverport;
  }

  config.influxdbshiptime = doc["influxdbshiptime"];
  if (config.influxdbshiptime == 0) {
    initiatesave = true;
    config.influxdbshiptime = default_influxdbshiptime;
  }

  file.close();

  if (initiatesave) {
    Serial.println("Default configuration values loaded, saving configuration to " + String(filename));
    saveConfiguration(filename, config);
  }
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

  StaticJsonDocument<2000> doc;

  // Set the values in the document
  doc["hostname"] = config.hostname;
  doc["device"] = config.device;
  doc["appname"] = config.appname;
  doc["ssid"] = config.ssid;
  doc["wifipassword"] = config.wifipassword;
  doc["relaypin"] = config.relaypin;
  doc["ledpin"] = config.ledpin;
  doc["httpuser"] = config.httpuser;
  doc["httppassword"] = config.httppassword;
  doc["httpapitoken"] = config.httpapitoken;
  doc["syslogserver"] = config.syslogserver;
  doc["syslogport"] = config.syslogport;
  doc["inmaintenance"] = config.inmaintenance;
  doc["ntptimezone"] = config.ntptimezone;
  doc["ntpsynctime"] = config.ntpsynctime;
  doc["ntpwaitsynctime"] = config.ntpwaitsynctime;
  doc["ntpserver"] = config.ntpserver;
  doc["mfrcslaveselectpin"] = config.mfrcslaveselectpin;
  doc["mfrcresetpin"] = config.mfrcresetpin;
  doc["mfrccardwaittime"] = config.mfrccardwaittime;
  doc["lcdi2caddress"] = config.lcdi2caddress;
  doc["lcdwidth"] = config.lcdwidth;
  doc["lcdheight"] = config.lcdheight;
  doc["webserverporthttp"] = config.webserverporthttp;
  doc["webserverporthttps"] = config.webserverporthttps;
  doc["webapiwaittime"] = config.webapiwaittime;
  doc["webpagedelay"] = config.webpagedelay;
  doc["serverurl"] = config.serverurl;
  doc["serverapitoken"] = config.serverapitoken;
  doc["checkuserpage"] = config.checkuserpage;
  doc["getuserpage"] = config.getuserpage;
  doc["moduserpage"] = config.moduserpage;
  doc["overridecodes"] = config.overridecodes;
  doc["influxdbenable"] = config.influxdbenable;
  doc["influxdbserver"] = config.influxdbserver;
  doc["influxdbserverport"] = config.influxdbserverport;
  doc["influxdbshiptime"] = config.influxdbshiptime;

  // Serialize JSON to file
  if (serializeJson(doc, file) == 0) {
    Serial.println(F("Failed to write to file"));
  }

  // need to print out the deserialisation to discern size

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
  Serial.print("          hostname: "); Serial.println(config.hostname);
  Serial.print("            device: "); Serial.println(config.device);
  Serial.print("           appname: "); Serial.println(config.appname);
  Serial.print("              ssid: "); Serial.println(config.ssid);
  Serial.print("      wifipassword: "); Serial.println("**********");
  Serial.print("          relaypin: "); Serial.println(config.relaypin);
  Serial.print("            ledpin: "); Serial.println(config.ledpin);
  Serial.print("          httpuser: "); Serial.println(config.httpuser);
  Serial.print("      httppassword: "); Serial.println("**********");
  Serial.print("      httpapitoken: "); Serial.println("**********");
  Serial.print("      syslogserver: "); Serial.println(config.syslogserver);
  Serial.print("        syslogport: "); Serial.println(config.syslogport);
  Serial.print("     inmaintenance: "); Serial.println(config.inmaintenance);
  Serial.print("       ntptimezone: "); Serial.println(config.ntptimezone);
  Serial.print("       ntpsynctime: "); Serial.println(config.ntpsynctime);
  Serial.print("   ntpwaitsynctime: "); Serial.println(config.ntpwaitsynctime);
  Serial.print("         ntpserver: "); Serial.println(config.ntpserver);
  Serial.print("mfrcslaveselectpin: "); Serial.println(config.mfrcslaveselectpin);
  Serial.print("      mfrcresetpin: "); Serial.println(config.mfrcresetpin);
  Serial.print("  mfrccardwaittime: "); Serial.println(config.mfrccardwaittime);
  Serial.print("     lcdi2caddress: "); Serial.println(config.lcdi2caddress);
  Serial.print("          lcdwidth: "); Serial.println(config.lcdwidth);
  Serial.print("         lcdheight: "); Serial.println(config.lcdheight);
  Serial.print(" webserverporthttp: "); Serial.println(config.webserverporthttp);
  Serial.print("webserverporthttps: "); Serial.println(config.webserverporthttps);
  Serial.print("    webapiwaittime: "); Serial.println(config.webapiwaittime);
  Serial.print("      webpagedelay: "); Serial.println(config.webpagedelay);
  Serial.print("         serverurl: "); Serial.println(config.serverurl);
  Serial.print("    serverapitoken: "); Serial.println("**********");
  Serial.print("     checkuserpage: "); Serial.println(config.checkuserpage);
  Serial.print("       getuserpage: "); Serial.println(config.getuserpage);
  Serial.print("       moduserpage: "); Serial.println(config.moduserpage);
  Serial.print("     overridecodes: "); Serial.println(config.overridecodes);
  Serial.print("    influxdbenable: "); Serial.println(config.influxdbenable);
  Serial.print("    influxdbserver: "); Serial.println(config.influxdbserver);
  Serial.print("influxdbserverport: "); Serial.println(config.influxdbserverport);
  Serial.print("  influxdbshiptime: "); Serial.println(config.influxdbshiptime);
}
