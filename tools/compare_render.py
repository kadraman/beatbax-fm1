#!/usr/bin/env python3
"""Compare a host-rendered mono WAV against a BeatBax CLI stereo WAV.
Usage: compare_render.py <fm1.wav> <beatbax.wav>
Prints normalized correlation and RMS dB difference over the non-silent region.
"""
import wave, struct, math, sys

def read_mono(path):
    with wave.open(path, 'rb') as w:
        n = w.getnframes()
        ch = w.getnchannels()
        sw = w.getsampwidth()
        data = w.readframes(n)
    if sw == 2:
        fmt = '<%dh' % (n * ch)
        samples = struct.unpack(fmt, data)
    else:
        raise ValueError('bad sampwidth %d' % sw)
    if ch == 1:
        return list(samples)
    # downmix interleaved stereo -> mono (mean of L/R)
    mono = [(samples[i*2] + samples[i*2+1]) / 2.0 for i in range(n)]
    return mono

a = read_mono(sys.argv[1])
b = read_mono(sys.argv[2])
L = min(len(a), len(b))
a = a[:L]
b = b[:L]

# trim leading/trailing silence for a fair non-silent region comparison
def rms(x):
    return math.sqrt(sum(v*v for v in x)/len(x)) if x else 0

# find first/last non-silent in beatbax reference
thr = 200  # int16 amplitude threshold
start = 0
while start < L and abs(b[start]) < thr:
    start += 1
end = L
while end > start and abs(b[end-1]) < thr:
    end -= 1
if end - start < 1000:
    print('reference too silent')
    sys.exit(1)

seg_a = a[start:end]
seg_b = b[start:end]

# normalized Pearson correlation
ma = sum(seg_a)/len(seg_a)
mb = sum(seg_b)/len(seg_b)
num = sum((x-ma)*(y-mb) for x,y in zip(seg_a,seg_b))
da = math.sqrt(sum((x-ma)**2 for x in seg_a))
db = math.sqrt(sum((y-mb)**2 for y in seg_b))
corr = num/(da*db) if da>0 and db>0 else 0.0

# RMS dB difference
ra = rms(seg_a); rb = rms(seg_b)
db = 20*math.log10(rb/ra) if ra>0 and rb>0 else float('inf')

print('samples: fm1=%d beatbax=%d overlap=%d' % (len(a), len(b), L))
print('non-silent region: [%d, %d) len=%d' % (start, end, end-start))
print('normalized correlation = %.4f' % corr)
print('rms fm1=%.1f beatbax=%.1f  diff=%.2f dB' % (ra, rb, db))
