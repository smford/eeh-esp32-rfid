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
}
