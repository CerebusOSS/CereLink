"""
Shared memory buffer names used in CereLink.

These names match those defined by Central
"""


class BufferNames:
    """Named shared memory buffers created by CereLink."""

    # Buffer base names (instance number appended for multi-instance)
    REC_BUFFER = "cbRECbuffer"      # Receive buffer for incoming packets
    CFG_BUFFER = "cbCFGbuffer"      # Configuration buffer
    XMT_GLOBAL = "XmtGlobal"        # Global transmit buffer
    XMT_LOCAL = "XmtLocal"          # Local transmit buffer
    STATUS = "cbSTATUSbuffer"       # PC status buffer
    SPK_BUFFER = "cbSPKbuffer"      # Spike cache buffer
    SIG_EVENT = "cbSIGNALevent"     # Signal event/semaphore

    @staticmethod
    def get_buffer_name(base_name: str, instance: int = 0) -> str:
        """
        Get the full buffer name for a given instance.

        Parameters
        ----------
        base_name : str
            Base buffer name (e.g., BufferNames.REC_BUFFER)
        instance : int
            Instance number (0 for default, >0 for multi-instance)

        Returns
        -------
        str
            Full buffer name (e.g., "cbRECbuffer" or "cbRECbuffer1")
        """
        if instance == 0:
            return base_name
        else:
            return f"{base_name}{instance}"
