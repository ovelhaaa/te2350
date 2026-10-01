# M9.1 — Manual Authority & Preset Preservation


## Escopo e método

Referência M9: `7b37cd8`; referência pré-M9: `ffc7755`. Mesmos presets,
48 kHz, blocos de 128, 3 s de entrada + 9 s de cauda, sem normalização.
Fontes 0 impulso, 1 harmônico, 2 percussão, 3 cluster sustentado;
fonte 4 acrescenta ataque percussivo a uma senoide sustentada.
Os renders focados contêm WILD 0/.5/.75/1 nas cinco fontes e os 13 presets
nas fontes 1 e 2: 46 WAVs por versão. A matriz completa mantém os 520 WAVs da M9.

As métricas conservam as definições M9: tail energy é energia após 3 s;
modulation proxy é RMS da derivada; pitch variance é variância do bin dominante
FFT, em Hz². Não são rastreadores de pitch/delay nem prova de percepção auditiva.
As avaliações objetivas não substituem escuta de movimento, harshness ou clareza.

## Changes

1. Removida a compressão global de Feedback acima de .85. O MacroEngine deixa
   o valor manual intacto com todos os macros em zero, inclusive 1.05.
2. WILD e BLOOM acrescentam somente na margem segura de Feedback, nunca reduzem
   o manual: `C=.95+.04*W`, `Fw=M+(.12*W/1.05)*max(0,C-M)`,
   `F=Fw+(.30*B/1.05)*max(0,C-Fw)`. Os endpoints das definições M9 não mudaram.
   Manual acima de C não recebe mais regeneração; isso é ausência de margem
   segura, não reinterpretação do knob. Com WILD=0 e BLOOM>0, BLOOM ainda contribui;
   a igualdade manual é exigida com BLOOM=0, como no contrato de macros zerados.
3. SPACE→Time mantém o offset log original enquanto existe margem suficiente.
   Somente a adição entra numa shoulder racional perto do limite de 2000 ms.
   Para `H=2000-M`, `K=.99*H`, `S=H-K`, `d=max(0,offset-K)`:
   `addition=offset` até K; depois `K+S*d/(S+d)` (zero se H=0).
   A base manual nunca é comprimida. Até SPACE=100%, o default de Time ainda
   segue exatamente 420→1080 ms. A mudança preserva o tempo de repetição dos
   presets que não precisam dessa proteção. Junto ao limite, a resolução float
   e o arredondamento do delay para samples podem tornar passos muito pequenos
   indistinguíveis; não se promete resolução ilimitada nessa fronteira física.

Nenhuma outra curva/target foi recalibrada. SPACE mantém Shimmer secundário .08,
Width .82 e Diffusion .70; WILD mantém Progressive e os endpoints M9; BLOOM
continua sem habilitar Shimmer automaticamente. Não houve ajuste no core/wrapper:
seu teto `.95+.10*W`, proteção por chaos, Freeze e Q31 permanecem existentes.
Assim, Feedback MacroEngine=1.05 não significa ganho de loop=1.05.

## Manual authority

Medida real após 100 updates de 128 samples, smoothing de 8 ms existente.
Tolerância `1e-6*max(1,abs(manual))`, para acomodar APVTS/skew e precisão float.
O target, antes do smoothing, é o valor APVTS sem compressão quando macros=0.

| Parameter | manual | effective macros=0 | PASS/FAIL |
| --- | --- | --- | --- |
| feedback | 0 | 1.9791e-15 | PASS |
| feedback | 0.2625 | 0.2625 | PASS |
| feedback | 0.525 | 0.525 | PASS |
| feedback | 0.7875 | 0.7875 | PASS |
| feedback | 1.05 | 1.05 | PASS |
| feedback | 0.2 | 0.2 | PASS |
| feedback | 0.7 | 0.7 | PASS |
| feedback | 0.85 | 0.85 | PASS |
| feedback | 0.9 | 0.9 | PASS |
| feedback | 1.05 | 1.05 | PASS |
| wetWidth | 0 | 2.00294e-15 | PASS |
| wetWidth | 0.25 | 0.25 | PASS |
| wetWidth | 0.5 | 0.5 | PASS |
| wetWidth | 0.75 | 0.75 | PASS |
| wetWidth | 1 | 1 | PASS |
| shimmerAmount | 0 | 1.4013e-45 | PASS |
| shimmerAmount | 0.25 | 0.25 | PASS |
| shimmerAmount | 0.5 | 0.5 | PASS |
| shimmerAmount | 0.75 | 0.75 | PASS |
| shimmerAmount | 1 | 1 | PASS |
| duckAmount | 0 | 3.33824e-16 | PASS |
| duckAmount | 0.25 | 0.25 | PASS |
| duckAmount | 0.5 | 0.5 | PASS |
| duckAmount | 0.75 | 0.75 | PASS |
| duckAmount | 1 | 1 | PASS |
| diffusion | 0 | 1.33529e-15 | PASS |
| diffusion | 0.25 | 0.25 | PASS |
| diffusion | 0.5 | 0.5 | PASS |
| diffusion | 0.75 | 0.75 | PASS |
| diffusion | 1 | 1 | PASS |
| modDepth | 0 | 3.33824e-16 | PASS |
| modDepth | 0.25 | 0.25 | PASS |
| modDepth | 0.5 | 0.5 | PASS |
| modDepth | 0.75 | 0.75 | PASS |
| modDepth | 1 | 1 | PASS |
| chaos | 0 | 1.4013e-45 | PASS |
| chaos | 0.25 | 0.25 | PASS |
| chaos | 0.5 | 0.5 | PASS |
| chaos | 0.75 | 0.75 | PASS |
| chaos | 1 | 1 | PASS |
| wobble | 0 | 3.33824e-16 | PASS |
| wobble | 0.25 | 0.25 | PASS |
| wobble | 0.5 | 0.5 | PASS |
| wobble | 0.75 | 0.75 | PASS |
| wobble | 1 | 1 | PASS |
| modRateHz | 0.02 | 0.02 | PASS |
| modRateHz | 0.515 | 0.515 | PASS |
| modRateHz | 1.01 | 1.01 | PASS |
| modRateHz | 1.505 | 1.505 | PASS |
| modRateHz | 2 | 2 | PASS |
| highCutHz | 1000 | 1000 | PASS |
| highCutHz | 5250 | 5250 | PASS |
| highCutHz | 9500 | 9500 | PASS |
| highCutHz | 13750 | 13750 | PASS |
| highCutHz | 18000 | 18000 | PASS |
| lowCutHz | 20 | 20 | PASS |
| lowCutHz | 265 | 265 | PASS |
| lowCutHz | 510 | 510 | PASS |
| lowCutHz | 755 | 755 | PASS |
| lowCutHz | 1000 | 1000 | PASS |
| timeMs | 10 | 10 | PASS |
| timeMs | 507.5 | 507.5 | PASS |
| timeMs | 1005 | 1005 | PASS |
| timeMs | 1502.5 | 1502.5 | PASS |
| timeMs | 2000 | 2000 | PASS |
| mix | 0 | 1.16838e-15 | PASS |
| mix | 0.25 | 0.25 | PASS |
| mix | 0.5 | 0.5 | PASS |
| mix | 0.75 | 0.75 | PASS |
| mix | 1 | 1 | PASS |


## Feedback interaction

Superfície real manual × WILD × BLOOM; valores em unidades físicas.

| Manual | WILD | BLOOM | Effective Feedback |
| --- | --- | --- | --- |
| 0.2 | 0 | 0 | 0.2 |
| 0.2 | 0 | 0.5 | 0.307143 |
| 0.2 | 0 | 1 | 0.414286 |
| 0.2 | 0.5 | 0 | 0.244 |
| 0.2 | 0.5 | 0.5 | 0.347714 |
| 0.2 | 0.5 | 1 | 0.451429 |
| 0.2 | 1 | 0 | 0.290286 |
| 0.2 | 1 | 0.5 | 0.390245 |
| 0.2 | 1 | 1 | 0.490204 |
| 0.45 | 0 | 0 | 0.45 |
| 0.45 | 0 | 0.5 | 0.521429 |
| 0.45 | 0 | 1 | 0.592857 |
| 0.45 | 0.5 | 0 | 0.479714 |
| 0.45 | 0.5 | 0.5 | 0.549755 |
| 0.45 | 0.5 | 1 | 0.619796 |
| 0.45 | 1 | 0 | 0.511714 |
| 0.45 | 1 | 0.5 | 0.580041 |
| 0.45 | 1 | 1 | 0.648367 |
| 0.7 | 0 | 0 | 0.7 |
| 0.7 | 0 | 0.5 | 0.735714 |
| 0.7 | 0 | 1 | 0.771429 |
| 0.7 | 0.5 | 0 | 0.715429 |
| 0.7 | 0.5 | 0.5 | 0.751796 |
| 0.7 | 0.5 | 1 | 0.788163 |
| 0.7 | 1 | 0 | 0.733143 |
| 0.7 | 1 | 0.5 | 0.769837 |
| 0.7 | 1 | 1 | 0.806531 |
| 0.9 | 0 | 0 | 0.9 |
| 0.9 | 0 | 0.5 | 0.907143 |
| 0.9 | 0 | 1 | 0.914286 |
| 0.9 | 0.5 | 0 | 0.904 |
| 0.9 | 0.5 | 0.5 | 0.913428 |
| 0.9 | 0.5 | 1 | 0.922857 |
| 0.9 | 1 | 0 | 0.910286 |
| 0.9 | 1 | 0.5 | 0.921673 |
| 0.9 | 1 | 1 | 0.933061 |
| 1.05 | 0 | 0 | 1.05 |
| 1.05 | 0 | 0.5 | 1.05 |
| 1.05 | 0 | 1 | 1.05 |
| 1.05 | 0.5 | 0 | 1.05 |
| 1.05 | 0.5 | 0.5 | 1.05 |
| 1.05 | 0.5 | 1 | 1.05 |
| 1.05 | 1 | 0 | 1.05 |
| 1.05 | 1 | 0.5 | 1.05 |
| 1.05 | 1 | 1 | 1.05 |


## Presets — M9 vs M9.1

Nenhum valor de preset foi editado. ΔRMS positivo indica M9.1 mais alto.
Também é exibida a referência pré-M9 para evitar confundir preservação do voicing
original com preservação dos desvios acidentais da M9.

| Preset | Source | ΔRMS M9→M9.1 dB | Peak M9→M9.1 | Tail energy M9→M9.1 | ΔRMS pré-M9: M9→M9.1 dB | Tail Δ pré-M9 |
| --- | --- | --- | --- | --- | --- | --- |
| 0 Low Orbit | 1 | +0.212 | 0.103773 → 0.103566 | 0.000221329 → 0.000140745 | -0.229 → -0.017 | +2.3% |
| 0 Low Orbit | 2 | -0.791 | 0.189163 → 0.189149 | 6.07931e-06 → 5.38219e-06 | +0.773 → -0.018 | +3.2% |
| 1 Tidal Lock | 1 | -2.595 | 0.159069 → 0.103667 | 0.00116099 → 0.000923778 | +2.672 → +0.077 | -5.4% |
| 1 Tidal Lock | 2 | +0.194 | 0.174228 → 0.182853 | 1.09524e-05 → 2.16353e-05 | -0.103 → +0.091 | -4.4% |
| 2 Trade Winds | 1 | -0.318 | 0.100295 → 0.0993967 | 0.000229977 → 0.000377127 | +0.394 → +0.077 | -4.7% |
| 2 Trade Winds | 2 | +0.031 | 0.180437 → 0.180459 | 6.56334e-06 → 6.57225e-06 | -0.038 → -0.007 | -2.5% |
| 3 Solar Wind | 1 | +2.914 | 0.0970437 → 0.142746 | 0.000626225 → 0.00132035 | -2.968 → -0.054 | -13.0% |
| 3 Solar Wind | 2 | +0.002 | 0.180339 → 0.180336 | 1.47351e-05 → 1.49326e-05 | +0.002 → +0.004 | -4.5% |
| 4 Escape Velocity | 1 | +0.234 | 0.102386 → 0.102592 | 0.000432669 → 0.000521912 | -0.235 → -0.001 | +3.5% |
| 4 Escape Velocity | 2 | +0.052 | 0.18491 → 0.184909 | 1.338e-05 → 1.84123e-05 | -0.049 → +0.003 | -2.6% |
| 5 Zero-G | 1 | -0.066 | 0.137804 → 0.140169 | 0.00352109 → 0.00380293 | -0.414 → -0.480 | -18.7% |
| 5 Zero-G | 2 | +0.117 | 0.178831 → 0.181363 | 4.80239e-05 → 7.87198e-05 | +0.243 → +0.360 | -15.7% |
| 6 Event Horizon | 1 | +0.721 | 0.145773 → 0.197425 | 0.00456923 → 0.00648029 | -0.492 → +0.229 | -18.6% |
| 6 Event Horizon | 2 | +0.341 | 0.172242 → 0.187187 | 7.82924e-05 → 0.000125041 | +0.806 → +1.147 | -17.7% |
| 7 Glass Transit | 1 | +2.334 | 0.0933509 → 0.108804 | 0.000592313 → 0.000485832 | -2.253 → +0.081 | -0.1% |
| 7 Glass Transit | 2 | +0.118 | 0.172926 → 0.175307 | 8.29425e-06 → 8.56711e-06 | -0.029 → +0.090 | +1.0% |
| 8 Cassette Moon | 1 | -0.044 | 0.147103 → 0.145775 | 0.000407898 → 0.000398887 | -0.063 → -0.107 | -18.3% |
| 8 Cassette Moon | 2 | +0.061 | 0.194426 → 0.194346 | 6.4883e-06 → 6.67711e-06 | -0.030 → +0.031 | -8.2% |
| 9 Pulsar Eighths | 1 | -0.010 | 0.169293 → 0.168957 | 0.000348505 → 0.00034487 | -0.153 → -0.163 | -17.4% |
| 9 Pulsar Eighths | 2 | -0.002 | 0.191628 → 0.191584 | 8.15055e-07 → 7.98496e-07 | +0.009 → +0.007 | -30.1% |
| 10 Frozen Choir | 1 | +3.162 | 0.0916491 → 0.1631 | 0.00187209 → 0.00397552 | -2.961 → +0.201 | -16.5% |
| 10 Frozen Choir | 2 | +0.318 | 0.151349 → 0.151351 | 4.7087e-05 → 4.95145e-05 | +0.589 → +0.907 | -15.9% |
| 11 Dark Matter | 1 | +1.906 | 0.0898609 → 0.140541 | 0.00102067 → 0.00209445 | -1.625 → +0.280 | -18.2% |
| 11 Dark Matter | 2 | +0.139 | 0.16734 → 0.170432 | 2.03213e-05 → 3.38505e-05 | +0.459 → +0.598 | -14.3% |
| 12 Microgravity Slap | 1 | +0.916 | 0.120692 → 0.144435 | 4.20417e-05 → 6.95937e-05 | -0.926 → -0.010 | -0.7% |
| 12 Microgravity Slap | 2 | -0.010 | 0.201067 → 0.201172 | 2.73901e-08 → 1.90064e-08 | +0.001 → -0.009 | -0.7% |


## Causas medidas, antes da correção de Time

Counterfactuals offline: após corrigir Feedback, mas antes de preservar Time,
restaurou-se um grupo de alvos antigo por vez através de inversão da curva APVTS
atual. Macros enviados ao core e demais alvos foram mantidos. Não são novos
presets nem compensações do plugin. O CSV wanted/reached identifica alvos
inatingíveis na curva corrente; esses resultados são aproximações, não restores
exatos. SPACE Time/Tone inclui Time, Low Cut e High Cut; não atribui sozinho toda
a diferença ao tempo. A intervenção final mudou exclusivamente Time e confirmou
a causa em renders do plugin. Os efeitos isolados não podem ser somados.

| Preset | Restored group | ΔRMS vs feedback-only dB | Tail Δ |
| --- | --- | --- | --- |
| 0 | wild_feedback | -0.027 | +1.1% |
| 0 | bloom_feedback | +0.031 | -1.3% |
| 0 | space_shimmer | -0.000 | -0.0% |
| 0 | bloom_shimmer | -0.000 | -0.0% |
| 0 | space_spatial | +0.018 | -1.8% |
| 0 | wild_modulation | -0.000 | -0.1% |
| 0 | space_time_tone | +0.206 | -36.2% |
| 0 | bloom_tone_mix | +0.016 | -0.4% |
| 1 | wild_feedback | +0.044 | +7.2% |
| 1 | bloom_feedback | -0.030 | -4.8% |
| 1 | space_shimmer | +0.000 | +0.0% |
| 1 | bloom_shimmer | +0.000 | +0.0% |
| 1 | space_spatial | +0.000 | -0.6% |
| 1 | wild_modulation | +0.001 | +0.0% |
| 1 | space_time_tone | -2.586 | -19.2% |
| 1 | bloom_tone_mix | -0.022 | +4.3% |
| 2 | wild_feedback | -0.018 | -0.6% |
| 2 | bloom_feedback | +0.006 | +0.3% |
| 2 | space_shimmer | +0.000 | +0.0% |
| 2 | bloom_shimmer | +0.000 | +0.0% |
| 2 | space_spatial | -0.006 | -1.5% |
| 2 | wild_modulation | -0.004 | -0.1% |
| 2 | space_time_tone | -0.319 | +63.9% |
| 2 | bloom_tone_mix | +0.000 | +0.0% |
| 3 | wild_feedback | -0.020 | +5.4% |
| 3 | bloom_feedback | +0.006 | -1.0% |
| 3 | space_shimmer | -0.001 | -0.0% |
| 3 | bloom_shimmer | -0.000 | -0.0% |
| 3 | space_spatial | +0.011 | -2.2% |
| 3 | wild_modulation | +0.001 | +0.1% |
| 3 | space_time_tone | +2.912 | +111.8% |
| 3 | bloom_tone_mix | -0.001 | +0.2% |
| 5 | wild_feedback | +0.067 | +8.0% |
| 5 | bloom_feedback | -0.015 | -1.8% |
| 5 | space_shimmer | -0.000 | +0.0% |
| 5 | bloom_shimmer | -0.000 | +0.0% |
| 5 | space_spatial | +0.006 | +0.0% |
| 5 | wild_modulation | +0.002 | +0.0% |
| 5 | space_time_tone | +0.486 | +9.1% |
| 5 | bloom_tone_mix | -0.157 | +13.2% |
| 6 | wild_feedback | +0.027 | +1.7% |
| 6 | bloom_feedback | -0.013 | -0.5% |
| 6 | space_shimmer | +0.001 | +0.0% |
| 6 | bloom_shimmer | +0.000 | +0.0% |
| 6 | space_spatial | +0.021 | +0.6% |
| 6 | wild_modulation | +0.014 | +0.4% |
| 6 | space_time_tone | +0.694 | +39.9% |
| 6 | bloom_tone_mix | -0.498 | +19.9% |
| 7 | wild_feedback | -0.030 | +0.9% |
| 7 | bloom_feedback | +0.039 | -1.2% |
| 7 | space_shimmer | -0.001 | -0.0% |
| 7 | bloom_shimmer | -0.000 | -0.0% |
| 7 | space_spatial | +0.005 | -1.8% |
| 7 | wild_modulation | -0.000 | -0.0% |
| 7 | space_time_tone | +2.324 | -17.7% |
| 7 | bloom_tone_mix | -0.106 | +2.0% |
| 10 | wild_feedback | +0.002 | -0.1% |
| 10 | bloom_feedback | -0.001 | +0.1% |
| 10 | space_shimmer | -0.000 | -0.1% |
| 10 | bloom_shimmer | -0.000 | -0.0% |
| 10 | space_spatial | +0.001 | +0.0% |
| 10 | wild_modulation | -0.002 | -0.1% |
| 10 | space_time_tone | +3.162 | +112.2% |
| 10 | bloom_tone_mix | -0.528 | +17.9% |
| 11 | wild_feedback | +0.012 | +2.2% |
| 11 | bloom_feedback | -0.001 | -0.3% |
| 11 | space_shimmer | -0.000 | -0.0% |
| 11 | bloom_shimmer | -0.000 | -0.0% |
| 11 | space_spatial | +0.004 | +0.0% |
| 11 | wild_modulation | +0.009 | +0.6% |
| 11 | space_time_tone | +1.906 | +100.0% |
| 11 | bloom_tone_mix | -0.647 | +14.4% |
| 12 | wild_feedback | +0.003 | +1.4% |
| 12 | bloom_feedback | -0.001 | -0.4% |
| 12 | space_shimmer | +0.000 | +0.0% |
| 12 | bloom_shimmer | +0.000 | +0.0% |
| 12 | space_spatial | +0.001 | +0.1% |
| 12 | wild_modulation | -0.000 | -0.0% |
| 12 | space_time_tone | +0.913 | +65.4% |
| 12 | bloom_tone_mix | +0.007 | -0.4% |


Alvos limitados: 18/390 medições; detalhe em `counterfactual_targets.csv`.


### Divergências residuais e decisão musical

No harmônico, todos os 13 presets agora estão abaixo de |1 dB| contra pré-M9,
e todas as alterações de tail energy estão abaixo de 30%. Os índices 1/3/7/10/11
voltaram próximos ao voicing original sem reativar Bloom→Shimmer ou aumentar
regeneração de WILD. Reduzir Shimmer/modulação/spatial não explicou os desvios
grandes; restaurar Time/Tone os explicou, e mudar só Time os resolveu.

Na percussão, permanecem duas exceções documentadas, sem compensação por preset:

- Event Horizon (6): o Mix alto com Bloom usa margem, conservando mais ataque dry
  que a soma antiga. O counterfactual Bloom Tone/Mix diminui o RMS percussivo em
  cerca de 1.31 dB; restaurar WILD Feedback/modulação muda o RMS muito pouco.
  Elevar Mix global para compensar esse único preset mascararia ataques nos
  demais e reduziria a autoridade manual aprovada na M9. Mantém-se esse resultado.
- Pulsar Eighths (9): tail energy percussiva fica cerca de 30% menor. O restore
  WILD Feedback eleva sua tail em cerca de 44%; tempo sincronizado não muda no
  counterfactual SPACE Time/Tone. A redução resulta da regeneração moderada M9,
  desejada para WILD não substituir Feedback. Não se restaura globalmente a soma
  agressiva antiga só para recuperar esse percentual de cauda.

Os números exatos e os picos das duas fontes estão na tabela acima. Os limites
de 1 dB/30% são referências de auditoria, não gates para forçar voicing.

## WILD — 0 / 50 / 75 / 100

Endpoints e Progressive não mudaram. Apenas a margem de sua contribuição de
Feedback foi corrigida. Instability mede o controle; FFT e derivada são proxies
do áudio. WILD alto não exige aumento monotônico de RMS ou tail energy.

| WILD | Feedback | Chaos | Wobble | Rate Hz | Depth | Instability |
| --- | --- | --- | --- | --- | --- | --- |
| 0 | 0.45 | 0 | 0.1 | 0.15 | 0.1 | 0.02 |
| 0.5 | 0.479714 | 0.243 | 0.26875 | 0.367423 | 0.285625 | 0.3638 |
| 0.75 | 0.495429 | 0.45225 | 0.414062 | 0.575049 | 0.445469 | 0.5786 |
| 1 | 0.511714 | 0.72 | 0.6 | 0.9 | 0.65 | 0.822 |

| Source | WILD % | Modulation proxy M9→M9.1 | Pitch variance proxy M9→M9.1 | Width M9→M9.1 | Tail energy M9→M9.1 | Wave delta vs previous level |
| --- | --- | --- | --- | --- | --- | --- |
| 0 | 0 | 0.000699821 → 0.000699821 | 853673 → 853673 | 0.0298666 → 0.0298666 | 4.22192e-12 → 4.22192e-12 | 0 |
| 0 | 50 | 0.000700008 → 0.000699958 | 2.19734e+06 → 873640 | 0.0298876 → 0.0299763 | 4.28756e-12 → 4.27436e-12 | 0.116153 |
| 0 | 75 | 0.000700075 → 0.000700033 | 1.22686e+06 → 1.21901e+06 | 0.0322487 → 0.0322027 | 4.36985e-12 → 4.33405e-12 | 0.35199 |
| 0 | 100 | 0.000700391 → 0.000700342 | 1.41046e+06 → 1.40423e+06 | 0.0395881 → 0.039525 | 4.43237e-12 → 4.38432e-12 | 0.295586 |
| 1 | 0 | 0.00101632 → 0.00101632 | 31.7627 → 31.7627 | 0.0136087 → 0.0136087 | 0.000114711 → 0.000114711 | 0 |
| 1 | 50 | 0.00101448 → 0.00101473 | 43.5682 → 44.1041 | 0.01373 → 0.0137319 | 0.000113907 → 0.000113987 | 0.0117812 |
| 1 | 75 | 0.00101497 → 0.00101538 | 53.5369 → 53.5369 | 0.0147551 → 0.0147562 | 0.000113487 → 0.000113591 | 0.0189665 |
| 1 | 100 | 0.00101899 → 0.00101937 | 3666.34 → 3666.34 | 0.0202737 → 0.0202746 | 0.000108685 → 0.000108734 | 0.0424095 |
| 2 | 0 | 0.000221569 → 0.000221569 | 24385 → 24385 | 0.0112125 → 0.0112125 | 6.21086e-07 → 6.21086e-07 | 0 |
| 2 | 50 | 0.000221611 → 0.000221601 | 39435.8 → 35906.6 | 0.0114282 → 0.0114142 | 6.89493e-07 → 6.79318e-07 | 0.00452889 |
| 2 | 75 | 0.000221643 → 0.00022163 | 81673.9 → 44946.8 | 0.0124779 → 0.012458 | 7.31469e-07 → 7.17937e-07 | 0.00539583 |
| 2 | 100 | 0.00022177 → 0.000221743 | 110667 → 75789.3 | 0.0179783 → 0.017949 | 8.32416e-07 → 8.16921e-07 | 0.0139337 |
| 3 | 0 | 0.000582561 → 0.000582561 | 468.025 → 468.025 | 0.0194053 → 0.0194053 | 5.00776e-05 → 5.00776e-05 | 0 |
| 3 | 50 | 0.000582585 → 0.000582556 | 453.993 → 453.993 | 0.0198847 → 0.0198436 | 5.08323e-05 → 5.06991e-05 | 0.00657281 |
| 3 | 75 | 0.00058191 → 0.000581869 | 410.951 → 410.951 | 0.021651 → 0.021591 | 5.06501e-05 → 5.04475e-05 | 0.00846852 |
| 3 | 100 | 0.000577952 → 0.000577943 | 439.321 → 452.137 | 0.0304944 → 0.0303945 | 4.76401e-05 → 4.74562e-05 | 0.0241221 |
| 4 | 0 | 0.000774439 → 0.000774439 | 296.021 → 296.021 | 0.0132781 → 0.0132781 | 9.26754e-05 → 9.26754e-05 | 0 |
| 4 | 50 | 0.000774778 → 0.00077472 | 307.16 → 305.484 | 0.0133745 → 0.0133785 | 9.15838e-05 → 9.16964e-05 | 0.00814445 |
| 4 | 75 | 0.000775065 → 0.000774979 | 310.305 → 310.305 | 0.0143107 → 0.0143169 | 9.08039e-05 → 9.0905e-05 | 0.0132792 |
| 4 | 100 | 0.000773582 → 0.00077349 | 450.721 → 447.177 | 0.0193424 → 0.0193516 | 8.56268e-05 → 8.57347e-05 | 0.0311088 |


A diferença relativa 75→100 permanece mensurável nas cinco fontes; o arquivo
`wild_waveform_comparison.csv` também compara M9/M9.1 diretamente. Não foi
necessário recalibrar WILD. A interpretação sutil/vivo/experimental ainda exige
escuta, disponível no A/B abaixo; não se afirma aprovação perceptiva por FFT.

## Tests

MacroCalibrationTest mantém ranges, monotonicidade útil, headroom manual,
superfícies Duck/Feedback, estabilidade das interações e todos os presets.
Acrescenta identidade explícita macros=0 para 13 controles, extremos e cinco
valores obrigatórios de Feedback; 45 combinações manual×WILD×BLOOM; e preservação
de Time com margem, com monotonicidade numérica perto do limite.

```text
Test project C:/progs/vst/te2350/build/te2350-vst-ninja-net
      Start  3: TE2350GoldenReferenceRender
      Start  2: TE2350OfflineReferenceRender
 1/14 Test  #2: TE2350OfflineReferenceRender .....   Passed    0.08 sec
      Start  1: TE2350MacroCalibrationTest
 2/14 Test  #3: TE2350GoldenReferenceRender ......   Passed    0.42 sec
      Start 14: TE2350FreezeConsistencyTest
 3/14 Test #14: TE2350FreezeConsistencyTest ......   Passed   31.58 sec
      Start 13: TE2350MusicalBehaviourTest
 4/14 Test #13: TE2350MusicalBehaviourTest .......   Passed    9.86 sec
      Start  7: TE2350CalibrationTest
 5/14 Test  #7: TE2350CalibrationTest ............   Passed    3.07 sec
      Start 10: TE2350UIRenderTest
 6/14 Test #10: TE2350UIRenderTest ...............   Passed    1.54 sec
      Start 12: TE2350ReleaseReadinessTest
 7/14 Test #12: TE2350ReleaseReadinessTest .......   Passed    0.57 sec
      Start  8: TE2350HostCompatibilityTest
 8/14 Test  #8: TE2350HostCompatibilityTest ......   Passed    0.73 sec
      Start 11: TE2350PresetWorkflowTest
 9/14 Test #11: TE2350PresetWorkflowTest .........   Passed    0.39 sec
      Start  9: TE2350VST3LoadTest
10/14 Test  #9: TE2350VST3LoadTest ...............   Passed    0.12 sec
      Start  6: TE2350PluginSmokeTest
11/14 Test  #6: TE2350PluginSmokeTest ............   Passed    0.39 sec
      Start  5: TE2350CoreControlTest
12/14 Test  #5: TE2350CoreControlTest ............   Passed    0.55 sec
      Start  4: TE2350GoldenReferenceCompare
13/14 Test  #4: TE2350GoldenReferenceCompare .....   Passed    0.05 sec
14/14 Test  #1: TE2350MacroCalibrationTest .......   Passed  160.84 sec

100% tests passed, 0 tests failed out of 14

Total Test time (real) = 160.94 sec

```


## Compatibility

- ParameterIDs unchanged.
- Ranges unchanged, incluindo Feedback 0..1.05.
- Defaults unchanged.
- Preset data unchanged.
- Automation unchanged.
- State recall unchanged: valores brutos e versão de estado preservados.

HostCompatibility/PresetWorkflow/ReleaseReadiness/VST3Load verificam carga e
recall; a resposta de áudio recalibrada pode mudar intencionalmente. Nenhuma
referência golden foi atualizada. Golden compara o core C, não os macros da UI.
MacroEngine continua em control/block rate: apenas aritmética escalar adicional,
sem allocations, locks ou filesystem no realtime. As variações e FFTs deste
harness são exclusivamente offline.

## Artefatos e reprodução

- CSVs e log completos em `docs/M91`.
- A/B M9/M9.1 sem normalização: `build/m91/listen.html`; cinco fontes WILD
  0/50/75/100 e todos os presets harmônico/percussão.
- Executável M9 preservado localmente: `build/m91/MacroCalibration-M9.exe`.
- Render foco: `TE2350MacroCalibrationTest.exe <output-dir> focus`.
- Diagnóstico: `TE2350MacroCalibrationTest.exe <output-dir> diagnostic`.
  Os CSVs diagnósticos entregues foram medidos no estágio Feedback-only, antes
  da correção de Time; executar no final produz novos counterfactuals contra
  o estado final. Consulte `feedback_only_metrics.csv` para a base correta.
- Suíte: `ctest --test-dir build/te2350-vst-ninja-net --output-on-failure -j 2`.
- Relatório: `python te2350-vst/Tests/manual_authority_report.py`.
  Também funciona pelos CSVs versionados sem os WAVs/logs de build; nesse caso
  preserva a medição de waveform entregue e não refaz a comparação de WAVs.

Implementação e critérios objetivos verificados. A/B foi produzido, sem
normalização; a aprovação perceptiva final continua dependendo de escuta.
