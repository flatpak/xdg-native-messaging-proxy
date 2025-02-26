# SPDX-License-Identifier: LGPL-2.1-or-later

from typing import Iterator
import pytest
import dbus
import dbusmock
import os
import tempfile
import subprocess
import time
import signal
import json
from pathlib import Path
from dbus.mainloop.glib import DBusGMainLoop


def pytest_configure() -> None:
    ensure_environment_set()
    DBusGMainLoop(set_as_default=True)


def pytest_sessionfinish(session, exitstatus):
    # Meson and ginsttest-runner expect tests to exit with status 77 if all
    # tests were skipped
    if exitstatus == pytest.ExitCode.NO_TESTS_COLLECTED:
        session.exitstatus = 77


def ensure_environment_set() -> None:
    env_vars = [
        "XDG_NATIVE_MESSAGING_PROXY_PATH",
    ]

    for env_var in env_vars:
        if not os.getenv(env_var):
            raise Exception(f"{env_var} must be set")


@pytest.fixture(autouse=True)
def create_test_dirs() -> Iterator[None]:
    env_dirs = [
        "HOME",
        "TMPDIR",
    ]

    test_root = tempfile.TemporaryDirectory(
        prefix="xnmp-testroot-", ignore_cleanup_errors=True
    )

    for env_dir in env_dirs:
        directory = Path(test_root.name) / env_dir.lower()
        directory.mkdir(mode=0o700, parents=True)
        os.environ[env_dir] = directory.absolute().as_posix()

    yield

    test_root.cleanup()


@pytest.fixture
def create_test_dbus() -> Iterator[dbusmock.DBusTestCase]:
    bus = dbusmock.DBusTestCase()
    bus.setUp()
    bus.start_session_bus()
    bus.start_system_bus()

    yield bus

    bus.tearDown()
    bus.tearDownClass()


@pytest.fixture
def dbus_con(create_test_dbus: dbusmock.DBusTestCase) -> dbus.Bus:
    """
    Default fixture which provides the python-dbus session bus of the test.
    """
    con = create_test_dbus.get_dbus(system_bus=False)
    assert con
    return con


@pytest.fixture(autouse=True)
def create_dbus_monitor(create_test_dbus) -> Iterator[subprocess.Popen | None]:
    if not os.getenv("XNMP_DBUS_MONITOR"):
        yield None
        return

    dbus_monitor = subprocess.Popen(["dbus-monitor", "--session"])

    yield dbus_monitor

    dbus_monitor.terminate()
    dbus_monitor.wait()


def test_dir() -> Path:
    return Path(__file__).resolve().parent


@pytest.fixture
def xnmp_host_locations(create_test_dirs) -> Path | None:
    return Path(os.environ["TMPDIR"]) / "native-messaging-hosts"


@pytest.fixture(autouse=True)
def manifests(xnmp_host_locations):
    nmhd = test_dir() / "native-messaging-hosts"
    manifests = {}

    xnmp_host_locations.mkdir(parents=True)

    for manifest_path in nmhd.glob("*.json"):
        manifest = json.loads(manifest_path.read_text())

        assert manifest["name"] == manifest_path.stem

        path = manifest["path"]
        if path[0] != "/":
            manifest["path"] = (nmhd / path).absolute().as_posix()

        destination = xnmp_host_locations / manifest_path.name
        destination.write_text(json.dumps(manifest))
        manifests[manifest_path.stem] = manifest

    return manifests


@pytest.fixture
def xdg_native_messaging_proxy_path() -> Path:
    return Path(os.environ["XDG_NATIVE_MESSAGING_PROXY_PATH"])


@pytest.fixture
def xnmp_overwrite_env() -> dict[str, str]:
    return {}


@pytest.fixture
def xnmp_env(
    xnmp_overwrite_env: dict[str, str],
    xnmp_host_locations: Path | None,
) -> dict[str, str]:
    env = os.environ.copy()
    env["G_DEBUG"] = "fatal-criticals"
    env["G_MESSAGES_DEBUG"] = "all"
    env["XDG_CURRENT_DESKTOP"] = "test"

    if xnmp_host_locations:
        env["XNMP_HOST_LOCATIONS"] = xnmp_host_locations.absolute().as_posix()

    for key, val in xnmp_overwrite_env.items():
        env[key] = val

    return env


@pytest.fixture
def xdg_native_messaging_proxy(
    dbus_con: dbus.Bus,
    xdg_native_messaging_proxy_path: Path,
    xnmp_env: dict[str, str],
) -> Iterator[subprocess.Popen]:
    if not xdg_native_messaging_proxy_path.exists():
        raise FileNotFoundError(f"{xdg_native_messaging_proxy_path} does not exist")

    xdg_native_messaging_proxy = subprocess.Popen(
        [xdg_native_messaging_proxy_path],
        env=xnmp_env,
    )

    while not dbus_con.name_has_owner("org.freedesktop.NativeMessagingProxy"):
        returncode = xdg_native_messaging_proxy.poll()
        if returncode is not None:
            raise subprocess.SubprocessError(
                f"xdg-native-messaging-proxy exited with {returncode}"
            )
        time.sleep(0.1)

    yield xdg_native_messaging_proxy

    xdg_native_messaging_proxy.send_signal(signal.SIGHUP)
    returncode = xdg_native_messaging_proxy.wait()
    assert returncode == 0
