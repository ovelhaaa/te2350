# M8 — Shimmer Feedback Conditioning + Adaptive Ducking

Implementação local sobre `ovelhaaa/te2350`, commit base `d4759c0adc17ab5354346059c94ab174a3d60b0b`. Nenhuma instalação de plugin foi feita. Core validado; aprovação de release depende dos testes JUCE/VST3 e da escuta dos WAVs A/B.

## Shimmer: auditoria e topologia

O baseline não apresentou runaway nos 90 casos normais. O problema medido foi energia aguda nas caudas de quinta/+oitava e uma segunda voz pitchada que entrava depois do condicionador principal. `shimmerFeedback` é mapeado pelo wrapper para `octave_feedback_amount`; não se trata apenas do ganho da voz paralela.

Antes: voz shimmer → HP/LP/air/saturação → wet e retorno ponderado à malha; segunda voz pitchada → wet direto e blend na malha depois do filtro/SVF/DC/saturação principal. A segunda voz não recebia damping progressivo próprio.

Depois: as contribuições diretas continuam usando a mesma voz e os mesmos ganhos. Apenas as duas cópias destinadas à malha recebem estados independentes de HP/LP, blend progressivo e trim. Não houve troca do pitch shifter, UI, efeitos grandes ou limiter novo.

O peso de condicionamento usa `(0.25 * feedback + 0.75 * shimmerFeedback)^2`, com acréscimo suave de 0,12 em Freeze. O trim usa `strength * (0.12 + 0.08 * shimmerAmount + 0.06 * Bloom + 0.05 * Freeze)`. Em Amount 0,75/Regen 0,90/Bloom 0,90, o ganho da cópia recirculada aproxima-se de 0,810 no modo normal e 0,736 em Freeze. São curvas contínuas e saturadas.

Coeficientes de referência a 48 kHz: LP 0,72→0,34 e HP 0,025→0,004, interpolados pelo controle de intervalo. −1 oitava favorece remoção de graves; quinta recebe damping intermediário; +1 oitava recebe mais damping agudo. Regen altera a proporção da cópia filtrada, reduzindo progressivamente a abertura efetiva. Coeficientes usam transformação racional para sample rate, atualização a cada 64 samples e smoothing de aproximadamente 12 ms. Nenhuma função trigonométrica nova no callback.

### Métricas antes/depois

Médias dos 10 casos por intervalo com Regen 0,90 (2 Amounts × 5 fontes), 48 kHz, Bloom 0,90. A razão HF da cauda usa a parte após 1 s; a razão global inclui o ataque. Valores em porcentagem da energia espectral:

| Intervalo | HF cauda >8 kHz (%) | HF global (%) | LF <150 Hz (%) | Centroide (Hz) | RMS |
| --- | --- | --- | --- | --- | --- |
| −1 oitava | 0.00341 → 0.00508 | 22.806 → 23.255 | 21.011 → 20.107 | 4851.2 → 4931.8 | 0.013475 → 0.013419 |
| quinta | 0.41852 → 0.02789 | 25.956 → 25.902 | 17.926 → 17.964 | 5325.9 → 5320.5 | 0.013254 → 0.013211 |
| +1 oitava | 0.98285 → 0.04002 | 27.494 → 27.152 | 17.892 → 17.970 | 5601.5 → 5538.2 | 0.013940 → 0.013794 |

A redução HF na cauda é ~93% na quinta e ~96% em +1 oitava. Em −1 oitava, a razão LF cai ~4%; a razão HF sobe ligeiramente pela remoção de graves, sem evidência de runaway. O peak máximo da matriz permanece 0,477719. A redução do RMS global é pequena, preservando a energia inicial. Todos os valores individuais, janelas RMS por segundo e crescimento da cauda estão em `baseline/shimmer.csv` e `final/shimmer.csv`.

Não é correto afirmar que havia runaway no baseline: os dados não sustentam isso. O benefício aqui é o controle espectral da recirculação, e não a correção de uma explosão de nível observada.

## Freeze

O baseline perdeu 97–98% do RMS entre a janela de 2 s e o fim de 22 s. Aumentar damping somente no pitch agravaria essa perda. Por isso a mudança necessária também toca `dsp_fdn.c/.h`, mantendo o FDN existente: a crossfade de Freeze reduz entrada nova e profundidade de modulação, preserva a mistura Hadamard normalizada, mantém 0,1% de absorção por circulação e usa ganho subunitário 0,9999. Os filtros continuam acompanhando o sinal para a saída de Freeze. A malha pitchada permanece condicionada, sem bloquear toda evolução espectral.

Resultado: nos 9 casos (3 intervalos × 3 rates), RMS final/janela de 2 s passa de 0,0163–0,0305 para 0,8974–0,9455. Energia final fica em ~80,5–89,4% da energia da janela inicial, sem crescimento contínuo. O campo capturado permanece presente durante mais de 20 s.

Esse resultado foi validado com o FDN ligado. Com `atmosFdnOn` desligado, a retenção do campo FDN não existe; o caminho de delay/pitch conserva a topologia anterior e não recebeu um redesenho de Freeze. Essa configuração não está certificada como Freeze estacionário.

## Ducking

Antes: envelope compartilhado, threshold linear, multiplicação por 4 até clamp e redução direta do wet, sem smoothing próprio. Com Amount 0,90, seno sustentado atingia ganho 0,10. Além disso, o ducking retirava até 0,22 × envelope × Amount do ganho de feedback, consumindo energia da cauda.

Depois: detector combina envelope e transient hint, passa pelo mesmo threshold e preserva sua ação. A resposta normalizada combina peso base de 0,48 com até 0,40 de componente transiente. A profundidade continua proporcional a Duck Amount. Sustain mantém ducking moderado; um transiente recebe profundidade adicional. Não foram criados parâmetros.

O estado de redução tem attack de 0,6 ms e release dependente da redução atual, com coeficientes limitados às constantes de 65–180 ms. Como o release é adaptativo e o detector também decai, recovery medido de 90% pode ser maior que essas constantes. Ducking atua apenas na saída wet: removeu-se a redução de feedback antiga para preservar tail e voz pitchada após o ataque.

Bloom recebe somente o headroom restante de Duck Amount: `duck + 0.45 * Bloom² * (1 - duck)`. Assim Bloom alto não produz clamp precoce nem elimina o restante da faixa útil. A alteração em MacroEngine se restringe à contribuição de duckAmount; definições, parâmetros, ranges e defaults não mudaram.

### Métricas antes/depois

48 kHz, threshold −24 dB, Amount 0,90, Shimmer 0,75/Regen 0,90/Bloom 0,90. Pulsos isolados/sequência têm 2 ms; sequência espaçada 250 ms. Attack é o tempo até 90% da redução máxima daquele evento; recovery é retorno a 10% da redução após o último evento. Pumping é desvio padrão do ganho na janela 2,5–3 s (inclui os ataques no caso da sequência).

| Fonte | Ganho mínimo | Attack90 (ms) | Recovery90 (ms) | Pumping | RMS tail/ref |
| --- | --- | --- | --- | --- | --- |
| Pulso isolado | 0.1000 → 0.5645 | 0.1875 → 0.6875 | 110.6042 → 247.2917 | 0.0000 → 0.0000 | 1.0116 → 1.0000 |
| Sequência | 0.1000 → 0.5613 | 0.1875 → 0.7083 | 110.6250 → 247.0833 | 0.3625 → 0.1466 | 0.9948 → 1.0000 |
| Seno sustentado | 0.1000 → 0.5523 | 0.9792 → 1.1250 | 79.4167 → 215.4375 | 0.0000 → 0.0001 | 0.9566 → 1.0000 |
| Ataque + sustain | 0.1000 → 0.5407 | 0.6250 → 0.8958 | 42.8542 → 146.2292 | 0.0043 → 0.0001 | 0.9787 → 1.0000 |

Threshold −48/−24/−12 dB foi testado nos três sample rates. O teste de pulso 0,20 demonstra profundidade ordenada e nenhuma redução relevante abaixo do threshold −12 dB. Tail/ref compara a saída ducked com uma referência sem ducking na janela 4–5 s: no novo core a razão é 1,000 nos casos medidos. CSV completo em `baseline/duck.csv` e `final/duck.csv`.

## Tests e limites da validação

- Baseline capturado antes das mudanças de DSP; DLL de baseline compilada do commit citado. Nenhuma golden reference foi atualizada.
- Matriz shimmer: 90 casos por versão, −1 oitava/quinta/+1 oitava × Amount 0,35/0,75 × Regen 0,30/0,75/0,90 × seno/impulso/ruído curto/percussão sintética/harmônicos, 48 kHz, 20 s.
- Freeze: 9 casos por versão, 44,1/48/96 kHz, 22 s, captura em 1 s, >20 s de estado congelado, FDN ligado.
- Ducking: 36 casos por versão, 44,1/48/96 kHz × −48/−24/−12 dB × pulso/sequência/sustain/ataque+sustain, 5 s, Shimmer/Regen/Bloom altos. Cada caso compara referência sem ducking.
- Métricas espectrais: STFT Hann de 4096 samples, hop 2048, soma da potência dos dois canais; centroide, energia >8 kHz e <150 Hz, peak, RMS, janelas RMS e crescimento final.
- `MusicalBehaviourTest.cpp` chama a suíte comum `M8Behaviour.h`: matriz de estabilidade por intervalo/rate, Freeze ≥20 s, retenção e não crescimento de energia, HF em Freeze, HF low/high Regen, attack/recovery, sustain/pumping, threshold e extremos de aritmética Q31. Acrescentou-se teste de faixa útil Bloom+Duck no plugin.
- Runner independente JUCE: CMake/CTest em `te2350-vst/Tests/M8`, compilado com GCC/G++ 14.2, Release. Todas as verificações do runner passaram, incluindo allocator audit. Logs em `core-tests.txt` e `allocation-tests.txt`.
- Repetições de renders são bit a bit idênticas.
- Build JUCE 8.0.8/VST3 não executável neste ambiente: juceaide rejeita MinGW, e não foi encontrado MSVC/Windows SDK nem distro WSL instalada. O teste integrado de MacroEngine e os testes antigos de plugin/presets/host/golden/state recall foram escritos/preservados, mas não executados. Não há VST3 novo certificado.
- UBSan foi tentado, mas o toolchain não contém `libubsan`; não se reivindica validação por sanitizer.
- WAVs A/B são entregues para escuta humana. A avaliação feita nesta execução é objetiva; não se reivindica audição subjetiva, ausência de todos os clicks de automação, ou identidade musical certificada.

## Realtime e Q31

O runner usa wrapping GNU de malloc/calloc/realloc/free com flag ativa exclusivamente em `te2350_process`. Resultado: zero alocações/liberações no callback em dezenas de milhões de samples. Buffers de medição, FFT, WAV, filesystem e alocações dos testes ficam fora do core. Não há locks ou filesystem nos caminhos do core inspecionados. Nenhuma função trigonométrica nova por sample; a conversão de coeficientes usa somente operações racionais a cada 64 samples.

`q31_mul` conserva o caso saturado MIN×MIN e produtos int64; `q31_add_sat`/`q31_sub_sat` saturam as somas e diferenças novas. O numerador da normalização transiente usa int64 e nunca excede MAX²; o denominador é calculado em int64. Filtros one-pole usam as primitivas saturadas existentes. Freeze permanece subunitário; os mixes e gains continuam limitados a Q31. Essa é uma auditoria do código e teste dos extremos, sem cobertura completa por sanitizer.

Benchmark pareado, 12 medições quentes por versão/caso, renders de 10 s, mediana, sem FFT no trecho medido:

| Freeze | Duck | CPU baseline (s) | CPU novo (s) | Variação (%) | Tempo real consumido (%) |
| --- | --- | --- | --- | --- | --- |
| False | 0 | 0.149994 | 0.153196 | 2.13 | 1.53 |
| False | 0.9 | 0.150138 | 0.152649 | 1.67 | 1.53 |
| True | 0.9 | 0.127294 | 0.129718 | 1.90 | 1.30 |

Não foi observada regressão significativa no benchmark desktop (~1,7–2,1%). Isso não certifica headroom/underruns no RP2350; hardware embarcado e callback do plugin completo continuam pendentes.

## Compatibility

ParameterIDs, ranges, defaults, factory presets, código de user presets, state migration/serialização, layouts de automação e identidade VST3 não foram alterados. Os estados adicionados ao core são internos e inicializados; não são serializados como preset. O pool de delays não aumentou. A mudança audível decorre do DSP interno e da contribuição Bloom→duckAmount refinada.

Essa preservação foi confirmada pelo escopo do diff, não por execução de recall/automação em host. Verificar PresetWorkflowTest/HostCompatibilityTest/GoldenReferenceTest e VST3LoadTest em um ambiente JUCE suportado antes do merge/release. Se as golden references divergirem, revisar a diferença esperada; não atualizá-las automaticamente.

## Reprodução e artefatos

Aplicar `te2350-M8.patch` sobre o commit base. O ZIP de fontes contém somente os arquivos modificados/adicionados com caminhos relativos ao repositório.

```sh
git apply --check te2350-M8.patch
git apply te2350-M8.patch
cmake -S te2350-vst/Tests/M8 -B build/m8-core -DCMAKE_BUILD_TYPE=Release
cmake --build build/m8-core
ctest --test-dir build/m8-core --output-on-failure
python te2350-vst/Tests/m8_measure.py CAMINHO_M8Render OUTPUT_DIR
python te2350-vst/Tests/m8_benchmark.py BASELINE_DLL NOVO_DLL performance.csv
```

Para gerar baseline, compilar o bridge `M8Render.c` com as sete fontes C do commit base, `-shared -O3 -DTE_MAIN_DELAY_SIZE=524288`, sem `M8_ADAPTIVE`. Para o novo core, adicionar `-DM8_ADAPTIVE`. Python/NumPy são dependências somente da medição offline. O runner core não depende de Python nem JUCE.

`m8-AB.zip` contém métricas/logs, WAVs baseline/final e pares sequenciais A/B: primeiros 20 s baseline, 0,5 s silêncio, próximos 20 s novo (para ducking: 5 s + silêncio + 5 s). Nenhuma normalização de loudness foi aplicada. `new/` intermediário é uma experiência sem retenção FDN e não faz parte da entrega.
