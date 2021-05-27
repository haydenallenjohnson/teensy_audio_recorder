file = 'final_8vpp_1khz.RAW';

fid = fopen(file,'r');
samples = fread(fid, inf, 'int16', 'ieee-le');
fclose(fid);

bits = 16;
fs = 44100;

vSignal_amp = 4; % 3.875;


vADC = samples/2^(bits-1);
gain = sqrt(2)*std(vADC)/vSignal_amp
vSignal = vADC/gain;

figure(1);
plot((1:100)/fs,vADC(1:100));
xlabel('Time (s)');
ylabel('vSignal');

nfft = 2^12;
[pxx,f] = pwelch(vSignal,hanning(nfft),nfft/2,nfft,fs);

figure(2);
plot(f,10*log10(pxx));
set(gca,'xscale','log');
hold off;
xlabel('Frequency (Hz)');
ylabel('dB re 1 V^2/Hz');
grid on;
