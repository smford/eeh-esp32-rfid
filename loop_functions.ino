void loopBreakout(String message) {
  // display ntp sync events on serial
  events();

  // reboot if we've told it to reboot
  if (shouldReboot) {
    rebootESP("Web Admin - " + message);
  }

  if (gotoToggleMaintenance) {
    toggleMaintenance();
  }

  if (gotoLogoutCurrentUser) {
    logoutCurrentUser();
  }

  if (config.influxdbenable) {
    //unsigned long currentRunTime = millis();

    // do nothing and return if trying to ship metrics too fast
    if ((millis() - influxdbLastRunTime) > (config.influxdbshiptime * 1000)) {
      shipUsage();
      shipTemp();
      influxdbLastRunTime = millis();
    }
  }
}
