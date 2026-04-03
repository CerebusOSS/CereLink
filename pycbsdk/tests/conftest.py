"""Shared test fixtures for pycbsdk integration tests.

Downloads nPlayServer binaries and test data from the CereLink GitHub release,
manages nPlayServer lifecycle, and provides session fixtures.
"""

import ctypes
import ctypes.util
import os
import platform
import subprocess
import sys
import time
import zipfile
from pathlib import Path
from urllib.request import urlretrieve

import pytest

CERELINK_RELEASE_URL = "https://github.com/CerebusOSS/CereLink/releases/download/v9.3.0"
CACHE_DIR = Path(__file__).parent / ".test_cache"


def _sem_unlink(name: str) -> None:
    """Unlink a POSIX named semaphore via libc sem_unlink.

    nPlayServer creates a named semaphore as a lockfile.  On macOS (and Linux)
    these persist in the kernel after the process exits, so we must clean them
    up explicitly.  Windows does not use POSIX semaphores, so this is a no-op
    there.
    """
    if platform.system() == "Windows":
        return
    libc_name = ctypes.util.find_library("c")
    if not libc_name:
        libc_name = "libc.dylib" if sys.platform == "darwin" else "libc.so.6"
    libc = ctypes.CDLL(libc_name, use_errno=True)
    libc.sem_unlink.argtypes = [ctypes.c_char_p]
    libc.sem_unlink.restype = ctypes.c_int
    sem_name = f"/{name}".encode()
    libc.sem_unlink(sem_name)  # ignore errors (ENOENT is fine)


def _nplay_asset_name() -> str | None:
    """Return the nPlayServer zip asset name for this platform, or None."""
    system = platform.system()
    machine = platform.machine().lower()
    if system == "Darwin" and machine == "arm64":
        return "nPlayServer-MacOS-arm.zip"
    elif system == "Windows":
        return "nPlayServer-Win64.zip"
    elif system == "Linux" and machine in ("x86_64", "amd64"):
        return "nPlayServer-Linux-x64.zip"
    return None


def _download(url: str, dest: Path) -> None:
    """Download *url* to *dest*, skipping if *dest* already exists."""
    if dest.exists():
        return
    dest.parent.mkdir(parents=True, exist_ok=True)
    print(f"Downloading {url}")
    urlretrieve(url, dest)


def _extract_zip(zip_path: Path, dest_dir: Path) -> None:
    """Extract *zip_path* into *dest_dir*, skipping if *dest_dir* already exists."""
    if dest_dir.exists():
        return
    with zipfile.ZipFile(zip_path) as zf:
        zf.extractall(dest_dir)


# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------

@pytest.fixture(scope="session")
def test_data_dir() -> Path:
    """Download and extract test data (256-channel, .ns6 + .nev + .ccf)."""
    zip_path = CACHE_DIR / "dnss256.zip"
    extract_dir = CACHE_DIR / "dnss256"
    _download(f"{CERELINK_RELEASE_URL}/dnss256.zip", zip_path)
    _extract_zip(zip_path, extract_dir)
    return extract_dir


@pytest.fixture(scope="session")
def ns6_path(test_data_dir: Path) -> Path:
    """Path to the .ns6 file in the test data."""
    matches = list(test_data_dir.rglob("*.ns6"))
    assert matches, "No .ns6 file found in test data"
    return matches[0]


@pytest.fixture(scope="session")
def ccf_path(test_data_dir: Path) -> Path:
    """Path to the .ccf file in the test data."""
    matches = list(test_data_dir.rglob("*.ccf"))
    assert matches, "No .ccf file found in test data"
    return matches[0]


@pytest.fixture(scope="session")
def cmp_path() -> Path:
    """Path to a bundled .cmp file from the repo test data."""
    repo_root = Path(__file__).parent.parent.parent
    cmp = repo_root / "tests" / "96ChannelDefaultMapping.cmp"
    assert cmp.exists(), f"CMP file not found at {cmp}"
    return cmp


@pytest.fixture(scope="session")
def nplayserver_binary() -> Path | None:
    """Locate or download the nPlayServer binary for this platform."""
    env_path = os.environ.get("NPLAYSERVER_PATH")
    if env_path:
        p = Path(env_path)
        if p.exists():
            return p
        pytest.fail(f"NPLAYSERVER_PATH={env_path} does not exist")

    asset_name = _nplay_asset_name()
    if asset_name is None:
        return None

    zip_path = CACHE_DIR / asset_name
    extract_dir = CACHE_DIR / asset_name.removesuffix(".zip")
    _download(f"{CERELINK_RELEASE_URL}/{asset_name}", zip_path)
    _extract_zip(zip_path, extract_dir)

    exe_suffix = ".exe" if platform.system() == "Windows" else ""
    for candidate in extract_dir.rglob(f"nPlayServer{exe_suffix}"):
        if candidate.is_file():
            if platform.system() != "Windows":
                candidate.chmod(0o755)
            return candidate
    return None


@pytest.fixture(scope="session")
def nplayserver(nplayserver_binary: Path | None, ns6_path: Path):
    """Start nPlayServer playing back test data; kill on teardown.

    Skips all dependent tests if nPlayServer is not available for this platform.
    """
    if nplayserver_binary is None:
        pytest.skip(
            f"nPlayServer not available for {platform.system()}/{platform.machine()}"
        )

    lock_name = f"nplay_test_{os.getpid()}"

    proc = subprocess.Popen(
        [
            str(nplayserver_binary),
            "--lockfile", lock_name,
            "-A",  # autostart playback
            str(ns6_path),
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )

    # Allow time for the server to bind its UDP ports
    time.sleep(3)

    if proc.poll() is not None:
        stdout = proc.stdout.read().decode(errors="replace") if proc.stdout else ""
        stderr = proc.stderr.read().decode(errors="replace") if proc.stderr else ""
        pytest.fail(
            f"nPlayServer exited immediately (rc={proc.returncode})\n"
            f"stdout: {stdout}\nstderr: {stderr}"
        )

    yield proc

    proc.terminate()
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()
    _sem_unlink(lock_name)


@pytest.fixture(scope="session")
def nplay_session(nplayserver):
    """A single Session connected to nPlayServer, shared across the entire test run.

    Most tests should use this fixture instead of creating their own Session to
    avoid shmem/port exhaustion from repeated session creation.
    """
    from pycbsdk import DeviceType, Session

    with Session(DeviceType.NPLAY) as session:
        yield session
