project(
  'xdg-native-messaging-proxy',
  'c',
  version: '0.1.0',
  meson_version: '>= 1.1',
  license: 'LGPL-2.0-or-later',
)

gnome = import('gnome')

glib_dep = dependency('glib-2.0', version: '>= 2.72')
gio_dep = dependency('gio-2.0')
gio_unix_dep = dependency('gio-unix-2.0')
libdex_dep = dependency('libdex-1')
json_glib_dep = dependency('json-glib-1.0')

prefix = get_option('prefix')
datadir = prefix / get_option('datadir')
libdir = prefix / get_option('libdir')
libexecdir = prefix / get_option('libexecdir')
sysconfdir = prefix / get_option('sysconfdir')
localedir = prefix / get_option('localedir')

dbus_service_dir = get_option('dbus-service-dir')
if dbus_service_dir == ''
    dbus_service_dir = datadir / 'dbus-1' / 'services'
endif

systemd_userunit_dir = get_option('systemd-user-unit-dir')
if systemd_userunit_dir == ''
    # This is deliberately not ${libdir}: systemd units always go in
    # .../lib, never .../lib64 or .../lib/x86_64-linux-gnu
    systemd_userunit_dir = prefix / 'lib' / 'systemd' / 'user'
endif

common_incs = include_directories('.')  # config.h
src_incs = include_directories('src')

config_h = configuration_data()
config_h.set('_GNU_SOURCE', 1)
config_h.set_quoted('G_LOG_DOMAIN', 'xdg-native-messaging-proxy')
config_h.set_quoted('GETTEXT_PACKAGE', 'xdg-native-messaging-proxy')
config_h.set_quoted('DATADIR', datadir)
config_h.set_quoted('LIBDIR', libdir)
config_h.set_quoted('LIBEXECDIR', libexecdir)
config_h.set_quoted('SYSCONFDIR', sysconfdir)
config_h.set_quoted('LOCALEDIR', localedir)
config_h.set_quoted(
  'PACKAGE_STRING',
  'xdg-native-messaging-proxy @0@'.format(meson.project_version()),
)
configure_file(output: 'config.h', configuration: config_h)

base_config = configuration_data()
base_config.set('libexecdir', libexecdir)

subdir('data')
subdir('src')
subdir('tests')