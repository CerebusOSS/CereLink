# CereLink

Software development kit for Blackrock Neurotech neural signal processing hardware (Cerebus, CerePlex, NSP, Gemini).

## Components

- **cbsdk** — C/C++ library for two-way communication with hardware
- **pycbsdk** — Python wrapper (cffi, no compiler needed at install time)

## Architecture

Modular library stack:

| Module | Purpose |
|--------|---------|
| `cbproto` | Protocol definitions, packet types, version translation |
| `cbshm` | Shared memory (Central-compat and native layouts) |
| `cbdev` | Device transport (UDP sockets, handshake, clock sync) |
| `cbsdk` | SDK orchestration (device + shmem + callbacks + config) |
| `ccfutils` | CCF XML config file load/save |
| `pycbsdk` | Python package via cffi ABI mode |

## Connection Modes

By default (`ConnectionMode::NATIVE`) a session talks to the device directly:

- **STANDALONE** — the first session for a device owns its connection and shared memory
- **CLIENT** — later sessions for the same device attach to that shared memory

This works the same whether or not Central is running on the same PC. On Windows the
session shares the device's UDP port with Central and both receive the full data stream,
in either start order; config changes made from either side are seen by the other, and
recording commands (`startCentralRecording`) still reach Central.

- **CENTRAL CLIENT** (*deprecated*, `ConnectionMode::CENTRAL`) — attach to Central's shared
  memory instead, with on-the-fly protocol translation. Opt-in only; slated for removal in
  the next major release.

## Build

See [BUILD.md](./BUILD.md) for build instructions.

## Python

```
pip install pycbsdk
```

Or build from source — see [pycbsdk/](./pycbsdk/).

## Testing with nPlayServer

On Windows, download and install the latest version of Cerebus Central Suite from the [Blackrock Neurotech support website (scroll down)](https://blackrockneurotech.com/support/). You may also wish to download some sample data from this same website.

### Testing on localhost

Navigate to `C:\Program Files\Blackrock Microsystems\Cerebus Central Suite\` and run `runNPlayAndCentral.bat`. This runs a device simulator (nPlayServer) and Central on localhost loopback.

### Testing on network

Configure IP addresses: client at 192.168.137.198 or .199, device at 192.168.137.128 (NSP) or 192.168.137.200 (Gemini Hub).

Run `nPlayServer --help` for options:
* Legacy NSP: `nPlayServer -L --network inst=192.168.137.128:51001 --network bcast=192.168.137.255:51002`
* Gemini Hub: `nPlayServer -L --network inst=192.168.137.200:51002 --network bcast=192.168.137.255:51002`

### Linux Network

**Firewall:**
```
sudo ufw allow in on enp6s0 from 192.168.137.0/24 to any port 51001:51005 proto udp
```
Replace `enp6s0` with your ethernet adapter.

**Socket Buffer Size:**
```
echo "net.core.rmem_max=16777216" | sudo tee -a /etc/sysctl.d/99-cerebus.conf
echo "net.core.rmem_default=8388608" | sudo tee -a /etc/sysctl.d/99-cerebus.conf
```
Then reboot.

## Development

Install pre-commit hooks (runs ruff linter/formatter on `pycbsdk/src/`):

```
pre-commit install
```

## Getting Help

1. Read the [project wiki](https://github.com/CerebusOSS/CereLink/wiki)
2. Search [issues on GitHub](https://github.com/CerebusOSS/CereLink/issues)
3. Open an issue

This is a community project and is not officially supported by Blackrock Neurotech.
