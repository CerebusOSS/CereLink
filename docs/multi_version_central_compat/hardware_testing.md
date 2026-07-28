# Hardware Test Plan

CI cannot cover the parts of the Central compatibility layer that matter most:

- It has **no real Central**. Version detection inspects `Central.exe`'s
  ProductVersion via the running-process list, so it is Windows-only and needs
  Central actually running.
- The adapter unit tests are **set/get round-trips**. They verify that
  `toLegacy` and `fromLegacy` are mutual inverses, which cannot detect a struct
  layout that is self-consistently wrong. Only a real Central's bytes can.
- It never **crosses a receive-ring wrap** against a real writer, and
  `cbRECBUFFLEN` — and therefore the wrap threshold — differs per version.
- It has no non-Gemini device, so the ticks-to-nanoseconds conversion is
  untested.

Everything below targets exactly those gaps.

## Harness

`pycbsdk/src/pycbsdk/cli/soak.py` runs every automatable check for one
version/hardware combination and prints a PASS/FAIL/WARN table plus a short
manual checklist of the values it read.

```bash
# Windows, Central 7.0.6 running and streaming (protocol 3.11)
python -m pycbsdk.cli.soak --device LEGACY_NSP --central 7.0 --intense

# Windows, Central 7.6 running and streaming (protocol 4.1)
python -m pycbsdk.cli.soak --device LEGACY_NSP --central 7.6 --intense

# macOS/Linux, Central closed: spawns its own STANDALONE producer and
# attaches to it as a NATIVE CLIENT
python -m pycbsdk.cli.soak --device LEGACY_NSP --native --intense
```

Exit code 0 means every automated check passed. Baseline is roughly 10 minutes;
`--intense` adds 10-15 more.

pycbsdk auto-detects the layout — CENTRAL CLIENT first, then NATIVE CLIENT, then
NATIVE STANDALONE — so no flag selects the mode. Two consequences:

- **Close Central to test STANDALONE**, or you will silently be testing
  CENTRAL CLIENT instead.
- `DeviceType.NPLAY` maps to instrument index -1 and can never attach to
  Central. In CENTRAL mode the device type only selects which instrument slot
  is read (`HUB1`->0, `HUB2`->1, `HUB3`->2, `NSP`->3, `LEGACY_NSP`->0),
  independent of what feeds Central, so Central's own file playback can drive
  the version sweep when hardware is scarce.

## Soak duration

Ring sizes come from Central's compiled constants, so they do **not** shrink
with a low-channel-count device — they set how long the soak must run. At
128 channels x 30 kHz the stream is about 8.2 MB/s:

| Setup | ring source | ring | one wrap |
|---|---|---|---|
| Central 7.0 | FE 256 x 32768 x 4 | 128 MiB | ~16 s |
| Central 7.5 / 7.6 | FE 512 x 32768 x 4 | 256 MiB | ~33 s |
| Central 7.7 | FE 512 x 65536 x 4 | 512 MiB | ~34 s |
| Central 7.8 | FE 768 x 65536 x 4 - 1 | 768 MiB | ~98 s |
| NATIVE | FE 256 x 65536 x 4 - 1 | 256 MiB | ~33 s |

The harness measures the real packet rate, then sizes the soak itself to reach
`--wraps` crossings (default 3), so these numbers are reference only.

## Matrix

A complete sweep is one run per distinct layout. Every Central version in the
table is a distinct struct layout — 7.6 and 7.7 share protocol 4.1 but differ in
`cbMAXPROCS`, `cbRECBUFFLEN` and the wire/central constant split, so neither
substitutes for the other.

| # | Central | Device | Mode | What only this covers |
|---|---|---|---|---|
| 1 | 7.8 | Gemini hub | CLIENT | current layout, protocol 4.2, 768 MiB ring, native-ns timestamps |
| 2 | 7.7 | any | CLIENT | protocol 4.1 with the 512 MiB ring + 7.7-only constants |
| 3 | 7.6 | LEGACY_NSP | CLIENT | protocol 4.1, 256 MiB ring, ticks->ns conversion |
| 4 | 7.5 | any | CLIENT | protocol 4.0 and the 16-byte (`reserved[2]`) header |
| 5 | 7.0 | any | CLIENT | protocol 3.11: 8-byte header, 32-bit PROCTIME, 128 MiB ring |
| 6 | 7.8 | two hubs | 2x CLIENT | per-instrument demux; each client sees only its own slot |
| 7 | closed | non-zero-index device (e.g. HUB2) | STANDALONE | NATIVE reserve-zone wrap, forced-index-0 behaviour |

For a legacy-only, non-Gemini set the reachable subset is **#3, #5 and #7**.
That covers both legacy protocol paths (3.11 and 4.1), both Central ring sizes
in play, the NATIVE path, and the non-Gemini timestamp conversion. It leaves
protocol 4.0 (Central 7.5), Central 7.7/7.8, per-instrument demux (7.0 is
single-instrument and one legacy NSP occupies slot 0 under 7.6), and Gemini
timestamp passthrough untested.

## What the harness checks

Baseline:

- **attach** — CLIENT mode, `protocol_version` matches the installed Central,
  channel count
- **config** — `sysfreq == 30000`, printable channel labels, filter info, 30 kHz
  group membership. Garbage here means that version's `central_types/v7_X.h`
  layout is wrong.
- **soak** — self-sized; asserts zero malformed packets (non-zero high byte in
  `type`, or out-of-range `chid`), wraps achieved, zero drops
- **clock** — offset and uncertainty available, `device_to_monotonic` within
  100 ms of `monotonic()`
- **config write** — label and spike threshold round-trip, then restore
- **reattach** — close/reopen 5x while the producer keeps streaming
- **bounds** (7.0 only) — `HUB2` must not attach to Central instrument 1 on a
  single-instrument build

`--intense` adds:

- **overrun + recovery** — sleeps inside a callback (callbacks run on the shmem
  receive thread) for longer than one ring fill, forcing the producer to lap the
  consumer. Requires that an overrun/desync error was reported, that the stream
  recovers, and that **no malformed packets** follow recovery. This is the only
  test that drives the fail-safe resync path against a real writer.
- **re-attach storm** — 25 rapid attach/detach cycles with varied dwell so
  attachment lands at different ring phases, probing the torn
  `(head_index, head_wrap)` adoption window
- **concurrent clients** — three simultaneous clients on one segment set, each
  with an independent tail
- **config sweep** — every getter for every channel, requiring
  `get_channel_<field>()`, `get_channel_field()` and `get_channel_config()` to
  agree. Three independent paths must match, so unlike a round-trip this cannot
  be fooled by a self-consistently wrong translation.
- **invalid inputs** — out-of-range channel, filter and group ids must raise or
  return `None`, never a plausible value
- **label boundaries** — 15/16/17/32-char and empty labels; read-back must be a
  prefix, must not exceed the 16-byte field, and must leave the neighbouring
  channel's label intact (overflow canary)
- **write matrix** — sample group, AC/DC coupling, spike extraction, sorting,
  lncrate, filters, autothreshold and a multi-attribute `configure_channel`,
  each with settle-polled read-back and a full restore
- **timestamps** — non-decreasing over ~2000 packets; median inter-packet delta
  must be 1 us - 10 ms (about 33 us at 30 kHz — a median near 1 means raw device
  ticks leaked instead of being converted); batch vs single agreement;
  `stream_id` monotonicity clamping
- **spikes/events**, **ContinuousReader**, **CCF save/load round-trip**, and
  **misc/transmit** (identity, runlevel, time advance, stats reset, sync,
  `send_comment`, digital output, AOUT monitor)

## Manual steps

Two things cannot be automated, both about ground truth:

1. **Cross-check against Central's GUI.** The harness prints every value it read
   under *"confirm these against Central's GUI"* — labels, `sysfreq`, filter,
   group size. A CereLink-only read-back is symmetric and cannot detect a
   self-consistently wrong layout, so this comparison is the real verification.
2. **monsource, on Central 7.0 and 7.5 only.** `moninst`/`monchan` are not
   exposed through pycbsdk, so the packed-`monsource` translation cannot be read
   back in Python. The harness performs the *write*
   (`set_analog_output_monitor`, AOUT 1 -> monitor channel 5) and asks you to
   confirm it in Central's GUI. Also set a digout to timed/frequency output to
   cover the other arm of the union, which is packed differently.

## Pass criteria

- `protocol_version` matches the installed Central (3.11 / 4.0 / 4.1 / 4.1 / 4.2)
- Zero malformed packets in every soak — one is a real bug
- Bytes through the ring at least 3x the ring size for that setup
- `packets_dropped == 0` while the reader keeps up, and no unexpected
  overrun/desync errors outside the deliberate overrun phase
- Labels, `sysfreq` and filters match Central's GUI
- Config writes land and read back, and originals are restored
- Session 6: each client's packet `instrument` field equals its own slot
- Session 7: a non-zero-index device actually delivers data

`WARN` is used where a capability is genuinely absent rather than broken —
numpy not installed, no spikes on a quiet input, or a setter unsupported in that
mode — so it does not fail the run. A `WARN` on a config write under
`--native` is worth investigating rather than assuming unsupported, since it
depends on the STANDALONE owner forwarding from the transmit buffer.
