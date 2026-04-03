"""
pycbsdk - Python bindings for CereLink SDK (Blackrock Neurotech Cerebus devices).

Usage::

    from pycbsdk import Session, DeviceType, ChannelType

    with Session(DeviceType.HUB1) as session:
        @session.on_event(ChannelType.FRONTEND)
        def on_spike(header, data):
            print(f"Spike on ch {header.chid}, t={header.time}")

        import time
        time.sleep(10)

        print(session.stats)
"""

from .session import (
    Session,
    DeviceType,
    ChannelType,
    SampleRate,
    ChanInfoField,
    ProtocolVersion,
    Stats,
    ContinuousReader,
)

try:
    from ._version import __version__
except ImportError:
    __version__ = "0.0.0"

__all__ = [
    "Session",
    "DeviceType",
    "ChannelType",
    "SampleRate",
    "ChanInfoField",
    "ProtocolVersion",
    "Stats",
    "ContinuousReader",
    "__version__",
]
