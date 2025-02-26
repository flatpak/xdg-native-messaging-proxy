dbus_interfaces = files(
  'org.freedesktop.NativeMessagingProxy.xml',
)

install_data(
  dbus_interfaces,
  install_dir: get_option('datadir') / 'dbus-1' / 'interfaces',
)

configure_file(
  input: 'xdg-native-messaging-proxy.service.in',
  output: '@BASENAME@',
  configuration: base_config,
  install: true,
  install_dir: systemd_userunit_dir,
)

configure_file(
  input: 'org.freedesktop.NativeMessagingProxy.service.in',
  output: '@BASENAME@',
  configuration: base_config,
  install: true,
  install_dir: dbus_service_dir,
)