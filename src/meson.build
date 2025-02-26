dbus_sources = gnome.gdbus_codegen(
  'xdg-native-messaging-proxy-dbus',
  sources: dbus_interfaces,
  interface_prefix: 'org.freedesktop',
  namespace: 'XnmpDbus',
)

xdg_native_messaging_proxy_sources = files(
  'xdg-native-messaging-proxy.c',
  'xnmp-impl.c',
  'xnmp-service.c',
)
xdg_native_messaging_proxy_sources += dbus_sources

xdg_native_messaging_proxy_deps = [
  glib_dep,
  gio_dep,
  gio_unix_dep,
  libdex_dep,
  json_glib_dep,
]

xdg_native_messaging_proxy_incs = [
  common_incs,
  src_incs,
]

xdg_native_messaging_proxy = executable(
  'xdg-native-messaging-proxy',
  xdg_native_messaging_proxy_sources,
  dependencies: xdg_native_messaging_proxy_deps,
  include_directories: xdg_native_messaging_proxy_incs,
  install: true,
  install_dir: get_option('libexecdir'),
)