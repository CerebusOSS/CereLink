"""
Pythonic session wrapper for the CereLink SDK.
"""

from __future__ import annotations

import time as _time
import threading
from dataclasses import dataclass, field
from typing import Callable, Optional

from ._lib import ffi, load_library

lib = None  # Lazy-loaded


def _get_lib():
    global lib
    if lib is None:
        lib = load_library()
    return lib


# Device type mapping
DEVICE_TYPES = {
    "LEGACY_NSP": "CBPROTO_DEVICE_TYPE_LEGACY_NSP",
    "NSP": "CBPROTO_DEVICE_TYPE_NSP",
    "HUB1": "CBPROTO_DEVICE_TYPE_HUB1",
    "HUB2": "CBPROTO_DEVICE_TYPE_HUB2",
    "HUB3": "CBPROTO_DEVICE_TYPE_HUB3",
    "NPLAY": "CBPROTO_DEVICE_TYPE_NPLAY",
}

# Channel type mapping
CHANNEL_TYPES = {
    "FRONTEND": "CBPROTO_CHANNEL_TYPE_FRONTEND",
    "ANALOG_IN": "CBPROTO_CHANNEL_TYPE_ANALOG_IN",
    "ANALOG_OUT": "CBPROTO_CHANNEL_TYPE_ANALOG_OUT",
    "AUDIO": "CBPROTO_CHANNEL_TYPE_AUDIO",
    "DIGITAL_IN": "CBPROTO_CHANNEL_TYPE_DIGITAL_IN",
    "SERIAL": "CBPROTO_CHANNEL_TYPE_SERIAL",
    "DIGITAL_OUT": "CBPROTO_CHANNEL_TYPE_DIGITAL_OUT",
}


def _check(result: int, msg: str = ""):
    """Check a cbsdk_result_t and raise on error."""
    if result != 0:
        _lib = _get_lib()
        err = ffi.string(_lib.cbsdk_get_error_message(result)).decode()
        raise RuntimeError(f"{msg}: {err}" if msg else err)


@dataclass
class Stats:
    """SDK session statistics."""
    packets_received: int = 0
    bytes_received: int = 0
    packets_to_shmem: int = 0
    packets_queued: int = 0
    packets_delivered: int = 0
    packets_dropped: int = 0
    queue_depth: int = 0
    queue_max_depth: int = 0
    packets_sent: int = 0
    shmem_errors: int = 0
    receive_errors: int = 0
    send_errors: int = 0


class Session:
    """CereLink SDK session.

    Connects to a Blackrock Neurotech Cerebus device (or attaches to an
    existing session's shared memory) and delivers packets via callbacks.

    Args:
        device_type: Device type string. One of: "LEGACY_NSP", "NSP",
            "HUB1", "HUB2", "HUB3", "NPLAY".
        callback_queue_depth: Number of packets to buffer (default: 16384).

    Example::

        session = Session("HUB1")

        @session.on_event("FRONTEND")
        def on_spike(header, data):
            print(f"Spike on ch {header.chid}")

        # ... do work ...

        session.close()

    Can also be used as a context manager::

        with Session("HUB1") as session:
            # session is connected and running
            ...
    """

    def __init__(
        self,
        device_type: str = "LEGACY_NSP",
        callback_queue_depth: int = 16384,
    ):
        _lib = _get_lib()

        config = _lib.cbsdk_config_default()
        device_type_upper = device_type.upper()
        if device_type_upper not in DEVICE_TYPES:
            raise ValueError(
                f"Unknown device_type: {device_type!r}. "
                f"Must be one of: {', '.join(DEVICE_TYPES)}"
            )
        config.device_type = getattr(_lib, DEVICE_TYPES[device_type_upper])
        config.callback_queue_depth = callback_queue_depth

        session_p = ffi.new("cbsdk_session_t *")
        _check(_lib.cbsdk_session_create(session_p, ffi.addressof(config)), "Failed to create session")
        self._session = session_p[0]
        self._handles: list[int] = []
        # prevent Python callback pointers from being garbage collected
        self._callback_refs: list = []
        self._lock = threading.Lock()
        self._closed = False
        # Calibrate monotonic ↔ steady_clock offset for device_to_monotonic()
        self._mono_to_steady_offset_ns = self._calibrate_monotonic_offset()

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    def close(self):
        """Stop and destroy the session."""
        if self._closed:
            return
        self._closed = True
        _lib = _get_lib()
        # Unregister all callbacks first to avoid calling into dead Python objects
        for handle in self._handles:
            _lib.cbsdk_session_unregister_callback(self._session, handle)
        self._handles.clear()
        self._callback_refs.clear()
        _lib.cbsdk_session_destroy(self._session)
        self._session = ffi.NULL

    @property
    def running(self) -> bool:
        """Whether the session is running."""
        return bool(_get_lib().cbsdk_session_is_running(self._session))

    # --- Callbacks ---

    def on_event(
        self, channel_type: str = "FRONTEND"
    ) -> Callable:
        """Decorator to register a callback for event packets (spikes, etc.).

        The callback receives ``(header, data)`` where ``header`` is the packet
        header (with ``.time``, ``.chid``, ``.type``, ``.dlen``) and ``data``
        is a cffi buffer of the raw payload bytes.

        Args:
            channel_type: Channel type filter. One of the CHANNEL_TYPES keys,
                or "ANY" for all event channels.
        """
        def decorator(fn):
            self._register_event_callback(channel_type, fn)
            return fn
        return decorator

    def on_group(self, group_id: int = 5, *, as_array: bool = False) -> Callable:
        """Decorator to register a callback for continuous sample group packets.

        The callback receives ``(header, data)`` where ``data`` is either a
        cffi pointer to ``int16_t[N]`` or (if *as_array* is True) a numpy
        ``int16`` array of shape ``(n_channels,)``.

        Args:
            group_id: Group ID (1-6, where 5=30kHz, 6=raw).
            as_array: If True, deliver data as a numpy int16 array (zero-copy).
                Requires numpy.
        """
        def decorator(fn):
            if as_array:
                self._register_group_callback_numpy(group_id, fn)
            else:
                self._register_group_callback(group_id, fn)
            return fn
        return decorator

    def on_config(self, packet_type: int) -> Callable:
        """Decorator to register a callback for config/system packets.

        Args:
            packet_type: Packet type to match.
        """
        def decorator(fn):
            self._register_config_callback(packet_type, fn)
            return fn
        return decorator

    def on_packet(self) -> Callable:
        """Decorator to register a callback for ALL packets (catch-all)."""
        def decorator(fn):
            self._register_packet_callback(fn)
            return fn
        return decorator

    def on_error(self, fn: Callable[[str], None]):
        """Register a callback for errors.

        Args:
            fn: Called with an error message string.
        """
        _lib = _get_lib()

        @ffi.callback("void(const char*, void*)")
        def c_error_cb(error_message, user_data):
            try:
                fn(ffi.string(error_message).decode())
            except Exception:
                pass  # Never let exceptions propagate into C

        _lib.cbsdk_session_set_error_callback(self._session, c_error_cb, ffi.NULL)
        self._callback_refs.append(c_error_cb)

    def _register_event_callback(self, channel_type: str, fn):
        _lib = _get_lib()
        ct_upper = channel_type.upper()
        if ct_upper == "ANY":
            c_channel_type = ffi.cast("cbproto_channel_type_t", -1)
        elif ct_upper in CHANNEL_TYPES:
            c_channel_type = getattr(_lib, CHANNEL_TYPES[ct_upper])
        else:
            raise ValueError(
                f"Unknown channel_type: {channel_type!r}. "
                f"Must be one of: ANY, {', '.join(CHANNEL_TYPES)}"
            )

        @ffi.callback("void(const cbPKT_GENERIC*, void*)")
        def c_event_cb(pkt, user_data):
            try:
                fn(pkt.cbpkt_header, pkt.data_u8)
            except Exception:
                pass

        handle = _lib.cbsdk_session_register_event_callback(
            self._session, c_channel_type, c_event_cb, ffi.NULL
        )
        if handle == 0:
            raise RuntimeError("Failed to register event callback")
        self._handles.append(handle)
        self._callback_refs.append(c_event_cb)

    def _register_group_callback(self, group_id: int, fn):
        _lib = _get_lib()

        @ffi.callback("void(const cbPKT_GROUP*, void*)")
        def c_group_cb(pkt, user_data):
            try:
                fn(pkt.cbpkt_header, pkt.data)
            except Exception:
                pass

        handle = _lib.cbsdk_session_register_group_callback(
            self._session, group_id, c_group_cb, ffi.NULL
        )
        if handle == 0:
            raise RuntimeError("Failed to register group callback")
        self._handles.append(handle)
        self._callback_refs.append(c_group_cb)

    def _register_group_callback_numpy(self, group_id: int, fn):
        from ._numpy import group_data_as_array

        _lib = _get_lib()
        channels = self.get_group_channels(group_id)
        n_ch = len(channels)

        @ffi.callback("void(const cbPKT_GROUP*, void*)")
        def c_group_cb(pkt, user_data):
            try:
                n = n_ch if n_ch > 0 else pkt.cbpkt_header.dlen * 2
                arr = group_data_as_array(pkt.data, n)
                fn(pkt.cbpkt_header, arr)
            except Exception:
                pass

        handle = _lib.cbsdk_session_register_group_callback(
            self._session, group_id, c_group_cb, ffi.NULL
        )
        if handle == 0:
            raise RuntimeError("Failed to register group callback")
        self._handles.append(handle)
        self._callback_refs.append(c_group_cb)

    def _register_config_callback(self, packet_type: int, fn):
        _lib = _get_lib()

        @ffi.callback("void(const cbPKT_GENERIC*, void*)")
        def c_config_cb(pkt, user_data):
            try:
                fn(pkt.cbpkt_header, pkt.data_u32)
            except Exception:
                pass

        handle = _lib.cbsdk_session_register_config_callback(
            self._session, packet_type, c_config_cb, ffi.NULL
        )
        if handle == 0:
            raise RuntimeError("Failed to register config callback")
        self._handles.append(handle)
        self._callback_refs.append(c_config_cb)

    def _register_packet_callback(self, fn):
        _lib = _get_lib()

        @ffi.callback("void(const cbPKT_GENERIC*, size_t, void*)")
        def c_packet_cb(pkts, count, user_data):
            try:
                for i in range(count):
                    pkt = pkts[i]
                    fn(pkt.cbpkt_header, pkt.data_u8)
            except Exception:
                pass

        handle = _lib.cbsdk_session_register_packet_callback(
            self._session, c_packet_cb, ffi.NULL
        )
        if handle == 0:
            raise RuntimeError("Failed to register packet callback")
        self._handles.append(handle)
        self._callback_refs.append(c_packet_cb)

    # --- Stats ---

    @property
    def stats(self) -> Stats:
        """Get current session statistics."""
        _lib = _get_lib()
        c_stats = ffi.new("cbsdk_stats_t *")
        _lib.cbsdk_session_get_stats(self._session, c_stats)
        return Stats(
            packets_received=c_stats.packets_received_from_device,
            bytes_received=c_stats.bytes_received_from_device,
            packets_to_shmem=c_stats.packets_stored_to_shmem,
            packets_queued=c_stats.packets_queued_for_callback,
            packets_delivered=c_stats.packets_delivered_to_callback,
            packets_dropped=c_stats.packets_dropped,
            queue_depth=c_stats.queue_current_depth,
            queue_max_depth=c_stats.queue_max_depth,
            packets_sent=c_stats.packets_sent_to_device,
            shmem_errors=c_stats.shmem_store_errors,
            receive_errors=c_stats.receive_errors,
            send_errors=c_stats.send_errors,
        )

    def reset_stats(self):
        """Reset statistics counters to zero."""
        _get_lib().cbsdk_session_reset_stats(self._session)

    # --- Configuration Access ---

    @property
    def runlevel(self) -> int:
        """Get the current device run level."""
        return _get_lib().cbsdk_session_get_runlevel(self._session)

    @staticmethod
    def max_chans() -> int:
        """Total number of channels (cbMAXCHANS)."""
        return _get_lib().cbsdk_get_max_chans()

    @staticmethod
    def num_fe_chans() -> int:
        """Number of front-end channels."""
        return _get_lib().cbsdk_get_num_fe_chans()

    @staticmethod
    def num_analog_chans() -> int:
        """Number of analog channels (front-end + analog input)."""
        return _get_lib().cbsdk_get_num_analog_chans()

    def get_channel_label(self, chan_id: int) -> Optional[str]:
        """Get a channel's label (1-based channel ID)."""
        _lib = _get_lib()
        ptr = _lib.cbsdk_session_get_channel_label(self._session, chan_id)
        if ptr == ffi.NULL:
            return None
        return ffi.string(ptr).decode()

    def get_channel_smpgroup(self, chan_id: int) -> int:
        """Get a channel's sample group (0 = disabled, 1-6)."""
        return _get_lib().cbsdk_session_get_channel_smpgroup(self._session, chan_id)

    def get_channel_chancaps(self, chan_id: int) -> int:
        """Get a channel's capability flags."""
        return _get_lib().cbsdk_session_get_channel_chancaps(self._session, chan_id)

    def get_group_label(self, group_id: int) -> Optional[str]:
        """Get a sample group's label (group_id 1-6)."""
        _lib = _get_lib()
        ptr = _lib.cbsdk_session_get_group_label(self._session, group_id)
        if ptr == ffi.NULL:
            return None
        return ffi.string(ptr).decode()

    def get_group_channels(self, group_id: int) -> list[int]:
        """Get the list of channel IDs in a sample group."""
        _lib = _get_lib()
        max_chans = _lib.cbsdk_get_num_analog_chans()
        buf = ffi.new(f"uint16_t[{max_chans}]")
        count = ffi.new("uint32_t *", max_chans)
        result = _lib.cbsdk_session_get_group_list(self._session, group_id, buf, count)
        if result != 0:
            return []
        return [buf[i] for i in range(count[0])]

    # --- Channel Configuration ---

    def set_channel_sample_group(
        self,
        n_chans: int,
        channel_type: str,
        group_id: int,
        disable_others: bool = False,
    ):
        """Set sampling group for channels of a specific type.

        Args:
            n_chans: Number of channels to configure.
            channel_type: Channel type filter (e.g., "FRONTEND").
            group_id: Sampling group (0-6, 0 disables).
            disable_others: Disable sampling on unselected channels.
        """
        _lib = _get_lib()
        ct_upper = channel_type.upper()
        if ct_upper not in CHANNEL_TYPES:
            raise ValueError(f"Unknown channel_type: {channel_type!r}")
        c_type = getattr(_lib, CHANNEL_TYPES[ct_upper])
        _check(
            _lib.cbsdk_session_set_channel_sample_group(
                self._session, n_chans, c_type, group_id, disable_others
            ),
            "Failed to set channel sample group",
        )

    # --- CCF Configuration Files ---

    def save_ccf(self, filename: str):
        """Save the current device configuration to a CCF (XML) file.

        Args:
            filename: Path to the CCF file to write.
        """
        _check(
            _get_lib().cbsdk_session_save_ccf(self._session, filename.encode()),
            "Failed to save CCF",
        )

    def load_ccf(self, filename: str):
        """Load a CCF file and apply its configuration to the device.

        Reads the CCF file and sends the configuration packets to the device.
        The device must be connected in STANDALONE mode.

        Args:
            filename: Path to the CCF file to read.
        """
        _check(
            _get_lib().cbsdk_session_load_ccf(self._session, filename.encode()),
            "Failed to load CCF",
        )

    # --- Instrument Time ---

    @property
    def time(self) -> int:
        """Most recent device timestamp from shared memory.

        On Gemini (protocol 4.0+) this is PTP nanoseconds.
        On legacy NSP (protocol 3.x) this is 30kHz ticks.

        To convert to host ``time.monotonic()``, use :meth:`device_to_monotonic`.
        """
        _lib = _get_lib()
        t = ffi.new("uint64_t *")
        _check(_lib.cbsdk_session_get_time(self._session, t), "Failed to get time")
        return t[0]

    # --- Patient Information ---

    def set_patient_info(
        self,
        id: str,
        firstname: str = "",
        lastname: str = "",
        dob_month: int = 0,
        dob_day: int = 0,
        dob_year: int = 0,
    ):
        """Set patient information for recorded files.

        Must be called before starting recording for info to be included.

        Args:
            id: Patient identification string (required).
            firstname: Patient first name.
            lastname: Patient last name.
            dob_month: Birth month (1-12, 0 = unset).
            dob_day: Birth day (1-31, 0 = unset).
            dob_year: Birth year (e.g. 1990, 0 = unset).
        """
        _lib = _get_lib()
        _check(
            _lib.cbsdk_session_set_patient_info(
                self._session,
                id.encode(),
                firstname.encode() if firstname else ffi.NULL,
                lastname.encode() if lastname else ffi.NULL,
                dob_month, dob_day, dob_year,
            ),
            "Failed to set patient info",
        )

    # --- Analog Output ---

    def set_analog_output_monitor(
        self,
        aout_channel: int,
        monitor_channel: int,
        track_last: bool = True,
        spike_only: bool = False,
    ):
        """Route a channel's signal to an analog/audio output for monitoring.

        Args:
            aout_channel: 1-based channel ID of the analog/audio output.
            monitor_channel: 1-based channel ID of the channel to monitor.
            track_last: If True, track last channel clicked in Central.
            spike_only: If True, monitor spike signal; if False, monitor continuous.
        """
        _check(
            _get_lib().cbsdk_session_set_analog_output_monitor(
                self._session, aout_channel, monitor_channel,
                track_last, spike_only,
            ),
            "Failed to set analog output monitor",
        )

    # --- Recording Control ---

    def start_central_recording(self, filename: str, comment: str = ""):
        """Start Central file recording on the device.

        Requires Central to be running.

        Args:
            filename: Base filename without extension.
            comment: Recording comment.
        """
        _lib = _get_lib()
        _check(
            _lib.cbsdk_session_start_central_recording(
                self._session, filename.encode(),
                comment.encode() if comment else ffi.NULL,
            ),
            "Failed to start Central recording",
        )

    def stop_central_recording(self):
        """Stop Central file recording on the device."""
        _check(
            _get_lib().cbsdk_session_stop_central_recording(self._session),
            "Failed to stop Central recording",
        )

    def open_central_file_dialog(self):
        """Open Central's File Storage dialog.

        Must be called before start_central_recording().
        Wait ~250ms after this call for the dialog to initialize.
        """
        _check(
            _get_lib().cbsdk_session_open_central_file_dialog(self._session),
            "Failed to open Central file dialog",
        )

    def close_central_file_dialog(self):
        """Close Central's File Storage dialog."""
        _check(
            _get_lib().cbsdk_session_close_central_file_dialog(self._session),
            "Failed to close Central file dialog",
        )

    # --- Spike Sorting ---

    def set_channel_spike_sorting(
        self,
        n_chans: int,
        channel_type: str,
        sort_options: int,
    ):
        """Set spike sorting options for channels of a specific type.

        Args:
            n_chans: Number of channels to configure.
            channel_type: Channel type filter (e.g., "FRONTEND").
            sort_options: Spike sorting option flags (cbAINPSPK_*).
        """
        _lib = _get_lib()
        ct_upper = channel_type.upper()
        if ct_upper not in CHANNEL_TYPES:
            raise ValueError(f"Unknown channel_type: {channel_type!r}")
        c_type = getattr(_lib, CHANNEL_TYPES[ct_upper])
        _check(
            _lib.cbsdk_session_set_channel_spike_sorting(
                self._session, n_chans, c_type, sort_options
            ),
            "Failed to set spike sorting",
        )

    # --- Clock Synchronization ---

    @staticmethod
    def _calibrate_monotonic_offset() -> int:
        """Compute offset between time.monotonic() and C++ steady_clock.

        On Linux, macOS, and Windows with Python 3.12+, both clocks use the
        same underlying source (CLOCK_MONOTONIC / mach_absolute_time /
        QueryPerformanceCounter) so the offset is exactly 0.

        On older Windows Python (<3.12), time.monotonic() may use
        GetTickCount64 while steady_clock uses QueryPerformanceCounter,
        so we measure the offset empirically.

        Returns:
            steady_clock_ns - monotonic_ns (int).
        """
        import sys
        import platform

        if platform.system() != "Windows" or sys.version_info >= (3, 12):
            return 0

        # Windows < 3.12: clocks may differ, measure empirically
        _lib = _get_lib()
        t1 = _time.monotonic()
        steady_ns = _lib.cbsdk_get_steady_clock_ns()
        t2 = _time.monotonic()
        mono_ns = int((t1 + t2) / 2 * 1_000_000_000)
        return steady_ns - mono_ns

    @property
    def clock_offset_ns(self) -> Optional[int]:
        """Clock offset in nanoseconds (device_ns - host_ns), or None if unavailable."""
        _lib = _get_lib()
        offset = ffi.new("int64_t *")
        result = _lib.cbsdk_session_get_clock_offset(self._session, offset)
        if result != 0:
            return None
        return offset[0]

    @property
    def clock_uncertainty_ns(self) -> Optional[int]:
        """Clock uncertainty (half-RTT) in nanoseconds, or None if unavailable."""
        _lib = _get_lib()
        uncertainty = ffi.new("int64_t *")
        result = _lib.cbsdk_session_get_clock_uncertainty(self._session, uncertainty)
        if result != 0:
            return None
        return uncertainty[0]

    def send_clock_probe(self):
        """Send a clock synchronization probe to the device."""
        _check(
            _get_lib().cbsdk_session_send_clock_probe(self._session),
            "Failed to send clock probe",
        )

    def device_to_monotonic(self, device_time_ns: int) -> float:
        """Convert a device timestamp to ``time.monotonic()`` seconds.

        Chains two offsets:
        1. device_ns → steady_clock_ns  (via clock_offset_ns from device sync)
        2. steady_clock_ns → monotonic_ns  (via calibration at session creation)

        Args:
            device_time_ns: Device timestamp in nanoseconds (e.g., header.time).

        Returns:
            Corresponding ``time.monotonic()`` value in seconds.

        Raises:
            RuntimeError: If no clock sync data is available yet.

        Example::

            @session.on_event("FRONTEND")
            def on_spike(header, data):
                t = session.device_to_monotonic(header.time)
                latency_ms = (time.monotonic() - t) * 1000
                print(f"Spike latency: {latency_ms:.1f} ms")
        """
        offset = self.clock_offset_ns
        if offset is None:
            raise RuntimeError("No clock sync data available")
        steady_ns = device_time_ns - offset
        mono_ns = steady_ns - self._mono_to_steady_offset_ns
        return mono_ns / 1_000_000_000

    # --- Commands ---

    def send_comment(self, comment: str, rgba: int = 0, charset: int = 0):
        """Send a comment to the device (appears in recorded data)."""
        _lib = _get_lib()
        _check(
            _lib.cbsdk_session_send_comment(
                self._session, comment.encode(), rgba, charset
            ),
            "Failed to send comment",
        )

    def set_digital_output(self, chan_id: int, value: int):
        """Set a digital output channel value."""
        _check(
            _get_lib().cbsdk_session_set_digital_output(self._session, chan_id, value),
            "Failed to set digital output",
        )

    def set_runlevel(self, runlevel: int):
        """Set the system run level."""
        _check(
            _get_lib().cbsdk_session_set_runlevel(self._session, runlevel),
            "Failed to set run level",
        )

    # --- numpy Data Collection ---

    def continuous_reader(
        self, group_id: int = 5, buffer_seconds: float = 10.0
    ) -> ContinuousReader:
        """Create a ring buffer that accumulates continuous group data.

        Registers a group callback internally. Call :meth:`ContinuousReader.read`
        to retrieve the most recent samples as a numpy array.

        Args:
            group_id: Group ID (1-6).
            buffer_seconds: Ring buffer duration in seconds.

        Returns:
            A :class:`ContinuousReader` instance.
        """
        from ._numpy import GROUP_RATES

        n_channels = len(self.get_group_channels(group_id))
        if n_channels == 0:
            raise ValueError(f"Group {group_id} has no channels configured")
        rate = GROUP_RATES.get(group_id, 30000)
        buffer_samples = int(buffer_seconds * rate)
        return ContinuousReader(self, group_id, n_channels, buffer_samples)

    def read_continuous(self, group_id: int = 5, duration: float = 1.0):
        """Collect continuous data for a specified duration.

        Blocks for *duration* seconds while accumulating group samples,
        then returns the collected data.

        Args:
            group_id: Group ID (1-6).
            duration: Collection duration in seconds.

        Returns:
            numpy.ndarray of shape ``(n_channels, n_samples)``, dtype ``int16``.
        """
        import time
        import numpy as np
        from ._numpy import GROUP_RATES

        n_channels = len(self.get_group_channels(group_id))
        if n_channels == 0:
            raise ValueError(f"Group {group_id} has no channels configured")

        rate = GROUP_RATES.get(group_id, 30000)
        max_samples = int(duration * rate * 1.2)  # 20% headroom
        buf = np.zeros((n_channels, max_samples), dtype=np.int16)
        count = [0]

        _lib = _get_lib()

        @ffi.callback("void(const cbPKT_GROUP*, void*)")
        def c_group_cb(pkt, user_data):
            try:
                if count[0] < max_samples:
                    src = ffi.buffer(pkt.data, n_channels * 2)
                    arr = np.frombuffer(src, dtype=np.int16, count=n_channels)
                    buf[:, count[0]] = arr
                    count[0] += 1
            except Exception:
                pass

        handle = _lib.cbsdk_session_register_group_callback(
            self._session, group_id, c_group_cb, ffi.NULL
        )
        if handle == 0:
            raise RuntimeError("Failed to register group callback")
        self._handles.append(handle)
        self._callback_refs.append(c_group_cb)

        try:
            time.sleep(duration)
        finally:
            _lib.cbsdk_session_unregister_callback(self._session, handle)
            try:
                self._handles.remove(handle)
            except ValueError:
                pass

        return buf[:, :count[0]]

    # --- Version ---

    @staticmethod
    def version() -> str:
        """Get the SDK version string."""
        return ffi.string(_get_lib().cbsdk_get_version()).decode()


class ContinuousReader:
    """Ring buffer that accumulates continuous group data into a numpy array.

    Created via :meth:`Session.continuous_reader`.

    Example::

        reader = session.continuous_reader(group_id=5, buffer_seconds=10)
        import time
        time.sleep(2)
        data = reader.read()  # (n_channels, ~60000) int16 array
        reader.close()

    Attributes:
        n_channels: Number of channels in the group.
        sample_rate: Sample rate in Hz.
    """

    def __init__(self, session: Session, group_id: int, n_channels: int,
                 buffer_samples: int):
        import numpy as np
        from ._numpy import GROUP_RATES

        self._session = session
        self._group_id = group_id
        self.n_channels = n_channels
        self.sample_rate = GROUP_RATES.get(group_id, 30000)
        self._buffer_samples = buffer_samples
        self._buffer = np.zeros((n_channels, buffer_samples), dtype=np.int16)
        self._write_pos = 0
        self._total_samples = 0
        self._closed = False
        self._handle = None
        self._cb_ref = None
        self._register()

    def _register(self):
        from ._numpy import group_data_as_array

        _lib = _get_lib()
        n_ch = self.n_channels

        @ffi.callback("void(const cbPKT_GROUP*, void*)")
        def c_group_cb(pkt, user_data):
            try:
                arr = group_data_as_array(pkt.data, n_ch)
                pos = self._write_pos % self._buffer_samples
                self._buffer[:, pos] = arr
                self._write_pos += 1
                self._total_samples += 1
            except Exception:
                pass

        self._cb_ref = c_group_cb
        handle = _lib.cbsdk_session_register_group_callback(
            self._session._session, self._group_id, c_group_cb, ffi.NULL
        )
        if handle == 0:
            raise RuntimeError("Failed to register group callback")
        self._handle = handle
        self._session._handles.append(handle)
        self._session._callback_refs.append(c_group_cb)

    def read(self, n_samples: int | None = None):
        """Read the most recent samples from the ring buffer.

        Args:
            n_samples: Number of samples to read. If ``None``, reads all
                available (up to buffer size).

        Returns:
            numpy.ndarray of shape ``(n_channels, n_samples)``, dtype ``int16``.
            Always returns a copy.
        """
        import numpy as np

        available = min(self._total_samples, self._buffer_samples)
        if n_samples is None:
            n_samples = available
        n_samples = min(n_samples, available)
        if n_samples == 0:
            return np.zeros((self.n_channels, 0), dtype=np.int16)

        end = self._write_pos % self._buffer_samples
        start = (end - n_samples) % self._buffer_samples
        if start < end:
            return self._buffer[:, start:end].copy()
        else:
            return np.concatenate([
                self._buffer[:, start:],
                self._buffer[:, :end],
            ], axis=1)

    @property
    def available(self) -> int:
        """Number of samples currently in the buffer."""
        return min(self._total_samples, self._buffer_samples)

    @property
    def total_samples(self) -> int:
        """Total number of samples received (may exceed buffer size)."""
        return self._total_samples

    @property
    def dropped(self) -> int:
        """Number of samples lost due to buffer overflow."""
        return max(0, self._total_samples - self._buffer_samples)

    def close(self):
        """Unregister the callback and release resources."""
        if self._closed:
            return
        self._closed = True
        if self._handle is not None:
            _get_lib().cbsdk_session_unregister_callback(
                self._session._session, self._handle
            )
            try:
                self._session._handles.remove(self._handle)
            except ValueError:
                pass

    def __del__(self):
        self.close()
