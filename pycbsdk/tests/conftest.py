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

CERELINK_RELEASE_URL = "https://github.com/CerebusOSS/CereLink/releases/download/v9.11.0"
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


def _remove_lockfile(name: str) -> None:
    """Remove nPlayServer's lock FILE ($TMPDIR/<name>.lock).

    nPlayServer's lock is a file guarded by O_EXCL (not a POSIX semaphore), so a
    hard-killed instance leaves it behind — which makes the next launch with the
    same name fail with "Cannot run multiple instances".  Safe when absent.
    """
    if platform.system() == "Windows":
        return
    tmp = os.environ.get("TMPDIR") or "/tmp"
    (Path(tmp.rstrip("/")) / f"{name}.lock").unlink(missing_ok=True)


def pytest_configure(config):
    """Fail fast if the DEV-built libcbsdk is OLDER than the C++ sources.

    In a source checkout pycbsdk loads ``cmake-build-*/src/cbsdk/libcbsdk.dylib``
    (there is no wheel-bundled lib).  If that dylib was not rebuilt after a C++
    change, the tests silently run STALE code — which once masked a clock-sync
    fix for an entire debugging session.  Only a build-tree dylib is checked (an
    installed/wheel-bundled lib has no sources to compare against); bypass with
    ``PYCBSDK_ALLOW_STALE_LIB=1``.
    """
    if os.environ.get("PYCBSDK_ALLOW_STALE_LIB"):
        return
    try:
        from pycbsdk._lib import _find_library
        lib_path = Path(_find_library())
    except Exception:
        return  # can't resolve the lib — let the normal load path report it
    lib_str = str(lib_path)
    if "cmake-build" not in lib_str and f"{os.sep}build{os.sep}" not in lib_str:
        return  # installed/bundled lib, not a dev build tree
    if not lib_path.exists():
        return
    repo_root = Path(__file__).resolve().parents[2]
    src_root = repo_root / "src"
    if not src_root.exists():
        return
    lib_mtime = lib_path.stat().st_mtime
    newest, newest_file = lib_mtime, None
    for f in src_root.rglob("*"):
        if f.suffix in (".cpp", ".h", ".hpp", ".c", ".cc") and f.is_file():
            m = f.stat().st_mtime
            if m > newest:
                newest, newest_file = m, f
    if newest_file is not None:
        raise pytest.UsageError(
            f"Stale libcbsdk: {lib_path}\n"
            f"  built {time.ctime(lib_mtime)} is OLDER than C++ source "
            f"{newest_file.relative_to(repo_root)} ({time.ctime(newest)}).\n"
            f"  Tests would run pre-change code. Rebuild the dev dylib:\n"
            f"    cmake --build {lib_path.parents[2]} --target cbsdk_shared\n"
            f"  (or set PYCBSDK_ALLOW_STALE_LIB=1 to bypass this check)."
        )


@pytest.hookimpl(trylast=True)
def pytest_collection_modifyitems(config, items):
    """Auto-skip ``lsl`` tests when file-based nPlay tests are also collected.

    Both bring up a STANDALONE ``Session(DeviceType.NPLAY)`` whose shared memory
    is keyed by device type, so two nPlayServer instances cannot coexist.  When a
    single run collects both, skip the LSL ones with guidance to run them alone.

    Runs ``trylast`` so pytest's own ``-m``/``-k`` deselection happens FIRST:
    under ``pytest -m lsl`` the file-based tests are already removed from
    ``items``, so the LSL tests are NOT skipped and actually run.
    """
    lsl_items = [it for it in items if it.get_closest_marker("lsl")]
    nplay_file_items = [
        it for it in items
        if it.get_closest_marker("integration") and not it.get_closest_marker("lsl")
    ]
    if lsl_items and nplay_file_items:
        skip = pytest.mark.skip(
            reason="LSL nPlay tests need an exclusive nPlayServer "
                   "(one shmem per device type); run separately: pytest -m lsl"
        )
        for it in lsl_items:
            it.add_marker(skip)


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
def test_data_long_dir() -> Path:
    """Download and extract long test data (4-channel, ~12s, fewer wraps)."""
    zip_path = CACHE_DIR / "dnss.zip"
    extract_dir = CACHE_DIR / "dnss"
    _download(f"{CERELINK_RELEASE_URL}/dnss.zip", zip_path)
    _extract_zip(zip_path, extract_dir)
    return extract_dir


@pytest.fixture(scope="session")
def ns6_path(test_data_long_dir: Path) -> Path:
    """Path to the long .ns6 file used for nPlayServer playback.

    Uses the ~12s recording (dnss) instead of the ~2s one (dnss256) so
    that device timestamps wrap infrequently, avoiding flaky clock-sync
    and timing tests.
    """
    matches = list(test_data_long_dir.rglob("*.ns6"))
    assert matches, "No .ns6 file found in long test data"
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
def manufacturer_cmp_path() -> Path:
    """Path to the sanitized 128-channel manufacturer CMP fixture.

    The default 96-channel file has rows that are already in (bank, electrode)
    order — useful for parser tests but not for exercising the sort. The
    manufacturer sample has out-of-order rows like a real .cmp file.
    """
    repo_root = Path(__file__).parent.parent.parent
    cmp = repo_root / "tests" / "128ChannelManufacturerMapping.cmp"
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
                _ensure_executable(candidate)
            return candidate
    return None


def _ensure_executable(path: Path) -> None:
    """Make *path* executable, tolerating a transient EPERM on macOS.

    A freshly-downloaded SIGNED binary is briefly locked by Gatekeeper /
    syspolicyd on first access, so chmod can raise PermissionError.  The file is
    usually already 0755 (preserved by the archive), so we skip when it's already
    executable and otherwise retry a few times before giving up.
    """
    for _ in range(5):
        if os.access(path, os.X_OK):
            return
        try:
            path.chmod(0o755)
            return
        except PermissionError:
            time.sleep(0.5)


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
            "--audio", "none",
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


@pytest.fixture
def client_session(nplay_session):
    """A CLIENT-mode Session attached to the STANDALONE nplay_session's shmem.

    Function-scoped: created fresh for each test that requests it.
    The STANDALONE nplay_session already owns native shmem for NPLAY;
    creating another Session(DeviceType.NPLAY) finds that shmem and
    attaches as CLIENT (skipping device creation).
    """
    from pycbsdk import DeviceType, Session

    with Session(DeviceType.NPLAY) as session:
        assert not session.is_standalone, (
            "Expected CLIENT mode but got STANDALONE — "
            "is the nplay_session fixture alive?"
        )
        yield session


# ---------------------------------------------------------------------------
# nPlay + LSL fixtures (mutually exclusive with the file-playback fixtures above;
# see pytest_collection_modifyitems).  Run with: pytest -m lsl
# ---------------------------------------------------------------------------

@pytest.fixture(scope="session")
def lsl_stream_name() -> str:
    """Unique LSL stream name for this test process."""
    return f"pycbsdk_lsl_{os.getpid()}"


@pytest.fixture(scope="session")
def lsl_generator(lsl_stream_name: str):
    """Launch the 30 kHz int16 LSL generator and wait until its stream resolves.

    Skips dependent tests if pylsl is unavailable.  Yields the stream name.
    """
    pylsl = pytest.importorskip("pylsl", reason="pylsl required for LSL tests")

    gen_script = Path(__file__).parent / "_lsl_generator.py"
    proc = subprocess.Popen(
        [sys.executable, str(gen_script), lsl_stream_name, "8"],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )

    streams = pylsl.resolve_byprop("name", lsl_stream_name, timeout=10)
    if not streams:
        proc.terminate()
        proc.wait()
        out = proc.stdout.read().decode(errors="replace") if proc.stdout else ""
        pytest.fail(f"LSL generator stream '{lsl_stream_name}' never appeared\n{out}")

    yield lsl_stream_name

    proc.terminate()
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()


@pytest.fixture(scope="session")
def nplay_lsl(nplayserver_binary: Path | None, lsl_generator: str):
    """Start nPlayServer as an LSL inlet reading the generator stream.

    nPlayServer derives device timestamps from the stream's originating time
    (× 30 kHz), so this exercises the LSL clock path rather than file playback.
    """
    if nplayserver_binary is None:
        pytest.skip(
            f"nPlayServer not available for {platform.system()}/{platform.machine()}"
        )

    lock_name = f"nplay_lsl_{os.getpid()}"
    _remove_lockfile(lock_name)  # clear any stale lock from a hard-killed prior run

    proc = subprocess.Popen(
        [
            str(nplayserver_binary),
            "--audio", "none",
            "--lockfile", lock_name,
            "--device=LSL", lsl_generator,
            "-A",  # autostart
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )

    # Allow time to resolve the LSL inlet and bind its UDP ports.
    time.sleep(3)

    if proc.poll() is not None:
        stdout = proc.stdout.read().decode(errors="replace") if proc.stdout else ""
        stderr = proc.stderr.read().decode(errors="replace") if proc.stderr else ""
        _remove_lockfile(lock_name)
        pytest.fail(
            f"nPlayServer --device=LSL exited immediately (rc={proc.returncode})\n"
            f"stdout: {stdout}\nstderr: {stderr}"
        )

    yield proc

    proc.terminate()
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()
    _remove_lockfile(lock_name)


@pytest.fixture(scope="session")
def nplay_lsl_session(nplay_lsl):
    """A STANDALONE Session connected to the LSL-fed nPlayServer."""
    from pycbsdk import DeviceType, Session

    with Session(DeviceType.NPLAY) as session:
        yield session
