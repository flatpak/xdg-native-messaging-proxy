from typing import Callable
from gi.repository import GLib
import dbus


def wait(ms: int):
    """
    Waits for the specified amount of milliseconds.
    """
    mainloop = GLib.MainLoop()
    GLib.timeout_add(ms, mainloop.quit)
    mainloop.run()


def wait_for(fn: Callable[[], bool]):
    """
    Waits and dispatches to mainloop until the function fn returns true. This is
    useful in combination with a lambda which captures a variable:

        my_var = False
        def callback():
            my_var = True
        do_something_later(callback)
        xdp.wait_for(lambda: my_var)
    """
    mainloop = GLib.MainLoop()
    while not fn():
        GLib.timeout_add(50, mainloop.quit)
        mainloop.run()


def get_iface(dbus_con: dbus.Bus) -> dbus.Interface:
    return dbus.Interface(
        dbus_con.get_object(
            "org.freedesktop.NativeMessagingProxy",
            "/org/freedesktop/nativemessagingproxy",
        ),
        dbus_interface="org.freedesktop.NativeMessagingProxy",
    )
