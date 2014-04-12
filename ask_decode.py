from __future__ import print_function
import scipy.signal
import numpy as np
import matplotlib.pyplot as plt

Fs = 240000.0
bd = 300.0
bufsize = 131072
bufno = 3

data = np.fromfile("rtlsdr_out.bin", np.uint8).reshape((-1, 2)).astype(float)
data = data[bufsize*bufno:bufsize*(bufno+2)]
data -= 127.0
data -= data.mean(axis=0)
data = data[:, 0] + 1j*data[:, 1]

ft = np.fft.fft(data)
freqs = np.fft.fftfreq(ft.shape[0], 1/Fs)
mags = np.abs(ft)
#plt.plot(freqs, mags)
#plt.plot(freqs, np.angle(ft))
#plt.show()

h = scipy.signal.firwin(numtaps=16, cutoff=400.0, nyq=240000/2.0)
y = scipy.signal.lfilter(h, 1.0, np.abs(data))
y = y[16:]
th = (np.max(y) - np.min(y)) / 3
th = np.mean(y) / 2
#idx = (bd * bufsize) / Fs
#print("idx", idx)
#ph = np.angle(ft[idx])
#print("ph", ph)
#offset = (ph / (2*np.pi)) * (Fs / bd)
#print("offset", offset)
##offset = -offset
#offset %= Fs / bd
#print("offset", offset)
#y = y[offset:]

z = np.zeros(y.shape, dtype=np.uint8)
z[y > th] = 1

print(z)
first_zero = np.nonzero(z==0)[0][0]
print(first_zero)
z = z[first_zero:]
y = y[first_zero:]

bits = np.array((1, 0)).repeat(Fs / bd)
bits = np.tile(bits, y.size // ((Fs/bd) * 2))
plt.fill_between(np.arange(bits.size), bits*50, alpha=0.5, color='g')
plt.fill_between(np.arange(z.size), z*100, alpha=0.5)
plt.plot(y)
plt.show()

sidxs = np.arange((Fs/bd)/2, z.size, (Fs/bd), dtype=np.int)
d = z[sidxs]

bytesout = []
bitsbuf = []
idx = 1
state = 'wait'
while True:
    print("Loop start, idx=", idx, "bit=", d[idx])
    if state == 'wait':
        print("Was waiting...")
        if d[idx - 1] == 1 and d[idx] == 0:
            print("Saw start bit")
            state = 'byte'
    elif state == 'byte':
        print("Reading byte...")
        bitsbuf.append(d[idx])
        if len(bitsbuf) == 8:
            print("Got a byte, ending")
            state = 'ending'
    elif state == 'ending':
        print("Was ending")
        if d[idx] == 1:
            print("Saw end bit")
            newbyte = 0
            for i in range(8):
                newbyte |= bitsbuf[i] << i
            print("Made byte", newbyte)
            bytesout.append(newbyte)
            bitsbuf = []
            state = 'wait'
    idx += 1
    if idx == d.size - 1:
        break

dataout = [chr(x) for x in bytesout]
print(dataout)
