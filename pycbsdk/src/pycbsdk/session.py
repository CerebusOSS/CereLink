"""
Pythonic session wrapper for the CereLink SDK.
"""

from __future__ import annotations

import enum
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


class DeviceType(enum.IntEnum):
    """Device type selection.

    Values match ``cbproto_device_type_t``.
    """
    LEGACY_NSP = 0
    NSP        = 1
    HUB1       = 2
    HUB2       = 3
    HUB3       = 4
    NPLAY      = 5
    CUSTOM     = 6

class ChannelType(enum.IntEnum):
    """Channel type classification.

    Values match ``cbproto_channel_type_t``.
    """
    FRONTEND    = 0
    ANALOG_IN   = 1
    ANALOG_OUT  = 2
    AUDIO       = 3
    DIGITAL_IN  = 4
    SERIAL      = 5
    DIGITAL_OUT = 6

class SampleRate(enum.IntEnum):
    """Continuous sampling rate selection.

    Values match ``cbproto_group_rate_t`` so they can be passed directly to cffi.
    """
    NONE  = 0
    SR_500  = 1   # 500 Hz
    SR_1kHz = 2   # 1 000 Hz
    SR_2kHz = 3   # 2 000 Hz
    SR_10kHz = 4  # 10 000 Hz
    SR_30kHz = 5  # 30 000 Hz
    SR_RAW  = 6   # Raw (30 000 Hz)

    @property
    def hz(self) -> int:
        """Sample rate in Hz."""
        return _RATE_HZ[self]


class ProtocolVersion(enum.IntEnum):
    """Protocol version detected during device handshake.

    Values match ``cbproto_protocol_version_t``.
    """
    UNKNOWN  = 0
    V3_11    = 1   # Legacy 32-bit timestamps
    V4_0     = 2   # Legacy 64-bit timestamps
    V4_1     = 3   # 64-bit timestamps, 16-bit packet types
    CURRENT  = 4   # 4.2+ (current)


class ChanInfoField(enum.IntEnum):
    """Channel info field selector for bulk extraction.

    Values match ``cbsdk_chaninfo_field_t``.
    """
    SMPGROUP    = 0
    SMPFILTER   = 1
    SPKFILTER   = 2
    AINPOPTS    = 3
    SPKOPTS     = 4
    SPKTHRLEVEL = 5
    LNCRATE     = 6
    REFELECCHAN = 7
    AMPLREJPOS  = 8
    AMPLREJNEG  = 9
    CHANCAPS    = 10
    BANK        = 11
    TERM        = 12


_RATE_HZ = {
    SampleRate.NONE:     0,
    SampleRate.SR_500:   500,
    SampleRate.SR_1kHz:  1000,
    SampleRate.SR_2kHz:  2000,
    SampleRate.SR_10kHz: 10000,
    SampleRate.SR_30kHz: 30000,
    SampleRate.SR_RAW:   30000,
}

# Aliases for lenient string → SampleRate coercion (user might type "30kHz"
# instead of "SR_30kHz").
_RATE_ALIASES = {
    "500HZ": SampleRate.SR_500,
    "1KHZ":  SampleRate.SR_1kHz,
    "2KHZ":  SampleRate.SR_2kHz,
    "10KHZ": SampleRate.SR_10kHz,
    "30KHZ": SampleRate.SR_30kHz,
    "RAW":   SampleRate.SR_RAW,
}


def _coerce_enum(enum_cls, value, aliases=None):
    """Coerce *value* to *enum_cls*, accepting enum members, ints, or strings.

    String lookup is case-insensitive: tries the canonical member name first,
    then *aliases* (if provided).
    """
    if isinstance(value, enum_cls):
        return value
    if isinstance(value, int):
        return enum_cls(value)
    if isinstance(value, str):
        key = value.upper()
        # Exact member name match (e.g., "FRONTEND", "SR_30kHz")
        try:
            return enum_cls[key]
        except KeyError:
            pass
        # Alias match (e.g., "30kHz" → SR_30kHz)
        if aliases and key in aliases:
            return aliases[key]
        members = ", ".join(enum_cls.__members__)
        extra = ""
        if aliases:
            extra = " (or: " + ", ".join(
                k for k in aliases if k not in enum_cls.__members__
            ) + ")"
        raise ValueError(
            f"Unknown {enum_cls.__name__}: {value!r}. "
            f"Must be one of: {members}{extra}"
        )
    raise TypeError(
        f"Expected {enum_cls.__name__}, int, or str, got {type(value).__name__}"
    )


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
        device_type: Device type (e.g., ``DeviceType.HUB1``).
        callback_queue_depth: Number of packets to buffer (default: 16384).

    Example::

        session = Session(DeviceType.HUB1)

        @session.on_event(ChannelType.FRONTEND)
        def on_spike(header, data):
            print(f"Spike on ch {header.chid}")

        # ... do work ...

        session.close()

    Can also be used as a context manager::

        with Session(DeviceType.HUB1) as session:
            # session is connected and running
            ...
    """

    def __init__(
        self,
        device_type: DeviceType = DeviceType.LEGACY_NSP,
        callback_queue_depth: int = 16384,
    ):
        _lib = _get_lib()

        config = _lib.cbsdk_config_default()
        config.device_type = int(_coerce_enum(DeviceType, device_type))
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
        self, channel_type: ChannelType | None = ChannelType.FRONTEND
    ) -> Callable:
        """Decorator to register a callback for event packets (spikes, etc.).

        The callback receives ``(header, data)`` where ``header`` is the packet
        header (with ``.time``, ``.chid``, ``.type``, ``.dlen``) and ``data``
        is a cffi buffer of the raw payload bytes.

        Args:
            channel_type: Channel type filter, or ``None`` for all event
                channels.
        """
        ct = None if channel_type is None else _coerce_enum(ChannelType, channel_type)
        def decorator(fn):
            self._register_event_callback(ct, fn)
            return fn
        return decorator

    def on_group(self, rate: SampleRate = SampleRate.SR_30kHz, *, as_array: bool = False) -> Callable:
        """Decorator to register a callback for continuous sample group packets.

        The callback receives ``(header, data)`` where ``data`` is either a
        cffi pointer to ``int16_t[N]`` or (if *as_array* is True) a numpy
        ``int16`` array of shape ``(n_channels,)``.

        Args:
            rate: Sample rate to subscribe to.
            as_array: If True, deliver data as a numpy int16 array (zero-copy).
                Requires numpy.
        """
        rate = _coerce_enum(SampleRate, rate, _RATE_ALIASES)
        def decorator(fn):
            if as_array:
                self._register_group_callback_numpy(int(rate), fn)
            else:
                self._register_group_callback(int(rate), fn)
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

    def _register_event_callback(self, channel_type: ChannelType | None, fn):
        _lib = _get_lib()
        if channel_type is None:
            c_channel_type = ffi.cast("cbproto_channel_type_t", -1)
        else:
            c_channel_type = int(channel_type)

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

    def _register_group_callback(self, rate: int, fn):
        _lib = _get_lib()

        @ffi.callback("void(const cbPKT_GROUP*, void*)")
        def c_group_cb(pkt, user_data):
            try:
                fn(pkt.cbpkt_header, pkt.data)
            except Exception:
                pass

        handle = _lib.cbsdk_session_register_group_callback(
            self._session, int(rate), c_group_cb, ffi.NULL
        )
        if handle == 0:
            raise RuntimeError("Failed to register group callback")
        self._handles.append(handle)
        self._callback_refs.append(c_group_cb)

    def _register_group_callback_numpy(self, rate: int, fn):
        from ._numpy import group_data_as_array

        _lib = _get_lib()
        channels = self.get_group_channels(rate)
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
            self._session, int(rate), c_group_cb, ffi.NULL
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

    @property
    def protocol_version(self) -> ProtocolVersion:
        """Protocol version detected during device handshake.

        Returns ``ProtocolVersion.UNKNOWN`` in CLIENT mode (no device session).
        """
        v = _get_lib().cbsdk_session_get_protocol_version(self._session)
        try:
            return ProtocolVersion(v)
        except ValueError:
            return ProtocolVersion.UNKNOWN

    @property
    def spike_length(self) -> int:
        """Global spike event length in samples."""
        return _get_lib().cbsdk_session_get_spike_length(self._session)

    @property
    def spike_pretrigger(self) -> int:
        """Global spike pre-trigger length in samples."""
        return _get_lib().cbsdk_session_get_spike_pretrigger(self._session)

    def set_spike_length(self, spike_length: int, spike_pretrigger: int):
        """Set the global spike event length and pre-trigger.

        Args:
            spike_length: Total spike waveform length in samples.
            spike_pretrigger: Pre-trigger samples (must be < spike_length).
        """
        _check(
            _get_lib().cbsdk_session_set_spike_length(
                self._session, spike_length, spike_pretrigger
            ),
            "Failed to set spike length",
        )

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

    def get_channel_type(self, chan_id: int) -> Optional[ChannelType]:
        """Get a channel's type classification.

        Returns a :class:`ChannelType` member, or ``None`` if the channel is
        invalid or not connected.
        """
        _lib = _get_lib()
        result = _lib.cbsdk_session_get_channel_type(self._session, chan_id)
        ct = int(ffi.cast("int", result))
        try:
            return ChannelType(ct)
        except ValueError:
            return None

    def get_channel_smpfilter(self, chan_id: int) -> int:
        """Get a channel's continuous-time pathway filter ID."""
        return _get_lib().cbsdk_session_get_channel_smpfilter(self._session, chan_id)

    def get_channel_spkfilter(self, chan_id: int) -> int:
        """Get a channel's spike pathway filter ID."""
        return _get_lib().cbsdk_session_get_channel_spkfilter(self._session, chan_id)

    def get_channel_spkopts(self, chan_id: int) -> int:
        """Get a channel's spike processing options (cbAINPSPK_* flags)."""
        return _get_lib().cbsdk_session_get_channel_spkopts(self._session, chan_id)

    def get_channel_spkthrlevel(self, chan_id: int) -> int:
        """Get a channel's spike threshold level."""
        return _get_lib().cbsdk_session_get_channel_spkthrlevel(self._session, chan_id)

    def get_channel_ainpopts(self, chan_id: int) -> int:
        """Get a channel's analog input options (cbAINP_* flags)."""
        return _get_lib().cbsdk_session_get_channel_ainpopts(self._session, chan_id)

    def get_channel_lncrate(self, chan_id: int) -> int:
        """Get a channel's line noise cancellation adaptation rate."""
        return _get_lib().cbsdk_session_get_channel_lncrate(self._session, chan_id)

    def get_channel_refelecchan(self, chan_id: int) -> int:
        """Get a channel's software reference electrode channel."""
        return _get_lib().cbsdk_session_get_channel_refelecchan(self._session, chan_id)

    def get_channel_amplrejpos(self, chan_id: int) -> int:
        """Get a channel's positive amplitude rejection threshold."""
        return _get_lib().cbsdk_session_get_channel_amplrejpos(self._session, chan_id)

    def get_channel_amplrejneg(self, chan_id: int) -> int:
        """Get a channel's negative amplitude rejection threshold."""
        return _get_lib().cbsdk_session_get_channel_amplrejneg(self._session, chan_id)

    def get_channel_field(self, chan_id: int, field: ChanInfoField) -> int:
        """Get any numeric field from a single channel by field selector.

        This is the generic counterpart to the dedicated per-channel getters.
        Useful when the field is determined at runtime.

        Args:
            chan_id: 1-based channel ID.
            field: Which field to extract (e.g., ``ChanInfoField.BANK``).

        Returns:
            Field value as int (widened from the native type).
        """
        return _get_lib().cbsdk_session_get_channel_field(
            self._session, chan_id, int(field))

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

    # --- Bulk Channel Queries ---

    def get_matching_channel_ids(
        self,
        channel_type: ChannelType,
        n_chans: int = 0,
    ) -> list[int]:
        """Get 1-based IDs of channels matching a type.

        Args:
            channel_type: Channel type filter (e.g., ``ChannelType.FRONTEND``).
            n_chans: Max channels to return (0 or omit for all).

        Returns:
            List of 1-based channel IDs.
        """
        _lib = _get_lib()
        max_chans = _lib.cbsdk_get_max_chans()
        if n_chans <= 0:
            n_chans = max_chans
        buf = ffi.new(f"uint32_t[{max_chans}]")
        count = ffi.new("uint32_t *", max_chans)
        _check(
            _lib.cbsdk_session_get_matching_channels(
                self._session, n_chans,
                int(_coerce_enum(ChannelType, channel_type)),
                buf, count
            ),
            "Failed to get matching channel IDs",
        )
        return [buf[i] for i in range(count[0])]

    def get_channels_field(
        self,
        channel_type: ChannelType,
        field: ChanInfoField,
        n_chans: int = 0,
    ) -> list[int]:
        """Get a numeric field from all channels matching a type.

        Args:
            channel_type: Channel type filter (e.g., ``ChannelType.FRONTEND``).
            field: Which field to extract (e.g., ``ChanInfoField.SMPGROUP``).
            n_chans: Max channels to query (0 or omit for all).

        Returns:
            List of field values (same order as :meth:`get_matching_channel_ids`).
        """
        _lib = _get_lib()
        max_chans = _lib.cbsdk_get_max_chans()
        if n_chans <= 0:
            n_chans = max_chans
        buf = ffi.new(f"int64_t[{max_chans}]")
        count = ffi.new("uint32_t *", max_chans)
        _check(
            _lib.cbsdk_session_get_channels_field(
                self._session, n_chans,
                int(_coerce_enum(ChannelType, channel_type)),
                int(_coerce_enum(ChanInfoField, field)),
                buf, count
            ),
            "Failed to get channel field",
        )
        return [buf[i] for i in range(count[0])]

    def get_channels_labels(
        self,
        channel_type: ChannelType,
        n_chans: int = 0,
    ) -> list[str]:
        """Get labels from all channels matching a type.

        Args:
            channel_type: Channel type filter (e.g., ``ChannelType.FRONTEND``).
            n_chans: Max channels to query (0 or omit for all).

        Returns:
            List of label strings (same order as :meth:`get_matching_channel_ids`).
        """
        _lib = _get_lib()
        max_chans = _lib.cbsdk_get_max_chans()
        if n_chans <= 0:
            n_chans = max_chans
        label_stride = 16  # cbLEN_STR_LABEL = 16 (including null)
        buf = ffi.new(f"char[{max_chans * label_stride}]")
        count = ffi.new("uint32_t *", max_chans)
        _check(
            _lib.cbsdk_session_get_channels_labels(
                self._session, n_chans,
                int(_coerce_enum(ChannelType, channel_type)),
                buf, label_stride, count
            ),
            "Failed to get channel labels",
        )
        result = []
        for i in range(count[0]):
            s = ffi.string(buf + i * label_stride).decode()
            result.append(s)
        return result

    def get_channels_positions(
        self,
        channel_type: ChannelType,
        n_chans: int = 0,
    ) -> list[tuple[int, int, int, int]]:
        """Get positions from all channels matching a type.

        Each position is a 4-tuple ``(x, y, z, w)`` of ``int32`` values,
        corresponding to the ``cbPKT_CHANINFO.position[4]`` field.

        Args:
            channel_type: Channel type filter (e.g., ``ChannelType.FRONTEND``).
            n_chans: Max channels to query (0 or omit for all).

        Returns:
            List of ``(x, y, z, w)`` tuples (same order as
            :meth:`get_matching_channel_ids`).
        """
        _lib = _get_lib()
        max_chans = _lib.cbsdk_get_max_chans()
        if n_chans <= 0:
            n_chans = max_chans
        buf = ffi.new(f"int32_t[{max_chans * 4}]")
        count = ffi.new("uint32_t *", max_chans)
        _check(
            _lib.cbsdk_session_get_channels_positions(
                self._session, n_chans,
                int(_coerce_enum(ChannelType, channel_type)),
                buf, count
            ),
            "Failed to get channel positions",
        )
        result = []
        for i in range(count[0]):
            base = i * 4
            result.append((buf[base], buf[base + 1], buf[base + 2], buf[base + 3]))
        return result

    # --- Channel Configuration ---

    def set_channel_sample_group(
        self,
        n_chans: int,
        channel_type: ChannelType,
        rate: SampleRate,
        disable_others: bool = False,
    ):
        """Set sampling rate for channels of a specific type.

        Args:
            n_chans: Number of channels to configure.
            channel_type: Channel type filter (e.g., ``ChannelType.FRONTEND``).
            rate: Sample rate (e.g., ``SampleRate.SR_30kHz``, ``SampleRate.NONE``
                to disable).
            disable_others: Disable sampling on unselected channels.
        """
        _lib = _get_lib()
        _check(
            _lib.cbsdk_session_set_channel_sample_group(
                self._session, n_chans,
                int(_coerce_enum(ChannelType, channel_type)),
                int(_coerce_enum(SampleRate, rate, _RATE_ALIASES)),
                disable_others
            ),
            "Failed to set channel sample group",
        )

    def set_ac_input_coupling(
        self,
        n_chans: int,
        channel_type: ChannelType,
        enabled: bool,
    ):
        """Set AC/DC input coupling for channels of a specific type.

        Args:
            n_chans: Number of channels to configure.
            channel_type: Channel type filter (e.g., ``ChannelType.FRONTEND``).
            enabled: ``True`` for AC coupling (offset correction on),
                ``False`` for DC coupling.
        """
        _lib = _get_lib()
        _check(
            _lib.cbsdk_session_set_ac_input_coupling(
                self._session, n_chans,
                int(_coerce_enum(ChannelType, channel_type)),
                enabled
            ),
            "Failed to set AC input coupling",
        )

    def set_channel_label(self, chan_id: int, label: str):
        """Set a channel's label."""
        _check(
            _get_lib().cbsdk_session_set_channel_label(
                self._session, chan_id, label.encode()
            ),
            "Failed to set channel label",
        )

    def set_channel_smpfilter(self, chan_id: int, filter_id: int):
        """Set a channel's continuous-time pathway filter."""
        _check(
            _get_lib().cbsdk_session_set_channel_smpfilter(
                self._session, chan_id, filter_id
            ),
            "Failed to set channel smpfilter",
        )

    def set_channel_spkfilter(self, chan_id: int, filter_id: int):
        """Set a channel's spike pathway filter."""
        _check(
            _get_lib().cbsdk_session_set_channel_spkfilter(
                self._session, chan_id, filter_id
            ),
            "Failed to set channel spkfilter",
        )

    def set_channel_ainpopts(self, chan_id: int, ainpopts: int):
        """Set a channel's analog input options (cbAINP_* flags)."""
        _check(
            _get_lib().cbsdk_session_set_channel_ainpopts(
                self._session, chan_id, ainpopts
            ),
            "Failed to set channel ainpopts",
        )

    def set_channel_lncrate(self, chan_id: int, rate: int):
        """Set a channel's line noise cancellation adaptation rate."""
        _check(
            _get_lib().cbsdk_session_set_channel_lncrate(
                self._session, chan_id, rate
            ),
            "Failed to set channel lncrate",
        )

    def set_channel_spkopts(self, chan_id: int, spkopts: int):
        """Set a channel's spike processing options (cbAINPSPK_* flags)."""
        _check(
            _get_lib().cbsdk_session_set_channel_spkopts(
                self._session, chan_id, spkopts
            ),
            "Failed to set channel spkopts",
        )

    def set_channel_spkthrlevel(self, chan_id: int, level: int):
        """Set a channel's spike threshold level."""
        _check(
            _get_lib().cbsdk_session_set_channel_spkthrlevel(
                self._session, chan_id, level
            ),
            "Failed to set channel spkthrlevel",
        )

    def set_channel_autothreshold(self, chan_id: int, enabled: bool):
        """Enable or disable auto-thresholding for a channel."""
        _check(
            _get_lib().cbsdk_session_set_channel_autothreshold(
                self._session, chan_id, enabled
            ),
            "Failed to set channel autothreshold",
        )

    def configure_channel(self, chan_id: int, **kwargs):
        """Configure one or more attributes of a single channel.

        This is a convenience method that dispatches to the individual setters.
        Each keyword argument maps to a channel attribute.

        Args:
            chan_id: 1-based channel ID.

        Keyword Args:
            label (str): Channel label (max 15 chars).
            smpgroup (SampleRate): Sample rate (e.g., ``SampleRate.SR_30kHz``).
            smpfilter (int): Continuous-time filter ID.
            spkfilter (int): Spike pathway filter ID.
            ainpopts (int): Analog input option flags.
            lncrate (int): LNC adaptation rate.
            spkopts (int): Spike processing option flags.
            spkthrlevel (int): Spike threshold level.
            autothreshold (bool): Auto-threshold enable.

        Example::

            session.configure_channel(1,
                smpgroup=SampleRate.SR_30kHz,
                smpfilter=6,
                autothreshold=True,
            )
        """
        _dispatch = {
            "label": self.set_channel_label,
            "smpfilter": self.set_channel_smpfilter,
            "spkfilter": self.set_channel_spkfilter,
            "ainpopts": self.set_channel_ainpopts,
            "lncrate": self.set_channel_lncrate,
            "spkopts": self.set_channel_spkopts,
            "spkthrlevel": self.set_channel_spkthrlevel,
            "autothreshold": self.set_channel_autothreshold,
        }
        for key, value in kwargs.items():
            if key == "smpgroup":
                # Special case: smpgroup goes through the batch setter for one channel
                chan_type = self.get_channel_type(chan_id)
                if chan_type is not None:
                    _lib = _get_lib()
                    _check(
                        _lib.cbsdk_session_set_channel_sample_group(
                            self._session, 1, int(chan_type),
                            int(_coerce_enum(SampleRate, value, _RATE_ALIASES)),
                            False
                        ),
                        "Failed to set smpgroup",
                    )
            elif key in _dispatch:
                _dispatch[key](chan_id, value)
            else:
                raise ValueError(f"Unknown channel attribute: {key!r}")

    # --- Channel Mapping (CMP) Files ---

    def load_channel_map(self, filepath: str, bank_offset: int = 0):
        """Load a channel mapping file (.cmp) and apply electrode positions.

        CMP files define physical electrode positions on arrays. Because the device
        does not persist position data, positions are stored locally and overlaid
        onto channel info whenever updated config data arrives from the device.

        Can be called multiple times for different front-end ports on a Hub device,
        each with a different array and CMP file.

        Args:
            filepath: Path to the .cmp file.
            bank_offset: Offset added to CMP bank indices for multi-port Hubs.
                CMP bank letter A becomes absolute bank (1 + bank_offset).
                Port 1: offset 0 (A=bank 1). Port 2: offset 4 (A=bank 5), etc.
        """
        _check(
            _get_lib().cbsdk_session_load_channel_map(
                self._session, filepath.encode(), bank_offset
            ),
            "Failed to load channel map",
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
        channel_type: ChannelType,
        sort_options: int,
    ):
        """Set spike sorting options for channels of a specific type.

        Args:
            n_chans: Number of channels to configure.
            channel_type: Channel type filter (e.g., ``ChannelType.FRONTEND``).
            sort_options: Spike sorting option flags (cbAINPSPK_*).
        """
        _lib = _get_lib()
        _check(
            _lib.cbsdk_session_set_channel_spike_sorting(
                self._session, n_chans,
                int(_coerce_enum(ChannelType, channel_type)),
                sort_options
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
        self, rate: SampleRate = SampleRate.SR_30kHz, buffer_seconds: float = 10.0
    ) -> ContinuousReader:
        """Create a ring buffer that accumulates continuous group data.

        Registers a group callback internally. Call :meth:`ContinuousReader.read`
        to retrieve the most recent samples as a numpy array.

        Args:
            rate: Sample rate to subscribe to.
            buffer_seconds: Ring buffer duration in seconds.

        Returns:
            A :class:`ContinuousReader` instance.
        """
        rate = _coerce_enum(SampleRate, rate, _RATE_ALIASES)
        n_channels = len(self.get_group_channels(int(rate)))
        if n_channels == 0:
            raise ValueError(f"No channels configured for {rate.name}")
        buffer_samples = int(buffer_seconds * rate.hz)
        return ContinuousReader(self, rate, n_channels, buffer_samples)

    def read_continuous(self, rate: SampleRate = SampleRate.SR_30kHz, duration: float = 1.0):
        """Collect continuous data for a specified duration.

        Blocks for *duration* seconds while accumulating group samples,
        then returns the collected data.

        Args:
            rate: Sample rate to subscribe to.
            duration: Collection duration in seconds.

        Returns:
            numpy.ndarray of shape ``(n_channels, n_samples)``, dtype ``int16``.
        """
        import time
        import numpy as np

        rate = _coerce_enum(SampleRate, rate, _RATE_ALIASES)
        n_channels = len(self.get_group_channels(int(rate)))
        if n_channels == 0:
            raise ValueError(f"No channels configured for {rate.name}")

        max_samples = int(duration * rate.hz * 1.2)  # 20% headroom
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
            self._session, int(rate), c_group_cb, ffi.NULL
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

    # --- Bulk Configuration ---

    @property
    def sysfreq(self) -> int:
        """System sampling frequency in Hz (e.g., 30000)."""
        return _get_lib().cbsdk_session_get_sysfreq(self._session)

    @staticmethod
    def num_filters() -> int:
        """Number of available filters (cbMAXFILTS)."""
        return _get_lib().cbsdk_get_num_filters()

    def get_filter_info(self, filter_id: int) -> Optional[dict]:
        """Get a filter's description.

        Args:
            filter_id: Filter ID (0 to num_filters()-1).

        Returns:
            Dict with keys ``label``, ``hpfreq``, ``hporder``, ``lpfreq``,
            ``lporder``. Frequencies are in milliHertz. Returns None if invalid.
        """
        _lib = _get_lib()
        ptr = _lib.cbsdk_session_get_filter_label(self._session, filter_id)
        if ptr == ffi.NULL:
            return None
        return {
            "label": ffi.string(ptr).decode(),
            "hpfreq": _lib.cbsdk_session_get_filter_hpfreq(self._session, filter_id),
            "hporder": _lib.cbsdk_session_get_filter_hporder(self._session, filter_id),
            "lpfreq": _lib.cbsdk_session_get_filter_lpfreq(self._session, filter_id),
            "lporder": _lib.cbsdk_session_get_filter_lporder(self._session, filter_id),
        }

    def get_channel_config(self, chan_id: int) -> Optional[dict]:
        """Get full configuration for a single channel.

        Args:
            chan_id: 1-based channel ID.

        Returns:
            Dict with channel configuration fields, or None if the channel
            is invalid or not connected.
        """
        chan_type = self.get_channel_type(chan_id)
        if chan_type is None:
            return None
        return {
            "label": self.get_channel_label(chan_id) or "",
            "type": chan_type,
            "chancaps": self.get_channel_chancaps(chan_id),
            "smpgroup": self.get_channel_smpgroup(chan_id),
            "smpfilter": self.get_channel_smpfilter(chan_id),
            "spkfilter": self.get_channel_spkfilter(chan_id),
            "spkopts": self.get_channel_spkopts(chan_id),
            "spkthrlevel": self.get_channel_spkthrlevel(chan_id),
            "ainpopts": self.get_channel_ainpopts(chan_id),
            "lncrate": self.get_channel_lncrate(chan_id),
            "refelecchan": self.get_channel_refelecchan(chan_id),
            "amplrejpos": self.get_channel_amplrejpos(chan_id),
            "amplrejneg": self.get_channel_amplrejneg(chan_id),
        }

    def get_config(self) -> dict:
        """Get bulk device configuration.

        Returns a dictionary with system-level info, per-channel configuration
        (only for existing/connected channels), and group memberships.

        Returns:
            Dict with keys:
                - ``sysfreq`` (int): System sampling frequency in Hz.
                - ``channels`` (dict): Mapping of chan_id -> channel config dict.
                - ``groups`` (dict): Mapping of group_id -> group info dict.
                - ``filters`` (dict): Mapping of filter_id -> filter info dict.

        Example::

            config = session.get_config()
            for chan_id, info in config["channels"].items():
                print(f"Ch {chan_id}: {info['label']} type={info['type']}")
        """
        _lib = _get_lib()
        max_chans = _lib.cbsdk_get_max_chans()

        channels = {}
        for chan_id in range(1, max_chans + 1):
            info = self.get_channel_config(chan_id)
            if info is not None:
                channels[chan_id] = info

        groups = {}
        for group_id in range(1, 7):
            label = self.get_group_label(group_id)
            ch_list = self.get_group_channels(group_id)
            groups[group_id] = {
                "label": label or "",
                "channels": ch_list,
            }

        num_filt = _lib.cbsdk_get_num_filters()
        filters = {}
        for filt_id in range(num_filt):
            info = self.get_filter_info(filt_id)
            if info is not None and info["label"]:
                filters[filt_id] = info

        return {
            "sysfreq": self.sysfreq,
            "channels": channels,
            "groups": groups,
            "filters": filters,
        }

    # --- Version ---

    @staticmethod
    def version() -> str:
        """Get the SDK version string."""
        return ffi.string(_get_lib().cbsdk_get_version()).decode()


class ContinuousReader:
    """Ring buffer that accumulates continuous group data into a numpy array.

    Created via :meth:`Session.continuous_reader`.

    Example::

        reader = session.continuous_reader(rate=SampleRate.SR_30kHz, buffer_seconds=10)
        import time
        time.sleep(2)
        data = reader.read()  # (n_channels, ~60000) int16 array
        reader.close()

    Attributes:
        n_channels: Number of channels in the group.
        sample_rate: Sample rate in Hz.
    """

    def __init__(self, session: Session, rate: SampleRate, n_channels: int,
                 buffer_samples: int):
        import numpy as np

        self._session = session
        self._rate = rate
        self.n_channels = n_channels
        self.sample_rate = rate.hz
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
            self._session._session, int(self._rate), c_group_cb, ffi.NULL
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
