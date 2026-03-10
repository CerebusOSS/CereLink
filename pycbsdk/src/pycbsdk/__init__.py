"""
pycbsdk - Python bindings for CereLink SDK (Blackrock Neurotech Cerebus devices).

Usage::

    from pycbsdk import Session

    with Session("HUB1") as session:
        @session.on_event("FRONTEND")
        def on_spike(header, data):
            print(f"Spike on ch {header.chid}, t={header.time}")

        import time
        time.sleep(10)

        print(session.stats)
"""

from .session import Session, Stats, ContinuousReader

__all__ = ["Session", "Stats", "ContinuousReader"]
