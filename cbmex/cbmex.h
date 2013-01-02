/* =STS=> cbmex.h[4257].aa10   open     SMID:10 */
//////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2010 - 2011 Blackrock Microsystems
//
// $Workfile: cbmex.h $
// $Archive: /Cerebus/Human/WindowsApps/cbmex/cbmex.h $
// $Revision: 1 $
// $Date: 2/17/11 3:15p $
// $Author: Ehsan $
//
// $NoKeywords: $
//
//////////////////////////////////////////////////////////////////////////////
//
// PURPOSE:
//
// MATLAB executable (MEX) interface for cbsdk
//

#ifndef CBMEX_H_INCLUDED
#define CBMEX_H_INCLUDED

#include "mex.h"

///////////////////////////// Prototypes for all of the Matlab events ////////
void OnHelp          (int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] );
void OnOpen          (int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] );
void OnClose         (int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] );
void OnTime          (int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] );
void OnTrialConfig   (int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] );
void OnChanLabel     (int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] );
void OnTrialData     (int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] );
void OnTrialComment  (int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] );
void OnTrialTracking (int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] );
void OnFileConfig    (int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] );
void OnDigitalOut    (int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] );
void OnAnalogOut     (int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] );
void OnMask          (int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] );
void OnComment       (int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] );
void OnConfig        (int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] );
void OnCCF           (int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] );
void OnSystem        (int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] );
////////////////////// end of Prototypes for all of the Matlab events ////////

#define CBMEX_USAGE_CBMEX \
        "cbmex is the live MATLAB interface for Cerebus system\n" \
        "Format: cbmex(<command>, [arg1], ...)\n" \
        "Inputs:\n" \
        "<command> is a string and can be any of:\n" \
        "'help', 'open', 'close', 'time', 'trialconfig', 'chanlabel',\n" \
        "'trialdata', 'fileconfig', 'digitalout', 'mask', 'comment', 'config',\n" \
        "'analogout', 'trialcomment', 'trialtracking', 'ccf', 'system'\n" \
        "Use cbmex('help', <command>) for each command usage\n" \

#define CBMEX_USAGE_HELP \
        "Prints usage information about cbmex functions\n" \
        "Format: cbmex('help', [command])\n" \
        "Inputs:\n" \
        "command: is the command name\n" \
        "Use cbmex('help') to get the list of commands\n" \

#define CBMEX_USAGE_OPEN \
        "Opens the live MATLAB interface for Cerebus system\n" \
        "Format: [connection instrument] = cbmex('open', [interface = 0], [<parameter>[, value]])\n" \
        "Inputs:\n" \
        " interface (optional): 0 (Default), 1 (Central), 2 (UDP)\n" \
        "<parameter>[, value] pairs are optional, some parameters do not have values.\n" \
        " from left to right parameters will override previous ones or combine with them if possible.\n" \
        "<parameter>[, value] can be any of:\n" \
        "'instance', value: value is the library instance to open (default is 0)\n" \
        "'inst-addr', value: value is string containing instrument ipv4 address\n" \
        "'inst-port', value: value is the instrument port number\n" \
        "'central-addr', value: value is string containing central ipv4 address\n" \
        "'central-port', value: value is the central port number\n" \
        "\n" \
        "Outputs:\n" \
        " connection (optional): 1 (Central), 2 (UDP)\n" \
        " instrument (optional): 0 (NSP), 1 (Local nPlay), 2 (Local NSP), 3 (Remote nPlay)\n" \

#define CBMEX_USAGE_CLOSE \
        "Closes the live MATLAB interface for Cerebus system\n" \
        "Format: cbmex('close', [<parameter>[, value]])\n" \
        "Inputs:\n" \
        "<parameter>[, value] pairs are optional, some parameters do not have values.\n" \
        " from left to right parameters will override previous ones or combine with them if possible.\n" \
        "<parameter>[, value] can be any of:\n" \
        "'instance', value: value is the library instance to close (default is 0)\n" \

#define CBMEX_USAGE_TIME \
        "Get the latest Cerebus time past last reset\n" \
        "Format: cbmex('time', [<parameter>[, value]])\n" \
        "Inputs:\n" \
        "<parameter>[, value] pairs are optional, some parameters do not have values.\n" \
        " from left to right parameters will override previous ones or combine with them if possible.\n" \
        "<parameter>[, value] can be any of:\n" \
        "'instance', value: value is the library instance to use (default is 0)\n" \
        "'samples': if specified sample number is returned otherwise seconds\n" \

#define CBMEX_USAGE_CHANLABEL \
        "Get current channel labbels and optionally set new labels\n" \
        "Format: [label_cell_array] = cbmex('chanlabel', [channels_vector], [new_label_cell_array], [<parameter>[, value]])\n" \
        "Inputs:\n" \
        "channels_vector (optional): a vector of all the channel numbers to change their label\n" \
        " if not specified all the 156 channels are considered\n" \
        "new_label_cell_array (optional): cell array of new labels (each cell a string of maximum 16 characters)\n" \
        " number of labels must match number of channels\n" \
        "<parameter>[, value] pairs are optional, some parameters do not have values.\n" \
        " from left to right parameters will override previous ones or combine with them if possible.\n" \
        "<parameter>[, value] can be any of:\n" \
        "'instance', value: value is the library instance to use (default is 0)\n" \
        "\n" \
        "Outputs:\n" \
        " label_cell_array (optional): If specified previous labels are returned\n" \
        "Each row in label_cell_array looks like:\n" \
        "  For spike channels:\n" \
        "  'channel label' [spike_enabled] [unit0_valid] [unit1_valid] [unit2_valid] [unit3_valid] [unit4_valid]\n" \
        "  For digital input channels:\n" \
        "  'channel label' [digin_enabled]  ...remaining columns are empty...\n" \

#define CBMEX_USAGE_TRIALCONFIG \
        "Configures a trial to grab data with 'trialdata'\n" \
        "Format: [ active_state, [config_vector_out] ]\n" \
        "  = cbmex('trialconfig', active, [config_vector_in], [<parameter>[, value]])\n" \
        "Inputs:\n" \
        "active: set 1 to flush data cache and start collecting data immediately,\n" \
        "        set 0 to stop collecting data immediately\n" \
        "config_vector_in (optional):\n" \
        " vector [begchan begmask begval endchan endmask endval] specifying start and stop channels, default is zero all\n" \
        "<parameter>[, value] pairs are optional, some parameters do not have values.\n" \
        " from left to right parameters will override previous ones or combine with them if possible.\n" \
        "<parameter>[, value] can be any of:\n" \
        "'double': if specified, the data is in double precision format (old behaviour)\n" \
        "'absolute': if specified event timing is absolute (active will not reset time for events)\n" \
        "'nocontinuous': if specified, continuous data cache is not created nor configured (same as 'continuous',0)\n" \
        "'noevent': if specified, event data cache is not created nor configured (same as 'event',0)\n" \
        "'waveform', value: set the number of waveforms to be cached (internal cache if less than 400)\n" \
        "'continuous', value: set the number of continuous data to be cached\n" \
        "'event', value: set the number of evnets to be cached\n" \
        "'comment', value: set number of comments to be cached\n" \
        "'tracking', value: set the number of video tracking evnets to be cached" \
        "'instance', value: value is the library instance to use (default is 0)\n" \
        "\n" \
        "Outputs:\n" \
        "active_state: return 1 if data collection is active, 0 otherwise\n" \
        "config_vector_out:\n" \
        " vector [begchan begmask begval endchan endmask endval double waveform continuous event comment tracking]\n" \
        "  specifying the configuration state\n" \

#define CBMEX_USAGE_TRIALDATA \
        "Grab continuous and even data configured by 'trialconfig'\n" \
        "Format:\n" \
        " [timestamps_cell_array[, time, continuous_cell_array]] = cbmex('trialdata',  [active = 0], [<parameter>[, value]])\n" \
        " [time, continuous_cell_array] = cbmex('trialdata',  [active = 0], [<parameter>[, value]])\n" \
        "Note: above format means:\n" \
        " if one output requested it will be timestamps_cell_array\n" \
        " if two outputs requested it will be time and continuous_cell_array\n" \
        "Inputs:\n" \
        "active (optional):\n" \
        "    set 0 (default) to leave buffer intact\n" \
        "    set 1 to clear all the data and reset the trial time to the current time\n" \
        "<parameter>[, value] pairs are optional, some parameters do not have values.\n" \
        " from left to right parameters will override previous ones or combine with them if possible.\n" \
        "<parameter>[, value] can be any of:\n" \
        "'instance', value: value is the library instance to use (default is 0)\n" \
        "\n" \
        "Outputs:\n" \
        "timestamps_cell_array: Timestamps for events of 152 channels (152 rows). Each row in this matrix looks like:\n" \
        "  For spike channels:\n" \
        "  'channel name' [unclassified timestamps_vector] [u1_timestamps_vector]  [u2_timestamps_vector] [u3_timestamps_vector] [u4_timestamps_vector] [u5_timestamps_vector]\n" \
        "  For digital input channels:\n" \
        "  'channel name' [timestamps_vector] [values_vector] ...remaining columns are empty...\n" \
        "time: Time (in seconds) that the data buffer was most recently cleared.\n" \
        "continuous_cell_array: Continuous sample data, variable number of  rows. Each row in this matrix looks like:\n" \
        " [channel number] [sample rate (in samples / s)] [values_vector]\n" \

#define CBMEX_USAGE_TRIALCOMMENT \
        "Grab comments configured by 'trialconfig'\n" \
        "Format:\n" \
        " [comments_cell_array, [timestamps_vector], [rgba_vector], [charset_vector]] \n" \
        "     = cbmex('trialcomment',  [active = 0], [<parameter>[, value]])\n" \
        "Inputs:\n" \
        "active (optional):\n" \
        "    set 0 (default) to leave buffer intact\n" \
        "    set 1 to clear all the data and reset the trial time to the current time\n" \
        "<parameter>[, value] pairs are optional, some parameters do not have values.\n" \
        " from left to right parameters will override previous ones or combine with them if possible.\n" \
        "<parameter>[, value] can be any of:\n" \
        "'instance', value: value is the library instance to use (default is 0)\n" \
        "\n" \
        "Outputs:\n" \
        "comments_cell_array: cell-array of comments (strings of possibly different sizes)\n" \
        "timestamps_vector (optional): timastamps of the comments\n" \
        "rgba_vector (optional): comments colors\n" \
        "charset_vector (optional): characterset vector for comments\n" \


#define CBMEX_USAGE_TRIALTRACKING \
        "Grab tracking data configured by 'trialconfig'\n" \
        "Format:\n" \
        " [tracking_cell_array] = cbmex('trialtracking',  [active = 0], [<parameter>[, value]])\n" \
        "Inputs:\n" \
        "active (optional):\n" \
        "    set 0 (default) to leave buffer intact\n" \
        "    set 1 to clear all the data and reset the trial time to the current time\n" \
        "<parameter>[, value] pairs are optional, some parameters do not have values.\n" \
        " from left to right parameters will override previous ones or combine with them if possible.\n" \
        "<parameter>[, value] can be any of:\n" \
        "'instance', value: value is the library instance to use (default is 0)\n" \
        "\n" \
        "Outputs:\n" \
        "tracking_cell_array: Each row in this matrix looks like:\n" \
        "  'trackable_name' desc_vector timestamps_vector synch_timestamps_vector synch_frame_numbers_vector rb_cell_array\n" \
        "   Here is the description of output:\n" \
        "   desc_vector: column vector [type id max_point_count]\n" \
        "     type: 1 (2DMARKERS), 2 (2DBLOB), 3 (3DMARKERS), 4 (2DBOUNDARY)\n" \
        "     id: node unique ID\n" \
        "     max_point_count: maximum number of points for this trackable\n" \
        "   timestamps_vector: the timestamps of the tracking packets\n" \
        "   synch_timestamps_vector: synchronized timestamps of the tracking (in milliseconds)\n" \
        "   synch_frame_numbers_vector: synchronized frame numbers of tracking\n" \
        "   rb_cell_array: each cell is a matrix of rigid-body, the rows are points, columns are coordinates\n"\


#define CBMEX_USAGE_FILECONFIG \
        "Configures file recording\n" \
        "Format: cbmex('fileconfig', filename, comments, action, [<parameter>[, value]])\n" \
        "Inputs:\n" \
        "filename: file name string (255 character maximum)\n" \
        "comments: file comment string (255 character maximum)\n" \
        "action: set 1 to start recording, 0 to stop recording" \
        "<parameter>[, value] pairs are optional, some parameters do not have values.\n" \
        " from left to right parameters will override previous ones or combine with them if possible.\n" \
        "<parameter>[, value] can be any of:\n" \
        "'instance', value: value is the library instance to use (default is 0)\n" \

#define CBMEX_USAGE_DIGITALOUT \
        "Set digital output properties for given channel\n" \
        "Format: cbmex('digitalout', channel, default_value, [<parameter>[, value]])\n" \
        "Inputs:\n" \
        "channel: 153 (dout1), 154 (dout2), 155 (dout3), 156 (dout4)\n" \
        "default_value: 1 sets dout default to ttl high and 0 sets dout default to ttl low" \
        "<parameter>[, value] pairs are optional, some parameters do not have values.\n" \
        " from left to right parameters will override previous ones or combine with them if possible.\n" \
        "<parameter>[, value] can be any of:\n" \
        "'instance', value: value is the library instance to use (default is 0)\n" \
        

#define CBMEX_USAGE_ANALOGOUT \
        "Set analog output properties for given channel\n" \
        "Format: cbmex('analogout', channel, [<parameter>[, value]])\n" \
        "Inputs:\n" \
        "channel: 145 (aout1), 146 (aout2), 147 (aout3), 148 (aout4)\n" \
        "         149 (audout1), 150 (audout2)\n" \
        "Each analog output can exclusivlely monitor a channel, generate custom waveform or be disabled\n" \
        "<parameter>, <value> pairs are optional, Some parameters do not have values.\n" \
        " from left to right parameters will override previous ones or combine with them if possible.\n" \
        "<parameter>[, value] can be any of:\n" \
        "'pulses', waveform: waveform is vector \n" \
        "        [nPhase1Duration nPhase1Amplitude nInterPhaseDelay nPhase2Duration nPhase2Amplitude nInterPulseDelay]\n" \
        "'sequence', waveform: waveform is variable length vector of duration and amplitude\n" \
        "        Waveform format is [nDuration1 nAmplitude1 nDuration2 nAmplitude2 ...]\n" \
        "        Waveform must be a nonempty even-numbered vector of double-precision numbers\n" \
        "        Each duration must be followed by amplitude for that duration\n" \
        "        Durations must be positive\n" \
        "        Each duration-amplitude pair is a phase in the waveform\n" \
        "'sinusoid', waveform: waveform is vector [nFrequency nAmplitude]\n" \
        "'monitor', type: type can be any of 'spike', 'continuous'\n" \
        "           'spike' means spikes on 'input' channel are monitored\n" \
        "           'continuous' means continuous 'input' channel is monitored\n" \
        "'track': monitor the last tracked channel\n" \
        "'disable': disable analog output\n" \
        "'offset', offset: amplitude offset\n" \
        "'repeats', repeats: number of repeats. 0 (default) means non-stop\n" \
        "'index', index: trigger index (0 to 4) is the per-channel trigger index (default is 0)\n" \
        "'trigger', trigger: trigger can be any of 'instant' (default), 'off', 'dinrise', 'dinfall', 'spike', 'cmtcolor', 'softreset'\n" \
        "                    'off' means this trigger is not used\n" \
        "                    'instant' means immediate trigger (immediate analog output waveform)\n" \
        "                    'dinrise' is for digital input rising edge, 'dinfall' is for digital input falling edge\n" \
        "                    'spike' is the spike event on given input channel\n" \
        "                    'cmtcolor' is the trigger based on colored comment\n" \
        "                    'softreset' is the trigger based on software reset (e.g. result of file recording)\n" \
        "'input', input: input depends on 'trigger' or 'monitor'\n" \
        "                 If trigger is 'dinrise' or 'dinfall' then 'input' is bit number of 1 to 16 for first digital input\n" \
        "                 If trigger is 'spike' then 'input' is input channel with spike data\n" \
        "                 If trigger is 'cmtcolor' then 'input' is the high word (two bytes) of the comment color\n" \
        "                 If monitor is 'spike' then 'input' is input channel with spike processing\n" \
        "                 If monitor is 'continuous' then 'input' is input channel with continuous data\n" \
        "'value', value: trigger value depends on 'trigger'\n" \
        "          If trigger is 'cmtcolor' then 'value' is the low word (two bytes) of the comment color\n" \
        "          If trigger is 'spike' then 'value' is spike unit number\n" \
        "             (0 for unclassified, 1-5 for first to fifth unit and 254 for any unit)\n" \
        "'mv': if specified, voltages are considered in milli volts instead of raw integer value\n" \
        "'ms': if specified, intervals are considered in milli seconds instead of samples\n" \
        "'instance', value: value is the library instance to use (default is 0)\n" \


#define CBMEX_USAGE_MASK \
        "Masks or unmasks given channels from trials\n" \
        "Format: cbmex('mask', channel, [active = 1], [<parameter>[, value]])\n" \
        "Inputs:\n" \
        "channel: The channel number to mask\n" \
        "  channel 0 means all of the channels\n" \
        "active (optional): set 1 (default) to activate, 0 to deactivate (mask out)\n" \
        "<parameter>[, value] pairs are optional, some parameters do not have values.\n" \
        " from left to right parameters will override previous ones or combine with them if possible.\n" \
        "<parameter>[, value] can be any of:\n" \
        "'instance', value: value is the library instance to use (default is 0)\n" \


#define CBMEX_USAGE_COMMENT \
        "Send comment or user event\n" \
        "Format: cbmex('comment', rgba, charset, comment, [<parameter>[, value]])\n" \
        "Inputs:\n" \
        "rgba: color coding or custom event number\n" \
        "charset: character-set 0 for ASCII or 1 for UTF16 or any user-defined\n" \
        "comment: comment string (maximum 127 characters)\n" \
        "<parameter>[, value] pairs are optional, some parameters do not have values.\n" \
        " from left to right parameters will override previous ones or combine with them if possible.\n" \
        "<parameter>[, value] can be any of:\n" \
        "'instance', value: value is the library instance to use (default is 0)\n" \


#define CBMEX_USAGE_CONFIG \
        "Get channel config and optionally change some channel parameters\n" \
        "Format: [config_cell_aray] = cbmex('config', channel, [<parameter>[, value]])\n" \
        "Note: \n" \
        " if new configuration is given and config_cell_aray is not present, nothing will be returned\n" \
        "Inputs:\n" \
        "channel: The channel number\n" \
        "<parameter>[, value] pairs are optional, some parameters do not have values.\n" \
        "<parameter>[, value] can be any of:\n" \
        "'instance', value: value is the library instance to use (default is 0)\n" \
        "'userflags', value: a user-defind value for the channel\n" \
        "'doutopts', value: bit-field for digital output options\n" \
        "'dinpopts', value: bit-field for digital input options\n" \
        "'aoutopts', value: bit-field for analog output options\n" \
        "'ainpopts', value: bit-field for analog input options\n" \
        "'smpfilter', value: continuous filter number\n" \
        "'smpgroup', value: continuous sampling group\n" \
        "'spkfilter', value: spike filter number\n" \
        "'spkopts', value: bit-field for spike options\n" \
        "'spkthrlevel', value: spike threshold level\n" \
        "'spkgroup', value: NTrode group number\n" \
        "'amplrejpos', value: amplitude rejection positive range\n" \
        "'amplrejneg', value: amplitude rejection negative range\n" \
        "'refelecchan', value: reference electrode number\n" \
        "\n" \
        "Outputs:\n" \
        "config_cell_array (optional): if specified previous parameters are returned\n" \
        "Each row in this matrix looks like:" \
        " <parameter>, <value>" \
        " <parameter> can be anything that is also specified in the Inputs section (plus the additional parameters below)\n" \
        " <value> will be the configuration value for the channel\n" \
        " additional <parameter>\n" \
        "'analogUnit': unit of analog value\n" \
        "'maxAnalog': maximum analog value\n" \
        "'maxDigital': maximum digital value\n" \
        "'minAnalog': minimum analog value\n" \
        "'minDigital': minimum digital value\n" \

#define CBMEX_USAGE_CCF \
        "Read, write and send CCF configuration file\n" \
        "Format: cbmex('ccf', filename, [<parameter>[, value]])\n" \
        "Inputs:\n" \
        "filename: CCF filename to read\n" \
        "         Read from NSP if it is zero length string (i.e. '')\n" \
        "<parameter>[, value] pairs are optional, some parameters do not have values.\n" \
        " from left to right parameters will override previous ones or combine with them if possible.\n" \
        "<parameter>[, value] can be any of:\n" \
        "'send': read and send CCF file to NSP (default if no other parameter is specified)\n" \
        "'convert', newfilename: write what is read to new CCF file (in latest format)\n" \
        "'instance', value: value is the library instance to use (default is 0)\n" \
    
#define CBMEX_USAGE_SYSTEM \
        "Perform given cbmex system command\n" \
        "Format: cbmex('system', <command>, [<parameter>[, value]])\n" \
        "Inputs:\n" \
        "<command> can be any of the following\n" \
        "  'reset' : resets instrument\n" \
        "  'shutdown' : shuts down instrument\n" \
        "  'standby' : sends instrument to standby mode" \
        "<parameter>[, value] pairs are optional, some parameters do not have values.\n" \
        " from left to right parameters will override previous ones or combine with them if possible.\n" \
        "<parameter>[, value] can be any of:\n" \
        "'instance', value: value is the library instance to use (default is 0)\n" \

#endif /* CBMEX_H_INCLUDED */
