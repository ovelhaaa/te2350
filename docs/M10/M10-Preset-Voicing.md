# M10 — Factory Preset Musical Curation & Voicing

## Método e limites

Banco original: `c8797a3478c233d93da677a353c88d05a6a9f400`. Os dois bancos usam o mesmo DSP e MacroEngine M9.1. 48 kHz, blocos de 128, Hardware, fallback de 120 BPM, 100 blocos de silêncio antes da entrada. Seis fontes mono idênticas: impulso 0,5; pluck aditivo com oito notas e ataques a cada 0,5 s; fonte harmônica vocal com formantes; pad; percussão determinística; quatro acordes. Cada WAV contém 4 s de fonte e 20 s de cauda, sem normalização (24-bit PCM). São 234 renders por banco + seis dry. Mode 0 = plugin mix; 1 = plugin Kill Dry; 2 = wrapper wet sem shimmer amount/regen, preservando a evolução real dos demais controles, inclusive alinhamento de latência Hardware. Paridade wrapper/plugin verificada em quatro presets incluindo Sync. Os limites de segurança usam float antes da escrita PCM.

RMS nas tabelas de loudness = média dos dB RMS **ativos** das cinco fontes musicais, excluindo impulso; representa nível comparável entre presets, não LUFS/percepção humana. Peak = maior pico das seis fontes mix. Espectro, largura, cauda, pitch e movimento usam wet para não esconder diferenças atrás do dry. CSVs completos por fonte incluem RMS integral, crest factor, LF <200 Hz, HF >6 kHz, centroid de potência por canal e tail energy integral depois de 4 s. O crest factor usa pico/RMS da janela inteira, incluindo silêncio/cauda.

Decay proxy = último bin wet de 100 ms acima de -40 dB do máximo; para fontes musicais desconta os 4 s de entrada, para impulso começa em 0. Censored significa que a janela acabou antes da queda. T20 = extrapolação de -5 a -25 dB da integral de Schroeder somente no impulso, com span ≥15 dB, R² >0,85 e exclusão dos últimos 2 s; NaN significa ajuste inaplicável. Não é RT60 certificado nem medição de sala. Transient preservation = RMS mix / dry nas janelas de 20 ms dos ataques, não inteligibilidade. Para pad/vocal o ataque sintético é suave.

Width = sqrt(Eside/(Emid+Eside)), não o knob Width. Modulation proxy = desvio relativo do envelope wet de 10 ms à tendência de 250 ms; inclui ritmo/dinâmica e não prova profundidade de LFO. Pitch variance = variância do bin espectral dominante (Hz²), não tracking polifônico. Adicionalmente vocal_pitch_std_cents usa interpolação do fundamental 160–280 Hz durante a entrada; é apenas proxy. Shimmer contribution = RMS(wet − wet sem shimmer)/RMS(wet); inclui mudança no feedback e pode exceder 1. SPACE ainda adiciona shimmer discreto mesmo com Amount manual zero; Regen zero evita sua recirculação nos presets sem shimmer central. Nenhuma alteração no MacroEngine foi feita para contornar isso.

Não houve escuta humana validada nesta execução. Os papéis são intenções de voicing sustentadas por renders/proxies, para aprovação auditiva posterior. Todos os nomes e a ordem dos 13 presets foram preservados. Freeze carrega desligado: Frozen Choir precisa receber áudio antes de capturar; seu render de performance liga Freeze aos 4 s e libera aos 12 s. Event Horizon usa feedback longo sem Freeze automático.

## Reprodução

1. No commit original, adicione somente `PresetVoicingTest.cpp` e seu target CMake; compile e execute `TE2350PresetVoicingTest.exe C:/progs/vst/te2350/build/m10/before --audit-only`. O baseline pula os novos gates de segurança/diversidade, mas mede todo o áudio e verifica paridade/estados/transições.
2. No banco final, compile os targets de regressão e rode CTest. O target `TE2350PresetVoicingTest` grava `build/te2350-vst-ninja-net/PresetVoicingOutput`.
3. Execute `python te2350-vst/Tests/preset_voicing_report.py` (NumPy requerido). `--before-only` mede apenas baseline; `--cached` regenera tabelas a partir dos CSVs salvos.
4. Abra `docs/M10/listen.html` para A/B das seis fontes de cada preset. Os WAVs permanecem em `build`, ignorados pelo Git, sem normalização. O índice depende desses arquivos locais.

Os CSVs de parâmetros registram valores brutos e efetivos do MacroEngine. Sync Eighths substitui o Time efetivo por 250 ms a 120 BPM; o CSV registra o valor do MacroEngine antes desse override. A matriz usa escala comum antes/depois, salva em `feature_scaling.csv`, para comparação reproduzível.

## Before — valores salvos (não efetivos)

| Nome | SPACE | WILD | BLOOM | Time ms | Feedback | Mix | High Cut Hz | Low Cut Hz | Diffusion | Width | Shimmer | Regen | Interval 0/1/2 | Ducking | Rate Hz | Depth | Chaos | Wobble | Freeze | Atmos |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Low Orbit | 0.35 | 0.05 | 0.2 | 360 | 0.42 | 0.32 | 8200 | 80 | 0.48 | 0.6 | 0 | 0.3 | 2 | 0.1 | 0.15 | 0.1 | 0 | 0.1 | 0 | 0 |
| Tidal Lock | 0.58 | 0.12 | 0.44 | 720 | 0.62 | 0.46 | 9000 | 80 | 0.72 | 0.6 | 0 | 0.3 | 2 | 0.24 | 0.15 | 0.1 | 0 | 0.1 | 0 | 0 |
| Trade Winds | 0.42 | 0.28 | 0.3 | 540 | 0.45 | 0.35 | 9000 | 80 | 0.4 | 0.74 | 0 | 0.3 | 2 | 0.1 | 0.32 | 0.28 | 0 | 0.32 | 0 | 0 |
| Solar Wind | 0.66 | 0.34 | 0.55 | 900 | 0.72 | 0.35 | 9000 | 80 | 0.4 | 0.6 | 0.18 | 0.42 | 1 | 0.1 | 0.15 | 0.1 | 0 | 0.1 | 0 | 0 |
| Escape Velocity | 0.76 | 0.62 | 0.48 | 1120 | 0.78 | 0.35 | 9000 | 80 | 0.4 | 0.6 | 0 | 0.3 | 2 | 0.1 | 0.15 | 0.1 | 0.36 | 0.48 | 0 | 1 |
| Zero-G | 0.88 | 0.18 | 0.74 | 1500 | 0.86 | 0.58 | 1.08e+04 | 80 | 0.86 | 0.6 | 0 | 0.3 | 2 | 0.1 | 0.15 | 0.1 | 0 | 0.1 | 0 | 1 |
| Event Horizon | 1 | 0.92 | 0.82 | 1900 | 0.95 | 0.7 | 9000 | 80 | 0.4 | 0.6 | 0.34 | 0.3 | 2 | 0.1 | 0.15 | 0.1 | 0.72 | 0.82 | 0 | 1 |
| Glass Transit | 0.54 | 0.08 | 0.46 | 640 | 0.57 | 0.42 | 1.3e+04 | 80 | 0.68 | 0.92 | 0.26 | 0.38 | 2 | 0.1 | 0.15 | 0.1 | 0 | 0.1 | 0 | 0 |
| Cassette Moon | 0.38 | 0.46 | 0.24 | 460 | 0.52 | 0.4 | 5800 | 110 | 0.32 | 0.7 | 0 | 0.3 | 2 | 0.1 | 0.2 | 0.46 | 0.18 | 0.62 | 0 | 0 |
| Pulsar Eighths | 0.28 | 0.24 | 0.18 | 420 | 0.64 | 0.44 | 9000 | 80 | 0.24 | 0.82 | 0 | 0.3 | 2 | 0.42 | 0.5 | 0.34 | 0.12 | 0.18 | 0 | 0 |
| Frozen Choir | 0.82 | 0.1 | 0.86 | 1350 | 0.82 | 0.64 | 1.2e+04 | 80 | 0.92 | 1 | 0.38 | 0.55 | 1 | 0.1 | 0.15 | 0.1 | 0 | 0.1 | 0 | 1 |
| Dark Matter | 0.9 | 0.56 | 0.7 | 1700 | 0.88 | 0.62 | 4200 | 160 | 0.8 | 0.88 | 0.12 | 0.25 | 0 | 0.1 | 0.09 | 0.52 | 0.48 | 0.6 | 0 | 1 |
| Microgravity Slap | 0.12 | 0.08 | 0.06 | 95 | 0.32 | 0.28 | 1.05e+04 | 140 | 0.18 | 0.55 | 0 | 0.3 | 2 | 0.1 | 0.4 | 0.08 | 0 | 0.1 | 0 | 0 |


## Identity

| Preset | Função musical |
| --- | --- |
| Low Orbit | Escuro / compacto / focado / deriva discreta; acompanhamento sem ocupar o primeiro plano. |
| Tidal Lock | Profundo / largo / ecos longos ducked; abre nos espaços entre frases. |
| Trade Winds | Aéreo / movimento de chorus / cauda média; anima arpejos e pads. |
| Solar Wind | Brilhante / quinta acima / movimento caótico / regen baixo; halo ativo. |
| Escape Velocity | Expansão rápida / forte movimento / ataques claros; transições e linhas articuladas. |
| Zero-G | Flutuante / suave / muito difuso / deriva lenta / sem regen de shimmer; cama atmosférica. |
| Event Horizon | Enorme / escuro / cauda muito longa / movimento contido; drone quase infinito sem Freeze automático. |
| Glass Transit | Vítreo / transparente / oitava acima / pouca modulação; brilho limpo atrás de notas. |
| Cassette Moon | Quente / fita instável / stereo moderado / ecos definidos; textura lo-fi. |
| Pulsar Eighths | Oitavas rítmicas sincronizadas / pouca difusão / ducking moderado; groove e repetição clara. |
| Frozen Choir | Coro sustentado / quinta / regen alto / difusão longa; pronto para capturar Freeze manualmente. |
| Dark Matter | Denso / escuro / oitava abaixo / movimento mínimo / largura moderada; profundidade grave. |
| Microgravity Slap | Slap curto / focado / seco / percussivo; definição rítmica com pequeno espaço. |


## Redundancy / Similarity

Distância = RMS das diferenças dos vetores z-score com escala comum antes/depois e seis fontes com peso igual. Energia/centroid/variância passam por log1p. Similaridade = exp(-distância). Pares com distância <0,25 são sinalizados, sem bloquear por julgamento numérico. CSVs contêm matrizes 13×13 e vetores normalizados.

### before
| A | B | Distância | Similaridade | Revisão |
| --- | --- | --- | --- | --- |
| Low Orbit | Trade Winds | 0.5798 | 0.56 | — |
| Tidal Lock | Escape Velocity | 0.6698 | 0.5118 | — |
| Trade Winds | Solar Wind | 0.6898 | 0.5017 | — |
| Tidal Lock | Trade Winds | 0.7182 | 0.4876 | — |
| Low Orbit | Tidal Lock | 0.7504 | 0.4722 | — |
| Tidal Lock | Solar Wind | 0.7776 | 0.4595 | — |
| Trade Winds | Glass Transit | 0.7814 | 0.4578 | — |
| Low Orbit | Pulsar Eighths | 0.7872 | 0.4551 | — |
| Solar Wind | Escape Velocity | 0.7995 | 0.4496 | — |
| Tidal Lock | Glass Transit | 0.8045 | 0.4473 | — |


| Preset | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1. Low Orbit | 1.00 | 0.47 | 0.56 | 0.41 | 0.34 | 0.20 | 0.11 | 0.44 | 0.40 | 0.46 | 0.19 | 0.17 | 0.32 |
| 2. Tidal Lock | 0.47 | 1.00 | 0.49 | 0.46 | 0.51 | 0.29 | 0.17 | 0.45 | 0.41 | 0.38 | 0.25 | 0.22 | 0.26 |
| 3. Trade Winds | 0.56 | 0.49 | 1.00 | 0.50 | 0.37 | 0.21 | 0.12 | 0.46 | 0.41 | 0.39 | 0.25 | 0.18 | 0.27 |
| 4. Solar Wind | 0.41 | 0.46 | 0.50 | 1.00 | 0.45 | 0.27 | 0.16 | 0.43 | 0.35 | 0.40 | 0.29 | 0.21 | 0.28 |
| 5. Escape Velocity | 0.34 | 0.51 | 0.37 | 0.45 | 1.00 | 0.31 | 0.18 | 0.39 | 0.40 | 0.31 | 0.25 | 0.24 | 0.23 |
| 6. Zero-G | 0.20 | 0.29 | 0.21 | 0.27 | 0.31 | 1.00 | 0.37 | 0.23 | 0.19 | 0.20 | 0.23 | 0.23 | 0.14 |
| 7. Event Horizon | 0.11 | 0.17 | 0.12 | 0.16 | 0.18 | 0.37 | 1.00 | 0.14 | 0.11 | 0.12 | 0.19 | 0.21 | 0.09 |
| 8. Glass Transit | 0.44 | 0.45 | 0.46 | 0.43 | 0.39 | 0.23 | 0.14 | 1.00 | 0.32 | 0.39 | 0.28 | 0.21 | 0.24 |
| 9. Cassette Moon | 0.40 | 0.41 | 0.41 | 0.35 | 0.40 | 0.19 | 0.11 | 0.32 | 1.00 | 0.33 | 0.17 | 0.17 | 0.31 |
| 10. Pulsar Eighths | 0.46 | 0.38 | 0.39 | 0.40 | 0.31 | 0.20 | 0.12 | 0.39 | 0.33 | 1.00 | 0.16 | 0.14 | 0.37 |
| 11. Frozen Choir | 0.19 | 0.25 | 0.25 | 0.29 | 0.25 | 0.23 | 0.19 | 0.28 | 0.17 | 0.16 | 1.00 | 0.25 | 0.11 |
| 12. Dark Matter | 0.17 | 0.22 | 0.18 | 0.21 | 0.24 | 0.23 | 0.21 | 0.21 | 0.17 | 0.14 | 0.25 | 1.00 | 0.12 |
| 13. Microgravity Slap | 0.32 | 0.26 | 0.27 | 0.28 | 0.23 | 0.14 | 0.09 | 0.24 | 0.31 | 0.37 | 0.11 | 0.12 | 1.00 |


### after
| A | B | Distância | Similaridade | Revisão |
| --- | --- | --- | --- | --- |
| Low Orbit | Trade Winds | 0.6855 | 0.5038 | — |
| Zero-G | Event Horizon | 0.6879 | 0.5026 | — |
| Tidal Lock | Zero-G | 0.7161 | 0.4887 | — |
| Low Orbit | Cassette Moon | 0.7552 | 0.4699 | — |
| Trade Winds | Cassette Moon | 0.7816 | 0.4577 | — |
| Solar Wind | Glass Transit | 0.8677 | 0.4199 | — |
| Tidal Lock | Trade Winds | 0.8837 | 0.4133 | — |
| Low Orbit | Microgravity Slap | 0.9025 | 0.4056 | — |
| Trade Winds | Escape Velocity | 0.9265 | 0.3959 | — |
| Solar Wind | Escape Velocity | 0.9267 | 0.3959 | — |


| Preset | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1. Low Orbit | 1.00 | 0.33 | 0.50 | 0.25 | 0.32 | 0.26 | 0.24 | 0.29 | 0.47 | 0.38 | 0.15 | 0.26 | 0.41 |
| 2. Tidal Lock | 0.33 | 1.00 | 0.41 | 0.30 | 0.34 | 0.49 | 0.37 | 0.33 | 0.34 | 0.29 | 0.23 | 0.26 | 0.21 |
| 3. Trade Winds | 0.50 | 0.41 | 1.00 | 0.27 | 0.40 | 0.32 | 0.30 | 0.29 | 0.46 | 0.33 | 0.16 | 0.29 | 0.31 |
| 4. Solar Wind | 0.25 | 0.30 | 0.27 | 1.00 | 0.40 | 0.29 | 0.23 | 0.42 | 0.22 | 0.25 | 0.36 | 0.19 | 0.19 |
| 5. Escape Velocity | 0.32 | 0.34 | 0.40 | 0.40 | 1.00 | 0.31 | 0.28 | 0.30 | 0.30 | 0.26 | 0.21 | 0.22 | 0.23 |
| 6. Zero-G | 0.26 | 0.49 | 0.32 | 0.29 | 0.31 | 1.00 | 0.50 | 0.27 | 0.24 | 0.23 | 0.22 | 0.28 | 0.15 |
| 7. Event Horizon | 0.24 | 0.37 | 0.30 | 0.23 | 0.28 | 0.50 | 1.00 | 0.21 | 0.24 | 0.20 | 0.17 | 0.29 | 0.14 |
| 8. Glass Transit | 0.29 | 0.33 | 0.29 | 0.42 | 0.30 | 0.27 | 0.21 | 1.00 | 0.24 | 0.29 | 0.34 | 0.17 | 0.22 |
| 9. Cassette Moon | 0.47 | 0.34 | 0.46 | 0.22 | 0.30 | 0.24 | 0.24 | 0.24 | 1.00 | 0.31 | 0.14 | 0.26 | 0.28 |
| 10. Pulsar Eighths | 0.38 | 0.29 | 0.33 | 0.25 | 0.26 | 0.23 | 0.20 | 0.29 | 0.31 | 1.00 | 0.16 | 0.17 | 0.31 |
| 11. Frozen Choir | 0.15 | 0.23 | 0.16 | 0.36 | 0.21 | 0.22 | 0.17 | 0.34 | 0.14 | 0.16 | 1.00 | 0.14 | 0.11 |
| 12. Dark Matter | 0.26 | 0.26 | 0.29 | 0.19 | 0.22 | 0.28 | 0.29 | 0.17 | 0.26 | 0.17 | 0.14 | 1.00 | 0.15 |
| 13. Microgravity Slap | 0.41 | 0.21 | 0.31 | 0.19 | 0.23 | 0.15 | 0.14 | 0.22 | 0.28 | 0.31 | 0.11 | 0.15 | 1.00 |


A proximidade original Low Orbit/Trade Winds foi tratada com foco estreito/escuro contra movimento de chorus. Tidal Lock/Escape Velocity passou a eco profundo ducked contra expansão rápida e forte movimento. Zero-G/Event Horizon/Dark Matter mantêm caudas atmosféricas, mas separam difusão flutuante sem regen, órbita quase infinita e sombra de oitava abaixo. Após o revoicing, Low Orbit/Trade Winds e Zero-G/Event Horizon são prioridades de escuta humana: a matriz os coloca mais próximos. O primeiro par separa foco/cauda curta de chorus médio; o segundo separa campo flutuante de órbita mais longa e escura. Tidal Lock/Zero-G também merecem A/B por compartilharem atmosfera sem regen, embora divirjam em cauda, difusão e modulação. Nenhum par abaixo do limiar de revisão foi detectado; isso não comprova distinção musical humana.

Menor distância do banco: 0.580 antes → 0.686 depois, com escala comum. O aumento indica menos proximidade extrema neste conjunto de features, não uma nota musical absoluta.

## Changes

### Low Orbit
| Parameter | Old | New | Reason |
| --- | --- | --- | --- |
| wild | 0.05 | 0.02 | Reposiciona movimento e deixa margem para intensificar WILD. Escurece e concentra a ambiência curta; reduz recirculação e preserva margem manual. |
| bloom | 0.2 | 0.18 | Equilibra sustain, abertura tonal e ducking do BLOOM. Escurece e concentra a ambiência curta; reduz recirculação e preserva margem manual. |
| timeMs | 360 | 230 | Define duração e separação dos ecos para o papel musical. Escurece e concentra a ambiência curta; reduz recirculação e preserva margem manual. |
| feedback | 0.42 | 0.34 | Controla persistência dos ecos sem depender do teto de feedback. Escurece e concentra a ambiência curta; reduz recirculação e preserva margem manual. |
| mix | 0.32 | 0.3 | Equilibra presença wet, nível ativo e clareza do dry sem usar trim. Escurece e concentra a ambiência curta; reduz recirculação e preserva margem manual. |
| lowCutHz | 80 | 100 | Equilibra corpo grave e acúmulo na recirculação. Escurece e concentra a ambiência curta; reduz recirculação e preserva margem manual. |
| highCutHz | 8200 | 4800 | Ajusta cor tonal considerando a abertura efetiva de BLOOM. Escurece e concentra a ambiência curta; reduz recirculação e preserva margem manual. |
| diffusion | 0.48 | 0.35 | Distingue ecos articulados de nuvens densas, preservando margem manual. Escurece e concentra a ambiência curta; reduz recirculação e preserva margem manual. |
| wobble | 0.1 | 0.04 | Separa deriva lenta de instabilidade de fita/órbita. Escurece e concentra a ambiência curta; reduz recirculação e preserva margem manual. |
| presence | 0.5 | 0.42 | Ajusta articulação e cor interna do preset. Escurece e concentra a ambiência curta; reduz recirculação e preserva margem manual. |
| modRateHz | 0.15 | 0.12 | Define velocidade de deriva/movimento conforme a função musical. Escurece e concentra a ambiência curta; reduz recirculação e preserva margem manual. |
| modDepth | 0.1 | 0.03 | Define amplitude de movimento e mantém margem para WILD/manual. Escurece e concentra a ambiência curta; reduz recirculação e preserva margem manual. |
| shimmerFeedback | 0.3 | 0 | Separa halo/sustain de recirculação; zero evita regen em papéis sem shimmer. Escurece e concentra a ambiência curta; reduz recirculação e preserva margem manual. |
| duckAmount | 0.1 | 0.08 | Equilibra abertura da cauda entre frases e preservação do ataque. Escurece e concentra a ambiência curta; reduz recirculação e preserva margem manual. |
| wetWidth | 0.6 | 0.28 | Distingue foco stereo de expansão sem carregar o knob no máximo. Escurece e concentra a ambiência curta; reduz recirculação e preserva margem manual. |


### Tidal Lock
| Parameter | Old | New | Reason |
| --- | --- | --- | --- |
| space | 0.58 | 0.56 | Reposiciona dimensão e deixa margem para expansão de SPACE. Diferencia ecos profundos da nuvem Zero-G; ducking protege frases e regen zero evita halo repetido. |
| wild | 0.12 | 0.1 | Reposiciona movimento e deixa margem para intensificar WILD. Diferencia ecos profundos da nuvem Zero-G; ducking protege frases e regen zero evita halo repetido. |
| bloom | 0.44 | 0.48 | Equilibra sustain, abertura tonal e ducking do BLOOM. Diferencia ecos profundos da nuvem Zero-G; ducking protege frases e regen zero evita halo repetido. |
| timeMs | 720 | 650 | Define duração e separação dos ecos para o papel musical. Diferencia ecos profundos da nuvem Zero-G; ducking protege frases e regen zero evita halo repetido. |
| feedback | 0.62 | 0.67 | Controla persistência dos ecos sem depender do teto de feedback. Diferencia ecos profundos da nuvem Zero-G; ducking protege frases e regen zero evita halo repetido. |
| mix | 0.46 | 0.36 | Equilibra presença wet, nível ativo e clareza do dry sem usar trim. Diferencia ecos profundos da nuvem Zero-G; ducking protege frases e regen zero evita halo repetido. |
| lowCutHz | 80 | 135 | Equilibra corpo grave e acúmulo na recirculação. Diferencia ecos profundos da nuvem Zero-G; ducking protege frases e regen zero evita halo repetido. |
| highCutHz | 9000 | 6800 | Ajusta cor tonal considerando a abertura efetiva de BLOOM. Diferencia ecos profundos da nuvem Zero-G; ducking protege frases e regen zero evita halo repetido. |
| diffusion | 0.72 | 0.6 | Distingue ecos articulados de nuvens densas, preservando margem manual. Diferencia ecos profundos da nuvem Zero-G; ducking protege frases e regen zero evita halo repetido. |
| chaos | 0 | 0.01 | Separa centro estável de órbita caótica. Diferencia ecos profundos da nuvem Zero-G; ducking protege frases e regen zero evita halo repetido. |
| wobble | 0.1 | 0.12 | Separa deriva lenta de instabilidade de fita/órbita. Diferencia ecos profundos da nuvem Zero-G; ducking protege frases e regen zero evita halo repetido. |
| presence | 0.5 | 0.4 | Ajusta articulação e cor interna do preset. Diferencia ecos profundos da nuvem Zero-G; ducking protege frases e regen zero evita halo repetido. |
| modRateHz | 0.15 | 0.12 | Define velocidade de deriva/movimento conforme a função musical. Diferencia ecos profundos da nuvem Zero-G; ducking protege frases e regen zero evita halo repetido. |
| shimmerFeedback | 0.3 | 0 | Separa halo/sustain de recirculação; zero evita regen em papéis sem shimmer. Diferencia ecos profundos da nuvem Zero-G; ducking protege frases e regen zero evita halo repetido. |
| duckAmount | 0.24 | 0.26 | Equilibra abertura da cauda entre frases e preservação do ataque. Diferencia ecos profundos da nuvem Zero-G; ducking protege frases e regen zero evita halo repetido. |
| outputTrim | 1.25 | 0 | Retira compensação herdada; balanceamento usa Mix/voicing, trim neutro. |
| wetWidth | 0.6 | 0.78 | Distingue foco stereo de expansão sem carregar o knob no máximo. Diferencia ecos profundos da nuvem Zero-G; ducking protege frases e regen zero evita halo repetido. |


### Trade Winds
| Parameter | Old | New | Reason |
| --- | --- | --- | --- |
| space | 0.42 | 0.36 | Reposiciona dimensão e deixa margem para expansão de SPACE. Prioriza chorus aéreo e cauda média, com forma triangular e espaço para ampliar WILD. |
| wild | 0.28 | 0.3 | Reposiciona movimento e deixa margem para intensificar WILD. Prioriza chorus aéreo e cauda média, com forma triangular e espaço para ampliar WILD. |
| bloom | 0.3 | 0.26 | Equilibra sustain, abertura tonal e ducking do BLOOM. Prioriza chorus aéreo e cauda média, com forma triangular e espaço para ampliar WILD. |
| timeMs | 540 | 420 | Define duração e separação dos ecos para o papel musical. Prioriza chorus aéreo e cauda média, com forma triangular e espaço para ampliar WILD. |
| feedback | 0.45 | 0.46 | Controla persistência dos ecos sem depender do teto de feedback. Prioriza chorus aéreo e cauda média, com forma triangular e espaço para ampliar WILD. |
| mix | 0.35 | 0.32 | Equilibra presença wet, nível ativo e clareza do dry sem usar trim. Prioriza chorus aéreo e cauda média, com forma triangular e espaço para ampliar WILD. |
| lowCutHz | 80 | 95 | Equilibra corpo grave e acúmulo na recirculação. Prioriza chorus aéreo e cauda média, com forma triangular e espaço para ampliar WILD. |
| highCutHz | 9000 | 1.2e+04 | Ajusta cor tonal considerando a abertura efetiva de BLOOM. Prioriza chorus aéreo e cauda média, com forma triangular e espaço para ampliar WILD. |
| diffusion | 0.4 | 0.42 | Distingue ecos articulados de nuvens densas, preservando margem manual. Prioriza chorus aéreo e cauda média, com forma triangular e espaço para ampliar WILD. |
| chaos | 0 | 0.02 | Separa centro estável de órbita caótica. Prioriza chorus aéreo e cauda média, com forma triangular e espaço para ampliar WILD. |
| wobble | 0.32 | 0.28 | Separa deriva lenta de instabilidade de fita/órbita. Prioriza chorus aéreo e cauda média, com forma triangular e espaço para ampliar WILD. |
| presence | 0.5 | 0.62 | Ajusta articulação e cor interna do preset. Prioriza chorus aéreo e cauda média, com forma triangular e espaço para ampliar WILD. |
| modRateHz | 0.32 | 0.64 | Define velocidade de deriva/movimento conforme a função musical. Prioriza chorus aéreo e cauda média, com forma triangular e espaço para ampliar WILD. |
| modDepth | 0.28 | 0.4 | Define amplitude de movimento e mantém margem para WILD/manual. Prioriza chorus aéreo e cauda média, com forma triangular e espaço para ampliar WILD. |
| modShape | 0 | 1 | Escolhe o caráter temporal de modulação, em vez de variar apenas profundidade. Prioriza chorus aéreo e cauda média, com forma triangular e espaço para ampliar WILD. |
| shimmerFeedback | 0.3 | 0 | Separa halo/sustain de recirculação; zero evita regen em papéis sem shimmer. Prioriza chorus aéreo e cauda média, com forma triangular e espaço para ampliar WILD. |
| wetWidth | 0.74 | 0.64 | Distingue foco stereo de expansão sem carregar o knob no máximo. Prioriza chorus aéreo e cauda média, com forma triangular e espaço para ampliar WILD. |


### Solar Wind
| Parameter | Old | New | Reason |
| --- | --- | --- | --- |
| space | 0.66 | 0.58 | Reposiciona dimensão e deixa margem para expansão de SPACE. Separa halo de quinta animado do Glass Transit limpo e Frozen Choir sustentado. |
| wild | 0.34 | 0.58 | Reposiciona movimento e deixa margem para intensificar WILD. Separa halo de quinta animado do Glass Transit limpo e Frozen Choir sustentado. |
| bloom | 0.55 | 0.46 | Equilibra sustain, abertura tonal e ducking do BLOOM. Separa halo de quinta animado do Glass Transit limpo e Frozen Choir sustentado. |
| timeMs | 900 | 680 | Define duração e separação dos ecos para o papel musical. Separa halo de quinta animado do Glass Transit limpo e Frozen Choir sustentado. |
| feedback | 0.72 | 0.64 | Controla persistência dos ecos sem depender do teto de feedback. Separa halo de quinta animado do Glass Transit limpo e Frozen Choir sustentado. |
| mix | 0.35 | 0.33 | Equilibra presença wet, nível ativo e clareza do dry sem usar trim. Separa halo de quinta animado do Glass Transit limpo e Frozen Choir sustentado. |
| lowCutHz | 80 | 180 | Equilibra corpo grave e acúmulo na recirculação. Separa halo de quinta animado do Glass Transit limpo e Frozen Choir sustentado. |
| highCutHz | 9000 | 1.45e+04 | Ajusta cor tonal considerando a abertura efetiva de BLOOM. Separa halo de quinta animado do Glass Transit limpo e Frozen Choir sustentado. |
| diffusion | 0.4 | 0.48 | Distingue ecos articulados de nuvens densas, preservando margem manual. Separa halo de quinta animado do Glass Transit limpo e Frozen Choir sustentado. |
| chaos | 0 | 0.32 | Separa centro estável de órbita caótica. Separa halo de quinta animado do Glass Transit limpo e Frozen Choir sustentado. |
| wobble | 0.1 | 0.46 | Separa deriva lenta de instabilidade de fita/órbita. Separa halo de quinta animado do Glass Transit limpo e Frozen Choir sustentado. |
| presence | 0.62 | 0.68 | Ajusta articulação e cor interna do preset. Separa halo de quinta animado do Glass Transit limpo e Frozen Choir sustentado. |
| modRateHz | 0.15 | 0.45 | Define velocidade de deriva/movimento conforme a função musical. Separa halo de quinta animado do Glass Transit limpo e Frozen Choir sustentado. |
| modDepth | 0.1 | 0.5 | Define amplitude de movimento e mantém margem para WILD/manual. Separa halo de quinta animado do Glass Transit limpo e Frozen Choir sustentado. |
| shimmerAmount | 0.18 | 0.21 | Define quanto o pitch processing participa da identidade. Separa halo de quinta animado do Glass Transit limpo e Frozen Choir sustentado. |
| shimmerFeedback | 0.42 | 0.23 | Separa halo/sustain de recirculação; zero evita regen em papéis sem shimmer. Separa halo de quinta animado do Glass Transit limpo e Frozen Choir sustentado. |
| duckAmount | 0.1 | 0.18 | Equilibra abertura da cauda entre frases e preservação do ataque. Separa halo de quinta animado do Glass Transit limpo e Frozen Choir sustentado. |
| outputTrim | 0.4 | 0 | Retira compensação herdada; balanceamento usa Mix/voicing, trim neutro. |
| wetWidth | 0.6 | 0.78 | Distingue foco stereo de expansão sem carregar o knob no máximo. Separa halo de quinta animado do Glass Transit limpo e Frozen Choir sustentado. |


### Escape Velocity
| Parameter | Old | New | Reason |
| --- | --- | --- | --- |
| space | 0.76 | 0.44 | Reposiciona dimensão e deixa margem para expansão de SPACE. Troca órbita longa redundante por expansão mais rápida; movimento forte e ducking sustentam ataques. |
| wild | 0.62 | 0.65 | Reposiciona movimento e deixa margem para intensificar WILD. Troca órbita longa redundante por expansão mais rápida; movimento forte e ducking sustentam ataques. |
| bloom | 0.48 | 0.38 | Equilibra sustain, abertura tonal e ducking do BLOOM. Troca órbita longa redundante por expansão mais rápida; movimento forte e ducking sustentam ataques. |
| timeMs | 1120 | 330 | Define duração e separação dos ecos para o papel musical. Troca órbita longa redundante por expansão mais rápida; movimento forte e ducking sustentam ataques. |
| feedback | 0.78 | 0.6 | Controla persistência dos ecos sem depender do teto de feedback. Troca órbita longa redundante por expansão mais rápida; movimento forte e ducking sustentam ataques. |
| mix | 0.35 | 0.31 | Equilibra presença wet, nível ativo e clareza do dry sem usar trim. Troca órbita longa redundante por expansão mais rápida; movimento forte e ducking sustentam ataques. |
| lowCutHz | 80 | 155 | Equilibra corpo grave e acúmulo na recirculação. Troca órbita longa redundante por expansão mais rápida; movimento forte e ducking sustentam ataques. |
| highCutHz | 9000 | 1.15e+04 | Ajusta cor tonal considerando a abertura efetiva de BLOOM. Troca órbita longa redundante por expansão mais rápida; movimento forte e ducking sustentam ataques. |
| diffusion | 0.4 | 0.57 | Distingue ecos articulados de nuvens densas, preservando margem manual. Troca órbita longa redundante por expansão mais rápida; movimento forte e ducking sustentam ataques. |
| chaos | 0.36 | 0.28 | Separa centro estável de órbita caótica. Troca órbita longa redundante por expansão mais rápida; movimento forte e ducking sustentam ataques. |
| wobble | 0.48 | 0.35 | Separa deriva lenta de instabilidade de fita/órbita. Troca órbita longa redundante por expansão mais rápida; movimento forte e ducking sustentam ataques. |
| presence | 0.5 | 0.58 | Ajusta articulação e cor interna do preset. Troca órbita longa redundante por expansão mais rápida; movimento forte e ducking sustentam ataques. |
| modRateHz | 0.15 | 0.75 | Define velocidade de deriva/movimento conforme a função musical. Troca órbita longa redundante por expansão mais rápida; movimento forte e ducking sustentam ataques. |
| modDepth | 0.1 | 0.56 | Define amplitude de movimento e mantém margem para WILD/manual. Troca órbita longa redundante por expansão mais rápida; movimento forte e ducking sustentam ataques. |
| shimmerFeedback | 0.3 | 0 | Separa halo/sustain de recirculação; zero evita regen em papéis sem shimmer. Troca órbita longa redundante por expansão mais rápida; movimento forte e ducking sustentam ataques. |
| duckAmount | 0.1 | 0.32 | Equilibra abertura da cauda entre frases e preservação do ataque. Troca órbita longa redundante por expansão mais rápida; movimento forte e ducking sustentam ataques. |
| outputTrim | 0.5 | 0 | Retira compensação herdada; balanceamento usa Mix/voicing, trim neutro. |
| wetWidth | 0.6 | 0.72 | Distingue foco stereo de expansão sem carregar o knob no máximo. Troca órbita longa redundante por expansão mais rápida; movimento forte e ducking sustentam ataques. |


### Zero-G
| Parameter | Old | New | Reason |
| --- | --- | --- | --- |
| space | 0.88 | 0.72 | Reposiciona dimensão e deixa margem para expansão de SPACE. Prioriza campo difuso flutuante, lenta deriva e ausência de recirculação shimmer. |
| wild | 0.18 | 0.06 | Reposiciona movimento e deixa margem para intensificar WILD. Prioriza campo difuso flutuante, lenta deriva e ausência de recirculação shimmer. |
| bloom | 0.74 | 0.68 | Equilibra sustain, abertura tonal e ducking do BLOOM. Prioriza campo difuso flutuante, lenta deriva e ausência de recirculação shimmer. |
| timeMs | 1500 | 1000 | Define duração e separação dos ecos para o papel musical. Prioriza campo difuso flutuante, lenta deriva e ausência de recirculação shimmer. |
| feedback | 0.86 | 0.7 | Controla persistência dos ecos sem depender do teto de feedback. Prioriza campo difuso flutuante, lenta deriva e ausência de recirculação shimmer. |
| mix | 0.58 | 0.34 | Equilibra presença wet, nível ativo e clareza do dry sem usar trim. Prioriza campo difuso flutuante, lenta deriva e ausência de recirculação shimmer. |
| lowCutHz | 80 | 95 | Equilibra corpo grave e acúmulo na recirculação. Prioriza campo difuso flutuante, lenta deriva e ausência de recirculação shimmer. |
| diffusion | 0.86 | 0.82 | Distingue ecos articulados de nuvens densas, preservando margem manual. Prioriza campo difuso flutuante, lenta deriva e ausência de recirculação shimmer. |
| wobble | 0.1 | 0.06 | Separa deriva lenta de instabilidade de fita/órbita. Prioriza campo difuso flutuante, lenta deriva e ausência de recirculação shimmer. |
| presence | 0.5 | 0.46 | Ajusta articulação e cor interna do preset. Prioriza campo difuso flutuante, lenta deriva e ausência de recirculação shimmer. |
| modRateHz | 0.15 | 0.07 | Define velocidade de deriva/movimento conforme a função musical. Prioriza campo difuso flutuante, lenta deriva e ausência de recirculação shimmer. |
| modDepth | 0.1 | 0.08 | Define amplitude de movimento e mantém margem para WILD/manual. Prioriza campo difuso flutuante, lenta deriva e ausência de recirculação shimmer. |
| shimmerFeedback | 0.3 | 0 | Separa halo/sustain de recirculação; zero evita regen em papéis sem shimmer. Prioriza campo difuso flutuante, lenta deriva e ausência de recirculação shimmer. |
| outputTrim | 2.25 | 0 | Retira compensação herdada; balanceamento usa Mix/voicing, trim neutro. |
| wetWidth | 0.6 | 0.86 | Distingue foco stereo de expansão sem carregar o knob no máximo. Prioriza campo difuso flutuante, lenta deriva e ausência de recirculação shimmer. |


### Event Horizon
| Parameter | Old | New | Reason |
| --- | --- | --- | --- |
| space | 1 | 0.78 | Reposiciona dimensão e deixa margem para expansão de SPACE. Mantém papel quase infinito com tom escuro e movimento contido; evita extremos simultâneos dos macros. |
| wild | 0.92 | 0.32 | Reposiciona movimento e deixa margem para intensificar WILD. Mantém papel quase infinito com tom escuro e movimento contido; evita extremos simultâneos dos macros. |
| bloom | 0.82 | 0.42 | Equilibra sustain, abertura tonal e ducking do BLOOM. Mantém papel quase infinito com tom escuro e movimento contido; evita extremos simultâneos dos macros. |
| timeMs | 1900 | 1450 | Define duração e separação dos ecos para o papel musical. Mantém papel quase infinito com tom escuro e movimento contido; evita extremos simultâneos dos macros. |
| feedback | 0.95 | 0.89 | Controla persistência dos ecos sem depender do teto de feedback. Mantém papel quase infinito com tom escuro e movimento contido; evita extremos simultâneos dos macros. |
| mix | 0.7 | 0.34 | Equilibra presença wet, nível ativo e clareza do dry sem usar trim. Mantém papel quase infinito com tom escuro e movimento contido; evita extremos simultâneos dos macros. |
| lowCutHz | 80 | 60 | Equilibra corpo grave e acúmulo na recirculação. Mantém papel quase infinito com tom escuro e movimento contido; evita extremos simultâneos dos macros. |
| highCutHz | 9000 | 1000 | Ajusta cor tonal considerando a abertura efetiva de BLOOM. Mantém papel quase infinito com tom escuro e movimento contido; evita extremos simultâneos dos macros. |
| diffusion | 0.4 | 0.76 | Distingue ecos articulados de nuvens densas, preservando margem manual. Mantém papel quase infinito com tom escuro e movimento contido; evita extremos simultâneos dos macros. |
| chaos | 0.72 | 0.1 | Separa centro estável de órbita caótica. Mantém papel quase infinito com tom escuro e movimento contido; evita extremos simultâneos dos macros. |
| wobble | 0.82 | 0.26 | Separa deriva lenta de instabilidade de fita/órbita. Mantém papel quase infinito com tom escuro e movimento contido; evita extremos simultâneos dos macros. |
| presence | 0.5 | 0.18 | Ajusta articulação e cor interna do preset. Mantém papel quase infinito com tom escuro e movimento contido; evita extremos simultâneos dos macros. |
| modRateHz | 0.15 | 0.07 | Define velocidade de deriva/movimento conforme a função musical. Mantém papel quase infinito com tom escuro e movimento contido; evita extremos simultâneos dos macros. |
| modDepth | 0.1 | 0.2 | Define amplitude de movimento e mantém margem para WILD/manual. Mantém papel quase infinito com tom escuro e movimento contido; evita extremos simultâneos dos macros. |
| modShape | 2 | 1 | Escolhe o caráter temporal de modulação, em vez de variar apenas profundidade. Mantém papel quase infinito com tom escuro e movimento contido; evita extremos simultâneos dos macros. |
| shimmerInterval | 2 | 0 | Seleciona registro harmônico; 0 = oitava abaixo, 1 = quinta, 2 = oitava acima. Mantém papel quase infinito com tom escuro e movimento contido; evita extremos simultâneos dos macros. |
| shimmerAmount | 0.34 | 0 | Define quanto o pitch processing participa da identidade. Mantém papel quase infinito com tom escuro e movimento contido; evita extremos simultâneos dos macros. |
| shimmerFeedback | 0.3 | 0 | Separa halo/sustain de recirculação; zero evita regen em papéis sem shimmer. Mantém papel quase infinito com tom escuro e movimento contido; evita extremos simultâneos dos macros. |
| duckAmount | 0.1 | 0.16 | Equilibra abertura da cauda entre frases e preservação do ataque. Mantém papel quase infinito com tom escuro e movimento contido; evita extremos simultâneos dos macros. |
| outputTrim | 3.25 | 0 | Retira compensação herdada; balanceamento usa Mix/voicing, trim neutro. |
| wetWidth | 0.6 | 0.83 | Distingue foco stereo de expansão sem carregar o knob no máximo. Mantém papel quase infinito com tom escuro e movimento contido; evita extremos simultâneos dos macros. |


### Glass Transit
| Parameter | Old | New | Reason |
| --- | --- | --- | --- |
| space | 0.54 | 0.46 | Reposiciona dimensão e deixa margem para expansão de SPACE. Prioriza transparência e oitava limpa, reduz movimento e mantém largura sem endpoint. |
| wild | 0.08 | 0.04 | Reposiciona movimento e deixa margem para intensificar WILD. Prioriza transparência e oitava limpa, reduz movimento e mantém largura sem endpoint. |
| bloom | 0.46 | 0.36 | Equilibra sustain, abertura tonal e ducking do BLOOM. Prioriza transparência e oitava limpa, reduz movimento e mantém largura sem endpoint. |
| timeMs | 640 | 510 | Define duração e separação dos ecos para o papel musical. Prioriza transparência e oitava limpa, reduz movimento e mantém largura sem endpoint. |
| feedback | 0.57 | 0.52 | Controla persistência dos ecos sem depender do teto de feedback. Prioriza transparência e oitava limpa, reduz movimento e mantém largura sem endpoint. |
| mix | 0.42 | 0.34 | Equilibra presença wet, nível ativo e clareza do dry sem usar trim. Prioriza transparência e oitava limpa, reduz movimento e mantém largura sem endpoint. |
| lowCutHz | 80 | 180 | Equilibra corpo grave e acúmulo na recirculação. Prioriza transparência e oitava limpa, reduz movimento e mantém largura sem endpoint. |
| highCutHz | 1.3e+04 | 1.55e+04 | Ajusta cor tonal considerando a abertura efetiva de BLOOM. Prioriza transparência e oitava limpa, reduz movimento e mantém largura sem endpoint. |
| diffusion | 0.68 | 0.52 | Distingue ecos articulados de nuvens densas, preservando margem manual. Prioriza transparência e oitava limpa, reduz movimento e mantém largura sem endpoint. |
| wobble | 0.1 | 0.04 | Separa deriva lenta de instabilidade de fita/órbita. Prioriza transparência e oitava limpa, reduz movimento e mantém largura sem endpoint. |
| presence | 0.68 | 0.74 | Ajusta articulação e cor interna do preset. Prioriza transparência e oitava limpa, reduz movimento e mantém largura sem endpoint. |
| modRateHz | 0.15 | 0.18 | Define velocidade de deriva/movimento conforme a função musical. Prioriza transparência e oitava limpa, reduz movimento e mantém largura sem endpoint. |
| modDepth | 0.1 | 0.05 | Define amplitude de movimento e mantém margem para WILD/manual. Prioriza transparência e oitava limpa, reduz movimento e mantém largura sem endpoint. |
| shimmerAmount | 0.26 | 0.3 | Define quanto o pitch processing participa da identidade. Prioriza transparência e oitava limpa, reduz movimento e mantém largura sem endpoint. |
| shimmerFeedback | 0.38 | 0.34 | Separa halo/sustain de recirculação; zero evita regen em papéis sem shimmer. Prioriza transparência e oitava limpa, reduz movimento e mantém largura sem endpoint. |
| duckAmount | 0.1 | 0.14 | Equilibra abertura da cauda entre frases e preservação do ataque. Prioriza transparência e oitava limpa, reduz movimento e mantém largura sem endpoint. |
| outputTrim | 0.5 | 0 | Retira compensação herdada; balanceamento usa Mix/voicing, trim neutro. |
| wetWidth | 0.92 | 0.87 | Distingue foco stereo de expansão sem carregar o knob no máximo. Prioriza transparência e oitava limpa, reduz movimento e mantém largura sem endpoint. |


### Cassette Moon
| Parameter | Old | New | Reason |
| --- | --- | --- | --- |
| space | 0.38 | 0.32 | Reposiciona dimensão e deixa margem para expansão de SPACE. Concentra cor quente e instabilidade de fita; menos difusão para preservar identidade dos ecos. |
| bloom | 0.24 | 0.2 | Equilibra sustain, abertura tonal e ducking do BLOOM. Concentra cor quente e instabilidade de fita; menos difusão para preservar identidade dos ecos. |
| timeMs | 460 | 375 | Define duração e separação dos ecos para o papel musical. Concentra cor quente e instabilidade de fita; menos difusão para preservar identidade dos ecos. |
| feedback | 0.52 | 0.44 | Controla persistência dos ecos sem depender do teto de feedback. Concentra cor quente e instabilidade de fita; menos difusão para preservar identidade dos ecos. |
| mix | 0.4 | 0.31 | Equilibra presença wet, nível ativo e clareza do dry sem usar trim. Concentra cor quente e instabilidade de fita; menos difusão para preservar identidade dos ecos. |
| lowCutHz | 110 | 100 | Equilibra corpo grave e acúmulo na recirculação. Concentra cor quente e instabilidade de fita; menos difusão para preservar identidade dos ecos. |
| highCutHz | 5800 | 3400 | Ajusta cor tonal considerando a abertura efetiva de BLOOM. Concentra cor quente e instabilidade de fita; menos difusão para preservar identidade dos ecos. |
| diffusion | 0.32 | 0.23 | Distingue ecos articulados de nuvens densas, preservando margem manual. Concentra cor quente e instabilidade de fita; menos difusão para preservar identidade dos ecos. |
| chaos | 0.18 | 0.08 | Separa centro estável de órbita caótica. Concentra cor quente e instabilidade de fita; menos difusão para preservar identidade dos ecos. |
| wobble | 0.62 | 0.52 | Separa deriva lenta de instabilidade de fita/órbita. Concentra cor quente e instabilidade de fita; menos difusão para preservar identidade dos ecos. |
| presence | 0.38 | 0.3 | Ajusta articulação e cor interna do preset. Concentra cor quente e instabilidade de fita; menos difusão para preservar identidade dos ecos. |
| modRateHz | 0.2 | 0.1 | Define velocidade de deriva/movimento conforme a função musical. Concentra cor quente e instabilidade de fita; menos difusão para preservar identidade dos ecos. |
| modDepth | 0.46 | 0.4 | Define amplitude de movimento e mantém margem para WILD/manual. Concentra cor quente e instabilidade de fita; menos difusão para preservar identidade dos ecos. |
| shimmerInterval | 2 | 0 | Seleciona registro harmônico; 0 = oitava abaixo, 1 = quinta, 2 = oitava acima. Concentra cor quente e instabilidade de fita; menos difusão para preservar identidade dos ecos. |
| shimmerFeedback | 0.3 | 0 | Separa halo/sustain de recirculação; zero evita regen em papéis sem shimmer. Concentra cor quente e instabilidade de fita; menos difusão para preservar identidade dos ecos. |
| duckAmount | 0.1 | 0.12 | Equilibra abertura da cauda entre frases e preservação do ataque. Concentra cor quente e instabilidade de fita; menos difusão para preservar identidade dos ecos. |
| outputTrim | 1 | 0 | Retira compensação herdada; balanceamento usa Mix/voicing, trim neutro. |
| wetWidth | 0.7 | 0.48 | Distingue foco stereo de expansão sem carregar o knob no máximo. Concentra cor quente e instabilidade de fita; menos difusão para preservar identidade dos ecos. |


### Pulsar Eighths
| Parameter | Old | New | Reason |
| --- | --- | --- | --- |
| space | 0.28 | 0.18 | Reposiciona dimensão e deixa margem para expansão de SPACE. Prioriza oitavas nítidas no tempo, reduz caos/difusão e protege a articulação com ducking moderado. |
| wild | 0.24 | 0.08 | Reposiciona movimento e deixa margem para intensificar WILD. Prioriza oitavas nítidas no tempo, reduz caos/difusão e protege a articulação com ducking moderado. |
| bloom | 0.18 | 0.14 | Equilibra sustain, abertura tonal e ducking do BLOOM. Prioriza oitavas nítidas no tempo, reduz caos/difusão e protege a articulação com ducking moderado. |
| timeMs | 420 | 250 | Define duração e separação dos ecos para o papel musical. Prioriza oitavas nítidas no tempo, reduz caos/difusão e protege a articulação com ducking moderado. |
| feedback | 0.64 | 0.58 | Controla persistência dos ecos sem depender do teto de feedback. Prioriza oitavas nítidas no tempo, reduz caos/difusão e protege a articulação com ducking moderado. |
| mix | 0.44 | 0.33 | Equilibra presença wet, nível ativo e clareza do dry sem usar trim. Prioriza oitavas nítidas no tempo, reduz caos/difusão e protege a articulação com ducking moderado. |
| lowCutHz | 80 | 150 | Equilibra corpo grave e acúmulo na recirculação. Prioriza oitavas nítidas no tempo, reduz caos/difusão e protege a articulação com ducking moderado. |
| highCutHz | 9000 | 1e+04 | Ajusta cor tonal considerando a abertura efetiva de BLOOM. Prioriza oitavas nítidas no tempo, reduz caos/difusão e protege a articulação com ducking moderado. |
| diffusion | 0.24 | 0.06 | Distingue ecos articulados de nuvens densas, preservando margem manual. Prioriza oitavas nítidas no tempo, reduz caos/difusão e protege a articulação com ducking moderado. |
| chaos | 0.12 | 0 | Separa centro estável de órbita caótica. Prioriza oitavas nítidas no tempo, reduz caos/difusão e protege a articulação com ducking moderado. |
| wobble | 0.18 | 0.06 | Separa deriva lenta de instabilidade de fita/órbita. Prioriza oitavas nítidas no tempo, reduz caos/difusão e protege a articulação com ducking moderado. |
| presence | 0.5 | 0.68 | Ajusta articulação e cor interna do preset. Prioriza oitavas nítidas no tempo, reduz caos/difusão e protege a articulação com ducking moderado. |
| modRateHz | 0.5 | 0.3 | Define velocidade de deriva/movimento conforme a função musical. Prioriza oitavas nítidas no tempo, reduz caos/difusão e protege a articulação com ducking moderado. |
| modDepth | 0.34 | 0.05 | Define amplitude de movimento e mantém margem para WILD/manual. Prioriza oitavas nítidas no tempo, reduz caos/difusão e protege a articulação com ducking moderado. |
| modShape | 2 | 1 | Escolhe o caráter temporal de modulação, em vez de variar apenas profundidade. Prioriza oitavas nítidas no tempo, reduz caos/difusão e protege a articulação com ducking moderado. |
| shimmerFeedback | 0.3 | 0 | Separa halo/sustain de recirculação; zero evita regen em papéis sem shimmer. Prioriza oitavas nítidas no tempo, reduz caos/difusão e protege a articulação com ducking moderado. |
| duckThreshold | -28 | -24 | Prioriza oitavas nítidas no tempo, reduz caos/difusão e protege a articulação com ducking moderado. |
| duckAmount | 0.42 | 0.27 | Equilibra abertura da cauda entre frases e preservação do ataque. Prioriza oitavas nítidas no tempo, reduz caos/difusão e protege a articulação com ducking moderado. |
| outputTrim | 1 | 0 | Retira compensação herdada; balanceamento usa Mix/voicing, trim neutro. |
| wetWidth | 0.82 | 0.52 | Distingue foco stereo de expansão sem carregar o knob no máximo. Prioriza oitavas nítidas no tempo, reduz caos/difusão e protege a articulação com ducking moderado. |


### Frozen Choir
| Parameter | Old | New | Reason |
| --- | --- | --- | --- |
| space | 0.82 | 0.68 | Reposiciona dimensão e deixa margem para expansão de SPACE. Separa sustain de coro do shimmer limpo; regen alto e difusão densa, Freeze manual e margem de macro. |
| bloom | 0.86 | 0.78 | Equilibra sustain, abertura tonal e ducking do BLOOM. Separa sustain de coro do shimmer limpo; regen alto e difusão densa, Freeze manual e margem de macro. |
| timeMs | 1350 | 1050 | Define duração e separação dos ecos para o papel musical. Separa sustain de coro do shimmer limpo; regen alto e difusão densa, Freeze manual e margem de macro. |
| feedback | 0.82 | 0.77 | Controla persistência dos ecos sem depender do teto de feedback. Separa sustain de coro do shimmer limpo; regen alto e difusão densa, Freeze manual e margem de macro. |
| mix | 0.64 | 0.35 | Equilibra presença wet, nível ativo e clareza do dry sem usar trim. Separa sustain de coro do shimmer limpo; regen alto e difusão densa, Freeze manual e margem de macro. |
| lowCutHz | 80 | 220 | Equilibra corpo grave e acúmulo na recirculação. Separa sustain de coro do shimmer limpo; regen alto e difusão densa, Freeze manual e margem de macro. |
| highCutHz | 1.2e+04 | 1.15e+04 | Ajusta cor tonal considerando a abertura efetiva de BLOOM. Separa sustain de coro do shimmer limpo; regen alto e difusão densa, Freeze manual e margem de macro. |
| diffusion | 0.92 | 0.86 | Distingue ecos articulados de nuvens densas, preservando margem manual. Separa sustain de coro do shimmer limpo; regen alto e difusão densa, Freeze manual e margem de macro. |
| wobble | 0.1 | 0.08 | Separa deriva lenta de instabilidade de fita/órbita. Separa sustain de coro do shimmer limpo; regen alto e difusão densa, Freeze manual e margem de macro. |
| presence | 0.7 | 0.6 | Ajusta articulação e cor interna do preset. Separa sustain de coro do shimmer limpo; regen alto e difusão densa, Freeze manual e margem de macro. |
| modRateHz | 0.15 | 0.09 | Define velocidade de deriva/movimento conforme a função musical. Separa sustain de coro do shimmer limpo; regen alto e difusão densa, Freeze manual e margem de macro. |
| modDepth | 0.1 | 0.14 | Define amplitude de movimento e mantém margem para WILD/manual. Separa sustain de coro do shimmer limpo; regen alto e difusão densa, Freeze manual e margem de macro. |
| shimmerAmount | 0.38 | 0.44 | Define quanto o pitch processing participa da identidade. Separa sustain de coro do shimmer limpo; regen alto e difusão densa, Freeze manual e margem de macro. |
| shimmerFeedback | 0.55 | 0.62 | Separa halo/sustain de recirculação; zero evita regen em papéis sem shimmer. Separa sustain de coro do shimmer limpo; regen alto e difusão densa, Freeze manual e margem de macro. |
| duckAmount | 0.1 | 0.18 | Equilibra abertura da cauda entre frases e preservação do ataque. Separa sustain de coro do shimmer limpo; regen alto e difusão densa, Freeze manual e margem de macro. |
| outputTrim | 1.5 | 0 | Retira compensação herdada; balanceamento usa Mix/voicing, trim neutro. |
| wetWidth | 1 | 0.76 | Distingue foco stereo de expansão sem carregar o knob no máximo. Separa sustain de coro do shimmer limpo; regen alto e difusão densa, Freeze manual e margem de macro. |


### Dark Matter
| Parameter | Old | New | Reason |
| --- | --- | --- | --- |
| space | 0.9 | 0.7 | Reposiciona dimensão e deixa margem para expansão de SPACE. Concentra sombra densa com oitava abaixo e pouco movimento; largura moderada distingue Event Horizon. |
| wild | 0.56 | 0.04 | Reposiciona movimento e deixa margem para intensificar WILD. Concentra sombra densa com oitava abaixo e pouco movimento; largura moderada distingue Event Horizon. |
| bloom | 0.7 | 0.3 | Equilibra sustain, abertura tonal e ducking do BLOOM. Concentra sombra densa com oitava abaixo e pouco movimento; largura moderada distingue Event Horizon. |
| timeMs | 1700 | 1150 | Define duração e separação dos ecos para o papel musical. Concentra sombra densa com oitava abaixo e pouco movimento; largura moderada distingue Event Horizon. |
| feedback | 0.88 | 0.77 | Controla persistência dos ecos sem depender do teto de feedback. Concentra sombra densa com oitava abaixo e pouco movimento; largura moderada distingue Event Horizon. |
| mix | 0.62 | 0.34 | Equilibra presença wet, nível ativo e clareza do dry sem usar trim. Concentra sombra densa com oitava abaixo e pouco movimento; largura moderada distingue Event Horizon. |
| lowCutHz | 160 | 60 | Equilibra corpo grave e acúmulo na recirculação. Concentra sombra densa com oitava abaixo e pouco movimento; largura moderada distingue Event Horizon. |
| highCutHz | 4200 | 1500 | Ajusta cor tonal considerando a abertura efetiva de BLOOM. Concentra sombra densa com oitava abaixo e pouco movimento; largura moderada distingue Event Horizon. |
| diffusion | 0.8 | 0.84 | Distingue ecos articulados de nuvens densas, preservando margem manual. Concentra sombra densa com oitava abaixo e pouco movimento; largura moderada distingue Event Horizon. |
| chaos | 0.48 | 0 | Separa centro estável de órbita caótica. Concentra sombra densa com oitava abaixo e pouco movimento; largura moderada distingue Event Horizon. |
| wobble | 0.6 | 0.03 | Separa deriva lenta de instabilidade de fita/órbita. Concentra sombra densa com oitava abaixo e pouco movimento; largura moderada distingue Event Horizon. |
| presence | 0.22 | 0.12 | Ajusta articulação e cor interna do preset. Concentra sombra densa com oitava abaixo e pouco movimento; largura moderada distingue Event Horizon. |
| modRateHz | 0.09 | 0.045 | Define velocidade de deriva/movimento conforme a função musical. Concentra sombra densa com oitava abaixo e pouco movimento; largura moderada distingue Event Horizon. |
| modDepth | 0.52 | 0.04 | Define amplitude de movimento e mantém margem para WILD/manual. Concentra sombra densa com oitava abaixo e pouco movimento; largura moderada distingue Event Horizon. |
| shimmerAmount | 0.12 | 0.13 | Define quanto o pitch processing participa da identidade. Concentra sombra densa com oitava abaixo e pouco movimento; largura moderada distingue Event Horizon. |
| shimmerFeedback | 0.25 | 0.22 | Separa halo/sustain de recirculação; zero evita regen em papéis sem shimmer. Concentra sombra densa com oitava abaixo e pouco movimento; largura moderada distingue Event Horizon. |
| outputTrim | 2 | 0 | Retira compensação herdada; balanceamento usa Mix/voicing, trim neutro. |
| wetWidth | 0.88 | 0.4 | Distingue foco stereo de expansão sem carregar o knob no máximo. Concentra sombra densa com oitava abaixo e pouco movimento; largura moderada distingue Event Horizon. |


### Microgravity Slap
| Parameter | Old | New | Reason |
| --- | --- | --- | --- |
| space | 0.12 | 0.07 | Reposiciona dimensão e deixa margem para expansão de SPACE. Encurta slap e concentra stereo; pouca difusão/modulação preserva percussão. |
| wild | 0.08 | 0.03 | Reposiciona movimento e deixa margem para intensificar WILD. Encurta slap e concentra stereo; pouca difusão/modulação preserva percussão. |
| bloom | 0.06 | 0.04 | Equilibra sustain, abertura tonal e ducking do BLOOM. Encurta slap e concentra stereo; pouca difusão/modulação preserva percussão. |
| timeMs | 95 | 75 | Define duração e separação dos ecos para o papel musical. Encurta slap e concentra stereo; pouca difusão/modulação preserva percussão. |
| feedback | 0.32 | 0.24 | Controla persistência dos ecos sem depender do teto de feedback. Encurta slap e concentra stereo; pouca difusão/modulação preserva percussão. |
| mix | 0.28 | 0.25 | Equilibra presença wet, nível ativo e clareza do dry sem usar trim. Encurta slap e concentra stereo; pouca difusão/modulação preserva percussão. |
| lowCutHz | 140 | 160 | Equilibra corpo grave e acúmulo na recirculação. Encurta slap e concentra stereo; pouca difusão/modulação preserva percussão. |
| highCutHz | 1.05e+04 | 1.25e+04 | Ajusta cor tonal considerando a abertura efetiva de BLOOM. Encurta slap e concentra stereo; pouca difusão/modulação preserva percussão. |
| diffusion | 0.18 | 0.1 | Distingue ecos articulados de nuvens densas, preservando margem manual. Encurta slap e concentra stereo; pouca difusão/modulação preserva percussão. |
| wobble | 0.1 | 0.02 | Separa deriva lenta de instabilidade de fita/órbita. Encurta slap e concentra stereo; pouca difusão/modulação preserva percussão. |
| presence | 0.7 | 0.72 | Ajusta articulação e cor interna do preset. Encurta slap e concentra stereo; pouca difusão/modulação preserva percussão. |
| modDepth | 0.08 | 0 | Define amplitude de movimento e mantém margem para WILD/manual. Encurta slap e concentra stereo; pouca difusão/modulação preserva percussão. |
| modShape | 0 | 1 | Escolhe o caráter temporal de modulação, em vez de variar apenas profundidade. Encurta slap e concentra stereo; pouca difusão/modulação preserva percussão. |
| shimmerFeedback | 0.3 | 0 | Separa halo/sustain de recirculação; zero evita regen em papéis sem shimmer. Encurta slap e concentra stereo; pouca difusão/modulação preserva percussão. |
| duckAmount | 0.1 | 0.04 | Equilibra abertura da cauda entre frases e preservação do ataque. Encurta slap e concentra stereo; pouca difusão/modulação preserva percussão. |
| wetWidth | 0.55 | 0.2 | Distingue foco stereo de expansão sem carregar o knob no máximo. Encurta slap e concentra stereo; pouca difusão/modulação preserva percussão. |


## Loudness

### Before (mediana -31.57 dBFS)
| Preset | RMS ativo dBFS | Peak linear | Δ vs median dB |
| --- | --- | --- | --- |
| Low Orbit | -31.71 | 0.3614 | -0.1477 |
| Tidal Lock | -31.76 | 0.3498 | -0.1934 |
| Trade Winds | -31.95 | 0.3449 | -0.384 |
| Solar Wind | -31.39 | 0.3448 | 0.1749 |
| Escape Velocity | -31.53 | 0.3535 | 0.03453 |
| Zero-G | -31.57 | 0.3422 | 0 |
| Event Horizon | -31.06 | 0.3285 | 0.5056 |
| Glass Transit | -31.71 | 0.3304 | -0.1401 |
| Cassette Moon | -31.44 | 0.3719 | 0.126 |
| Pulsar Eighths | -30.66 | 0.3599 | 0.909 |
| Frozen Choir | -32.66 | 0.2891 | -1.088 |
| Dark Matter | -31.88 | 0.3205 | -0.3091 |
| Microgravity Slap | -30.02 | 0.3844 | 1.547 |


### After (mediana -31.72 dBFS)
| Preset | RMS ativo dBFS | Peak linear | Δ vs median dB |
| --- | --- | --- | --- |
| Low Orbit | -31.28 | 0.3695 | 0.445 |
| Tidal Lock | -32.39 | 0.3304 | -0.6681 |
| Trade Winds | -31.72 | 0.3576 | 0 |
| Solar Wind | -31.17 | 0.3414 | 0.5477 |
| Escape Velocity | -31.34 | 0.3531 | 0.3837 |
| Zero-G | -31.87 | 0.3241 | -0.1445 |
| Event Horizon | -31.89 | 0.3407 | -0.1636 |
| Glass Transit | -31.82 | 0.3445 | -0.09759 |
| Cassette Moon | -31.54 | 0.3648 | 0.1831 |
| Pulsar Eighths | -30.74 | 0.3618 | 0.9789 |
| Frozen Choir | -32.62 | 0.3147 | -0.8964 |
| Dark Matter | -31.84 | 0.3483 | -0.1169 |
| Microgravity Slap | -30.67 | 0.3962 | 1.05 |


## Tail / Width / Tone — After
| Preset | Impulse decay s | Tail energy média | Width wet | Centroid Hz | LF | HF | Shimmer contribution | Motion |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Low Orbit | 1.2 | 6.138e-05 | 0.03763 | 796.6 | 0.3936 | 0.02584 | 0.001023 | 0.3829 |
| Tidal Lock | 6.7 | 0.0002789 | 0.09878 | 1069 | 0.4051 | 0.03893 | 0.00139 | 0.3504 |
| Trade Winds | 3 | 0.0002127 | 0.07271 | 734.4 | 0.4119 | 0.02236 | 0.0011 | 0.3497 |
| Solar Wind | 5 | 0.0004356 | 0.1212 | 1366 | 0.3184 | 0.05516 | 0.1747 | 0.37 |
| Escape Velocity | 3.3 | 0.0001894 | 0.1101 | 932.5 | 0.3841 | 0.03209 | 0.00145 | 0.3216 |
| Zero-G | 11.4 | 0.0005433 | 0.1143 | 931.3 | 0.4185 | 0.0329 | 0.001428 | 0.3542 |
| Event Horizon | 15.4 | 0.0007257 | 0.102 | 731.6 | 0.4296 | 0.02234 | 0.0007411 | 0.3558 |
| Glass Transit | 3 | 0.0002228 | 0.1047 | 1333 | 0.3039 | 0.0523 | 0.2071 | 0.387 |
| Cassette Moon | 2.1 | 0.0001518 | 0.0497 | 795.5 | 0.4408 | 0.02496 | 0.001618 | 0.4224 |
| Pulsar Eighths | 1.6 | 4.716e-05 | 0.04317 | 843.3 | 0.3755 | 0.03101 | 0.0008979 | 0.3893 |
| Frozen Choir | 5.8 | 0.000433 | 0.1113 | 1720 | 0.29 | 0.07377 | 0.3598 | 0.3722 |
| Dark Matter | 6.2 | 0.0004501 | 0.07496 | 759.5 | 0.4668 | 0.02408 | 0.1729 | 0.3581 |
| Microgravity Slap | 0.7 | 5.601e-06 | 0.01617 | 1051 | 0.3534 | 0.04233 | 0.0007644 | 0.3422 |


O ajuste final reduz High Cut efetivo de Event Horizon para cerca de 4,47 kHz e Dark Matter para 3,68 kHz, em vez de confiar nos valores brutos sob BLOOM alto. Os centroids wet médios finais confirmam a direção escura em relação a Solar Wind/Glass Transit/Frozen Choir. Width foi medido com entrada mono para observar abertura criada pelo efeito; não mede todo o comportamento com material stereo. Freeze de performance fornece o estado sustentado/infinite-like sem salvar uma captura vazia no factory preset.

## A/B — mix RMS/pico/transiente e wet tail/width/centroid
| Preset | ΔRMS dB | Peak before → after | Tail energy before → after | Width before → after | Centroid before → after | Transient RMS before → after |
| --- | --- | --- | --- | --- | --- | --- |
| Low Orbit | 0.4374 | 0.3614 → 0.3695 | 0.0001626 → 6.138e-05 | 0.06954 → 0.03763 | 774 → 796.6 | 0.04565 → 0.04713 |
| Tidal Lock | -0.63 | 0.3498 → 0.3304 | 0.0005139 → 0.0002789 | 0.09367 → 0.09878 | 918.4 → 1069 | 0.044 → 0.04252 |
| Trade Winds | 0.2287 | 0.3449 → 0.3576 | 0.0002348 → 0.0002127 | 0.08207 → 0.07271 | 774.8 → 734.4 | 0.04347 → 0.04537 |
| Solar Wind | 0.2176 | 0.3448 → 0.3414 | 0.0006904 → 0.0004356 | 0.08162 → 0.1212 | 824.1 → 1366 | 0.04369 → 0.04497 |
| Escape Velocity | 0.1938 | 0.3535 → 0.3531 | 0.0007536 → 0.0001894 | 0.1112 → 0.1101 | 855.3 → 932.5 | 0.04476 → 0.04455 |
| Zero-G | -0.2998 | 0.3422 → 0.3241 | 0.001576 → 0.0005433 | 0.09907 → 0.1143 | 945.2 → 931.3 | 0.0463 → 0.04118 |
| Event Horizon | -0.8245 | 0.3285 → 0.3407 | 0.001917 → 0.0007257 | 0.1749 → 0.102 | 991.9 → 731.6 | 0.0451 → 0.04292 |
| Glass Transit | -0.1128 | 0.3304 → 0.3445 | 0.0004175 → 0.0002228 | 0.1122 → 0.1047 | 780.8 → 1333 | 0.0422 → 0.04358 |
| Cassette Moon | -0.09823 | 0.3719 → 0.3648 | 0.000219 → 0.0001518 | 0.0759 → 0.0497 | 859.2 → 795.5 | 0.04705 → 0.04611 |
| Pulsar Eighths | -0.08544 | 0.3599 → 0.3618 | 4.658e-05 → 4.716e-05 | 0.07313 → 0.04317 | 649.8 → 843.3 | 0.04583 → 0.04593 |
| Frozen Choir | 0.03662 | 0.2891 → 0.3147 | 0.001158 → 0.000433 | 0.136 → 0.1113 | 974.1 → 1720 | 0.038 → 0.04007 |
| Dark Matter | 0.03684 | 0.3205 → 0.3483 | 0.0007876 → 0.0004501 | 0.1537 → 0.07496 | 1367 → 759.5 | 0.04211 → 0.04407 |
| Microgravity Slap | -0.6528 | 0.3844 → 0.3962 | 1.772e-05 → 5.601e-06 | 0.04711 → 0.01617 | 1013 → 1051 | 0.04881 → 0.05019 |


## Preset transitions
26 cargas consecutivas (ordem normal e reversa), entrada contínua, 2 s por estado, sem reset do DSP. Maior pico: 0.1289; maior diferença RMS adjacente: 1.83 dB. Todos os parâmetros brutos e identidade comparados com carga limpa. Limite conservador de salto: 6 dB; não garante crossfade nem ausência perceptiva de clique.

## Frozen Choir — Freeze de performance
| engage_s | release_s | early_hold_rms | late_hold_rms | peak | final_rms |
| --- | --- | --- | --- | --- | --- |
| 4 | 12 | 0.007164 | 0.007652 | 0.09476 | 1.73e-06 |

O teste captura a cauda aos 4 s, mantém até 12 s e libera. A energia sustentada precisa permanecer entre 0,1× e 5× o RMS inicial do hold, com segurança também após liberação. O preset continua carregando Freeze desligado.

## Safety / headroom
234 renders float verificados. Maior pico incluindo wet-only e contrafactual: 0.3962; maior DC médio registrado: 0.000001. O gate de DC também verifica cada canal separadamente. Sem nonfinite, clipping próximo de 0 dBFS ou crescimento tardio acima dos limites conservadores.
| Preset | SPACE | WILD | BLOOM | Feedback manual | Diffusion manual | Width manual | Shimmer manual | Output Trim dB |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Low Orbit | 0.35 | 0.02 | 0.18 | 0.34 | 0.35 | 0.28 | 0 | 0 |
| Tidal Lock | 0.56 | 0.1 | 0.48 | 0.67 | 0.6 | 0.78 | 0 | 0 |
| Trade Winds | 0.36 | 0.3 | 0.26 | 0.46 | 0.42 | 0.64 | 0 | 0 |
| Solar Wind | 0.58 | 0.58 | 0.46 | 0.64 | 0.48 | 0.78 | 0.21 | 0 |
| Escape Velocity | 0.44 | 0.65 | 0.38 | 0.6 | 0.57 | 0.72 | 0 | 0 |
| Zero-G | 0.72 | 0.06 | 0.68 | 0.7 | 0.82 | 0.86 | 0 | 0 |
| Event Horizon | 0.78 | 0.32 | 0.42 | 0.89 | 0.76 | 0.83 | 0 | 0 |
| Glass Transit | 0.46 | 0.04 | 0.36 | 0.52 | 0.52 | 0.87 | 0.3 | 0 |
| Cassette Moon | 0.32 | 0.46 | 0.2 | 0.44 | 0.23 | 0.48 | 0 | 0 |
| Pulsar Eighths | 0.18 | 0.08 | 0.14 | 0.58 | 0.06 | 0.52 | 0 | 0 |
| Frozen Choir | 0.68 | 0.1 | 0.78 | 0.77 | 0.86 | 0.76 | 0.44 | 0 |
| Dark Matter | 0.7 | 0.04 | 0.3 | 0.77 | 0.84 | 0.4 | 0.13 | 0 |
| Microgravity Slap | 0.07 | 0.03 | 0.04 | 0.24 | 0.1 | 0.2 | 0 | 0 |

Todos os trims finais são neutros. A maioria deixa margem nos três macros e nos controles manuais. Event Horizon usa High Cut manual no mínimo para compensar a abertura tonal de BLOOM e sustentar sua identidade escura; esse endpoint é deliberado, não um ajuste de nível. O CSV effective documenta o resultado real.

## Tests

```text
Internal ctest changing into directory: C:/progs/vst/te2350/build/te2350-vst-ninja-net
Test project C:/progs/vst/te2350/build/te2350-vst-ninja-net
      Start  3: TE2350GoldenReferenceRender
      Start  2: TE2350OfflineReferenceRender
 1/11 Test  #2: TE2350OfflineReferenceRender .....   Passed    0.11 sec
      Start 11: TE2350PresetVoicingTest
 2/11 Test  #3: TE2350GoldenReferenceRender ......   Passed    0.43 sec
      Start  1: TE2350MacroCalibrationTest
 3/11 Test #11: TE2350PresetVoicingTest ..........   Passed  133.45 sec
      Start 15: TE2350FreezeConsistencyTest
 4/11 Test  #1: TE2350MacroCalibrationTest .......   Passed  148.51 sec
      Start 14: TE2350MusicalBehaviourTest
 5/11 Test #14: TE2350MusicalBehaviourTest .......   Passed   10.45 sec
      Start 13: TE2350ReleaseReadinessTest
 6/11 Test #13: TE2350ReleaseReadinessTest .......   Passed    0.64 sec
      Start  8: TE2350HostCompatibilityTest
 7/11 Test  #8: TE2350HostCompatibilityTest ......   Passed    0.97 sec
      Start 12: TE2350PresetWorkflowTest
 8/11 Test #12: TE2350PresetWorkflowTest .........   Passed    0.43 sec
      Start  9: TE2350VST3LoadTest
 9/11 Test  #9: TE2350VST3LoadTest ...............   Passed    0.12 sec
      Start  4: TE2350GoldenReferenceCompare
10/11 Test  #4: TE2350GoldenReferenceCompare .....   Passed    0.07 sec
11/11 Test #15: TE2350FreezeConsistencyTest ......   Passed   32.31 sec

100% tests passed, 0 tests failed out of 11

Total Test time (real) = 165.89 sec

```
