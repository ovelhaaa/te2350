"""Generate the M9.1 report and an unnormalised local A/B index.

Run the M9 executable in focus mode before changing MacroEngine, then the
final focus render and CTest. Checked-in CSVs also reproduce the written report.
NumPy is used exclusively for offline WAV comparisons.
"""
from pathlib import Path
import csv
import html
import math
import shutil
import wave

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "docs/M91"
BUILD = ROOT / "build/m91"
FULL = ROOT / "build/te2350-vst-ninja-net/MacroCalibrationOutput"
OUT.mkdir(parents=True, exist_ok=True)


def load(name, source):
    cached = OUT / (name + ".csv")
    if source.exists():
        shutil.copyfile(source, cached)
    with cached.open(encoding="utf-8") as stream:
        return list(csv.DictReader(stream))


def index(rows):
    return {(row["case"], row["source"]): row for row in rows}


def table(headers, rows):
    return ("| " + " | ".join(headers) + " |\n| "
            + " | ".join(["---"] * len(headers)) + " |\n"
            + "".join("| " + " | ".join(map(str, row)) + " |\n" for row in rows))


def db(after, before):
    return 20 * math.log10(float(after["rms"]) / float(before["rms"]))


def tail(after, before):
    return 100 * (float(after["tail_energy"]) / float(before["tail_energy"]) - 1)


def f(value):
    return f"{float(value):.6g}"


def audio(path):
    with wave.open(str(path), "rb") as stream:
        assert stream.getsampwidth() == 3 and stream.getnchannels() == 2
        raw = np.frombuffer(stream.readframes(stream.getnframes()), dtype=np.uint8).reshape(-1, 3)
        x = raw[:, 0].astype(np.int32) + (raw[:, 1].astype(np.int32) << 8) + (raw[:, 2].astype(np.int32) << 16)
        return (np.where(x & 0x800000, x - 0x1000000, x).astype(np.float64) / 8388608).reshape(-1, 2)


original = index(load("pre_m9_metrics", ROOT / "docs/M9/before_metrics.csv"))
m9 = index(load("m9_metrics", BUILD / "m9/audio_metrics.csv"))
initial = index(load("feedback_only_metrics", BUILD / "initial/audio_metrics.csv"))
final = index(load("m91_metrics", BUILD / "final/audio_metrics.csv"))
diagnostic = index(load("counterfactual_metrics", BUILD / "diagnostic/audio_metrics.csv"))
targets = load("counterfactual_targets", BUILD / "diagnostic/counterfactual_targets.csv")
authority = load("manual_authority", FULL / "manual_authority.csv")
surface = load("feedback_surface", FULL / "feedback_surface.csv")
motion = load("wild_controls", BUILD / "final/wild_controls.csv")

lines = ["# M9.1 — Manual Authority & Preset Preservation\n", """
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
"""]
lines.append(table(["Parameter", "manual", "effective macros=0", "PASS/FAIL"],
                   [[r["parameter"], r["manual"], r["effective_macros_zero"], r["result"]] for r in authority]))
lines.append("\n## Feedback interaction\n\nSuperfície real manual × WILD × BLOOM; valores em unidades físicas.\n")
lines.append(table(["Manual", "WILD", "BLOOM", "Effective Feedback"],
                   [[r["manual"], r["wild"], r["bloom"], r["effective_feedback"]] for r in surface]))
lines.append("""
## Presets — M9 vs M9.1

Nenhum valor de preset foi editado. ΔRMS positivo indica M9.1 mais alto.
Também é exibida a referência pré-M9 para evitar confundir preservação do voicing
original com preservação dos desvios acidentais da M9.
""")
names = ["Low Orbit", "Tidal Lock", "Trade Winds", "Solar Wind", "Escape Velocity", "Zero-G",
         "Event Horizon", "Glass Transit", "Cassette Moon", "Pulsar Eighths", "Frozen Choir",
         "Dark Matter", "Microgravity Slap"]
data = []
for i, name in enumerate(names):
    for source in ("1", "2"):
        key = (f"preset_{i}", source)
        a, b, c = original[key], m9[key], final[key]
        data.append([f"{i} {name}", source, f"{db(c,b):+.3f}",
                     f"{f(b['peak'])} → {f(c['peak'])}",
                     f"{f(b['tail_energy'])} → {f(c['tail_energy'])}",
                     f"{db(b,a):+.3f} → {db(c,a):+.3f}", f"{tail(c,a):+.1f}%"])
lines.append(table(["Preset", "Source", "ΔRMS M9→M9.1 dB", "Peak M9→M9.1",
                    "Tail energy M9→M9.1", "ΔRMS pré-M9: M9→M9.1 dB", "Tail Δ pré-M9"], data))

lines.append("""
## Causas medidas, antes da correção de Time

Counterfactuals offline: após corrigir Feedback, mas antes de preservar Time,
restaurou-se um grupo de alvos antigo por vez através de inversão da curva APVTS
atual. Macros enviados ao core e demais alvos foram mantidos. Não são novos
presets nem compensações do plugin. O CSV wanted/reached identifica alvos
inatingíveis na curva corrente; esses resultados são aproximações, não restores
exatos. SPACE Time/Tone inclui Time, Low Cut e High Cut; não atribui sozinho toda
a diferença ao tempo. A intervenção final mudou exclusivamente Time e confirmou
a causa em renders do plugin. Os efeitos isolados não podem ser somados.
""")
data = []
for i in (0, 1, 2, 3, 5, 6, 7, 10, 11, 12):
    base = initial[(f"preset_{i}", "1")]
    for group in ("wild_feedback", "bloom_feedback", "space_shimmer", "bloom_shimmer",
                  "space_spatial", "wild_modulation", "space_time_tone", "bloom_tone_mix"):
        key = (f"restore_{group}_preset_{i}", "1")
        result = diagnostic[key]
        data.append([i, group, f"{db(result,base):+.3f}", f"{tail(result,base):+.1f}%"])
lines.append(table(["Preset", "Restored group", "ΔRMS vs feedback-only dB", "Tail Δ"], data))
limited = [r for r in targets if r["result"] != "REACHED"]
lines.append(f"\nAlvos limitados: {len(limited)}/{len(targets)} medições; detalhe em `counterfactual_targets.csv`.\n")
lines.append("""
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
""")
lines.append(table(["WILD", "Feedback", "Chaos", "Wobble", "Rate Hz", "Depth", "Instability"],
                   [[r[k] for k in ("wild", "feedback", "chaos", "wobble", "mod_rate_hz", "mod_depth", "instability")]
                    for r in motion]))
data = []
for source in range(5):
    for level in (0, 50, 75, 100):
        key = (f"wild_{level}", str(source))
        a, b = m9[key], final[key]
        data.append([source, level, f"{f(a['modulation_proxy'])} → {f(b['modulation_proxy'])}",
                     f"{f(a['pitch_variance_proxy'])} → {f(b['pitch_variance_proxy'])}",
                     f"{f(a['width'])} → {f(b['width'])}",
                     f"{f(a['tail_energy'])} → {f(b['tail_energy'])}", b["adjacent_relative_difference"]])
lines.append(table(["Source", "WILD %", "Modulation proxy M9→M9.1", "Pitch variance proxy M9→M9.1",
                    "Width M9→M9.1", "Tail energy M9→M9.1", "Wave delta vs previous level"], data))

# Quantify direct M9/M9.1 waveform differences as well as adjacent WILD levels.
wave_path = OUT / "wild_waveform_comparison.csv"
if (BUILD / "m9/wild_75_source4.wav").exists() and (BUILD / "final/wild_75_source4.wav").exists():
    with wave_path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(["source", "wild", "m9_to_m91_relative_delta", "m9_adjacent_delta", "m91_adjacent_delta"])
        for source in range(5):
            prev_a = prev_b = None
            for level in (0, 50, 75, 100):
                a = audio(BUILD / f"m9/wild_{level}_source{source}.wav")
                b = audio(BUILD / f"final/wild_{level}_source{source}.wav")
                def delta(x, y):
                    return np.sqrt(np.sum((x-y)**2) / max(1e-20, np.sum(y*y)))
                writer.writerow([source, level, delta(b,a),
                                 delta(a,prev_a) if prev_a is not None else 0,
                                 delta(b,prev_b) if prev_b is not None else 0])
                prev_a, prev_b = a, b

lines.append("""
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
""")
log = BUILD / "regression-final.log"
if log.exists():
    shutil.copyfile(log, OUT / "regression-final.txt")
lines.append("```text\n" + (OUT / "regression-final.txt").read_text(encoding="utf-8") + "\n```\n")
lines.append("""
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
""")
(OUT / "M91-Manual-Authority.md").write_text("\n".join(lines), encoding="utf-8")

cases = [(f"wild_{level}", source) for level in (0, 50, 75, 100) for source in range(5)]
cases += [(f"preset_{i}", source) for i in range(13) for source in (1, 2)]
page = ["<!doctype html><meta charset='utf-8'><title>M9.1 A/B</title>",
        "<style>body{font:16px system-ui;background:#10141c;color:#eef;margin:32px}td,th{padding:12px;border-bottom:1px solid #345}audio{width:340px}</style>",
        "<h1>M9 / M9.1</h1><p>Sem normalização. 3 s de entrada + 9 s de cauda.</p><table><tr><th>Caso/fonte</th><th>M9</th><th>M9.1</th></tr>"]
for case, source in cases:
    page.append(f"<tr><td>{html.escape(case)} / {source}</td>"
                + "".join(f'<td><audio controls preload="none" src="{(BUILD / version / (case + "_source" + str(source) + ".wav")).as_uri()}"></audio></td>'
                          for version in ("m9", "final")) + "</tr>")
page.append("</table>")
BUILD.mkdir(parents=True, exist_ok=True)
(BUILD / "listen.html").write_text("\n".join(page), encoding="utf-8")
print("Generated:", OUT / "M91-Manual-Authority.md")
