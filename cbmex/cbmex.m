%CBMEX Cerebus real-time matlab interface for the NSP128E
%   This interface allows blocks of cerebus data from experimental
%   trials to be captured and loaded during real-time operation.
%
%   CBMEX('open') allocates real-time data cache memory for trials
%   and establishes a connection with the Central Application.
%
%   TIME = CBMEX('time') returns current Cerebus system time in sec.
%
%   [ACTcur,CFGcur] = CBMEX('trialconfig')     % cur = current state
%   [ACTnew,CFGcur] = CBMEX('trialconfig',ACT)
%   [ACTnew,CFGnew] = CBMEX('trialconfig',ACT,CFG) allow the active
%   state of the trial recording data cache to be set and allow NSP
%   input ports to be configured to start and stop trials directly.
%   ACT is 0 or 1 to indicate whether a trial is currently active.
%   CFG takes the form [ BEGCH BEGMSK BEGVAL ENDCH ENDMSK ENDVAL ]
%   For BEGx and ENDx, the channel CH is ANDed with MSK and compared
%   with value VAL to test for the beginning and end of trials.
%   For example, if the words of the digital input port (ch 151)
%   have their LSB set to 1 at the beginning and middle of a trial,
%   and a word with the LSB set to 0 is sent to mark the end of a
%   trial, you would use CFG = [ 151  1  1  151  1  0 ].
%
%   CF = CBMEX('chanconfig') gets the current configuration for the
%   first 152 system channels.  CF is a 2D cell array accessed with
%   each row describing aspects of a specific channel. For neural
%   channels CF{chid,:} = { 'label' CHEN U1EN U2EN U3EN U4EN U5EN }
%   where CHEN and UxEN are 0 or 1 if the channel and unit x are
%   enabled.  Analog outputs are defined by label only.  For digital
%   input channels, CF{chid,:} = { 'label' chen [] [] [] [] [] }
%
%   TD = CBMEX('trialdata') retrieves the current trial data from
%   the real-time data cache for the first 152 channels.  TD is a
%   cell array where each row contains data for a specific channel.
%   For neural channels, CF{chid,:} = {'label' U0 U1 U2 U3 U4 U5 },
%   where Ux are timestamps for unsorted (U0) and sorted (U1-U5)
%   events.  For Digital inputs, CF{chid,:} = {'label' TIM VAL .. }
%   where TIM is the timestamps and VAL are the digital values.
%   Timestamps are given in seconds from the trial beginning.
%
%   CBMEX('start')    - start streaming data to disk
%   CBMEX('stop')     - stop saving data to disk
%
%   CBMEX('close') closes the Central Application connection and
%   releases the trial real-time cache data memory.
%
%   $ Revision 0.9a  - written by Shane Guillory (Nov-2002)
%   $ Copyright 2002 - Bionic Technologies / Cyberkinetics, Inc.
%
