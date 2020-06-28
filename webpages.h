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
