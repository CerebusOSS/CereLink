try:
    from cerebus.__version__ import version as __version__
except ImportError:
    # Version file not generated yet (e.g., in development before first build)
    __version__ = "0.0.0.dev0"
