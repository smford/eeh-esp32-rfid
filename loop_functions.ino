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

  if (config.telegrafenable) {
    //unsigned long currentRunTime = millis();

    // do nothing and return if trying to ship metrics too fast
    if ((millis() - telegrafLastRunTime) > (config.telegrafshiptime * 1000)) {
      shipUsage();
      shipTemp();
      shipWifiSignal();
      telegrafLastRunTime = millis();
    }
  }

  // used by AsyncElegantOTA to detect when a reboot is required after an ota update
  AsyncElegantOTA.loop();
}
