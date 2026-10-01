"""Reproduce the M9 report and a local, unnormalised A/B listening index.
Usage: python te2350-vst/Tests/macro_calibration_report.py
Run the baseline executable against the pre-M9 source, then the final CTest.
NumPy is used only for offline WAV comparisons, never by the plugin.
"""
from pathlib import Path
import csv, math, re, shutil, wave, html
import numpy as np
ROOT=Path(__file__).resolve().parents[2]
BEFORE=ROOT/'build/m9/before'
AFTER=ROOT/'build/te2350-vst-ninja-net/MacroCalibrationOutput'
OUT=ROOT/'docs/M9'
OUT.mkdir(exist_ok=True)
def rows(p):
    with p.open() as f: return list(csv.DictReader(f))
def table(headers,data):
    return '| '+' | '.join(headers)+' |\n| '+' | '.join(['---']*len(headers))+' |\n'+''.join('| '+' | '.join(map(str,r))+' |\n' for r in data)+'\n'
def fmt(v): return f'{float(v):.6g}'
def audio(p):
    with wave.open(str(p),'rb') as w:
        b=np.frombuffer(w.readframes(w.getnframes()),dtype=np.uint8).reshape(-1,3)
        x=b[:,0].astype(np.int32)+(b[:,1].astype(np.int32)<<8)+(b[:,2].astype(np.int32)<<16)
        x=np.where(x & 0x800000,x-0x1000000,x)
        return (x.astype(np.float64)/8388608).reshape(-1,2)
b=rows(BEFORE/'audio_metrics.csv');a=rows(AFTER/'audio_metrics.csv')
bi={(r['case'],r['source']):r for r in b};ai={(r['case'],r['source']):r for r in a}
lines=['# M9 — Macro Musical Calibration & Interaction Matrix\n',
'''## Método e alcance

48 kHz, blocos de 128, Hardware, entrada de 3 s e render de 12 s (9 s de cauda),
100 blocos de silêncio para estabilizar o controle antes da excitação. Fontes:
0 impulso 0.5; 1 harmônico 220 Hz + harmônicos; 2 percussão determinística a cada
500 ms; 3 cluster sustentado. Entrada idêntica nas duas versões; nenhum WAV foi
normalizado. Os presets preservam seus valores originais. Referência anterior: commit `ffc77556ae4090d2b2734d7dda323e1931b19be1` (MacroEngine original).

Antes: 366 WAVs. Depois: 520 WAVs, incluindo grade dos três eixos em wet-only e varredura manual com todos os macros em 100%.

As medidas são proxies, não julgamento auditivo nem RT60 ajustado: decay é o fim
do último bin de 100 ms acima de -40 dB do maior bin, incluindo o dry. Um impulso
pode cair abaixo desse limiar mesmo com energia residual. Pitch variance mede a
variância da frequência do maior bin FFT (2048/Hann), não cents nem rastreamento
monofônico. Modulation proxy é RMS da derivada do áudio; a profundidade real de
controle está na tabela efetiva. Width = sqrt(side energy / (mid+side energy)).
HF >6 kHz, LF <200 Hz; centroid usa potência espectral. Density é a fração de
amostras da cauda com energia >1e-8. Transient RMS é dos primeiros 100 ms e deve
ser comparado com a mesma fonte; não representa sozinho inteligibilidade.

Os CSVs medem MacroEngine após 100 updates reais, incluindo clamp e headroom.
`manual_fraction=-1` representa o default, os demais usam frações do range
**linear em unidades físicas**, não a posição normalizada/skew do host. O APVTS
é escrito por convertTo0to1, como no plugin. SPACE → time é substituído pelo
host em Sync Mode; Kill Dry substitui Mix. Esses caminhos permanecem existentes.

## Current mapping — antes

`mapped(x)` é a curva declarada; a contribuição antiga era `mapped(x)-value@0`.
Os valores desta tabela são declarados; a coluna effective mede o resultado real
com manual no default, outros macros em zero. O CSV inclui cinco valores manuais.
''']
old=[('space','timeMs',420,1080,'Log','10..2000'),('space','lowCutHz',80,140,'Log','20..1000'),('space','highCutHz',9000,5200,'Log','1000..18000'),('space','diffusion',.4,.78,'Linear','0..1'),('space','shimmerAmount',0,.32,'Linear','0..1'),('space','wetWidth',.6,.95,'Linear','0..1'),('wild','feedback',.45,1.05,'Linear','0..1.05 + teto .95+.10W'),('wild','chaos',0,.85,'Exponential','0..1'),('wild','wobble',.1,.85,'Exponential','0..1'),('wild','modRateHz',.15,1.2,'Log','.02..2'),('wild','modDepth',.1,.85,'Exponential','0..1'),('bloom','highCutHz',9000,14000,'Log','1000..18000'),('bloom','duckAmount',.1,.55,'Exponential','0..1; headroom'),('bloom','shimmerAmount',0,.18,'Linear','0..1'),('bloom','mix',.35,.52,'Linear','0..1')]
oldmap=rows(BEFORE/'effective_mapping.csv')
data=[]
for macro,target,lo,hi,curve,clamp in old:
    values=[lo*(hi/lo)**x if curve=='Log' else lo+(hi-lo)*(x*x if curve=='Exponential' else x) for x in [0,.25,.5,.75,1]]
    measured=next(r for r in oldmap if r['macro']==macro and r['target']==target and r['manual_fraction']=='-1')
    data.append([macro,target,*map(fmt,values),curve,clamp,' / '.join(measured[f'value@{k}'] for k in [0,25,50,75,100])])
lines.append(table(['Macro','Target','value@0','value@25','value@50','value@75','value@100','curve','clamp','effective default 0/25/50/75/100'],data))
lines.append('''Além das definições: WILD controla o teto do wrapper (.95+.10W) e o instability meter; não há outro coeficiente WILD aplicado diretamente ao core C;
BLOOM entra em `p_tail`, voicing/dynamic ducking e calibratedTailFeedback.
SPACE tem seis destinos listados, sem caminho direto adicional de tamanho no core.
O piso de feedback de cauda é `pow(referenceFeedback, timeSeconds/.5)`, com pontos
BLOOM `[0,.25,.5,.75,.9,1]` → `[0,.195,.550,.690,.713,.721]`. O core toma o maior
entre feedback manual/macro e esse piso, adiciona wobble, faz crossfade de Freeze
para .995 e aplica teto `.995-.018*chaos_unstable`. O Q31 limita a unidade. Logo,
MacroEngine effective Feedback não é sozinho o ganho final do loop. A tabela e
os renders auditam esses dois níveis separadamente; não foi alterado o core.

## Problems found

- Soma antiga de WILD→Feedback: manual .70 + WILD .50 = 1.00 (teto), e a metade
  superior fica limitada pelo teto, substituindo grande parte da autoridade manual.
- SPACE→Width: manual .75 + SPACE .75 = 1.00; SPACE .75→1 não abre mais largura.
  Com SPACE=1, manuais .75 e 1 resultam ambos em 1.
- Shimmer: manual .75 + SPACE=1 = 1; BLOOM somava mais .18. Dois eixos ativavam
  o mesmo destino, com pouca margem para o knob manual.
- WILD: Depth/Wobble/Chaos compartilhavam x² e endpoints .85; pouca resposta
  inicial e saturação de knobs altos. Rate também atingia teto com valores altos.
- SPACE time/Tone e BLOOM Tone/Mix também tinham clamps com manuais próximos
  dos extremos; o ducking M8 já preservava margem e foi mantido.
- BLOOM ativava o feedback de oitava ao elevar shimmerAmount de zero. A pequena
  contribuição habilitava conditioning/regen existente e encurtava a resposta
  inicial de impulso/percussão. Os dados antes/depois registram o efeito.
- Não foi encontrado runaway/clipping grave nos renders de referência. A medição
  revela plateaus de controle; RMS quase constante não prova ausência de movimento.

## Changes

SPACE preserva time e filtros no default, mas usa margem direcional. Diffusion
.40→.78 passa a .40→.70; Width .60→.95 passa a .60→.82; Shimmer 0→.32 passa a
0→.08. Sua identidade fica em dimensão/estrutura, com shimmer secundário.

WILD Feedback .45→1.05 passa a .45→.57, linear e com margem. Chaos 0→.85 passa
a 0→.72; Wobble .10→.85 passa a .10→.60; Depth .10→.85 passa a .10→.65.
Os três x² passam a `.35*x+.65*x*x`, com inclinação inicial positiva e aceleração
na metade superior. Rate .15→1.20 passa a .15→.90, log e com margem.

BLOOM mantém Tone 9000→14000 log, Duck .10→.55 x² (equação M8), Mix .35→.52
linear, agora com margem também em Tone/Mix. Remove soma automática 0→.18 de
Shimmer: o core continua a enriquecer shimmer manualmente habilitado. Adiciona
Feedback .45→.75 linear, com margem, para aumentar sustain já no início em vez
de depender apenas do piso de cauda que fica abaixo do Feedback manual.

Para cada destino com escala h:
`offset = mapped(x)-value@0`; `effective = previous + offset*h*remainingHeadroom`.
A margem é `max-previous` para contribuições positivas e `previous-min` para
negativas. SPACE é aplicado antes de BLOOM no filtro compartilhado. Nem os
valores absolutos declarados nem a posição macro substituem o manual.

Feedback recebe depois uma compressão contínua acima de .85:
`ceiling=.95+.04*WILD`; `f=.85+(f-.85)*(ceiling-.85)/.20`.
Até .85 a resposta é preservada; acima a inclinação continua positiva. Manual
1.05 resulta em .95/.96/.97/.98/.99, sem intervalo plano nem clip na conversão
Q31. A proteção antiga do wrapper permanece, mas não limita esses novos alvos.
Isso altera intencionalmente a resposta manual acima do joelho mesmo com macros
zero; não altera range nem automação pública. O piso de cauda e a proteção interna
do DSP ainda podem dominar feedback muito baixo/alto; não se afirma que todo
controle de loop fica estritamente monotônico por sample.

## Mapping — depois, efetivo medido
''')
newmap=rows(AFTER/'effective_mapping.csv')
data=[]
for r in newmap:
    if r['manual_fraction']=='-1': data.append([r['macro'],r['target'],r['manual'],*[r[f'value@{k}'] for k in [0,25,50,75,100]]])
lines.append(table(['Macro','Target','Manual default','effective@0','@25','@50','@75','@100'],data))
lines.append('Exemplo pedido: BLOOM .75 + Duck manual .60 → `.60 + .45*.75²*(1-.60) = .70125`.\n\n')
lines.append('## Metrics — antes/depois\n\nFonte harmônica, mesmos ganhos. Energia de cauda em amplitude²·s.\n\n')
data=[]
for macro in ['space','wild','bloom']:
 for k in [0,50,100]:
    oldr=bi[(f'{macro}_{k}','1')];newr=ai[(f'{macro}_{k}','1')]
    data.append([f'{macro} {k}',*[f'{fmt(oldr[col])} → {fmt(newr[col])}' for col in ['rms','peak','tail_energy','width','centroid','decay_proxy_seconds']]])
lines.append(table(['Caso','RMS','Peak','Tail energy','Width','Centroid Hz','Decay proxy s'],data))
lines.append('### Factory presets — diferenças intencionais de áudio\n\nMesmos valores e sinal harmônico. ΔRMS = 20log10(after/before); nenhum preset foi reescrito.\n\n')
preset_data=[]
for i in range(13):
    br=bi[(f'preset_{i}','1')];ar=ai[(f'preset_{i}','1')]
    delta=20*math.log10(float(ar['rms'])/float(br['rms']))
    preset_data.append([i,f'{fmt(br["peak"])} → {fmt(ar["peak"])}',f'{delta:+.3f}',f'{fmt(br["tail_energy"])} → {fmt(ar["tail_energy"])}'])
lines.append(table(['Preset index','Peak before → after','ΔRMS dB','Tail energy before → after'],preset_data))
lines.append('## Dead zones — waveform e superfícies\n\nDiferença relativa `sqrt(sum((novo-anterior)²)/sum(anterior²))`, cada +10%.\n\n')
data=[]
for macro in ['space','wild','bloom']:
 for source in range(4):
    rel=[]
    for k in range(10,101,10):
        x=audio(BEFORE/f'{macro}_{k}_source{source}.wav');y=audio(BEFORE/f'{macro}_{k-10}_source{source}.wav')
        rel.append(math.sqrt(float(np.sum((x-y)**2))/max(1.e-20,float(np.sum(y*y)))))
    final=[float(ai[(f'{macro}_{k}',str(source))]['adjacent_relative_difference']) for k in range(10,101,10)]
    data.append([macro,source,fmt(min(rel)),fmt(min(final)),' / '.join(fmt(final[i]) for i in [0,1,4,5,8,9])])
lines.append(table(['Macro','Source','Min delta before','Min delta after','After 0–10/10–20/40–50/50–60/80–90/90–100'],data))
lines.append('''O limiar automático é .001 de diferença relativa: nenhum intervalo pode ficar
abaixo de 0.1% nos quatro sinais. Isso detecta identidade numérica, não garante
limiar psicoacústico. Em SPACE o alinhamento do delay muda a energia por fase;
RMS/centroid/width medidos podem oscilar entre passos mesmo com dimensão crescente.
O proxy de dimensão é o alvo time+diffusion+width, junto dos renders. A duração percussiva também é verificada com tolerância de um bin (100 ms). Para BLOOM,
energia pós-ataque harmônica/percussiva/cluster é o proxy robusto; o teste exige ausência de reversão maior que 0.5% por passo e crescimento de ao menos 25% entre endpoints; energia de
impulso muito tardia perto do piso numérico não é usada como monotonicidade absoluta.

## Interaction matrix

''')
for prefix,title in [('space_bloom','Space × Bloom'),('wild_feedback','Wild × Feedback'),('bloom_shimmer','Bloom × Shimmer'),('wild_freeze','Wild × Freeze'),('space_width','Space × Width'),('all_axes','Todos os eixos'),('manual_','Manuais com os três macros em 100%'),('preset_','Todos os factory presets')]:
    rr=[r for r in a if r['case'].startswith(prefix)]
    lines.append(f'- **{title}**: {len(rr)} renders; peak máximo {fmt(max(float(r["peak"]) for r in rr))}, RMS mínimo {fmt(min(float(r["rms"]) for r in rr))}, |DC| máximo {fmt(max(abs(float(r["dc"])) for r in rr))}.\n')
freeze_data=[]
for r in a:
    if not r['case'].startswith('wild_freeze_'): continue
    x=audio(AFTER/(r['case']+'_source'+r['source']+'.wav')).mean(axis=1)
    tail=x[3*48000:];max_step=float(np.max(np.abs(np.diff(tail))))
    window_dc=max(abs(float(np.mean(tail[i:i+48000]))) for i in range(0,len(tail),48000))
    stats=[]
    for lo,hi in [(3,5),(10,12)]:
        z=x[lo*48000:hi*48000];spec=np.abs(np.fft.rfft(z*np.hanning(len(z))))**2
        hz=np.fft.rfftfreq(len(z),1/48000);total=float(spec.sum())
        stats += [float(np.sqrt(np.mean(z*z))),float((spec*hz).sum()/max(1e-20,total)),float(spec[hz>6000].sum()/max(1e-20,total))]
    if window_dc>=.02: raise RuntimeError('Freeze DC accumulation: '+r['case'])
    freeze_data.append([r['case'],r['source'],max_step,window_dc,*stats])
with (OUT/'freeze_windows.csv').open('w',newline='') as f:
    w=csv.writer(f);w.writerow(['case','source','max_tail_sample_step','max_one_second_dc','rms_3_5s','centroid_3_5s','hf_3_5s','rms_10_12s','centroid_10_12s','hf_10_12s']);w.writerows(freeze_data)
lines.append('\nFreeze: `freeze_windows.csv` compara energia/centroid/HF em 3–5 s e 10–12 s e mede maior salto por sample e DC por janela de 1 s. DC local também permaneceu abaixo de .02.\n')
lines.append('''
A grade adicional dos três eixos 3×3×3 usa Kill Dry, para verificar que o efeito wet continua audível sem o dry. WILD instability é verificado a cada 10%.\n\nSpace × Bloom: grade 5×5 com WILD=0. Time/diffusion/width continuam exclusivos de
SPACE; BLOOM controla expansão wet, duck e sustain. O filtro compartilhado possui
margem em ambas direções; a abertura de BLOOM não apaga totalmente o manual.

Wild × Feedback: 5×4 em Feedback .20/.45/.70/.90. WILD muda movimento e eleva o
feedback apenas moderadamente. Os valores APVTS são unidades físicas (o parâmetro
Feedback tem range 0..1.05); essa grade usa os valores .20/.45/.70/.90 pedidos.
A superfície de headroom adicional percorre todo o range público até 1.05.

Bloom × Shimmer: 5×4×2, shimmer 0/.25/.50/.75, regen .3/.75. BLOOM sozinho não
habilita a lane pitchada. Shimmer manual continua com resposta crescente até 1
na superfície dos três macros; só SPACE reserva uma contribuição secundária .08.

Wild × Freeze: 5×2 (Atmos OFF/ON), Freeze ligado em 2 s após carregar o loop.
Sustain prolongado é esperado. Verificações finitas, peak e DC nos renders;
a proteção do core permanece. FFT/derivada são proxies de evolução/movimento,
não substituem escuta para stepping/harshness. Não se confunde hold com runaway.

Space × Width: 3×3 (0/.5/1). Em SPACE=1, Width manual 0/.25/.5/.75/1 produz
.55/.6625/.775/.8875/1. O endpoint manual 1 permanece 1 por definição; não é
zona morta do macro, pois não há mais margem física.

Bloom × Duck: superfície 11×11 exportada, monotônica em manual. Em Duck=1,
BLOOM não pode elevar mais o ducking; para qualquer manual abaixo de 1, a curva
preserva margem até o fim. `Duck=.6, Bloom=.75 → .70125`, M8 preservada.

## Tests

''')
log=(ROOT/'build/m9/regression-final.log').read_text()
lines.append('```text\n'+log+'\n```\n')
strengthened = ROOT/'build/m9/calibration-strengthened.log'
if strengthened.exists():
    lines.append('Additional verification: Feedback response, both M8 ducking axes, and per-channel/window Freeze DC:\n\n```text\n' + strengthened.read_text() + '\n```\n')
lines.append('''UIRender teve a expectativa antiga de Feedback=.8 corrigida para verificar
que WILD eleva o manual .2 mantendo o teto seguro; a telemetria acompanha o
novo comportamento. Editor efetivo 1040×680 também inspecionado visualmente.

GoldenReferenceRender/Compare continua bit-exact para o core C. Esse teste não
exercita os defaults do MacroEngine: os WAVs before/after e métricas de presets
registram a diferença intencional nos presets com SPACE/BLOOM não zero. Nenhuma
referência golden foi atualizada.

## Compatibility e realtime

ParameterLayout, IDs, ranges, defaults, factory presets, state version,
serialização e paths de automação estão sem alterações. HostCompatibility,
PresetWorkflow, ReleaseReadiness e VST3Load verificam recall/carga. Estados
antigos recuperam os mesmos valores brutos; o áudio recalibrado pode diferir,
como esperado nesta milestone. Sem revoice de presets.

MacroEngine permanece em control/block rate, com a suavização de 8 ms existente.
Só acrescenta aritmética e consultas à tabela já existente: nenhuma alocação,
lock, filesystem ou processamento pesado por sample foi introduzido no plugin.
As alocações/FFT/WAVs deste harness ocorrem exclusivamente fora do realtime.

## Artefatos e reprodução

- CSVs neste diretório: medidas before/after, mapping before/after e duck surface.
- Todos os WAVs before: `build/m9/before`; final:
  `build/te2350-vst-ninja-net/MacroCalibrationOutput`.
- A/B sem normalização: `build/m9/listen.html`, eixos 0/50/100 e quatro interações.
- Build: `cmake --build build/te2350-vst-ninja-net --target TE2350MacroCalibrationTest`.
- Teste: `ctest --test-dir build/te2350-vst-ninja-net -R TE2350MacroCalibrationTest --output-on-failure`.
- Relatório: `python te2350-vst/Tests/macro_calibration_report.py` (NumPy, offline).

A implementação e os checks objetivos estão concluídos. A aceitação **perceptiva**
final exige escutar os A/B: proxies não permitem afirmar por si só que não há
harshness/stepping ou que cada +10% é igualmente audível para todas as fontes.
''')
(OUT/'M9-Macro-Calibration.md').write_text('\n'.join(lines),encoding='utf-8')
for label,path in [('before_metrics',BEFORE/'audio_metrics.csv'),('after_metrics',AFTER/'audio_metrics.csv'),('before_mapping',BEFORE/'effective_mapping.csv'),('after_mapping',AFTER/'effective_mapping.csv'),('duck_surface',AFTER/'duck_surface.csv')]: shutil.copyfile(path,OUT/(label+'.csv'))
items=[f'{m}_{k}' for m in ['space','wild','bloom'] for k in [0,50,100]]+['space_bloom_0_100','space_bloom_100_0','space_bloom_100_100','wild_feedback_0_90','wild_feedback_100_90','bloom_shimmer_0_75_0.75','bloom_shimmer_100_75_0.75','wild_freeze_0_0','wild_freeze_100_0','wild_freeze_100_1']
page=['<!doctype html><meta charset="utf-8"><title>M9 A/B</title><style>body{font:16px system-ui;background:#10141c;color:#e9ecf2;margin:32px}table{border-collapse:collapse}td,th{padding:12px;border-bottom:1px solid #343d50}audio{width:340px}</style><h1>M9 — A/B</h1><p>Antes / calibrado, mesmo sinal harmônico (source1), sem normalização. 3 s de entrada + 9 s de cauda. Freeze em 2 s.</p><table><tr><th>Caso</th><th>Antes</th><th>Calibrado</th></tr>']
for case in items:
    before=BEFORE/(case+'_source1.wav');after=AFTER/(case+'_source1.wav')
    page.append(f'<tr><td>{html.escape(case)}</td><td><audio controls preload="none" src="{before.as_uri()}"></audio></td><td><audio controls preload="none" src="{after.as_uri()}"></audio></td></tr>')
page.append('</table>');(ROOT/'build/m9/listen.html').write_text('\n'.join(page),encoding='utf-8')
print('Report and A/B index generated:',OUT/'M9-Macro-Calibration.md')
