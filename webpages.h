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
  <p>Free Storage: <span id="freespiffs">%FREESPIFFS%</span> | Used Storage: <span id="usedspiffs">%USEDSPIFFS%</span> | Total Storage: <span id="totalspiffs">%TOTALSPIFFS%</span></p>
  <button onclick="logoutButton()">Logout Web Admin</button>
  <button onclick="getUserDetailsButton()">Refresh Current Card User Details</button>
  <button onclick="grantAccessButton()" %GRANTBUTTONENABLE%>Grant Access to Current Card</button>
  <button onclick="revokeAccessButton()" %GRANTBUTTONENABLE%>Revoke Access to Current Card</button>
  <button onclick="displayConfig()">Display Running Config</button>
  <button onclick="displayWifi()">Display WiFi Networks</button>
  <button onclick="scani2c()">Scan I2C Devices</button>
  <button onclick="showUploadButton()">Upload File - Simple</button>
  <button onclick="showUploadButtonFancy()">Upload File - Fancy</button>
  <button onclick="changeBacklightButton('on')">LCD Backlight On</button>
  <button onclick="changeBacklightButton('off')">LCD Backlight Off</button>
  <button onclick="listFilesButton()">List Files</button>
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
  <p id="configstatus"></p>
  <p id="configdetails"></p>
<script>
function toggleCheckbox(element, pin) {
  var xhr = new XMLHttpRequest();
  if(element.checked){ xhr.open("GET", "/toggle?state=on&pin="+pin, true); }
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
  },%WEBPAGEDELAY%);
}
function logoutButton() {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/logout", true);
  xhr.send();
  setTimeout(function(){ window.open("/logged-out","_self"); }, %WEBPAGEDELAY%);
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
  },%WEBPAGEDELAY%);
}
function getUserDetailsButton() {
  document.getElementById("statusdetails").innerHTML = "Getting User Details ...";
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/getuser", true);
  xhr.send();
  setTimeout(function(){
    document.getElementById("statusdetails").innerHTML = "Refreshed User Details";
    document.getElementById("userdetails").innerHTML = xhr.responseText;
  },%WEBPAGEDELAY%);
}
function grantAccessButton() {
  document.getElementById("statusdetails").innerHTML = "Granting Access ...";
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/grant?access=grant", true);
  xhr.send();
  setTimeout(function(){
    document.getElementById("statusdetails").innerHTML = "Access Granted";
    document.getElementById("userdetails").innerHTML = xhr.responseText;
  },%WEBPAGEDELAY%);
}
function revokeAccessButton() {
  document.getElementById("statusdetails").innerHTML = "Revoking access ...";
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/grant?access=revoke", true);
  xhr.send();
  setTimeout(function(){
    document.getElementById("statusdetails").innerHTML = "Access Revoked";
    document.getElementById("userdetails").innerHTML = xhr.responseText;
  },%WEBPAGEDELAY%);
}
function changeBacklightButton(state) {
  document.getElementById("statusdetails").innerHTML = "Turning LCD Backlight " . state;
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/backlight?state=" + state, true);
  xhr.send();
  setTimeout(function(){
    document.getElementById("statusdetails").innerHTML = "LCD Backlight " + state;
  },%WEBPAGEDELAY%);
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
  },%WEBPAGEDELAY%);
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
function displayWifi() {
  document.getElementById("statusdetails").innerHTML = "Scanning for WiFi Networks ...";
  document.getElementById("configheader").innerHTML = "<h3>Available WiFi Networks<h3>";
  xmlhttp=new XMLHttpRequest();
  xmlhttp.open("GET", "/scanwifi", false);
  xmlhttp.send();
  var mydata = JSON.parse(xmlhttp.responseText);
  var displaydata = "<table><tr><th align='left'>SSID</th><th align='left'>BSSID</th><th align='left'>RSSI</th><th align='left'>Channel</th><th align='left'>Secure</th></tr>";
  for (var key of Object.keys(mydata)) {
    displaydata = displaydata + "<tr><td align='left'>" + mydata[key]["ssid"] + "</td><td align='left'>" + mydata[key]["bssid"] + "</td><td align='left'>" + mydata[key]["rssi"] + "</td><td align='left'>" + mydata[key]["channel"] + "</td><td align='left'>"+ mydata[key]["secure"] + "</td></tr>";
  }
  displaydata = displaydata + "</table>";
  document.getElementById("statusdetails").innerHTML = "WiFi Networks Scanned";
  document.getElementById("configdetails").innerHTML = displaydata;
}
function scani2c() {
  document.getElementById("statusdetails").innerHTML = "Scanning for I2C Devices";
  document.getElementById("configheader").innerHTML = "<h3>Available I2C Devices<h3>";
  xmlhttp=new XMLHttpRequest();
  xmlhttp.open("GET", "/scani2c", false);
  xmlhttp.send();
  var mydata = JSON.parse(xmlhttp.responseText);
  var displaydata = "<table><tr><th align='left'>Int</th><th align='left'>Hex</th><th align='left'>Error</th></tr>";
  for (var key of Object.keys(mydata)) {
    displaydata = displaydata + "<tr><td align='left'>" + mydata[key]["int"] + "</td><td align='left'>" + mydata[key]["hex"] + "</td><td align='left'>" + mydata[key]["error"] + "</td></tr>";
  }
  displaydata = displaydata + "</table>";
  document.getElementById("statusdetails").innerHTML = "I2C Devices Scanned";
  document.getElementById("configheader").innerHTML = "<h3>I2C Devices<h3>";
  document.getElementById("configdetails").innerHTML = displaydata;
}
function listFilesButton() {
  document.getElementById("statusdetails").innerHTML = "Listing Files ...";
  xmlhttp=new XMLHttpRequest();
  xmlhttp.open("GET", "/listfiles", false);
  xmlhttp.send();
  document.getElementById("configstatus").innerHTML = "Files Listed";
  document.getElementById("configheader").innerHTML = "<h3>Files<h3>";
  document.getElementById("configdetails").innerHTML = xmlhttp.responseText;
}
function downloadDeleteButton(filename, action) {
  var urltocall = "/file?name=" + filename + "&action=" + action;
  xmlhttp=new XMLHttpRequest();
  if (action == "delete") {
    xmlhttp.open("GET", urltocall, false);
    xmlhttp.send();
    document.getElementById("configstatus").innerHTML = xmlhttp.responseText;
    xmlhttp.open("GET", "/listfiles", false);
    xmlhttp.send();
    document.getElementById("configdetails").innerHTML = xmlhttp.responseText;
  }
  if (action == "download") {
    document.getElementById("configstatus").innerHTML = "";
    window.open(urltocall,"_blank");
  }
}
function showUploadButton() {
  document.getElementById("configheader").innerHTML = "<h3>Upload File<h3>"
  document.getElementById("configstatus").innerHTML = "";
  var uploadform = "<form method = \"POST\" action = \"/\" enctype=\"multipart/form-data\"><input type=\"file\" name=\"data\"/><input type=\"submit\" name=\"upload\" value=\"Upload\" title = \"Upload File\"></form>"
  document.getElementById("configdetails").innerHTML = uploadform;
}
function showUploadButtonFancy() {
  document.getElementById("configheader").innerHTML = "<h3>Upload File 3<h3>"
  document.getElementById("configstatus").innerHTML = "";
  var uploadform = "<form method = \"POST\" action = \"/\" enctype=\"multipart/form-data\"><input type=\"file\" name=\"data\"/><input type=\"submit\" name=\"upload\" value=\"Upload\" title = \"Upload File\"></form>"
  document.getElementById("configdetails").innerHTML = uploadform;
  var uploadform =
  "<form id=\"upload_form\" enctype=\"multipart/form-data\" method=\"post\">" +
  "<input type=\"file\" name=\"file1\" id=\"file1\" onchange=\"uploadFile()\"><br>" +
  "<progress id=\"progressBar\" value=\"0\" max=\"100\" style=\"width:300px;\"></progress>" +
  "<h3 id=\"status\"></h3>" +
  "<p id=\"loaded_n_total\"></p>" +
  "</form>";
  document.getElementById("configdetails").innerHTML = uploadform;
}
function _(el) {
  return document.getElementById(el);
}
function uploadFile() {
  var file = _("file1").files[0];
  // alert(file.name+" | "+file.size+" | "+file.type);
  var formdata = new FormData();
  formdata.append("file1", file);
  var ajax = new XMLHttpRequest();
  ajax.upload.addEventListener("progress", progressHandler, false);
  ajax.addEventListener("load", completeHandler, false); // doesnt appear to ever get called even upon success
  ajax.addEventListener("error", errorHandler, false);
  ajax.addEventListener("abort", abortHandler, false);
  ajax.open("POST", "/");
  ajax.send(formdata);
}
function progressHandler(event) {
  //_("loaded_n_total").innerHTML = "Uploaded " + event.loaded + " bytes of " + event.total; // event.total doesnt show accurate total file size
  _("loaded_n_total").innerHTML = "Uploaded " + event.loaded + " bytes";
  var percent = (event.loaded / event.total) * 100;
  _("progressBar").value = Math.round(percent);
  _("status").innerHTML = Math.round(percent) + "% uploaded... please wait";
  if (percent >= 100) {
    _("status").innerHTML = "Please wait, writing file to filesystem";
  }
}
function completeHandler(event) {
  _("status").innerHTML = "Upload Complete";
  _("progressBar").value = 0;
  xmlhttp=new XMLHttpRequest();
  xmlhttp.open("GET", "/listfiles", false);
  xmlhttp.send();
  document.getElementById("configstatus").innerHTML = "File Uploaded";
  document.getElementById("configheader").innerHTML = "<h3>Files<h3>";
  document.getElementById("configdetails").innerHTML = xmlhttp.responseText;
}
function errorHandler(event) {
  _("status").innerHTML = "Upload Failed";
}
function abortHandler(event) {
  _("status").innerHTML = "Upload Aborted";
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


/* const char simpleupload_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html lang="en">
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta charset="UTF-8">
</head>
<body>
  <form method = "POST" action = "/" enctype="multipart/form-data">
    <input type="file" name="data"/>
    <input type="submit" name="upload" value="Upload" title = "Upload Files">
  </form>
</body>
</html>
)rawliteral";
*/
