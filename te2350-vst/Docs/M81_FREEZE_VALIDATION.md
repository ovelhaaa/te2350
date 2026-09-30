# M8.1 — Freeze independente de Atmos

Validação em Windows/MinGW, build Release CMake em `build/te2350-vst-ninja-net`, em 2026-09-30. Baseline: `9e2ce17 (M8 integrada de origin/main)`.

## Causa e correção

O ganho de feedback 0,995 era aplicado depois de várias perdas: leitura Hermite com fator 15/16, mistura LP/HP/pitch normalizada, formant LP com fator 0,94, trim de Bloom, DC blocker e saturação. Ganho próximo de unity no fim dessa cadeia não conserva o campo. A matriz reproduziu colapso com Atmos OFF. No baseline M8 com Atmos ON, a retenção ficou em 0,106–0,297 nesta configuração; a FDN oferecia sustentação própria que não existia no caminho principal.

O retorno de retenção agora recircula o campo **na escrita do delay principal**, sem ativar FDN. Usa leitura inteira do campo armazenado, ganho subunitário 0,9995 na referência de 500 ms, mistura LP de 0,006, remoção lenta de LF/DC de 0,0005 e saturação suave. Mantém 0,004 de participação do retorno normal, incluindo shimmer/regen condicionados pela M8. As perdas são proporcionais à duração da volta para delays abaixo de 500 ms; delays maiores usam as perdas de referência. Coeficientes dos dois filtros são ajustados ao sample rate na inicialização.

A crossfade existente controla a escrita; nenhuma cabeça, filtro ou buffer é reinicializado nas transições. Conditioning M8, ducking, supressão suave de entrada e ganho efetivo existentes foram preservados. O helper de retenção usa noinline para manter sua aritmética fora do footprint de registradores/código do modo normal. Custo adicional somente enquanto a crossfade de Freeze é diferente de zero; mais 16 bytes no contexto do core, sem aumentar o pool de delays.

## Matriz e critérios

48 renders determinísticos de 30 s, 48 kHz, delay 500 ms: Atmos OFF/ON × -1 oct/5th/+1 oct × Shimmer 0,15/0,90 × Regen 0,10/0,90 × Bloom 0,10/0,90. Excitação multitone 220/880/7000 Hz e ruído determinístico durante 3 s; Freeze de 3 a 28 s; release de 28 a 30 s. Feedback normal 0,65, diffusion 0,45, mod depth 0,15; Regen ativa a malha octave feedback. O piso de Tail corresponde ao mapeamento desktop de Bloom nos dois pontos, em 500 ms.

RMS estéreo por janelas de 1 s. Retenção = RMS [24,25) / RMS [4,5): janelas totalmente em Freeze, separadas por 20 s. Energia = quadrado dessa razão. `captured_rms_ratio`/`captured_energy_ratio` usam [2,3), a última janela wet antes da captura, como referência adicional. LF/HF são **proxies de energia** por filtros de análise de primeira ordem (<200 Hz e >6 kHz); não são integrais de FFT com cortes ideais.

Gates: retenção 0,65–1,10; crescimento máximo entre janelas de hold <1,15; peak <0,95; HF final não excede HF inicial em mais de 0,02; janela inicial não silenciosa. A margem inferior 0,65 fica abaixo do pior baseline corrigido 0,819 e evita a aprovação de tails quase extintas. OFF e ON têm os mesmos limites de segurança, sem exigir áudio ou retenção idênticos.

| Medida | Atmos OFF | Atmos ON |
| --- | --- | --- |
| Retenção baseline M8 | 0,000031–0,000144 | 0,105797–0,297203 |
| Retenção corrigida | 0,820335–0,841698 | 0,819105–0,840721 |
| Energia final / inicial em hold | 0,672950–0,708455 | 0,670932–0,706812 |
| RMS final / wet antes da captura | 0,823805–0,896022 | 0,826900–0,901265 |
| Energia final / wet antes da captura | 0,678655–0,802855 | 0,683763–0,812279 |
| Peak durante Freeze (amplitude linear FS) | 0,161636–0,186498 | 0,163954–0,189275 |
| Crescimento das janelas / inicial | 1,000000–1,016353 | 1,000000–1,020563 |
| HF ratio inicial → final | 0,005157–0,007083 → 0,003951–0,005790 | 0,005169–0,007103 → 0,003971–0,005815 |
| LF ratio inicial → final | 0,334972–0,378273 → 0,351759–0,398583 | 0,334629–0,378030 → 0,351278–0,398213 |

Os dois modos conservam o campo, com leve escurecimento e sem crescimento sustentado. OFF mantém o caráter focado do delay/pitch; ON continua somando o campo estéreo da FDN e sua difusão. Isso descreve a topologia; não é uma conclusão de teste auditivo cego.

Dados completos por configuração: [baseline](M81/freeze_baseline.csv) e [corrigido](M81/freeze_corrected.csv). Janelas por segundo: `build/te2350-vst-ninja-net/FreezeConsistencyOutput/freeze_windows.csv`, regeneradas pelo CTest.

## Transições e entrada nova

Entradas OFF→ON e ON→OFF medidas nos primeiros 25 ms: maior diferença entre amostras 0,03399 e 0,01140 FS, respectivamente. Os gates comparam esses passos com o conteúdo imediatamente antes da transição (limite 1,5×, piso 0,005); não apareceu excesso de descontinuidade. Comparação byte a byte do contexto no setter verifica ausência de reset, inclusive estados estéreo/pitch/filtros. Isso constitui verificação objetiva de descontinuidade; a percepção de clicks depende também de audição.

Oito renders adicionais de 25 s, delays 20/50 ms em 44,1/48/96/192 kHz, Atmos OFF, +1 oct, Shimmer/Regen 0,9. Retenção 0,835105–0,854534. Comparação de dois cores idênticos, um recebendo nova entrada entre 6 e 9 s: delta RMS do campo armazenado de 0,000405–0,000495 relativo ao campo sem injeção (máximo 0,050%). A entrada nova não substituiu a captura. Gate de delta <0,05.

## Regressão JUCE/VST3 e golden

Todos os 13 CTests passaram: OfflineReferenceRender, GoldenReferenceRender, GoldenReferenceCompare, CoreControlTest, PluginSmokeTest, CalibrationTest, HostCompatibilityTest, VST3LoadTest, UIRenderTest, PresetWorkflowTest, ReleaseReadinessTest, MusicalBehaviourTest e FreezeConsistencyTest. Os seis executáveis pedidos estão incluídos. O CTest standalone `M8CoreTest` também passou (1/1), incluindo malloc/calloc/realloc/free rastreados na malha do core. O teste remoto de Bloom/ducking foi ajustado apenas para fornecer o argumento de fallback exigido pela API `MacroEngine::getEffectiveValue`; o comportamento da M8 não foi alterado.

GoldenReferenceCompare é o scaffold existente de igualdade entre dois renders do core; adicionalmente, o render normal foi comparado com o arquivo produzido com o **baseline M8 remoto**, antes da alteração: 384.000 bytes idênticos, erro RMS/peak zero, diferença espectral zero. SHA-256: `b949b61097048401472cdde46366250e717fb5ea94c27a6d5b4bce655494c234`. Nenhuma golden foi atualizada. A diferença de Freeze é intencional e quantificada pela matriz acima; não foi ocultada por atualização de referência.

O guard existente de alocações C++ no callback foi ampliado para alternar `freezeEngage` e Atmos OFF/ON, incluindo bypass. Resultado: zero alocações rastreadas. Auditoria do core: nenhuma alocação, lock ou trigonometria por sample introduzida; todos os buffers são preparados previamente. O standalone M8 confirmou também zero malloc/calloc/realloc/free na callback do core. A instrumentação não intercepta universalmente alocações internas de bibliotecas externas.

## Performance

Benchmark pareado do core: entrada pré-calculada, 7 repetições de 8 s por configuração, mediana, baseline e correção no mesmo executável, ordem alternada e afinidade do thread a um único processador. O baseline usa o código de `9e2ce17`, compilado com o mesmo GCC/O3 e símbolos renomeados; helpers próprios preservam o layout de contexto de cada versão. Coletas em processos separados apresentaram ruído de carga/frequência; a comparação pareada abaixo é a usada. Tempos em ns por frame mono→estéreo.

| Atmos / Freeze | Baseline M8 | M8.1 | Diferença |
| --- | ---: | ---: | ---: |
| OFF / normal | 315,443 | 316,646 | +0,38% |
| ON / normal | 366,585 | 365,575 | -0,28% |
| OFF / Freeze | 311,448 | 321,122 | +3,11% |
| ON / Freeze | 365,712 | 370,992 | +1,44% |

Sem regressão relevante do modo normal nesta máquina. O benchmark de callback JUCE existente também passou nos modos Hardware/Studio e em 192 kHz.

## Compatibilidade e reprodução

**ParameterIDs, ranges, defaults, presets, automation, state recall/migration, identidade VST3 e UI inalterados.** Apenas core DSP, testes e registro CTest foram modificados. PresetWorkflow, HostCompatibility e VST3Load validaram o comportamento existente. Nenhum parâmetro novo.

```powershell
cmake --build build/te2350-vst-ninja-net --target TE2350FreezeConsistencyTest TE2350MusicalBehaviourTest TE2350HostCompatibilityTest TE2350PresetWorkflowTest TE2350GoldenReference TE2350OfflineReference TE2350VST3LoadTest TE2350UIRender TE2350ReleaseReadinessTest TE2350CalibrationTest TE2350CoreControlTest TE2350PluginSmokeTest -j 4
ctest --test-dir build/te2350-vst-ninja-net --output-on-failure
./build/te2350-vst-ninja-net/TE2350FreezeConsistencyTest.exe --benchmark
```

`--audit` executa a matriz sem os gates de retenção, para medição de baseline. Os CSVs são escritos no diretório de trabalho. O benchmark pareado desta sessão está em `build/bench-paired.cpp`, `build/bench-base.c`, `build/bench-new.c` e `build/bench-paired.exe`; `--benchmark` oferece a medição individual portátil do mesmo cenário.

A validação é de regressão automatizada neste ambiente, sem equivaler a certificação em todos os DAWs ou aprovação auditiva.
