# Loop Tester

This tool comprises 2 executables. The first is an LSL stream source that generates a 30 kHz sawtooth signal in the first 2 channels and constant values in the remaining 128 channels, then streams it with specific identifiers. The second executable is a cbsdk client that connects to nPlayServer, receives the data, and checks that the data matches what is expected. It also checks that the timestamps are reasonable and that no packets are lost.

For this to work, you must have a special build of nPlayServer that includes LSL support (built with -DUSE_LSL=ON). Please contact Blackrock support to obtain this version of nPlayServer.

1. Run lsl_sawtooth (the LSL stream source).
2. Run nPlayServer with the appropriate command line arguments to enable LSL support. For example, to emulate a Gemini Hub:
   `nPlayServer --device=LSL <stream_name>`
3. Run cbsdk_sawtooth (the cbsdk client).
