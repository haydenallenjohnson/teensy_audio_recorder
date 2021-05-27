bits = 16;
fs = 44100;
gain = 0.207;

fid = fopen('REC00001.RAW','r');
samples = fread(fid, inf, 'int16', 'ieee-le');
fclose(fid);

vADC = samples/2^(bits-1);
vSignal = vADC/gain;

t = (1:length(samples))./fs;

figure(1);
plot(t,samples);