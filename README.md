# xdg-native-messaging-proxy

This is a small service which can be used to find native messaging host
manifests, as well as start and stop those native messaging hosts.

Applications running inside a sandbox might have a limited view of the host
which might prevent them from finding and executing native messaging hosts
which exist outside of the sandbox. This proxy is supposed to run outside of any
sandbox, which will make the native messaging hosts outside the sandbox
available to anyone with access to the dbus service.

It is critical to understand that exposing this proxy, or any other scheme which
makes the native messaging hosts from outside a sandbox available to a sandboxed
process, is potentially *INSECURE*!

Native messaging is a form of IPC but the specifics of which services get
exposed over this IPC mechanism are unknown. Any of the services could provide
some functionality that allows the caller to escape the sandbox. An obvious
example is the native messaging host for installing GNOME Shell extensions,
which allows sandboxed callers to run arbitrary code outside of the sandbox. A
more subtle example are native messaging hosts which allow writing files to
arbitrary locations, which can be used to, for example, add code to .bashrc or
change the permissions of a sandboxed flatpak application.

Additionally, users will not be able to judge if a native messaging host API
provides the tools to escape the sandbox, making it impossible to offload this
problem to the users.

Previously, the functionality that xdg-native-messaging-proxy provides was
implemented in a merge request for xdg-desktop-portal and was shipped by Ubuntu
for a number of releases. However, xdg-desktop-portal APIs are supposed to be
generically useful and secure for sandboxed applications which is not the case
for any native messaging proxy. By moving the functionality to its own dbus
name, we do not need to provide a secure API and applications which want to make
use of the proxy have to explicitly request talk permission to this dbus name in
their manifest.

