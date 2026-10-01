# M11 — Factory Preset Expansion

## Legacy preservation
Presets 0–12: nomes, ordem e todos os parâmetros normalizados e brutos comparados bit a bit com o catálogo M10 congelado em `Tests/M10FactorySnapshot.h`. Nenhum DSP, MacroEngine, UI ou contrato de parâmetros foi alterado.

## New presets — complete saved parameters
Valores brutos salvos; `parameters.csv` inclui também controles efetivos após assentamento dos macros. Interval: 0 = −1 oct, 1 = fifth, 2 = +1 oct.

| Parameter | Long Shadow | Afterimage | Diffuse Halo | Fifth Nebula | Submerged Choir | Prism Drift | Ghost Room |
| --- | --- | --- | --- | --- | --- | --- | --- |
| space | 0.82 | 0.78 | 0.72 | 0.76 | 0.7 | 0.58 | 0.62 |
| wild | 0.03 | 0.08 | 0.06 | 0.12 | 0.05 | 0.72 | 0.015 |
| bloom | 0.68 | 0.6 | 0.55 | 0.7 | 0.66 | 0.44 | 0.36 |
| timeMs | 1380 | 800 | 760 | 1050 | 1080 | 540 | 820 |
| syncMode | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| feedback | 0.86 | 0.77 | 0.7 | 0.79 | 0.79 | 0.61 | 0.72 |
| mix | 0.045 | 0.015 | 0.28 | 0.25 | 0.28 | 0.27 | 0.05 |
| killDry | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| lowCutHz | 140 | 170 | 200 | 230 | 260 | 210 | 160 |
| highCutHz | 5600 | 6400 | 8800 | 6800 | 2200 | 7900 | 5400 |
| diffusion | 0.86 | 0.88 | 0.9 | 0.94 | 0.91 | 0.78 | 0.87 |
| chaos | 0 | 0.015 | 0 | 0.015 | 0 | 0.24 | 0 |
| wobble | 0.03 | 0.06 | 0.04 | 0.08 | 0.035 | 0.34 | 0.015 |
| presence | 0.38 | 0.42 | 0.48 | 0.4 | 0.2 | 0.46 | 0.38 |
| modRateHz | 0.045 | 0.06 | 0.055 | 0.055 | 0.04 | 0.06 | 0.035 |
| modDepth | 0.035 | 0.055 | 0.06 | 0.1 | 0.04 | 0.38 | 0.02 |
| modShape | 1 | 1 | 1 | 1 | 1 | 1 | 1 |
| shimmerInterval | 2 | 2 | 2 | 1 | 0 | 1 | 2 |
| shimmerAmount | 0 | 0 | 0.25 | 0.28 | 0.32 | 0.16 | 0 |
| shimmerFeedback | 0 | 0 | 0.24 | 0.36 | 0.34 | 0.12 | 0 |
| duckThreshold | -24 | -24 | -24 | -24 | -24 | -24 | -24 |
| duckAmount | 0.36 | 0.72 | 0.24 | 0.32 | 0.3 | 0.26 | 0.38 |
| inputTrim | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| outputTrim | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| bypass | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| qualityMode | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| freezeEngage | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| freezeMode | 1 | 1 | 1 | 1 | 1 | 1 | 1 |
| atmosFdnOn | 1 | 1 | 1 | 1 | 1 | 0 | 0 |
| wetWidth | 0.7 | 0.78 | 0.79 | 0.85 | 0.73 | 0.75 | 0.43 |


Mix efetivo dos presets sutis: Long Shadow 0.215, Afterimage 0.170, Ghost Room 0.139. Os valores salvos menores compensam a contribuição positiva de BLOOM ao Mix; preservam a função musical sem alterar o MacroEngine. WILD deliberadamente baixo nesses papéis, mantendo margem para subir.

## Identity
| Preset | Role | Best for | Character |
| --- | --- | --- | --- |
| Long Shadow | long low-mix ambience | vocal/guitar/piano/harp/lead | dry-forward, deep trailing field |
| Afterimage | ducked afterimage | vocal/guitar/harp | nearly dry phrases, released trailing space |
| Diffuse Halo | diffuse octave halo | piano/pad/chords | bright harmonic mist |
| Fifth Nebula | diffuse fifth extension | chords/pad | slow wide harmonic nebula |
| Submerged Choir | dark octave-down cloud | vocal/guitar/pad | dense harmonic submerged choir |
| Prism Drift | moving hidden pitch | synth/percussion | irregular fifth fragments |
| Ghost Room | subtle long ambience | acoustic/vocal/piano | neutral-dark, understated room |


## Metrics
Mediana RMS ativo do banco: -31.557 dBFS. Comparação nas mesmas seis fontes da M10; fontes extras não mudam o peso da matriz.
| Preset | Active RMS dBFS | Δ median dB | Peak | Impulse decay s | Tail energy | Wet width | Centroid Hz | Shimmer contribution | Motion |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Low Orbit | -31.28 | 0.2801 | 0.3695 | 1.2 | 6.138e-05 | 0.03763 | 796.6 | 0.001023 | 0.3829 |
| Tidal Lock | -32.39 | -0.8331 | 0.3304 | 6.7 | 0.0002789 | 0.09878 | 1069 | 0.00139 | 0.3504 |
| Trade Winds | -31.72 | -0.1649 | 0.3576 | 3 | 0.0002127 | 0.07271 | 734.4 | 0.0011 | 0.3497 |
| Solar Wind | -31.17 | 0.3828 | 0.3414 | 5 | 0.0004356 | 0.1212 | 1366 | 0.1747 | 0.37 |
| Escape Velocity | -31.34 | 0.2187 | 0.3531 | 3.3 | 0.0001894 | 0.1101 | 932.5 | 0.00145 | 0.3216 |
| Zero-G | -31.87 | -0.3095 | 0.3241 | 11.4 | 0.0005433 | 0.1143 | 931.3 | 0.001428 | 0.3542 |
| Event Horizon | -31.89 | -0.3286 | 0.3407 | 15.4 | 0.0007257 | 0.102 | 731.6 | 0.0007411 | 0.3558 |
| Glass Transit | -31.82 | -0.2625 | 0.3445 | 3 | 0.0002228 | 0.1047 | 1333 | 0.2071 | 0.387 |
| Cassette Moon | -31.54 | 0.01814 | 0.3648 | 2.1 | 0.0001518 | 0.0497 | 795.5 | 0.001618 | 0.4224 |
| Pulsar Eighths | -30.74 | 0.814 | 0.3618 | 1.6 | 4.716e-05 | 0.04317 | 843.3 | 0.0008979 | 0.3893 |
| Frozen Choir | -32.62 | -1.061 | 0.3147 | 5.8 | 0.000433 | 0.1113 | 1720 | 0.3598 | 0.3722 |
| Dark Matter | -31.84 | -0.2819 | 0.3483 | 6.2 | 0.0004501 | 0.07496 | 759.5 | 0.1729 | 0.3581 |
| Microgravity Slap | -30.67 | 0.8846 | 0.3962 | 0.7 | 5.601e-06 | 0.01617 | 1051 | 0.0007644 | 0.3422 |
| Long Shadow | -30.38 | 1.179 | 0.4113 | 22.7 | 0.0006617 | 0.1026 | 1052 | 0.001527 | 0.3722 |
| Afterimage | -29.91 | 1.643 | 0.427 | 11.4 | 0.0004733 | 0.1091 | 1263 | 0.001492 | 0.3681 |
| Diffuse Halo | -31.79 | -0.2347 | 0.3513 | 5.9 | 0.0003249 | 0.1173 | 1445 | 0.2196 | 0.3725 |
| Fifth Nebula | -31.39 | 0.1666 | 0.3499 | 7.5 | 0.0003748 | 0.1241 | 1813 | 0.2623 | 0.3874 |
| Submerged Choir | -32.11 | -0.5522 | 0.3437 | 7.4 | 0.0002353 | 0.112 | 1926 | 0.196 | 0.3639 |
| Prism Drift | -31.58 | -0.01814 | 0.362 | 4.3 | 0.0001718 | 0.1274 | 1536 | 0.1341 | 0.3907 |
| Ghost Room | -29.61 | 1.947 | 0.4374 | 7 | 0.0004272 | 0.07713 | 1121 | 0.001466 | 0.3561 |


Decay: último bin de 100 ms acima de −40 dB do máximo wet; não é RT60. Caudas que alcançam o fim de 24 s ficam censuradas (`metrics.csv`). Shimmer contribution: RMS(wet − no-shimmer)/RMS(wet), inclui mudança de trajetória da recirculação. Motion: variação de envelope relativa a tendência de 250 ms; não é profundidade de LFO. Trims de saída neutros; exceções de nível são justificadas pela função low-mix.

## Archetype evidence

**Long Shadow**: Mix salvo 0.045; duck efetivo 0.493171; decay impulso 22.70 s; energia wet após frase vocal 0.000614942, pluck 0.00198265; preservação transiente vocal -1.65 dB, pluck -1.66 dB. Segurança: PASS. A correspondência perceptiva deve ser julgada nos WAVs; métricas não substituem audição.

**Afterimage**: Mix salvo 0.015; duck efetivo 0.76536; decay impulso 11.40 s; energia wet após frase vocal 0.000504351, pluck 0.00128169; preservação transiente vocal -1.31 dB, pluck -1.30 dB. Segurança: PASS. A correspondência perceptiva deve ser julgada nos WAVs; métricas não substituem audição.

**Diffuse Halo**: Mix salvo 0.280; duck efetivo 0.343455; decay impulso 5.90 s; energia wet após frase vocal 0.000171975, pluck 0.000948813; preservação transiente vocal -3.06 dB, pluck -3.00 dB. Segurança: PASS. A correspondência perceptiva deve ser julgada nos WAVs; métricas não substituem audição.

**Fifth Nebula**: Mix salvo 0.250; duck efetivo 0.46994; decay impulso 7.50 s; energia wet após frase vocal 0.000260811, pluck 0.00108931; preservação transiente vocal -3.09 dB, pluck -2.88 dB. Segurança: PASS. A correspondência perceptiva deve ser julgada nos WAVs; métricas não substituem audição.

**Submerged Choir**: Mix salvo 0.280; duck efetivo 0.437214; decay impulso 7.40 s; energia wet após frase vocal 0.000146572, pluck 0.000758361; preservação transiente vocal -3.24 dB, pluck -3.05 dB. Segurança: PASS. A correspondência perceptiva deve ser julgada nos WAVs; métricas não substituem audição.

**Prism Drift**: Mix salvo 0.270; duck efetivo 0.324469; decay impulso 4.30 s; energia wet após frase vocal 0.000126533, pluck 0.00049674; preservação transiente vocal -2.78 dB, pluck -2.68 dB. Segurança: PASS. A correspondência perceptiva deve ser julgada nos WAVs; métricas não substituem audição.

**Ghost Room**: Mix salvo 0.050; duck efetivo 0.416158; decay impulso 7.00 s; energia wet após frase vocal 0.000456307, pluck 0.00108456; preservação transiente vocal -1.09 dB, pluck -1.07 dB. Segurança: PASS. A correspondência perceptiva deve ser julgada nos WAVs; métricas não substituem audição.

## Shimmer + diffusion A/B
Mesmo shimmer/macros, difusão manual 0.08 versus preset. Concentração do bin dominante = proxy de saliência; flatness = densidade espectral; envelope-step = irregularidade relativa da cauda; RMS inicial = smear proxy. Estas medidas não demonstram por si só dissolução perceptiva.
| preset | source | diffusion | pitch_salience_proxy | spectral_flatness | tail_envelope_step | transient_smear_rms |
| --- | --- | --- | --- | --- | --- | --- |
| Diffuse Halo | impulse | high | 0.1114 | 0.001867 | 0.5344 | 0.0008922 |
| Diffuse Halo | impulse | low | 0.1336 | 0.0009709 | 0.5031 | 0.0008231 |
| Diffuse Halo | vocal | high | 0.2732 | 7.243e-06 | 0.06851 | 0.004405 |
| Diffuse Halo | vocal | low | 0.2533 | 8.956e-06 | 0.0702 | 0.004161 |
| Diffuse Halo | major | high | 0.2329 | 1.06e-05 | 0.2162 | 0.005402 |
| Diffuse Halo | major | low | 0.242 | 1.193e-05 | 0.2139 | 0.005113 |
| Diffuse Halo | minor | high | 0.1919 | 8.819e-06 | 0.2587 | 0.006602 |
| Diffuse Halo | minor | low | 0.1939 | 9.544e-06 | 0.2597 | 0.006227 |
| Diffuse Halo | suspended | high | 0.2153 | 3.715e-06 | 0.2108 | 0.005283 |
| Diffuse Halo | suspended | low | 0.2232 | 3.988e-06 | 0.212 | 0.004988 |
| Diffuse Halo | cluster | high | 0.336 | 8.891e-06 | 0.2455 | 0.005447 |
| Diffuse Halo | cluster | low | 0.3369 | 9.358e-06 | 0.2506 | 0.005195 |
| Fifth Nebula | impulse | high | 0.07513 | 0.0004322 | 0.6141 | 0.0008748 |
| Fifth Nebula | impulse | low | 0.1041 | 0.0003721 | 0.5836 | 0.0008091 |
| Fifth Nebula | vocal | high | 0.3485 | 1.019e-05 | 0.07273 | 0.004154 |
| Fifth Nebula | vocal | low | 0.3572 | 1.37e-05 | 0.07101 | 0.003933 |
| Fifth Nebula | major | high | 0.2325 | 1.864e-05 | 0.2735 | 0.004858 |
| Fifth Nebula | major | low | 0.2524 | 1.689e-05 | 0.2677 | 0.004612 |
| Fifth Nebula | minor | high | 0.2109 | 1.267e-05 | 0.2835 | 0.005893 |
| Fifth Nebula | minor | low | 0.2238 | 1.193e-05 | 0.2794 | 0.005576 |
| Fifth Nebula | suspended | high | 0.2312 | 7.542e-06 | 0.2517 | 0.004638 |
| Fifth Nebula | suspended | low | 0.2541 | 7.601e-06 | 0.2476 | 0.004377 |
| Fifth Nebula | cluster | high | 0.2803 | 1.589e-05 | 0.2678 | 0.004862 |
| Fifth Nebula | cluster | low | 0.301 | 1.508e-05 | 0.2676 | 0.004653 |


**Diffuse Halo**: saliência média high/low 0.971; densidade espectral high/low 1.071; saliência reduzida em 5/6 fontes. A difusão deve ser interpretada por fonte, sem presumir que sempre reduz a saliência.

**Fifth Nebula**: saliência média high/low 0.900; densidade espectral high/low 1.020; saliência reduzida em 6/6 fontes. A difusão deve ser interpretada por fonte, sem presumir que sempre reduz a saliência.

## Chords and low octave
Major/minor/suspended/cluster rendidos individualmente; Submerged Choir também com baixo 55 Hz, vocal, pluck e pad. LF representa fração de potência abaixo de 200 Hz e não é um gate de musicalidade.
| Preset | Source | Peak | LF fraction | Tail energy | Shimmer contribution |
| --- | --- | --- | --- | --- | --- |
| Diffuse Halo | major | 0.1035 | 0.4659 | 0.0001118 | 0.2659 |
| Diffuse Halo | minor | 0.1049 | 0.5519 | 0.000131 | 0.2728 |
| Diffuse Halo | suspended | 0.1119 | 0.5393 | 0.0001209 | 0.2566 |
| Diffuse Halo | cluster | 0.1011 | 0.8161 | 0.0001035 | 0.2979 |
| Fifth Nebula | major | 0.1031 | 0.3949 | 0.0001008 | 0.2889 |
| Fifth Nebula | minor | 0.1072 | 0.486 | 0.000129 | 0.3295 |
| Fifth Nebula | suspended | 0.1121 | 0.4165 | 0.0001035 | 0.3022 |
| Fifth Nebula | cluster | 0.1011 | 0.8035 | 0.0001048 | 0.385 |
| Submerged Choir | pluck | 0.1502 | 0.013 | 0.0007584 | 0.2225 |
| Submerged Choir | vocal | 0.05371 | 0.001344 | 0.0001466 | 0.2257 |
| Submerged Choir | pad | 0.1035 | 0.4586 | 0.0002064 | 0.1416 |
| Submerged Choir | bass | 0.1456 | 0.9999 | 0.0001907 | 0.2835 |


## Similarity
Matriz 20×20 em `similarity.csv`: exp(−distância), distância RMS de features z-score nas seis fontes comuns. Energia, centroid e variância pitch usam log1p. Proximidade pede A/B humano, sem score de aprovação.
| A | B | Distance | Similarity |
| --- | --- | --- | --- |
| Afterimage | Ghost Room | 0.6113 | 0.5427 |
| Solar Wind | Fifth Nebula | 0.7124 | 0.4905 |
| Low Orbit | Cassette Moon | 0.7321 | 0.4809 |
| Low Orbit | Trade Winds | 0.7502 | 0.4723 |
| Tidal Lock | Zero-G | 0.7891 | 0.4543 |
| Diffuse Halo | Prism Drift | 0.7903 | 0.4537 |
| Zero-G | Event Horizon | 0.8029 | 0.448 |
| Tidal Lock | Trade Winds | 0.8212 | 0.4399 |
| Diffuse Halo | Fifth Nebula | 0.8272 | 0.4373 |
| Glass Transit | Diffuse Halo | 0.8292 | 0.4364 |
| Frozen Choir | Fifth Nebula | 0.8328 | 0.4348 |
| Low Orbit | Microgravity Slap | 0.8373 | 0.4329 |
| Trade Winds | Cassette Moon | 0.8396 | 0.4319 |
| Trade Winds | Escape Velocity | 0.8407 | 0.4314 |
| Solar Wind | Prism Drift | 0.8505 | 0.4272 |


| New preset | Zero-G | Event Horizon | Frozen Choir | Dark Matter | Glass Transit | Solar Wind |
| --- | --- | --- | --- | --- | --- | --- |
| Long Shadow | 0.3174 | 0.3229 | 0.1444 | 0.163 | 0.1509 | 0.2011 |
| Afterimage | 0.3034 | 0.2507 | 0.175 | 0.1582 | 0.2233 | 0.278 |
| Diffuse Halo | 0.2975 | 0.1988 | 0.4015 | 0.1895 | 0.4364 | 0.4245 |
| Fifth Nebula | 0.2689 | 0.1875 | 0.4348 | 0.1548 | 0.4124 | 0.4905 |
| Submerged Choir | 0.256 | 0.1804 | 0.3782 | 0.2173 | 0.3227 | 0.3668 |
| Prism Drift | 0.2804 | 0.1817 | 0.3351 | 0.1625 | 0.4103 | 0.4272 |
| Ghost Room | 0.2834 | 0.2283 | 0.1615 | 0.1849 | 0.2151 | 0.2712 |


## Safety
486 renders principais float: pico máximo 0.437428; DC médio máximo 0.00000109. Gate por canal: DC < 0.005; pico < 0.98; RMS final < 0.12; crescimento tardio < max(0.003, 2× RMS penúltimo). A/B de difusão e Freeze também passam pelos gates.

## Listening
`listen.html` reúne mix completo, wet-only, no-shimmer e A/B difusão. WAVs stereo 48 kHz/24 bit, 24 s, fonte ativa 4 s, sem normalização. Fontes sintéticas são proxies, não gravações reais. Nenhuma aprovação auditiva humana é alegada.

## Tests
```text
Test project C:/progs/vst/te2350/build/te2350-vst-ninja-net
      Start  3: TE2350GoldenReferenceRender
      Start  2: TE2350OfflineReferenceRender
 1/11 Test  #2: TE2350OfflineReferenceRender .....   Passed    0.07 sec
      Start 11: TE2350PresetVoicingTest
 2/11 Test  #3: TE2350GoldenReferenceRender ......   Passed    0.36 sec
      Start  1: TE2350MacroCalibrationTest
 3/11 Test  #1: TE2350MacroCalibrationTest .......   Passed  140.74 sec
      Start 15: TE2350FreezeConsistencyTest
 4/11 Test #15: TE2350FreezeConsistencyTest ......   Passed   30.35 sec
      Start 14: TE2350MusicalBehaviourTest
 5/11 Test #14: TE2350MusicalBehaviourTest .......   Passed    9.44 sec
      Start 13: TE2350ReleaseReadinessTest
 6/11 Test #13: TE2350ReleaseReadinessTest .......   Passed    0.60 sec
      Start  8: TE2350HostCompatibilityTest
 7/11 Test  #8: TE2350HostCompatibilityTest ......   Passed    0.77 sec
      Start 12: TE2350PresetWorkflowTest
 8/11 Test #12: TE2350PresetWorkflowTest .........   Passed    0.40 sec
      Start  9: TE2350VST3LoadTest
 9/11 Test  #9: TE2350VST3LoadTest ...............   Passed    0.58 sec
      Start  4: TE2350GoldenReferenceCompare
10/11 Test  #4: TE2350GoldenReferenceCompare .....   Passed    0.56 sec
11/11 Test #11: TE2350PresetVoicingTest ..........   Passed  257.48 sec

100% tests passed, 0 tests failed out of 11

Total Test time (real) = 257.58 sec

```
