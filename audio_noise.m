bits = 16;
fs = 44100;
gain = 0.2;
file = 'final_input_shorted.RAW';

fid = fopen(file,'r');
samples = fread(fid, inf, 'int16', 'ieee-le');
fclose(fid);

vADC = samples/2^(bits-1);
vSignal = vADC/gain;

nfft = 2^12;
[pxx,f] = pwelch(vSignal,hanning(nfft),nfft/2,nfft,fs);

figure(4);
plot(f,10*log10(pxx));
set(gca,'xscale','log');
xlabel('Frequency (Hz)');
ylabel('dB re 1 V^2/Hz');
grid on;

figure(5);
histogram(samples,'normalization','PDF');

figure(6);
plot(samples);

