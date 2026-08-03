"""One-command hardware soak for CereLink PR #190 (legacy / non-Gemini sets).

Runs every automatable check for ONE version+hardware combination and prints a
PASS/FAIL table, then a short manual checklist of the values it actually read so
you can eyeball them against Central's GUI.

    # Windows, Central 7.0.6 running and streaming (protocol 3.11)
    python -m pycbsdk.cli.soak --device LEGACY_NSP --central 7.0 --intense

    # Windows, Central 7.6 running and streaming (protocol 4.1)
    python -m pycbsdk.cli.soak --device LEGACY_NSP --central 7.6 --intense

    # macOS, Central closed -- spawns its own STANDALONE producer, then
    # attaches as a NATIVE CLIENT and drives the reader-side checks
    python -m pycbsdk.cli.soak --device LEGACY_NSP --native --intense

Soak length is computed from the measured packet rate and the ring size for the
selected version, targeting --wraps ring crossings (default 3).  Exit code 0 =
every automated check passed.
"""

from __future__ import annotations

import argparse
import pathlib
import subprocess
import sys
import threading
import time

from pycbsdk import DeviceType, ProtocolVersion, SampleRate, Session

# cbRECBUFFLEN in DWORDS per Central application version; bytes = dwords * 4.
# These come from Central's own constants, so they do NOT shrink with a
# 128-channel device -- they set how long the soak must run.
RING_DWORDS = {
    "7.0": 256 * 32768 * 4,
    "7.5": 512 * 32768 * 4,
    "7.6": 512 * 32768 * 4,
    "7.7": 512 * 65536 * 4,
    "7.8": 768 * 65536 * 4 - 1,
    "native": 256 * 65536 * 4 - 1,  # NATIVE_cbRECBUFFLEN
}

EXPECT_PROTO = {
    "7.0": ProtocolVersion.V3_11,
    "7.5": ProtocolVersion.V4_0,
    "7.6": ProtocolVersion.V4_1,
    "7.7": ProtocolVersion.V4_1,
    "7.8": ProtocolVersion.CURRENT,
    "native": ProtocolVersion.CURRENT,
}

results: list[tuple[str, str, str]] = []  # (phase, PASS/FAIL/WARN, detail)
manual: list[str] = []


def record(phase: str, ok: bool | None, detail: str) -> None:
    status = "PASS" if ok else ("WARN" if ok is None else "FAIL")
    results.append((phase, status, detail))
    print(f"  [{status}] {phase}: {detail}")


class Canary:
    """Catch-all packet validator -- same assertions as the CI wrap regression."""

    def __init__(self, max_chans: int) -> None:
        self.max_chans = max_chans
        self.bad: list[tuple[str, int, int]] = []
        self.pkts = 0
        self.ring_bytes = 0
        self._lock = threading.Lock()

    def install(self, session: Session) -> None:
        @session.on_packet()
        def _on_pkt(header, data):
            t, chid = header.type, header.chid
            if (t & 0xFF00) != 0:
                self._flag("type-high-bits", t, chid)
            elif chid != 0 and chid != 0x8000 and chid > self.max_chans:
                self._flag("chid-out-of-range", chid, t)
            self.pkts += 1
            self.ring_bytes += (header.dlen + 4) * 4

        self._ref = _on_pkt

    def _flag(self, kind: str, a: int, b: int) -> None:
        with self._lock:
            if len(self.bad) < 20:
                self.bad.append((kind, int(a), int(b)))


def open_session(device: str, attempts: int = 3, delay: float = 4.0) -> Session:
    """Open a session, retrying transient creation failures.

    Kept as insurance rather than as a workaround: the known cause of
    intermittent creation failures on this hardware -- pre-7.5.1 firmware never
    transmitting the SYSREP that terminates a REQCONFIGALL dump -- is handled in
    SdkSession now. A bare failure here still aborts the whole run though, and
    in the --_hold producer the parent only reports "producer failed to start",
    which hides the reason, so a couple of retries are worth the seconds.
    """
    last_exc = None
    for attempt in range(attempts):
        if attempt:
            time.sleep(delay)
        try:
            return Session(DeviceType[device])
        except Exception as exc:  # noqa: BLE001 - diagnostic harness
            last_exc = exc
            print(
                f"  session create attempt {attempt + 1}/{attempts} failed: {exc}",
                file=sys.stderr,
                flush=True,
            )
    raise RuntimeError(
        f"could not open {device} session after {attempts} attempts: {last_exc}"
    )


def wait_for_packets(session: Session, timeout: float = 10.0) -> int:
    """Block until packets are flowing; return how many arrived."""
    seen = [0]

    @session.on_packet()
    def _count(header, data):
        seen[0] += 1

    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline and seen[0] == 0:
        time.sleep(0.1)
    return seen[0]


# ---------------------------------------------------------------------------
# Phases
# ---------------------------------------------------------------------------


def phase_attach(session: Session, key: str) -> None:
    standalone = session.is_standalone  # property
    proto = session.protocol_version
    expect = EXPECT_PROTO[key]
    if key == "native":
        record(
            "attach/mode", not standalone, f"NATIVE CLIENT (is_standalone={standalone})"
        )
    else:
        record(
            "attach/mode",
            not standalone,
            f"CENTRAL CLIENT (is_standalone={standalone})",
        )
    record("attach/protocol", proto == expect, f"{proto.name} (expected {expect.name})")
    record("attach/max_chans", session.max_chans() > 0, str(session.max_chans()))


def phase_config(session: Session) -> None:
    sysfreq = session.sysfreq
    record("config/sysfreq", sysfreq == 30000, f"{sysfreq} Hz")
    manual.append(f"sysfreq = {sysfreq}")

    ok = True
    # Clamp the probes to what this device actually has, so a smaller channel
    # count is not reported as a layout fault.
    n_chans = session.max_chans()
    for ch in [c for c in (1, 2, 64, 128) if c <= n_chans]:
        try:
            label = session.get_channel_label(ch)
        except Exception as exc:  # noqa: BLE001 - diagnostic harness
            record(f"config/label[{ch}]", False, f"raised {exc!r}")
            ok = False
            continue
        printable = bool(label) and all(32 <= ord(c) < 127 for c in label)
        if not printable:
            ok = False
        manual.append(f"chan {ch} label = {label!r}")
        record(f"config/label[{ch}]", printable, f"{label!r}")
    record(
        "config/labels",
        ok,
        "all printable" if ok else "garbage or error -- LAYOUT SUSPECT",
    )

    try:
        finfo = session.get_filter_info(1)
        record("config/filter[1]", bool(finfo), str(finfo))
        manual.append(f"filter 1 = {finfo}")
    except Exception as exc:  # noqa: BLE001 - diagnostic harness
        record("config/filter[1]", False, f"raised {exc!r}")

    grp = session.get_group_channels(int(SampleRate.SR_30kHz))
    record(
        "config/group30k", len(grp) > 0, f"{len(grp)} chans, first 8={list(grp[:8])}"
    )
    manual.append(f"30 kHz group = {len(grp)} chans, first 8 = {list(grp[:8])}")


def phase_soak(session: Session, key: str, wraps: int) -> None:
    ring = RING_DWORDS[key] * 4
    canary = Canary(session.max_chans())
    canary.install(session)

    print(f"\n  measuring rate for 10 s (ring = {ring / 1048576:.0f} MiB) ...")
    t0, b0 = time.monotonic(), canary.ring_bytes
    time.sleep(10.0)
    rate = (canary.ring_bytes - b0) / max(time.monotonic() - t0, 0.1)
    if rate < 1000:
        record(
            "soak",
            False,
            f"almost no traffic ({rate:.0f} B/s) -- is the device streaming?",
        )
        return
    # Fail fast when the target is unreachable instead of soaking to the cap and
    # then reporting a wrap shortfall. A device streaming only its idle trickle
    # (no sample group enabled, or the front-end channels live on a different
    # device) clears the "almost no traffic" guard above but would still need
    # hours to fill the ring.
    cap = 1800.0
    need = wraps * ring / rate
    if need > cap:
        record(
            "soak/rate",
            False,
            f"{rate / 1e6:.2f} MB/s reaches only {cap * rate / ring:.1f} of "
            f"{wraps} wraps within the {cap:.0f}s cap "
            f"(would need {need / 60:.0f} min) -- is a sample group enabled "
            "on this device's front-end channels?",
        )
        return
    duration = max(60.0, need)
    print(
        f"  rate {rate / 1e6:.2f} MB/s -> soaking {duration:.0f}s "
        f"for ~{duration * rate / ring:.1f} wraps"
    )

    start = time.monotonic()
    next_report = 15.0
    while time.monotonic() - start < duration:
        time.sleep(1.0)
        el = time.monotonic() - start
        if el >= next_report:
            next_report += 15.0
            print(
                f"    t={el:6.0f}s  pkts={canary.pkts:<12,}"
                f" {canary.ring_bytes / 1048576:8.1f} MiB  malformed={len(canary.bad)}"
            )

    mib = canary.ring_bytes / 1048576
    actual = canary.ring_bytes / ring
    record(
        "soak/malformed",
        not canary.bad,
        f"{len(canary.bad)} malformed {canary.bad[:3]}",
    )
    record(
        "soak/wraps",
        actual >= wraps,
        f"{actual:.1f} wraps ({mib:.0f} MiB through ring)",
    )
    stats = session.stats
    dropped = stats.packets_dropped
    if session.is_standalone:
        record("soak/drops", dropped == 0, f"packets_dropped={dropped}")
    else:
        # CLIENT sessions now count what they ingest from the ring, and record
        # ring reads that lost data, so this is a real check rather than a
        # deferral to phase_overrun_recovery.
        record(
            "soak/drops",
            stats.shmem_overruns == 0,
            f"shmem_overruns={stats.shmem_overruns}, packets_dropped={dropped}",
        )
        # packets_produced is a live producer-side count. On a single-instrument
        # ring the difference is the exact shortfall; on a Central ring it counts
        # every instrument, so treat it as an upper bound and only report it.
        behind = stats.packets_produced - stats.packets_received
        manual.append(
            f"ring: produced={stats.packets_produced:,} "
            f"received={stats.packets_received:,} (behind by {behind:,})"
        )
    manual.append(f"soak: {canary.pkts:,} pkts, {mib:.0f} MiB, {actual:.1f} wraps")


def phase_reattach(device: str, rounds: int) -> Session:
    """Close and reopen the client repeatedly while the producer keeps streaming."""
    session = None
    ok = True
    for i in range(1, rounds + 1):
        if session is not None:
            session.close()
        time.sleep(0.5)
        try:
            session = open_session(device)
            n = wait_for_packets(session, timeout=15.0)
        except Exception as exc:  # noqa: BLE001 - diagnostic harness
            record(f"reattach[{i}]", False, f"raised {exc!r}")
            ok = False
            continue
        if n == 0:
            ok = False
        record(f"reattach[{i}]", n > 0, f"{n} packets after re-open")
    record("reattach", ok, f"{rounds} rounds")
    return session


def phase_clock(session: Session) -> None:
    for _ in range(4):
        session.send_clock_probe()
        time.sleep(0.5)
    deadline = time.monotonic() + 10.0
    while session.clock_offset_ns is None and time.monotonic() < deadline:
        time.sleep(0.25)

    off, unc = session.clock_offset_ns, session.clock_uncertainty_ns
    record("clock/offset", off is not None, f"offset_ns={off}")
    record("clock/uncertainty", unc is not None, f"uncertainty_ns={unc}")
    manual.append(f"clock offset={off} uncertainty={unc}")
    if off is None:
        return

    last = {"ns": 0}

    @session.on_packet()
    def _grab(header, data):
        last["ns"] = header.time

    time.sleep(1.0)
    if not last["ns"]:
        record("clock/device_to_monotonic", None, "no packet timestamp captured")
        return
    try:
        conv = session.device_to_monotonic(last["ns"])
        delta_ms = (time.monotonic() - conv) * 1000
        record(
            "clock/device_to_monotonic",
            abs(delta_ms) < 100.0,
            f"{delta_ms:+.2f} ms vs monotonic",
        )
    except Exception as exc:  # noqa: BLE001 - diagnostic harness
        record("clock/device_to_monotonic", False, f"raised {exc!r}")


def phase_config_write(session: Session) -> None:
    chan = 1
    try:
        orig_label = session.get_channel_label(chan)
        orig_thr = session.get_channel_spkthrlevel(chan)
    except Exception as exc:  # noqa: BLE001 - diagnostic harness
        record("write/read-original", False, f"raised {exc!r}")
        return

    def settle(getter, want, tries: int = 20):
        for _ in range(tries):
            if getter() == want:
                return True
            time.sleep(0.1)
        return False

    try:
        session.set_channel_label(chan, "SOAKTEST", auto_sync=True)
        ok = settle(lambda: session.get_channel_label(chan), "SOAKTEST")
        record(
            "write/label",
            ok,
            f"set 'SOAKTEST' -> read {session.get_channel_label(chan)!r}",
        )
        session.set_channel_label(chan, orig_label or f"chan{chan}", auto_sync=True)
    except Exception as exc:  # noqa: BLE001 - diagnostic harness
        record("write/label", None, f"not supported here: {exc!r}")

    try:
        target = int(orig_thr) - 100 if orig_thr else -65000
        session.set_channel_spkthrlevel(chan, target)
        ok = settle(lambda: session.get_channel_spkthrlevel(chan), target)
        record(
            "write/spkthrlevel",
            ok,
            f"set {target} -> read {session.get_channel_spkthrlevel(chan)}",
        )
        session.set_channel_spkthrlevel(chan, int(orig_thr))
    except Exception as exc:  # noqa: BLE001 - diagnostic harness
        record("write/spkthrlevel", None, f"not supported here: {exc!r}")


# ---------------------------------------------------------------------------
# --intense phases
# ---------------------------------------------------------------------------

# ChanInfoField -> (dedicated getter, get_channel_config key).  Reading the same
# field through three independent code paths and demanding they agree is an
# ASYMMETRIC check: unlike a set/get round-trip it cannot be fooled by a
# translation that is self-consistently wrong.
FIELD_PATHS = {
    "SMPGROUP": ("get_channel_smpgroup", "smpgroup"),
    "SMPFILTER": ("get_channel_smpfilter", "smpfilter"),
    "SPKFILTER": ("get_channel_spkfilter", "spkfilter"),
    "AINPOPTS": ("get_channel_ainpopts", "ainpopts"),
    "SPKOPTS": ("get_channel_spkopts", "spkopts"),
    "SPKTHRLEVEL": ("get_channel_spkthrlevel", "spkthrlevel"),
    "LNCRATE": ("get_channel_lncrate", "lncrate"),
    "REFELECCHAN": ("get_channel_refelecchan", "refelecchan"),
    "AMPLREJPOS": ("get_channel_amplrejpos", "amplrejpos"),
    "AMPLREJNEG": ("get_channel_amplrejneg", "amplrejneg"),
    "CHANCAPS": ("get_channel_chancaps", "chancaps"),
}


def phase_api_sweep(session: Session, n_chans: int) -> None:
    """Read the whole config surface for every channel; demand cross-path agreement."""
    from pycbsdk import ChanInfoField, ChannelType

    mismatches: list[str] = []
    errors: list[str] = []
    for ch in range(1, n_chans + 1):
        try:
            cfg = session.get_channel_config(ch)
            for name, (getter, key) in FIELD_PATHS.items():
                a = getattr(session, getter)(ch)
                b = session.get_channel_field(ch, getattr(ChanInfoField, name))
                c = cfg.get(key) if cfg else None
                if a != b or (c is not None and c != a):
                    mismatches.append(f"ch{ch}.{name}: getter={a} field={b} config={c}")
            session.get_channel_type(ch)
            session.get_channel_scaling(ch)
        except Exception as exc:  # noqa: BLE001 - diagnostic harness
            errors.append(f"ch{ch}: {exc!r}")

    record("sweep/no-errors", not errors, f"{len(errors)} errors {errors[:3]}")
    record(
        "sweep/cross-path",
        not mismatches,
        f"{len(mismatches)} mismatches {mismatches[:3]} "
        "(disagreement => field offset/translation bug)",
    )

    # Bulk accessors must agree with the per-channel ones.
    try:
        bulk_labels = session.get_channels_labels(ChannelType.FRONTEND)
        per = [session.get_channel_label(c) for c in range(1, len(bulk_labels) + 1)]
        record("sweep/bulk-labels", bulk_labels == per, f"{len(bulk_labels)} labels")
        bulk_grp = session.get_channels_field(
            ChannelType.FRONTEND, ChanInfoField.SMPGROUP
        )
        per_grp = [session.get_channel_smpgroup(c) for c in range(1, len(bulk_grp) + 1)]
        record("sweep/bulk-field", bulk_grp == per_grp, f"{len(bulk_grp)} smpgroups")
        ids = session.get_matching_channel_ids(ChannelType.FRONTEND)
        # Compare against the bulk view rather than demanding a non-zero count:
        # a Gemini NSP is I/O-only and correctly has no front-end channels, so
        # "> 0" fails on healthy hardware.  Agreement between the two is the
        # real assertion, and it still catches a lookup returning nothing when
        # the device does have front-end channels.
        record(
            "sweep/matching-ids",
            len(ids) == len(bulk_labels),
            f"{len(ids)} FRONTEND ids (bulk reported {len(bulk_labels)})",
        )
    except Exception as exc:  # noqa: BLE001 - diagnostic harness
        record("sweep/bulk", False, f"raised {exc!r}")

    # Every filter and every group.
    fbad = []
    for f in range(session.num_filters()):  # filter ids are 0-based
        try:
            if not session.get_filter_info(f):
                fbad.append(f)
        except Exception as exc:  # noqa: BLE001 - diagnostic harness
            fbad.append(f"{f}:{exc!r}")
    record("sweep/filters", not fbad, f"0..{session.num_filters() - 1} bad={fbad[:5]}")

    gbad = []
    for g in range(1, 7):
        try:
            session.get_group_label(g)
            session.get_group_channels(g)
        except Exception as exc:  # noqa: BLE001 - diagnostic harness
            gbad.append(f"{g}:{exc!r}")
    record("sweep/groups", not gbad, f"groups 1..6 bad={gbad[:5]}")

    # Cross-check the two independent views of group membership: the per-group
    # channel list against each channel's own smpgroup.  Unlike the field
    # cross-check above, these reach the config by genuinely different routes,
    # so agreement here is evidence and disagreement is a real defect.
    #
    # This exists because a HUB2 CENTRAL CLIENT session once passed every check
    # in this phase while serving HUB1's configuration: three read paths agreed
    # with each other and were all wrong.  These two did disagree, and nothing
    # compared them.
    mismatches = []
    for g in range(1, 7):
        try:
            listed = set(session.get_group_channels(g))
        except Exception as exc:  # noqa: BLE001 - diagnostic harness
            mismatches.append(f"grp{g}:list raised {exc!r}")
            continue
        scanned = set()
        for ch in range(1, n_chans + 1):
            try:
                if session.get_channel_smpgroup(ch) == g:
                    scanned.add(ch)
            except Exception:  # noqa: BLE001 - absent channels are not a fault
                continue
        # Only channels within this device's range are comparable; the group
        # list may legitimately mention ids beyond it on some layouts.
        listed_in_range = {c for c in listed if 1 <= c <= n_chans}
        if listed_in_range != scanned:
            only_listed = sorted(listed_in_range - scanned)[:4]
            only_scanned = sorted(scanned - listed_in_range)[:4]
            mismatches.append(
                f"grp{g}: get_group_channels-only={only_listed} "
                f"smpgroup-only={only_scanned}"
            )
    record(
        "sweep/group-membership-agrees",
        not mismatches,
        "; ".join(mismatches[:3])
        if mismatches
        else "get_group_channels() agrees with get_channel_smpgroup()",
    )

    try:
        cfg = session.get_config()
        record(
            "sweep/get_config", isinstance(cfg, dict) and bool(cfg), f"{len(cfg)} keys"
        )
    except Exception as exc:  # noqa: BLE001 - diagnostic harness
        record("sweep/get_config", False, f"raised {exc!r}")


def phase_invalid_inputs(session: Session) -> None:
    """Out-of-range ids must raise or return None -- never a plausible value."""
    from pycbsdk import ChanInfoField

    maxch = session.max_chans()
    bad: list[str] = []

    def rejects(desc: str, fn) -> None:
        try:
            got = fn()
        except Exception:  # noqa: BLE001 - rejection is the expected outcome
            return
        if got is not None and got != [] and got != "":
            bad.append(f"{desc} -> {got!r}")

    for ch in (0, maxch + 1, maxch + 1000, -1):
        rejects(f"label({ch})", lambda ch=ch: session.get_channel_label(ch))
        rejects(f"config({ch})", lambda ch=ch: session.get_channel_config(ch))
        rejects(
            f"field({ch})",
            lambda ch=ch: session.get_channel_field(ch, ChanInfoField.SMPGROUP),
        )
    for f in (session.num_filters(), session.num_filters() + 1, 9999, -1):
        rejects(f"filter({f})", lambda f=f: session.get_filter_info(f))
    for g in (0, 7, 9999):
        rejects(f"group({g})", lambda g=g: session.get_group_channels(g))

    record(
        "invalid/rejected",
        not bad,
        f"{len(bad)} accepted out-of-range input {bad[:4]}",
    )


def phase_label_edges(session: Session) -> None:
    """cbLEN_STR_LABEL is 16 bytes; check the boundary and neighbour corruption."""
    ch, neighbour = 1, 2
    orig = session.get_channel_label(ch)
    orig_n = session.get_channel_label(neighbour)
    # cbLEN_STR_LABEL is 16 bytes and the setter documents a 15-char max, so the
    # exact cut-off is an implementation detail.  Assert the invariants that
    # matter: the read-back is a prefix of what we wrote, it never exceeds the
    # field, and the neighbouring channel is untouched (overflow canary).
    cases = [
        ("15 chars", "A" * 15),
        ("16 chars", "B" * 16),
        ("17 chars (truncate)", "C" * 17),
        ("32 chars (truncate)", "D" * 32),
        ("empty", ""),
    ]
    ok = True
    for desc, value in cases:
        try:
            session.set_channel_label(ch, value, auto_sync=True)
            # auto_sync syncs BEFORE dispatching the setter, not after, so the
            # write is still in flight here -- reading immediately returns the
            # previous label.  Poll until the device reports a value consistent
            # with what we wrote (or the settle window expires).
            got = ""
            for _ in range(30):
                got = session.get_channel_label(ch) or ""
                if got == value[: len(got)] and (got or not value):
                    break
                time.sleep(0.1)
            prefix_ok = value.startswith(got)
            len_ok = len(got) <= 16
            n_ok = (session.get_channel_label(neighbour) or "") == (orig_n or "")
            if not (prefix_ok and len_ok and n_ok):
                ok = False
            record(
                f"label/{desc}",
                prefix_ok and len_ok and n_ok,
                f"read {got!r} (len {len(got)}); prefix_ok={prefix_ok} "
                f"neighbour_intact={n_ok}",
            )
        except Exception as exc:  # noqa: BLE001 - diagnostic harness
            ok = False
            record(f"label/{desc}", False, f"raised {exc!r}")
    try:
        session.set_channel_label(ch, orig or f"chan{ch}", auto_sync=True)
    except Exception as exc:  # noqa: BLE001 - diagnostic harness
        ok = False
        record("label/restore", False, f"raised {exc!r}")
    record("label/edges", ok, "16-byte field boundary + neighbour integrity")


def phase_write_matrix(session: Session) -> None:
    """Exercise every writable field through translation, then restore it."""
    from pycbsdk import ChannelType

    chans = [1, 2]
    snap = {c: session.get_channel_config(c) for c in chans}

    def settle(getter, want, tries: int = 25) -> bool:
        for _ in range(tries):
            if getter() == want:
                return True
            time.sleep(0.1)
        return False

    def try_set(desc: str, apply, verify) -> None:
        try:
            apply()
            ok = verify()
            record(
                f"write/{desc}", ok, "read-back matched" if ok else "read-back MISMATCH"
            )
        except Exception as exc:  # noqa: BLE001 - diagnostic harness
            record(f"write/{desc}", None, f"not supported here: {exc!r}")

    for rate, want in ((SampleRate.SR_1kHz, 2), (SampleRate.SR_30kHz, 5)):
        try_set(
            f"sample_group[{rate.name}]",
            lambda r=rate: session.set_sample_group(chans, ChannelType.FRONTEND, r),
            lambda w=want: settle(lambda: session.get_channel_smpgroup(chans[0]), w),
        )
    for enabled in (True, False):
        try_set(
            f"ac_coupling[{enabled}]",
            lambda e=enabled: session.set_ac_input_coupling(
                chans, ChannelType.FRONTEND, e
            ),
            lambda: True,  # ainpopts bit layout varies; absence of error is the check
        )
    for enabled in (False, True):
        try_set(
            f"spike_extraction[{enabled}]",
            lambda e=enabled: session.set_spike_extraction(
                chans, ChannelType.FRONTEND, e
            ),
            lambda e=enabled: settle(
                lambda: bool(session.get_channel_spkopts(chans[0]) & 0x1), e
            ),
        )
    try_set(
        "spike_sorting",
        lambda: session.set_spike_sorting(chans, ChannelType.FRONTEND, 0),
        lambda: True,
    )
    try_set(
        "lncrate",
        lambda: session.set_channel_lncrate(chans[0], 2, auto_sync=True),
        lambda: settle(lambda: session.get_channel_lncrate(chans[0]), 2),
    )
    try_set(
        "smpfilter",
        lambda: session.set_channel_smpfilter(chans[0], 1, auto_sync=True),
        lambda: settle(lambda: session.get_channel_smpfilter(chans[0]), 1),
    )
    try_set(
        "spkfilter",
        lambda: session.set_channel_spkfilter(chans[0], 1, auto_sync=True),
        lambda: settle(lambda: session.get_channel_spkfilter(chans[0]), 1),
    )
    try_set(
        "autothreshold",
        lambda: session.set_channel_autothreshold(chans[0], True, auto_sync=True),
        lambda: True,
    )
    try_set(
        "configure_channel(multi)",
        lambda: session.configure_channel(
            chans[0], auto_sync=True, spkthrlevel=-60000, lncrate=1
        ),
        lambda: settle(lambda: session.get_channel_spkthrlevel(chans[0]), -60000),
    )

    # Restore everything we touched.
    restored = True
    for c, cfg in snap.items():
        if not cfg:
            continue
        try:
            session.configure_channel(
                c,
                auto_sync=True,
                smpgroup=cfg["smpgroup"],
                smpfilter=cfg["smpfilter"],
                spkfilter=cfg["spkfilter"],
                spkopts=cfg["spkopts"],
                spkthrlevel=cfg["spkthrlevel"],
                ainpopts=cfg["ainpopts"],
                lncrate=cfg["lncrate"],
            )
        except Exception as exc:  # noqa: BLE001 - diagnostic harness
            restored = False
            record(f"write/restore[{c}]", False, f"raised {exc!r}")
    record("write/restore", restored, f"restored chans {list(snap)}")


def phase_overrun_recovery(session: Session, key: str) -> None:
    """Stall the reader past a full ring, then require a clean recovery.

    This is the one test that drives the fail-safe paths (overrun detection and
    resync-to-head) against a real writer.  A user callback runs on the shmem
    receive thread, so sleeping in one stops the consumer while the producer
    keeps going.
    """
    ring = RING_DWORDS[key] * 4
    errs: list[str] = []

    @session.on_error
    def _on_err(msg: str) -> None:
        errs.append(msg)

    # Measure the rate so we stall for slightly longer than one ring.
    seen = {"bytes": 0}

    @session.on_packet()
    def _meter(header, data):
        seen["bytes"] += (header.dlen + 4) * 4

    time.sleep(5.0)
    rate = seen["bytes"] / 5.0
    if rate < 1000:
        record("overrun", None, "no traffic; skipped")
        return
    # The stall has to outlast one whole ring fill or the producer never laps
    # the consumer and there is simply nothing to detect.  A fixed cap turns a
    # slow device into a spurious failure: a Gemini NSP streaming 16 analog
    # channels at 1.45 MB/s needs ~277 s to fill a 256 MiB ring, so a 120 s
    # stall reported "no error" against perfectly healthy hardware.
    need = 1.5 * ring / rate
    cap = 400.0
    if need > cap:
        record(
            "overrun",
            None,
            f"{rate / 1e6:.2f} MB/s would need {need:.0f}s to lap a "
            f"{ring / 1048576:.0f} MiB ring, over the {cap:.0f}s cap -- skipped",
        )
        return
    stall = need

    state = {"stall": True, "bad": 0, "post": 0}
    maxch = session.max_chans()

    @session.on_packet()
    def _stall_then_watch(header, data):
        if state["stall"]:
            state["stall"] = False
            time.sleep(stall)  # blocks the receive thread -> forces an overrun
            return
        state["post"] += 1
        if (header.type & 0xFF00) != 0 or (
            header.chid not in (0, 0x8000) and header.chid > maxch
        ):
            state["bad"] += 1

    print(
        f"  stalling the reader for {stall:.0f}s to overrun a {ring / 1048576:.0f} MiB ring ..."
    )
    deadline = time.monotonic() + stall + 30.0
    while state["stall"] and time.monotonic() < deadline:
        time.sleep(0.5)
    time.sleep(stall + 10.0)

    overrun_seen = any("overrun" in e or "desync" in e for e in errs)
    record(
        "overrun/detected",
        overrun_seen,
        f"{len(errs)} errors, e.g. {errs[:1]}" if errs else "no error reported",
    )
    record(
        "overrun/recovered",
        state["post"] > 0,
        f"{state['post']} packets after the stall",
    )
    record(
        "overrun/clean-after",
        state["bad"] == 0,
        f"{state['bad']} malformed packets after recovery (MUST be 0)",
    )


def phase_reattach_storm(device: str, rounds: int) -> None:
    """Rapid attach/detach during heavy streaming -- hunts the torn-snapshot window."""
    fails = 0
    for i in range(rounds):
        try:
            s = open_session(device)
            n = wait_for_packets(s, timeout=10.0)
            maxch = s.max_chans()
            bad = [0]

            @s.on_packet()
            def _v(header, data, bad=bad, maxch=maxch):
                if (header.type & 0xFF00) != 0 or (
                    header.chid not in (0, 0x8000) and header.chid > maxch
                ):
                    bad[0] += 1

            time.sleep(0.05 + (i % 5) * 0.05)  # vary dwell to shift the attach phase
            s.close()
            if n == 0 or bad[0]:
                fails += 1
        except Exception:  # noqa: BLE001 - diagnostic harness
            fails += 1
    record("storm/reattach", fails == 0, f"{rounds} rapid cycles, {fails} failures")


def phase_concurrent_clients(device: str, n: int) -> None:
    """Several clients on one segment set -- each keeps its own independent tail."""
    sessions: list[Session] = []
    try:
        counts = []
        for _ in range(n):
            s = open_session(device)
            sessions.append(s)
            c = [0]

            @s.on_packet()
            def _c(header, data, c=c):
                c[0] += 1

            counts.append(c)
        time.sleep(10.0)
        got = [c[0] for c in counts]
        record(
            "concurrent/all-receive",
            all(g > 0 for g in got),
            f"per-client packets={got}",
        )
        if all(g > 0 for g in got):
            spread = max(got) / max(min(got), 1)
            record(
                "concurrent/balanced",
                spread < 5.0,
                f"max/min={spread:.2f} (independent tails should be comparable)",
            )
    except Exception as exc:  # noqa: BLE001 - diagnostic harness
        record("concurrent", False, f"raised {exc!r}")
    finally:
        for s in sessions:
            s.close()


def phase_timestamps(session: Session) -> None:
    """Non-decreasing, nanosecond-scale timestamps + batch/single agreement."""
    ts: list[int] = []

    @session.on_packet()
    def _collect(header, data):
        if len(ts) < 4000 and header.chid == 0:
            ts.append(int(header.time))

    deadline = time.monotonic() + 20.0
    while len(ts) < 2000 and time.monotonic() < deadline:
        time.sleep(0.2)
    if len(ts) < 100:
        record("ts", None, f"only {len(ts)} group packets captured; skipped")
        return

    regress = sum(1 for i in range(1, len(ts)) if ts[i] < ts[i - 1])
    record(
        "ts/monotonic", regress == 0, f"{regress} regressions over {len(ts)} packets"
    )

    deltas = sorted(ts[i] - ts[i - 1] for i in range(1, len(ts)))
    median = deltas[len(deltas) // 2]
    # 30 kHz in nanoseconds is ~33 333.  A median near 1 means raw device ticks
    # leaked through instead of being converted.
    record(
        "ts/nanosecond-scale",
        1_000 <= median <= 10_000_000,
        f"median delta {median} ns (~{median / 1e6:.3f} ms); ticks would be ~1",
    )

    try:
        sample = ts[:64]
        batch = session.device_to_monotonic_batch(sample, stream_id=-1)
        single = [session.device_to_monotonic(t, stream_id=-1) for t in sample]
        worst = max(abs(a - b) for a, b in zip(batch, single))
        record(
            "ts/batch-vs-single",
            worst < 1e-6,
            f"worst disagreement {worst * 1e9:.0f} ns",
        )
        mono = session.device_to_monotonic_batch(sample, stream_id=7)
        bad = sum(1 for i in range(1, len(mono)) if mono[i] < mono[i - 1])
        record("ts/stream-monotonic", bad == 0, f"{bad} regressions with stream_id=7")
        session.reset_monotonic(7)
        record("ts/reset_monotonic", True, "accepted")
    except Exception as exc:  # noqa: BLE001 - diagnostic harness
        record("ts/conversion", False, f"raised {exc!r}")


def phase_spikes(session: Session) -> None:
    """Force threshold crossings on noise, then validate the event path."""
    from pycbsdk import ChannelType

    chans = [1, 2, 3, 4]
    snap = {c: session.get_channel_config(c) for c in chans}
    events: list[tuple[int, int]] = []
    try:
        session.set_spike_extraction(chans, ChannelType.FRONTEND, True)
        for c in chans:
            session.set_channel_spkthrlevel(c, -100)  # trip on noise

        @session.on_event(ChannelType.FRONTEND)
        def _on_ev(header, data):
            if len(events) < 500:
                events.append((int(header.chid), int(header.type)))

        time.sleep(15.0)
        if not events:
            record("spikes/received", None, "no events (quiet input) -- path untested")
        else:
            maxa = session.num_analog_chans()
            bad = [e for e in events if not (1 <= e[0] <= maxa)]
            record("spikes/received", True, f"{len(events)} events")
            record(
                "spikes/chid-range", not bad, f"{len(bad)} out of 1..{maxa} {bad[:3]}"
            )
        record(
            "spikes/spike_length",
            session.spike_length > 0,
            f"len={session.spike_length} pre={session.spike_pretrigger}",
        )
    except Exception as exc:  # noqa: BLE001 - diagnostic harness
        record("spikes", False, f"raised {exc!r}")
    finally:
        for c, cfg in snap.items():
            if cfg:
                try:
                    session.configure_channel(
                        c,
                        auto_sync=True,
                        spkopts=cfg["spkopts"],
                        spkthrlevel=cfg["spkthrlevel"],
                    )
                except Exception as exc:  # noqa: BLE001 - best-effort restore
                    print(f"    (warn) could not restore chan {c}: {exc!r}")


def phase_continuous_reader(session: Session) -> None:
    try:
        reader = session.continuous_reader(SampleRate.SR_30kHz, buffer_seconds=2.0)
        time.sleep(3.0)
        data = reader.read(1000)
        shape = getattr(data, "shape", None)
        record("reader/read", shape is not None and shape[1] > 0, f"shape={shape}")
        record(
            "reader/counters",
            reader.total_samples > 0,
            f"total={reader.total_samples} available={reader.available} dropped={reader.dropped}",
        )
        reader.close()
    except ImportError as exc:
        record("reader", None, f"numpy not installed: {exc}")
    except Exception as exc:  # noqa: BLE001 - diagnostic harness
        record("reader", False, f"raised {exc!r}")
    try:
        arr = session.read_continuous(SampleRate.SR_30kHz, duration=0.5)
        record(
            "reader/read_continuous",
            getattr(arr, "shape", None) is not None,
            f"shape={getattr(arr, 'shape', None)}",
        )
    except ImportError as exc:
        record("reader/read_continuous", None, f"numpy not installed: {exc}")
    except Exception as exc:  # noqa: BLE001 - diagnostic harness
        record("reader/read_continuous", False, f"raised {exc!r}")


def phase_ccf(session: Session) -> None:
    """Save and reload a CCF -- a whole-config path through version translation."""
    import tempfile

    path = str(pathlib.Path(tempfile.gettempdir()) / "cerelink_soak.ccf")
    try:
        before = session.get_channel_label(1)
        session.save_ccf(path)
        size = pathlib.Path(path).stat().st_size
        record("ccf/save", size > 0, f"{size} bytes at {path}")
        session.load_ccf_sync(path, timeout=15.0)
        after = session.get_channel_label(1)
        record("ccf/roundtrip", before == after, f"chan1 label {before!r} -> {after!r}")
    except Exception as exc:  # noqa: BLE001 - diagnostic harness
        record("ccf", False, f"raised {exc!r}")


def phase_misc(session: Session, key: str) -> None:
    try:
        record(
            "misc/identity",
            bool(session.proc_ident) and bool(session.version()),
            f"proc={session.proc_ident!r} ver={session.version()!r} "
            f"fe={session.num_fe_chans()} analog={session.num_analog_chans()}",
        )
        record(
            "misc/runlevel",
            session.runlevel > 0,
            f"runlevel={session.runlevel} running={session.running} SR_30kHz.hz={SampleRate.SR_30kHz.hz}",
        )
        t1 = session.time
        time.sleep(0.3)
        record("misc/time-advances", session.time > t1, f"{t1} -> {session.time}")
        session.reset_stats()
        time.sleep(1.0)
        # packets_received is incremented on the shmem read path too, so a
        # CLIENT session must also show progress after a reset.
        received = session.stats.packets_received
        record("misc/stats", received > 0, f"received={received} after reset")
        session.sync(timeout=5.0)
        record("misc/sync", True, "completed")
    except Exception as exc:  # noqa: BLE001 - diagnostic harness
        record("misc", False, f"raised {exc!r}")

    # Transmit-path smokes: these reach the device/Central but cannot be read
    # back through pycbsdk, so absence of an error is the assertion.
    for desc, fn in (
        ("send_comment", lambda: session.send_comment("cerelink soak")),
        ("digital_output", lambda: session.set_digital_output(1, 0)),
    ):
        try:
            fn()
            record(f"tx/{desc}", True, "accepted")
        except Exception as exc:  # noqa: BLE001 - diagnostic harness
            record(f"tx/{desc}", None, f"not supported here: {exc!r}")

    # AOUT monitor is the WRITE side of the monsource translation -- the field it
    # lands in is not readable from pycbsdk, so it goes on the manual checklist.
    try:
        session.set_analog_output_monitor(
            aout_channel=1, monitor_channel=5, track_last=False
        )
        record("tx/aout_monitor", True, "wrote aout1 -> monitor chan 5")
        if key in ("7.0", "7.5"):
            manual.append(
                "monsource: script set AOUT 1 to monitor channel 5 -- confirm that "
                "in Central's GUI (packed monsource path)"
            )
    except Exception as exc:  # noqa: BLE001 - diagnostic harness
        record("tx/aout_monitor", None, f"not supported here: {exc!r}")


def phase_bounds() -> None:
    """Central 7.0 has cbMAXPROCS=1, so instrument index 1 (HUB2) must be rejected."""
    try:
        s = open_session("HUB2")
    except Exception as exc:  # noqa: BLE001 - diagnostic harness
        record("bounds/HUB2", True, f"rejected as expected ({type(exc).__name__})")
        return
    try:
        if s.is_standalone:
            record(
                "bounds/HUB2",
                True,
                "did not attach to Central (fell back to STANDALONE)",
            )
        else:
            record(
                "bounds/HUB2",
                False,
                "ATTACHED to Central instrument 1 on a 1-instrument build",
            )
    finally:
        s.close()


# ---------------------------------------------------------------------------


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--device", default="LEGACY_NSP")
    ap.add_argument(
        "--central",
        choices=sorted(RING_DWORDS),
        default=None,
        help="Central minor version, e.g. 7.0 or 7.6 (omit with --native)",
    )
    ap.add_argument(
        "--native",
        action="store_true",
        help="no Central; spawn own STANDALONE producer",
    )
    ap.add_argument(
        "--wraps", type=int, default=3, help="target ring crossings (default 3)"
    )
    ap.add_argument(
        "--reattach", type=int, default=5, help="re-attach rounds (default 5)"
    )
    ap.add_argument(
        "--intense",
        action="store_true",
        help="add edge-case + full-API phases: overrun recovery, re-attach storm, "
        "concurrent clients, all-channel config sweep, invalid inputs, label "
        "boundaries, write matrix, timestamps, spikes, reader, CCF (+10-15 min)",
    )
    ap.add_argument("--_hold", action="store_true", help=argparse.SUPPRESS)
    args = ap.parse_args(argv)

    # Internal: the STANDALONE producer for --native.  Holds the device open.
    if args._hold:
        s = open_session(args.device)
        print("READY", flush=True)
        try:
            while True:
                time.sleep(3600)
        finally:
            s.close()
        return 0

    if args.native == bool(args.central):
        ap.error("pass exactly one of --central VERSION or --native")
    key = "native" if args.native else args.central

    producer = None
    if args.native:
        print("spawning STANDALONE producer ...")
        producer = subprocess.Popen(
            [
                sys.executable,
                "-m",
                "pycbsdk.cli.soak",
                "--_hold",
                "--device",
                args.device,
            ],
            stdout=subprocess.PIPE,
            text=True,
        )
        if (producer.stdout.readline() or "").strip() != "READY":
            try:
                producer.wait(timeout=5.0)
            except subprocess.TimeoutExpired:
                producer.kill()
            print(
                f"producer failed to start (exit={producer.returncode}); "
                "its error output is above",
                file=sys.stderr,
            )
            return 2
        time.sleep(2.0)  # let it populate shmem

    print(f"\n=== {args.device} / {key} ===")
    session = None
    try:
        session = open_session(args.device)
        print("\n-- attach --")
        phase_attach(session, key)
        print("\n-- config --")
        phase_config(session)
        print("\n-- soak --")
        phase_soak(session, key, args.wraps)
        print("\n-- clock --")
        phase_clock(session)
        print("\n-- config write --")
        phase_config_write(session)
        print("\n-- reattach --")
        session.close()
        session = phase_reattach(args.device, args.reattach)

        if args.intense:
            print("\n-- [intense] config sweep (every channel, cross-path) --")
            phase_api_sweep(session, min(session.max_chans(), 160))
            print("\n-- [intense] invalid inputs --")
            phase_invalid_inputs(session)
            print("\n-- [intense] label boundaries --")
            phase_label_edges(session)
            print("\n-- [intense] write matrix --")
            phase_write_matrix(session)
            print("\n-- [intense] timestamps --")
            phase_timestamps(session)
            print("\n-- [intense] continuous reader --")
            phase_continuous_reader(session)
            print("\n-- [intense] spikes / events --")
            phase_spikes(session)
            print("\n-- [intense] CCF round-trip --")
            phase_ccf(session)
            print("\n-- [intense] misc + transmit path --")
            phase_misc(session, key)
            print("\n-- [intense] concurrent clients --")
            session.close()
            phase_concurrent_clients(args.device, 3)
            print("\n-- [intense] re-attach storm --")
            phase_reattach_storm(args.device, 25)
            print("\n-- [intense] overrun + recovery --")
            session = open_session(args.device)
            phase_overrun_recovery(session, key)

        if key == "7.0":
            print("\n-- instrument bounds --")
            phase_bounds()
    finally:
        if session is not None:
            session.close()
        if producer is not None:
            producer.terminate()

    print("\n" + "=" * 66)
    fails = [r for r in results if r[1] == "FAIL"]
    warns = [r for r in results if r[1] == "WARN"]
    for phase, status, detail in results:
        if status != "PASS":
            print(f"  {status}  {phase}: {detail}")
    print(
        f"\n{len(results) - len(fails) - len(warns)} passed, {len(warns)} warn, {len(fails)} FAILED"
    )

    print("\n--- confirm these against Central's GUI (the layout ground truth) ---")
    for line in manual:
        print(f"  {line}")
    if key in ("7.0", "7.5"):
        print(
            "  monsource: set a digout to 'monitor channel N', then to timed/frequency"
        )
        print(
            "             output, and confirm both in Central's GUI (not scriptable --"
        )
        print("             moninst/monchan are not exposed through pycbsdk)")

    print("\nRESULT:", "PASS" if not fails else "FAIL")
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main())
