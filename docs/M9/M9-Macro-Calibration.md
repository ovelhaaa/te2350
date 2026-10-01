# M9 — Macro Musical Calibration & Interaction Matrix

## Método e alcance

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

| Macro | Target | value@0 | value@25 | value@50 | value@75 | value@100 | curve | clamp | effective default 0/25/50/75/100 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| space | timeMs | 420 | 531.855 | 673.498 | 852.865 | 1080 | Log | 10..2000 | 420 / 531.855 / 673.498 / 852.865 / 1080 |
| space | lowCutHz | 80 | 92.0131 | 105.83 | 121.722 | 140 | Log | 20..1000 | 80 / 92.0131 / 105.83 / 121.722 / 140 |
| space | highCutHz | 9000 | 7846.62 | 6841.05 | 5964.35 | 5200 | Log | 1000..18000 | 9000 / 7846.62 / 6841.05 / 5964.35 / 5200 |
| space | diffusion | 0.4 | 0.495 | 0.59 | 0.685 | 0.78 | Linear | 0..1 | 0.4 / 0.495 / 0.59 / 0.685 / 0.78 |
| space | shimmerAmount | 0 | 0.08 | 0.16 | 0.24 | 0.32 | Linear | 0..1 | 1.06824e-15 / 0.08 / 0.16 / 0.24 / 0.32 |
| space | wetWidth | 0.6 | 0.6875 | 0.775 | 0.8625 | 0.95 | Linear | 0..1 | 0.6 / 0.6875 / 0.775 / 0.8625 / 0.95 |
| wild | feedback | 0.45 | 0.6 | 0.75 | 0.9 | 1.05 | Linear | 0..1.05 + teto .95+.10W | 0.45 / 0.6 / 0.75 / 0.9 / 1.05 |
| wild | chaos | 0 | 0.053125 | 0.2125 | 0.478125 | 0.85 | Exponential | 0..1 | 2.8375e-15 / 0.053125 / 0.2125 / 0.478125 / 0.85 |
| wild | wobble | 0.1 | 0.146875 | 0.2875 | 0.521875 | 0.85 | Exponential | 0..1 | 0.1 / 0.146875 / 0.2875 / 0.521875 / 0.85 |
| wild | modRateHz | 0.15 | 0.252269 | 0.424264 | 0.713524 | 1.2 | Log | .02..2 | 0.15 / 0.252269 / 0.424264 / 0.713524 / 1.2 |
| wild | modDepth | 0.1 | 0.146875 | 0.2875 | 0.521875 | 0.85 | Exponential | 0..1 | 0.1 / 0.146875 / 0.2875 / 0.521875 / 0.85 |
| bloom | highCutHz | 9000 | 10051.1 | 11225 | 12535.9 | 14000 | Log | 1000..18000 | 9000 / 10051.1 / 11225 / 12535.9 / 14000 |
| bloom | duckAmount | 0.1 | 0.128125 | 0.2125 | 0.353125 | 0.55 | Exponential | 0..1; headroom | 0.1 / 0.125312 / 0.20125 / 0.327812 / 0.505 |
| bloom | shimmerAmount | 0 | 0.045 | 0.09 | 0.135 | 0.18 | Linear | 0..1 | 6.00883e-16 / 0.045 / 0.09 / 0.135 / 0.18 |
| bloom | mix | 0.35 | 0.3925 | 0.435 | 0.4775 | 0.52 | Linear | 0..1 | 0.35 / 0.3925 / 0.435 / 0.4775 / 0.52 |


Além das definições: WILD controla o teto do wrapper (.95+.10W) e o instability meter; não há outro coeficiente WILD aplicado diretamente ao core C;
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

| Macro | Target | Manual default | effective@0 | @25 | @50 | @75 | @100 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| space | timeMs | 420 | 420 | 531.855 | 673.498 | 852.865 | 1080 |
| space | lowCutHz | 80 | 80 | 92.0131 | 105.83 | 121.722 | 140 |
| space | highCutHz | 9000 | 9000 | 7846.62 | 6841.05 | 5964.35 | 5200 |
| space | diffusion | 0.4 | 0.4 | 0.475 | 0.55 | 0.625 | 0.7 |
| space | shimmerAmount | 0 | 2.67059e-16 | 0.02 | 0.04 | 0.06 | 0.08 |
| space | wetWidth | 0.6 | 0.6 | 0.655 | 0.71 | 0.765 | 0.82 |
| wild | feedback | 0.45 | 0.45 | 0.467143 | 0.484286 | 0.501428 | 0.518571 |
| wild | chaos | 0 | 2.40353e-15 | 0.09225 | 0.243 | 0.45225 | 0.72 |
| wild | wobble | 0.1 | 0.1 | 0.164062 | 0.26875 | 0.414062 | 0.6 |
| wild | modRateHz | 0.15 | 0.15 | 0.234763 | 0.367423 | 0.575049 | 0.9 |
| wild | modDepth | 0.1 | 0.1 | 0.170469 | 0.285625 | 0.445469 | 0.65 |
| bloom | feedback | 0.45 | 0.45 | 0.492857 | 0.535714 | 0.578571 | 0.621429 |
| bloom | highCutHz | 9000 | 9000 | 10051.1 | 11225 | 12535.9 | 14000 |
| bloom | duckAmount | 0.1 | 0.1 | 0.125312 | 0.20125 | 0.327812 | 0.505 |
| bloom | mix | 0.35 | 0.35 | 0.3925 | 0.435 | 0.4775 | 0.52 |


Exemplo pedido: BLOOM .75 + Duck manual .60 → `.60 + .45*.75²*(1-.60) = .70125`.


## Metrics — antes/depois

Fonte harmônica, mesmos ganhos. Energia de cauda em amplitude²·s.


| Caso | RMS | Peak | Tail energy | Width | Centroid Hz | Decay proxy s |
| --- | --- | --- | --- | --- | --- | --- |
| space 0 | 0.0284634 → 0.0284634 | 0.103225 → 0.103225 | 0.000114711 → 0.000114711 | 0.0136087 → 0.0136087 | 254.438 → 254.438 | 4.3 → 4.3 |
| space 50 | 0.0359283 → 0.0360072 | 0.11461 → 0.114873 | 0.000214693 → 0.000219329 | 0.0214945 → 0.0193271 | 235.481 → 235.444 | 4.4 → 4.4 |
| space 100 | 0.0275155 → 0.0274584 | 0.101097 → 0.100989 | 0.000257317 → 0.00026797 | 0.0297905 → 0.024999 | 264.28 → 264.759 | 5.2 → 5.2 |
| wild 0 | 0.0284634 → 0.0284634 | 0.103225 → 0.103225 | 0.000114711 → 0.000114711 | 0.0136087 → 0.0136087 | 254.438 → 254.438 | 4.3 → 4.3 |
| wild 50 | 0.0284229 → 0.0284553 | 0.104571 → 0.103251 | 0.000114948 → 0.000113907 | 0.0137007 → 0.01373 | 251.561 → 254.102 | 4.7 → 4.3 |
| wild 100 | 0.0284665 → 0.0284665 | 0.106774 → 0.103115 | 0.000117007 → 0.000108685 | 0.0244972 → 0.0202737 | 251.888 → 254.983 | 5.2 → 4.3 |
| bloom 0 | 0.0284634 → 0.0284634 | 0.103225 → 0.103225 | 0.000114711 → 0.000114711 | 0.0136087 → 0.0136087 | 254.438 → 254.438 | 4.3 → 4.3 |
| bloom 50 | 0.0254062 → 0.0253345 | 0.0954593 → 0.0948187 | 0.000151835 → 0.000143655 | 0.0171344 → 0.016847 | 257.916 → 255.452 | 4.3 → 4.6 |
| bloom 100 | 0.0227222 → 0.0226094 | 0.0893983 → 0.0888033 | 0.000181361 → 0.000176537 | 0.020053 → 0.020007 | 259.442 → 256.108 | 4.7 → 4.8 |


### Factory presets — diferenças intencionais de áudio

Mesmos valores e sinal harmônico. ΔRMS = 20log10(after/before); nenhum preset foi reescrito.


| Preset index | Peak before → after | ΔRMS dB | Tail energy before → after |
| --- | --- | --- | --- |
| 0 | 0.103426 → 0.103773 | -0.229 | 0.000137598 → 0.000221329 |
| 1 | 0.101846 → 0.159069 | +2.672 | 0.000976874 → 0.00116099 |
| 2 | 0.0991419 → 0.100295 | +0.394 | 0.000395744 → 0.000229977 |
| 3 | 0.145132 → 0.0970437 | -2.968 | 0.00151691 → 0.000626225 |
| 4 | 0.100999 → 0.102386 | -0.235 | 0.000504375 → 0.000432669 |
| 5 | 0.173259 → 0.137804 | -0.414 | 0.00468025 → 0.00352109 |
| 6 | 0.197039 → 0.145773 | -0.492 | 0.00796253 → 0.00456923 |
| 7 | 0.107939 → 0.0933509 | -2.253 | 0.000486085 → 0.000592313 |
| 8 | 0.149477 → 0.147103 | -0.063 | 0.000488499 → 0.000407898 |
| 9 | 0.175345 → 0.169293 | -0.153 | 0.000417418 → 0.000348505 |
| 10 | 0.160658 → 0.0916491 | -2.961 | 0.00476246 → 0.00187209 |
| 11 | 0.142628 → 0.0898609 | -1.625 | 0.0025617 → 0.00102067 |
| 12 | 0.144701 → 0.120692 | -0.926 | 7.00617e-05 → 4.20417e-05 |


## Dead zones — waveform e superfícies

Diferença relativa `sqrt(sum((novo-anterior)²)/sum(anterior²))`, cada +10%.


| Macro | Source | Min delta before | Min delta after | After 0–10/10–20/40–50/50–60/80–90/90–100 |
| --- | --- | --- | --- | --- |
| space | 0 | 0.310196 | 0.309822 | 0.337229 / 0.337215 / 0.322199 / 0.318539 / 0.309822 / 0.316412 |
| space | 1 | 0.162688 | 0.162782 | 0.321322 / 0.162782 / 0.572738 / 0.461274 / 0.582322 / 0.406557 |
| space | 2 | 0.135123 | 0.138566 | 0.186048 / 0.187253 / 0.186124 / 0.169261 / 0.150884 / 0.138566 |
| space | 3 | 0.087337 | 0.0882052 | 0.317515 / 0.294573 / 0.0882052 / 0.192897 / 0.390384 / 0.177743 |
| wild | 0 | 0.0570543 | 0.0675792 | 0.110896 / 0.0675792 / 0.118624 / 0.299014 / 0.35186 / 0.340787 |
| wild | 1 | 0.0120847 | 0.00903515 | 0.00930956 / 0.0091589 / 0.0105612 / 0.0137007 / 0.032996 / 0.042835 |
| wild | 2 | 0.00662599 | 0.00275323 | 0.00320511 / 0.00275323 / 0.00299126 / 0.00431782 / 0.00901036 / 0.00871355 |
| wild | 3 | 0.0100652 | 0.00405494 | 0.00425157 / 0.0041768 / 0.00453873 / 0.00603758 / 0.0158869 / 0.0199662 |
| bloom | 0 | 0.0168134 | 0.0186015 | 0.0186015 / 0.0189322 / 0.0320627 / 0.0406762 / 0.0276688 / 0.0261474 |
| bloom | 1 | 0.0205657 | 0.0214452 | 0.0256206 / 0.0258784 / 0.0273701 / 0.0267329 / 0.0266503 / 0.0214452 |
| bloom | 2 | 0.0146822 | 0.0155596 | 0.0187785 / 0.0190432 / 0.0228348 / 0.0216039 / 0.0203616 / 0.0155596 |
| bloom | 3 | 0.0136549 | 0.0138844 | 0.0188858 / 0.0189133 / 0.0219122 / 0.0196987 / 0.0185822 / 0.0138844 |


O limiar automático é .001 de diferença relativa: nenhum intervalo pode ficar
abaixo de 0.1% nos quatro sinais. Isso detecta identidade numérica, não garante
limiar psicoacústico. Em SPACE o alinhamento do delay muda a energia por fase;
RMS/centroid/width medidos podem oscilar entre passos mesmo com dimensão crescente.
O proxy de dimensão é o alvo time+diffusion+width, junto dos renders. A duração percussiva também é verificada com tolerância de um bin (100 ms). Para BLOOM,
energia pós-ataque harmônica/percussiva/cluster é o proxy robusto; o teste exige ausência de reversão maior que 0.5% por passo e crescimento de ao menos 25% entre endpoints; energia de
impulso muito tardia perto do piso numérico não é usada como monotonicidade absoluta.

## Interaction matrix


- **Space × Bloom**: 50 renders; peak máximo 0.192321, RMS mínimo 0.0106459, |DC| máximo 2.51602e-06.

- **Wild × Feedback**: 40 renders; peak máximo 0.192667, RMS mínimo 0.0130349, |DC| máximo 5.67365e-07.

- **Bloom × Shimmer**: 80 renders; peak máximo 0.192321, RMS mínimo 0.0109711, |DC| máximo 5.83673e-07.

- **Wild × Freeze**: 20 renders; peak máximo 0.192379, RMS mínimo 0.0135193, |DC| máximo 7.39361e-07.

- **Space × Width**: 18 renders; peak máximo 0.192426, RMS mínimo 0.0128158, |DC| máximo 5.65399e-07.

- **Todos os eixos**: 54 renders; peak máximo 0.121666, RMS mínimo 0.00309677, |DC| máximo 1.00232e-06.

- **Manuais com os três macros em 100%**: 100 renders; peak máximo 0.158852, RMS mínimo 0.0106543, |DC| máximo 4.76793e-07.

- **Todos os factory presets**: 26 renders; peak máximo 0.201067, RMS mínimo 0.0100003, |DC| máximo 3.18086e-06.


Freeze: `freeze_windows.csv` compara energia/centroid/HF em 3–5 s e 10–12 s e mede maior salto por sample e DC por janela de 1 s. DC local também permaneceu abaixo de .02.


A grade adicional dos três eixos 3×3×3 usa Kill Dry, para verificar que o efeito wet continua audível sem o dry. WILD instability é verificado a cada 10%.

Space × Bloom: grade 5×5 com WILD=0. Time/diffusion/width continuam exclusivos de
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


```text
Test project C:/progs/vst/te2350/build/te2350-vst-ninja-net
      Start  3: TE2350GoldenReferenceRender
      Start  2: TE2350OfflineReferenceRender
 1/14 Test  #3: TE2350GoldenReferenceRender ......   Passed    0.22 sec
      Start  1: TE2350MacroCalibrationTest
 2/14 Test  #2: TE2350OfflineReferenceRender .....   Passed    0.48 sec
      Start 14: TE2350FreezeConsistencyTest
 3/14 Test #14: TE2350FreezeConsistencyTest ......   Passed   30.22 sec
      Start 13: TE2350MusicalBehaviourTest
 4/14 Test #13: TE2350MusicalBehaviourTest .......   Passed    9.21 sec
      Start 10: TE2350UIRenderTest
 5/14 Test #10: TE2350UIRenderTest ...............   Passed    1.47 sec
      Start  7: TE2350CalibrationTest
 6/14 Test  #7: TE2350CalibrationTest ............   Passed    2.84 sec
      Start 12: TE2350ReleaseReadinessTest
 7/14 Test #12: TE2350ReleaseReadinessTest .......   Passed    1.04 sec
      Start  8: TE2350HostCompatibilityTest
 8/14 Test  #8: TE2350HostCompatibilityTest ......   Passed    0.70 sec
      Start 11: TE2350PresetWorkflowTest
 9/14 Test #11: TE2350PresetWorkflowTest .........   Passed    0.62 sec
      Start  9: TE2350VST3LoadTest
10/14 Test  #9: TE2350VST3LoadTest ...............   Passed    0.37 sec
      Start  6: TE2350PluginSmokeTest
11/14 Test  #6: TE2350PluginSmokeTest ............   Passed    0.35 sec
      Start  4: TE2350GoldenReferenceCompare
12/14 Test  #4: TE2350GoldenReferenceCompare .....   Passed    0.11 sec
      Start  5: TE2350CoreControlTest
13/14 Test  #5: TE2350CoreControlTest ............   Passed    0.08 sec
14/14 Test  #1: TE2350MacroCalibrationTest .......   Passed  135.96 sec

100% tests passed, 0 tests failed out of 14

Total Test time (real) = 136.19 sec

```

Additional verification: Feedback response, both M8 ducking axes, and per-channel/window Freeze DC:

```text
Test project C:/progs/vst/te2350/build/te2350-vst-ninja-net
    Start 1: TE2350MacroCalibrationTest
1/1 Test #1: TE2350MacroCalibrationTest .......   Passed  134.50 sec

100% tests passed, 0 tests failed out of 1

Total Test time (real) = 134.50 sec

```

UIRender teve a expectativa antiga de Feedback=.8 corrigida para verificar
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
