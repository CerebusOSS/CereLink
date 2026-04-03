"""numpy integration utilities for pycbsdk."""

import numpy as np

from ._lib import ffi

# Structured dtype matching cbPKT_HEADER (16 bytes, little-endian)
HEADER_DTYPE = np.dtype(
    [
        ("time", "<u8"),
        ("chid", "<u2"),
        ("type", "<u2"),
        ("dlen", "<u2"),
        ("instrument", "u1"),
        ("reserved", "u1"),
    ]
)

# Standard Cerebus group sample rates (Hz)
GROUP_RATES = {1: 500, 2: 1000, 3: 2000, 4: 10000, 5: 30000, 6: 30000}


def group_data_as_array(pkt_data, n_channels):
    """Wrap cbPKT_GROUP.data as a numpy int16 array (zero-copy).

    The returned array shares memory with the packet buffer and is only
    valid during the callback.  Copy with ``arr.copy()`` to retain it.

    Args:
        pkt_data: cffi pointer to ``pkt.data`` (``int16_t[]``).
        n_channels: Number of valid channels in this group.

    Returns:
        numpy.ndarray of shape ``(n_channels,)``, dtype ``int16``.
    """
    buf = ffi.buffer(pkt_data, n_channels * 2)
    return np.frombuffer(buf, dtype=np.int16, count=n_channels)
