import os
import json
import xnmp
import errno
from pathlib import Path


class TestXnmp:
    def test_cat(self, xdg_native_messaging_proxy, manifests, dbus_con):
        iface = xnmp.get_iface(dbus_con)
        manifest_name = "org.example.cat"
        manifest = manifests[manifest_name]
        extension = "some-extension@example.org"
        mode = "firefox"

        manifest_str = iface.GetManifest(manifest_name, mode, {})
        assert json.loads(manifest_str) == manifest

        (stdin, stdout, stderr, handle) = iface.Start(
            manifest_name, extension, mode, {}
        )

        closed_received = False

        def on_closed(closed_handle, options):
            nonlocal closed_received
            nonlocal handle

            assert closed_handle == handle
            closed_received = True

        iface.connect_to_signal("Closed", on_closed)

        xnmp.wait_for(lambda: closed_received)

        stdout_fd = stdout.take()
        try:
            result = os.read(stdout_fd, 1024)
            assert json.loads(result) == manifest
        finally:
            os.close(stdout_fd)

    def test_echo(self, xdg_native_messaging_proxy, manifests, dbus_con):
        iface = xnmp.get_iface(dbus_con)
        manifest_name = "org.example.echo"
        manifest = manifests[manifest_name]
        extension = "some-extension@example.org"
        mode = "firefox"

        manifest_str = iface.GetManifest(manifest_name, mode, {})
        assert json.loads(manifest_str) == manifest

        (stdin, stdout, stderr, handle) = iface.Start(
            manifest_name, extension, mode, {}
        )

        stdout_fd = stdout.take()
        stdin_fd = stdin.take()
        try:
            msg = b"this is a test"
            os.write(stdin_fd, msg)
            result = os.read(stdout_fd, 1024)
            assert result == msg
        finally:
            os.close(stdout_fd)
            os.close(stdin_fd)

    def test_close(self, xdg_native_messaging_proxy, manifests, dbus_con):
        iface = xnmp.get_iface(dbus_con)
        manifest_name = "org.example.echo"
        extension = "some-extension@example.org"
        mode = "firefox"

        (stdin, stdout, stderr, handle) = iface.Start(
            manifest_name, extension, mode, {}
        )

        iface.Close(handle, {})

        closed_received = False

        def on_closed(closed_handle, options):
            nonlocal closed_received
            nonlocal handle

            assert closed_handle == handle
            closed_received = True

        iface.connect_to_signal("Closed", on_closed)

        xnmp.wait_for(lambda: closed_received)

    def test_dbus_close(self, xdg_native_messaging_proxy, manifests, dbus_con):
        iface = xnmp.get_iface(dbus_con)
        manifest_name = "org.example.echo"
        extension = "some-extension@example.org"
        mode = "firefox"

        (stdin, stdout, stderr, handle) = iface.Start(
            manifest_name, extension, mode, {}
        )

        dbus_con.close()

        stdin_fd = stdin.take()

        def fd_closed():
            try:
                msg = b"1"
                os.write(stdin_fd, msg)
            except IOError as e:
                return e.errno == errno.EPIPE
            except Exception:
                return False
            return False

        try:
            xnmp.wait_for(fd_closed)
        finally:
            os.close(stdin_fd)

    def test_close_stdin(self, xdg_native_messaging_proxy, manifests, dbus_con):
        iface = xnmp.get_iface(dbus_con)
        manifest_name = "org.example.writeonclose"
        extension = "some-extension@example.org"
        mode = "firefox"

        (stdin, stdout, stderr, handle) = iface.Start(
            manifest_name, extension, mode, {}
        )

        fpath = Path(os.environ["TMPDIR"]) / "xnmp-write-on-close"
        assert not fpath.exists()

        stdin_fd = stdin.take()

        os.close(stdin_fd)
        xnmp.wait_for(lambda: fpath.exists())
