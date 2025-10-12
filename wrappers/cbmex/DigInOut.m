% Author & Date:    Hyrum L. Sessions   14 Sept 2009
% Copyright:        Blackrock Microsystems
% Workfile:         DigInOut.m
% Purpose:          Read serial data from the NSP and compare with a
%                   predefined value.  If it is the same, generate a
%                   pulse on dout4

% This script will read data from the NSP for a period of 30 seconds.  It
% is waiting for a character 'd' on the Serial I/O port of the NSP.  If
% received it will generate a 10ms pulse on Digital Output 4

% initialize
close all;
clear variables;

run_time = 30;      % run for time
value = 100;        % value to look for (100 = d)
channel_in = 152;   % serial port = channel 152, digital = 151
channel_out = 156;  % dout 1 = 153, 2 = 154, 3 = 155, 4 = 156

t_col = tic;        % collection time

cbmex('open');              % open library
cbmex('trialconfig',1);     % start library collecting data

start = tic();

while (run_time > toc(t_col))
    pause(0.05);    % check every 50ms
    t_test = toc(t_col);
    spike_data = cbmex('trialdata', 1);  % read data
    found = (value == spike_data{channel_in, 3});
    if (0 ~= sum(found))
        cbmex('digitalout', channel_out, 1);
        pause(0.01);
        cbmex('digitalout', channel_out, 0);
    end
end

% close the app
cbmex('close');
