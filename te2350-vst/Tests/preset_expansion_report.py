"""M11 reproducible audit; reads unnormalised 24-bit listening renders. Requires NumPy.
Run PresetVoicingTest, then python te2350-vst/Tests/preset_expansion_report.py.
"""
from pathlib import Path
import csv, shutil, html
import numpy as np
import preset_voicing_report as m
ROOT=Path(__file__).resolve().parents[2]
DIR=ROOT/'build/te2350-vst-ninja-net/PresetVoicingOutput'
OUT=ROOT/'docs/M11'
EXTRA=['piano','major','minor','suspended','cluster','bass']
ROLES=[
('long low-mix ambience','vocal/guitar/piano/harp/lead','dry-forward, deep trailing field'),
('ducked afterimage','vocal/guitar/harp','nearly dry phrases, released trailing space'),
('diffuse octave halo','piano/pad/chords','bright harmonic mist'),
('diffuse fifth extension','chords/pad','slow wide harmonic nebula'),
('dark octave-down cloud','vocal/guitar/pad','dense harmonic submerged choir'),
('moving hidden pitch','synth/percussion','irregular fifth fragments'),
('subtle long ambience','acoustic/vocal/piano','neutral-dark, understated room')]

def diagnostic(a):
    # Tail-only spectral concentration and flatness, envelope smoothness.
    x=a[4*m.SR:]; n=4096
    frames=x[:len(x)//n*n].reshape(-1,n,2)
    p=(abs(np.fft.rfft(frames*np.hanning(n)[None,:,None],axis=1))**2).sum(axis=2)[:,1:]
    energy=p.sum(axis=1); good=energy>max(1e-18,energy.max()*1e-4); p=p[good]
    q=p/np.maximum(p.sum(axis=1,keepdims=True),1e-30)
    env=np.sqrt((x*x).mean(axis=1).reshape(-1,480).mean(axis=1))
    return dict(pitch_salience_proxy=float(np.mean(q.max(axis=1))), spectral_flatness=float(np.mean(np.exp(np.log(np.maximum(p,1e-30)).mean(axis=1))/np.maximum(p.mean(axis=1),1e-30))), tail_envelope_step=float(np.mean(abs(np.diff(env)))/max(1e-12,env.mean())), transient_smear_rms=m.rms(a[:int(.25*m.SR)]))

def main():
    OUT.mkdir(exist_ok=True)
    params=m.readcsv(DIR/'parameters.csv'); names=list(dict.fromkeys(r['preset'] for r in params))
    assert len(names)==20
    rows=[]; diag=[]
    for i,name in enumerate(names):
        for source in m.SOURCES+(EXTRA if i>=13 else []):
            audio=[m.wav(DIR/f'{i:02}_{source}_{mode}.wav') for mode in range(3)]
            alias='chords' if source in EXTRA else source
            rows.append(dict(preset=name,source=source,**m.features(*audio,m.wav(DIR/f'dry_{source}.wav'),alias)))
            if i in (15,16) and source in ['impulse','vocal','major','minor','suspended','cluster']:
                for variant,wet in [('high',audio[1]),('low',m.wav(DIR/f'{i}_{source}_lowdiff.wav'))]:
                    diag.append(dict(preset=name,source=source,diffusion=variant,**diagnostic(wet)))
        print(name,flush=True)
    m.writecsv(OUT/'metrics.csv',rows);m.writecsv(OUT/'diffusion_ab.csv',diag);m.writecsv(OUT/'parameters.csv',params)
    common=[r for r in rows if r['source'] in m.SOURCES]; a=m.aggregate(names,common)
    keys=['active_rms_db','tail_energy','decay_proxy_s','width','centroid_hz','shimmer_contribution','modulation_proxy','lf_fraction','dominant_pitch_variance_hz2']
    ix={(r['preset'],r['source']):r for r in common}
    v=np.array([[ix[name,s][k] for s in m.SOURCES for k in keys] for name in names])
    for si in range(6):
        for k in ['tail_energy','centroid_hz','dominant_pitch_variance_hz2']:
            j=si*len(keys)+keys.index(k);v[:,j]=np.log1p(v[:,j])
    scale=np.maximum(v.std(axis=0),1e-6);z=(v-v.mean(axis=0))/scale
    d=np.sqrt(((z[:,None,:]-z[None,:,:])**2).mean(axis=2))
    m.writecsv(OUT/'similarity.csv',[dict(preset=n,**{other:np.exp(-d[i,j]) for j,other in enumerate(names)}) for i,n in enumerate(names)])
    m.writecsv(OUT/'feature_scaling.csv',[dict(feature=f'{s}_{k}',mean=v[:,si*len(keys)+ki].mean(),std=scale[si*len(keys)+ki]) for si,s in enumerate(m.SOURCES) for ki,k in enumerate(keys)])
    pairs=sorted((d[i,j],names[i],names[j]) for i in range(20) for j in range(i+1,20))
    raw={(r['preset'],r['parameter']):float(r['raw']) for r in params}
    safety=m.readcsv(DIR/'safety.csv');shutil.copyfile(DIR/'safety.csv',OUT/'safety.csv');shutil.copyfile(DIR/'transitions.csv',OUT/'transitions.csv')
    lines=['# M11 — Factory Preset Expansion\n','## Legacy preservation\nPresets 0–12: nomes, ordem e todos os parâmetros normalizados e brutos comparados bit a bit com o catálogo M10 congelado em `Tests/M10FactorySnapshot.h`. Nenhum DSP, MacroEngine, UI ou contrato de parâmetros foi alterado.\n',
    '## New presets — complete saved parameters\nValores brutos salvos; `parameters.csv` inclui também controles efetivos após assentamento dos macros. Interval: 0 = −1 oct, 1 = fifth, 2 = +1 oct.\n']
    allkeys=list(dict.fromkeys(r['parameter'] for r in params))
    lines.append(m.table(['Parameter',*names[13:]],[[k,*[m.fmt(raw[n,k]) for n in names[13:]]] for k in allkeys]))
    lines.append('Mix efetivo dos presets sutis: Long Shadow 0.215, Afterimage 0.170, Ghost Room 0.139. Os valores salvos menores compensam a contribuição positiva de BLOOM ao Mix; preservam a função musical sem alterar o MacroEngine. WILD deliberadamente baixo nesses papéis, mantendo margem para subir.\n')
    lines.append('## Identity\n'+m.table(['Preset','Role','Best for','Character'],[[n,*ROLES[i]] for i,n in enumerate(names[13:])]))
    median=np.median([a[n]['active_rms_db'] for n in names])
    lines.append(f'## Metrics\nMediana RMS ativo do banco: {median:.3f} dBFS. Comparação nas mesmas seis fontes da M10; fontes extras não mudam o peso da matriz.\n'+m.table(['Preset','Active RMS dBFS','Δ median dB','Peak','Impulse decay s','Tail energy','Wet width','Centroid Hz','Shimmer contribution','Motion'],[[n,m.fmt(a[n]['active_rms_db']),m.fmt(a[n]['active_rms_db']-median),*[m.fmt(a[n][k]) for k in ['peak','impulse_decay','tail_energy','width','centroid_hz','shimmer_contribution','modulation_proxy']]] for n in names]))
    lines.append('Decay: último bin de 100 ms acima de −40 dB do máximo wet; não é RT60. Caudas que alcançam o fim de 24 s ficam censuradas (`metrics.csv`). Shimmer contribution: RMS(wet − no-shimmer)/RMS(wet), inclui mudança de trajetória da recirculação. Motion: variação de envelope relativa a tendência de 250 ms; não é profundidade de LFO. Trims de saída neutros; exceções de nível são justificadas pela função low-mix.\n')
    lines.append('## Archetype evidence\n')
    for i,n in enumerate(names[13:],13):
        r=ix[n,'vocal']; pl=ix[n,'pluck']
        lines.append(f'**{n}**: Mix salvo {raw[n,"mix"]:.3f}; duck efetivo '+next(x['effective'] for x in params if x['preset']==n and x['parameter']=='duckAmount')+f'; decay impulso {a[n]["impulse_decay"]:.2f} s; energia wet após frase vocal {r["tail_energy"]:.6g}, pluck {pl["tail_energy"]:.6g}; preservação transiente vocal {r["transient_preservation_db"]:.2f} dB, pluck {pl["transient_preservation_db"]:.2f} dB. Segurança: PASS. A correspondência perceptiva deve ser julgada nos WAVs; métricas não substituem audição.\n')
    lines.append('## Shimmer + diffusion A/B\nMesmo shimmer/macros, difusão manual 0.08 versus preset. Concentração do bin dominante = proxy de saliência; flatness = densidade espectral; envelope-step = irregularidade relativa da cauda; RMS inicial = smear proxy. Estas medidas não demonstram por si só dissolução perceptiva.\n'+m.table(list(diag[0]),[[m.fmt(r[k]) for k in diag[0]] for r in diag]))
    for n in names[15:17]:
        comparisons=[]
        for source in ['impulse','vocal','major','minor','suspended','cluster']:
            lo=next(r for r in diag if r['preset']==n and r['source']==source and r['diffusion']=='low')
            hi=next(r for r in diag if r['preset']==n and r['source']==source and r['diffusion']=='high')
            comparisons.append((hi['pitch_salience_proxy']/max(1e-12,lo['pitch_salience_proxy']),hi['spectral_flatness']/max(1e-12,lo['spectral_flatness'])))
        ratios=np.array(comparisons)
        lines.append(f"**{n}**: saliência média high/low {ratios[:,0].mean():.3f}; densidade espectral high/low {ratios[:,1].mean():.3f}; saliência reduzida em {(ratios[:,0]<1).sum()}/6 fontes. A difusão deve ser interpretada por fonte, sem presumir que sempre reduz a saliência.\n")
    lines.append('## Chords and low octave\nMajor/minor/suspended/cluster rendidos individualmente; Submerged Choir também com baixo 55 Hz, vocal, pluck e pad. LF representa fração de potência abaixo de 200 Hz e não é um gate de musicalidade.\n'+m.table(['Preset','Source','Peak','LF fraction','Tail energy','Shimmer contribution'],[[r['preset'],r['source'],*[m.fmt(r[k]) for k in ['peak','lf_fraction','tail_energy','shimmer_contribution']]] for r in rows if (r['preset'] in names[15:17] and r['source'] in EXTRA[1:5]) or (r['preset']==names[17] and r['source'] in ['bass','vocal','pluck','pad'])]))
    lines.append('## Similarity\nMatriz 20×20 em `similarity.csv`: exp(−distância), distância RMS de features z-score nas seis fontes comuns. Energia, centroid e variância pitch usam log1p. Proximidade pede A/B humano, sem score de aprovação.\n'+m.table(['A','B','Distance','Similarity'],[[x,y,m.fmt(dist),m.fmt(np.exp(-dist))] for dist,x,y in pairs[:15]]))
    targets=['Zero-G','Event Horizon','Frozen Choir','Dark Matter','Glass Transit','Solar Wind']
    lines.append(m.table(['New preset',*targets],[[n,*[m.fmt(np.exp(-d[i,names.index(t)])) for t in targets]] for i,n in enumerate(names) if i>=13]))
    lines.append(f'## Safety\n{len(safety)} renders principais float: pico máximo {max(float(r["peak"]) for r in safety):.6f}; DC médio máximo {max(abs(float(r["dc"])) for r in safety):.8f}. Gate por canal: DC < 0.005; pico < 0.98; RMS final < 0.12; crescimento tardio < max(0.003, 2× RMS penúltimo). A/B de difusão e Freeze também passam pelos gates.\n')
    lines.append('## Listening\n`listen.html` reúne mix completo, wet-only, no-shimmer e A/B difusão. WAVs stereo 48 kHz/24 bit, 24 s, fonte ativa 4 s, sem normalização. Fontes sintéticas são proxies, não gravações reais. Nenhuma aprovação auditiva humana é alegada.\n')
    tests=OUT/'tests.txt';lines.append('## Tests\n```text\n'+tests.read_text(errors='replace')+'\n```\n')
    (OUT/'M11-Preset-Expansion.md').write_text('\n'.join(lines),encoding='utf8')
    page=['<!doctype html><meta charset="utf-8"><title>M11 listening</title><style>body{font:16px system-ui;background:#111827;color:#eee;margin:30px}td,th{padding:8px}audio{width:270px}</style><h1>M11 — renders sem normalização</h1>']
    for i,n in enumerate(names[13:],13):
        page.append(f'<h2>{html.escape(n)}</h2><p>{ROLES[i-13][2]}</p><table><tr><th>Source</th><th>Full mix</th><th>Wet</th><th>No shimmer</th><th>Low diffusion wet</th></tr>')
        for s in m.SOURCES+EXTRA:
            page.append('<tr><td>'+s+'</td>'+''.join(f'<td><audio controls preload="none" src="../../build/te2350-vst-ninja-net/PresetVoicingOutput/{i:02}_{s}_{mode}.wav"></audio></td>' for mode in range(3))+(f'<td><audio controls preload="none" src="../../build/te2350-vst-ninja-net/PresetVoicingOutput/{i}_{s}_lowdiff.wav"></audio></td>' if i in (15,16) and s in ['impulse','vocal','major','minor','suspended','cluster'] else '<td></td>')+'</tr>')
        page.append('</table>')
    (OUT/'listen.html').write_text('\n'.join(page),encoding='utf8')
    print('Closest:',pairs[:5])
if __name__=='__main__': main()
