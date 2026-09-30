"""Deterministic offline core matrix: python m8_measure.py DLL OUTPUT_DIR.

Build M8Render.c with the seven core C files, -shared -O3 and
-DTE_MAIN_DELAY_SIZE=524288; add -DM8_ADAPTIVE for the conditioned core.
NumPy is only needed by this offline analysis, never by the plugin.
"""
import ctypes as c
import csv
import itertools
from pathlib import Path
import sys
import time
import wave
import numpy as np

lib = c.CDLL(str(Path(sys.argv[1]).resolve()))
out = Path(sys.argv[2]); out.mkdir(parents=True, exist_ok=True)
ptr = c.POINTER(c.c_float)
lib.m8_render.argtypes = [ptr, ptr, ptr, c.c_int, c.c_float, c.c_int,
                         c.c_float, c.c_float, c.c_float, c.c_float, c.c_float, c.c_int]

def source(kind, sr, seconds=20):
    x = np.zeros(int(sr*seconds), np.float32)
    t = np.arange(sr)/sr
    rng = np.random.default_rng(2350)
    if kind == 'sine': x[:sr] = .4*np.sin(2*np.pi*440*t)
    elif kind == 'impulse': x[0] = .75
    elif kind == 'noise': x[:sr//10] = .4*rng.uniform(-1,1,sr//10)
    elif kind == 'percussion':
        x[:sr] = .45*np.exp(-t*18)*(np.sin(2*np.pi*(75*t+60*t*t))+.4*rng.uniform(-1,1,sr))
    elif kind == 'harmonic':
        x[:sr] = .35*np.exp(-t*3)*sum(np.sin(2*np.pi*220*k*t)/k for k in range(1,9))/2
    elif kind == 'isolated': x[sr:sr+int(.002*sr)] = .7
    elif kind == 'sequence':
        for start in np.arange(1,3,.25): x[int(start*sr):int(start*sr)+int(.002*sr)] = .7
    elif kind == 'sustain': x[sr:3*sr] = .4*np.sin(2*np.pi*220*np.arange(2*sr)/sr)
    elif kind == 'attack_sustain':
        x[sr:3*sr] = (.2+.5*np.exp(-np.arange(2*sr)/sr*40))*np.sin(2*np.pi*220*np.arange(2*sr)/sr)
    return x

def render(x,sr,interval=2,amount=.75,regen=.9,bloom=.9,duck=0,threshold=0,freeze=-1):
    y = np.empty((len(x),2),np.float32); g = np.empty(len(x),np.float32)
    start = time.perf_counter()
    assert lib.m8_render(x.ctypes.data_as(ptr),y.ctypes.data_as(ptr),g.ctypes.data_as(ptr),len(x),sr,interval,amount,regen,bloom,duck,threshold,freeze)
    return y,g,time.perf_counter()-start

def metrics(y,sr):
    z = y.astype(np.float64)
    # Stereo power STFT, 4096 samples, Hann, 50% overlap; preserve side energy.
    frames = np.lib.stride_tricks.sliding_window_view(z,4096,axis=0)[::2048]
    frame_power = (abs(np.fft.rfft(frames*np.hanning(4096),axis=-1))**2).sum(axis=1)
    p = frame_power.sum(axis=0)
    f = np.fft.rfftfreq(4096,1/sr); total=p.sum()
    rms = [float(np.sqrt(np.mean(z[k*sr:(k+1)*sr]**2))) for k in range(len(y)//sr)]
    tail_power=frame_power[int(sr/2048):].sum(axis=0)
    return dict(peak=float(abs(z).max()),rms=float(np.sqrt(np.mean(z*z))),
                centroid=float((p*f).sum()/max(total,1e-30)),hf_ratio=float(p[f>8000].sum()/max(total,1e-30)),
                tail_hf_ratio=float(tail_power[f>8000].sum()/max(tail_power.sum(),1e-30)),
                lf_ratio=float(p[f<150].sum()/max(total,1e-30)),tail_rms=rms[-1],
                max_tail_rms=max(rms[2:]),tail_growth=rms[-1]/max(rms[2],1e-12),windows=';'.join(map(str,rms)))

def wav(name,y,sr):
    with wave.open(str(out/(name+'.wav')),'wb') as w:
        w.setnchannels(2); w.setsampwidth(2); w.setframerate(sr)
        w.writeframes((np.clip(y,-1,1)*32767).astype('<i2').tobytes())

rows=[]
for interval,amount,regen,kind in itertools.product(range(3),[.35,.75],[.30,.75,.90],['sine','impulse','noise','percussion','harmonic']):
    sr=48000; y,g,elapsed=render(source(kind,sr),sr,interval,amount,regen)
    rows.append(dict(mode='shimmer',sr=sr,interval=interval,amount=amount,regen=regen,source=kind,freeze=0,seconds=20,cpu_seconds=elapsed,**metrics(y,sr)))
    if interval==2 and amount==.75 and regen==.9: wav(kind,y,sr)
for sr,interval in itertools.product([44100,48000,96000],range(3)):
    y,g,elapsed=render(source('harmonic',sr,22),sr,interval,freeze=sr)
    rows.append(dict(mode='freeze',sr=sr,interval=interval,amount=.75,regen=.9,source='harmonic',freeze=1,seconds=22,cpu_seconds=elapsed,**metrics(y,sr)))
with (out/'shimmer.csv').open('w',newline='') as f:
    w=csv.DictWriter(f,fieldnames=rows[0]); w.writeheader(); w.writerows(rows)
rows=[]
for sr,db,kind in itertools.product([44100,48000,96000],[-48,-24,-12],['isolated','sequence','sustain','attack_sustain']):
    x=source(kind,sr,5); y,g,elapsed=render(x,sr,duck=.9,threshold=10**(db/20))
    base,_,_=render(x,sr,duck=0,threshold=10**(db/20))
    minimum=float(g.min()); depth=1-minimum; i=int(g.argmin());
    hits=np.flatnonzero(g[sr:i+1]<=1-depth*.9)
    event_end=sr+int(.002*sr) if kind=='isolated' else (int(2.75*sr)+int(.002*sr) if kind=='sequence' else 3*sr)
    recovery=np.flatnonzero(g[max(i,event_end):]>=1-depth*.1)
    rows.append(dict(sr=sr,threshold_db=db,source=kind,min_gain=minimum,
       attack90_ms=float(hits[0]*1000/sr) if len(hits) else -1,
       recovery90_ms=float(recovery[0]*1000/sr) if len(recovery) else -1,
       between_gain=float(g[int(1.24*sr)]),pumping=float(np.std(g[int(2.5*sr):3*sr])),
       tail_ratio=float(np.sqrt(np.mean(y[4*sr:]**2))/max(np.sqrt(np.mean(base[4*sr:]**2)),1e-12))))
    if sr==48000 and db==-24: wav('duck_'+kind,y,sr)
with (out/'duck.csv').open('w',newline='') as f:
    w=csv.DictWriter(f,fieldnames=rows[0]); w.writeheader(); w.writerows(rows)
print('Completed',len(rows),'duck cases and 99 shimmer/freeze cases:',out,flush=True)
