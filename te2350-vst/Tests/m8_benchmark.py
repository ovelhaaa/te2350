"""Paired warm CPU measurements and bit-identical deterministic render check."""
import ctypes as c
import csv
from pathlib import Path
import statistics
import sys
import time
import numpy as np

ptr=c.POINTER(c.c_float)
libraries=[c.CDLL(str(Path(p).resolve())) for p in sys.argv[1:3]]
for lib in libraries:
    lib.m8_render.argtypes=[ptr,ptr,ptr,c.c_int,c.c_float,c.c_int,c.c_float,c.c_float,c.c_float,c.c_float,c.c_float,c.c_int]
sr=48000;n=sr*10
x=np.zeros(n,np.float32);t=np.arange(sr)/sr
x[:sr]=.35*np.sin(2*np.pi*220*t)*np.exp(-t*3)
y=np.empty((n,2),np.float32);g=np.empty(n,np.float32)
rows=[]
for freeze,duck in [(False,0),(False,.9),(True,.9)]:
    timings=[[],[]]
    def run(which):
        begin=time.perf_counter()
        assert libraries[which].m8_render(x.ctypes.data_as(ptr),y.ctypes.data_as(ptr),g.ctypes.data_as(ptr),n,sr,2,.75,.9,.9,duck,.063096,sr if freeze else -1)
        return time.perf_counter()-begin
    for k in range(14):
        for which in ([0,1] if k%2==0 else [1,0]):
            elapsed=run(which)
            if k>=2:timings[which].append(elapsed)
    run(1);reference=y.copy();run(1)
    assert np.array_equal(reference,y),'Non-deterministic output'
    b,a=map(statistics.median,timings)
    rows.append(dict(freeze=freeze,duck=duck,baseline_seconds=b,new_seconds=a,change_percent=100*(a/b-1),new_realtime_percent=100*a/10,deterministic=True))
out=Path(sys.argv[3]);out.parent.mkdir(parents=True,exist_ok=True)
with out.open('w',newline='') as f:
    w=csv.DictWriter(f,fieldnames=rows[0]);w.writeheader();w.writerows(rows)
print(rows)
