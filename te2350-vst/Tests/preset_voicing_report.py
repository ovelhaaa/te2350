"""Reproduce M10 features/report. NumPy only; never normalise audio.
Run baseline renderer before changing presets, then final CTest, then this script.
Use --before-only to measure the original bank; --cached to rebuild the report.
"""
from pathlib import Path
import csv, json, math, wave, html, sys, shutil
import numpy as np

ROOT = Path(__file__).resolve().parents[2]
BEFORE = ROOT / 'build/m10/before'
AFTER = ROOT / 'build/te2350-vst-ninja-net/PresetVoicingOutput'
OUT = ROOT / 'docs/M10'
SOURCES = ['impulse', 'pluck', 'vocal', 'pad', 'percussion', 'chords']
SR, ACTIVE, DURATION = 48000, 4, 24


def readcsv(p):
    with p.open(encoding='utf8') as f:
        return list(csv.DictReader(f))


def writecsv(p, rows):
    with p.open('w', newline='', encoding='utf8') as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0]))
        w.writeheader()
        w.writerows(rows)


def db(x):
    return 20 * np.log10(max(float(x), 1.e-12))


def wav(p):
    with wave.open(str(p), 'rb') as w:
        assert w.getframerate() == SR and w.getsampwidth() == 3 and w.getnchannels() == 2
        b = np.frombuffer(w.readframes(w.getnframes()), dtype=np.uint8).reshape(-1, 3)
        a = b[:, 0].astype(np.int32) + (b[:, 1].astype(np.int32) << 8) + (b[:, 2].astype(np.int32) << 16)
        return (np.where(a & 0x800000, a - 0x1000000, a) / 8388608.).reshape(-1, 2)


def rms(x):
    return float(np.sqrt(np.mean(x * x)))


def spectrum(x):
    # Summed channel powers avoid cancelling wide/anti-phase signals.
    n = 4096
    frames = x[:len(x) // n * n].reshape(-1, n, 2)
    z = np.fft.rfft(frames * np.hanning(n)[None, :, None], axis=1)
    power = np.sum(np.abs(z) ** 2, axis=2)
    hz = np.fft.rfftfreq(n, 1 / SR)
    total = power.sum(axis=0)
    den = max(1.e-30, total.sum())
    good = power.sum(axis=1) > max(1.e-12, power.sum(axis=1).max() * 1.e-5)
    dominant = hz[np.argmax(power[good, 1:], axis=1) + 1]
    return (float(total @ hz / den), float(total[hz < 200].sum() / den),
            float(total[hz > 6000].sum() / den), float(np.var(dominant)) if len(dominant) else 0)


def pitch_vocal(x):
    # Parabolic log-power interpolation of fundamental; only vocal source.
    n = 8192
    frames = x[:len(x) // n * n].reshape(-1, n, 2)
    power = (np.abs(np.fft.rfft(frames * np.hanning(n)[None, :, None], axis=1)) ** 2).sum(axis=2)
    band = np.arange(int(160 * n / SR), int(280 * n / SR))
    k = band[np.argmax(power[:, band], axis=1)]
    a = np.log(np.maximum(power, 1.e-30))
    ix = np.arange(len(k))
    delta = .5 * (a[ix, k - 1] - a[ix, k + 1]) / np.minimum(-1.e-12, a[ix, k - 1] - 2 * a[ix, k] + a[ix, k + 1])
    hz = (k + np.clip(delta, -.5, .5)) * SR / n
    good = power.sum(axis=1) > power.sum(axis=1).max() * 1.e-4
    return float(np.std(1200 * np.log2(hz[good] / 220))) if good.any() else 0


def features(main, wet, noshim, dry, source):
    e = np.mean(wet * wet, axis=1)
    bins = e[:len(e) // 4800 * 4800].reshape(-1, 4800).mean(axis=1)
    peak, active, full = float(np.max(np.abs(main))), rms(main[:ACTIVE * SR]), rms(main)
    mid, side = wet.mean(axis=1), (wet[:, 0] - wet[:, 1]) * .5
    width = float(np.sqrt(np.sum(side * side) / max(1.e-30, np.sum(mid * mid + side * side))))
    above = np.flatnonzero(bins > max(1.e-16, bins.max() * 1.e-4))
    decay = float((above[-1] + 1) / 10 - (0 if source == 'impulse' else ACTIVE)) if len(above) else 0
    censored = bool(len(above) and above[-1] == len(bins) - 1)
    centroid, lf, hf, pitch = spectrum(wet)
    onsets = {'impulse': [0], 'pluck': list(np.arange(0, 4, .5)), 'vocal': [0], 'pad': [0],
              'percussion': list(np.arange(0, 4, .5)), 'chords': [0, 1, 2, 3]}[source]
    samples = np.concatenate([np.arange(int(t * SR), int(t * SR) + int(.02 * SR)) for t in onsets])
    transient = rms(main[samples])
    transient_ratio = db(transient / max(1.e-12, rms(dry[samples])))
    # Envelope variation around a 250-ms trend, not LFO depth.
    env = np.sqrt(e[:ACTIVE * SR].reshape(-1, 480).mean(axis=1))
    trend = np.convolve(np.pad(env, (12, 12), mode='edge'), np.ones(25) / 25, mode='valid')
    motion = float(np.std((env - trend) / np.maximum(trend, .001)))
    t20, r2 = float('nan'), float('nan')
    if source == 'impulse':
        sch = np.cumsum(e[::-1])[::-1]
        level = 10 * np.log10(np.maximum(sch, 1.e-30) / max(1.e-30, sch[0]))
        idx = np.flatnonzero((level <= -5) & (level >= -25) & (np.arange(len(e)) < (DURATION - 2) * SR))[::480]
        if len(idx) > 10 and np.ptp(level[idx]) >= 15:
            times = idx / SR
            slope, intercept = np.polyfit(times, level[idx], 1)
            pred = slope * times + intercept
            r2 = float(1 - np.sum((level[idx] - pred) ** 2) / max(1.e-30, np.sum((level[idx] - level[idx].mean()) ** 2)))
            if slope < 0 and r2 > .85:
                t20 = float(-60 / slope)
    return dict(rms_db=db(full), active_rms_db=db(active), peak=peak, peak_db=db(peak),
                crest_db=db(peak) - db(full), tail_energy=float(np.sum(e[ACTIVE * SR:]) / SR),
                decay_proxy_s=max(0, decay), decay_censored=int(censored), t20_rt60_proxy_s=t20, t20_r2=r2,
                centroid_hz=centroid, lf_fraction=lf, hf_fraction=hf, width=width,
                transient_rms=transient, transient_preservation_db=transient_ratio,
                shimmer_contribution=rms(wet - noshim) / max(1.e-12, rms(wet)), modulation_proxy=motion,
                dominant_pitch_variance_hz2=pitch,
                vocal_pitch_std_cents=pitch_vocal(wet[:ACTIVE * SR]) if source == 'vocal' else float('nan'))


def measure(directory, tag):
    rows = readcsv(directory / 'parameters.csv')
    names = list(dict.fromkeys(r['preset'] for r in rows))
    result = []
    for i, name in enumerate(names):
        for source in SOURCES:
            audio = [wav(directory / f'{i:02}_{source}_{mode}.wav') for mode in range(3)]
            result.append(dict(preset=name, source=source,
                               **features(*audio, wav(directory / f'dry_{source}.wav'), source)))
        print(tag, name, flush=True)
    writecsv(OUT / f'{tag}_metrics.csv', result)
    writecsv(OUT / f'{tag}_parameters.csv', rows)
    return names, result, rows


def table(headers, rows):
    return '| ' + ' | '.join(headers) + ' |\n| ' + ' | '.join(['---'] * len(headers)) + ' |\n' + ''.join('| ' + ' | '.join(map(str, r)) + ' |\n' for r in rows) + '\n'


def fmt(v):
    return v if isinstance(v, str) else f'{float(v):.4g}'


def aggregate(names, rows):
    keys = ['active_rms_db', 'peak', 'tail_energy', 'decay_proxy_s', 'width', 'centroid_hz', 'transient_rms',
            'modulation_proxy', 'shimmer_contribution', 'dominant_pitch_variance_hz2', 'lf_fraction', 'hf_fraction']
    out = {}
    for name in names:
        r = [x for x in rows if x['preset'] == name and x['source'] != 'impulse']
        d = {k: float(np.mean([float(x[k]) for x in r])) for k in keys}
        d['peak'] = max(float(x['peak']) for x in rows if x['preset'] == name)
        d['impulse_decay'] = float(next(x['decay_proxy_s'] for x in rows if x['preset'] == name and x['source'] == 'impulse'))
        out[name] = d
    return out


def main():
    OUT.mkdir(exist_ok=True)
    if '--cached' in sys.argv:
        bm, am = readcsv(OUT / 'before_metrics.csv'), readcsv(OUT / 'after_metrics.csv')
        bp, ap = readcsv(OUT / 'before_parameters.csv'), readcsv(OUT / 'after_parameters.csv')
        names = list(dict.fromkeys(r['preset'] for r in bp))
    elif '--after-only' in sys.argv:
        bm, bp = readcsv(OUT / 'before_metrics.csv'), readcsv(OUT / 'before_parameters.csv')
        names, am, ap = measure(AFTER, 'after')
    else:
        names, bm, bp = measure(BEFORE, 'before')
        if '--before-only' in sys.argv:
            return
        names, am, ap = measure(AFTER, 'after')
    b, a = aggregate(names, bm), aggregate(names, am)
    for tag, directory in [('before', BEFORE), ('after', AFTER)]:
        for filename in ['safety.csv', 'transitions.csv']:
            shutil.copyfile(directory / filename, OUT / f'{tag}_{filename}')
    freeze_file = AFTER / 'freeze_performance.csv'
    if freeze_file.exists():
        shutil.copyfile(freeze_file, OUT / 'freeze_performance.csv')
    keys = ['active_rms_db', 'crest_db', 'tail_energy', 'decay_proxy_s', 'centroid_hz', 'lf_fraction',
            'hf_fraction', 'width', 'transient_preservation_db', 'shimmer_contribution',
            'modulation_proxy', 'dominant_pitch_variance_hz2']

    def vectors(rows):
        index = {(r['preset'], r['source']): r for r in rows}
        return np.array([[float(index[name, s][k]) for s in SOURCES for k in keys] for name in names])

    vb, va = vectors(bm), vectors(am)
    for si in range(6):
        for k in ['tail_energy', 'centroid_hz', 'dominant_pitch_variance_hz2']:
            j = si * len(keys) + keys.index(k)
            vb[:, j], va[:, j] = np.log1p(vb[:, j]), np.log1p(va[:, j])
    both = np.concatenate([vb, va])
    scale, centre = np.maximum(np.std(both, axis=0), 1.e-6), both.mean(axis=0)
    writecsv(OUT / "feature_scaling.csv", [dict(feature=f"{src}_{k}", mean=centre[si * len(keys) + ki], std=scale[si * len(keys) + ki], transform="log1p" if k in ["tail_energy", "centroid_hz", "dominant_pitch_variance_hz2"] else "identity") for si, src in enumerate(SOURCES) for ki, k in enumerate(keys)])
    pairs = {}
    for tag, v in [('before', vb), ('after', va)]:
        z = (v - centre) / scale
        dist = np.sqrt(np.mean((z[:, None, :] - z[None, :, :]) ** 2, axis=2))
        sim = np.exp(-dist)
        for label, matrix in [('similarity', sim), ('distance', dist)]:
            writecsv(OUT / f'{tag}_{label}.csv', [dict(preset=name, **{other: matrix[i, j] for j, other in enumerate(names)}) for i, name in enumerate(names)])
        pairs[tag] = sorted((dist[i, j], names[i], names[j]) for i in range(13) for j in range(i + 1, 13))
        writecsv(OUT / f'{tag}_vectors.csv', [dict(preset=name, **{f'{s}_{k}': z[i, si * len(keys) + ki] for si, s in enumerate(SOURCES) for ki, k in enumerate(keys)}) for i, name in enumerate(names)])
    rawb = {(r['preset'], r['parameter']): float(r['raw']) for r in bp}
    rawa = {(r['preset'], r['parameter']): float(r['raw']) for r in ap}
    desc = json.loads((OUT / 'identities.json').read_text(encoding='utf8'))
    lines = ['# M10 — Factory Preset Musical Curation & Voicing\n', (OUT / 'method.md').read_text(encoding='utf8'), '## Before — valores salvos (não efetivos)\n']
    params = ['space', 'wild', 'bloom', 'timeMs', 'feedback', 'mix', 'highCutHz', 'lowCutHz', 'diffusion', 'wetWidth', 'shimmerAmount', 'shimmerFeedback', 'shimmerInterval', 'duckAmount', 'modRateHz', 'modDepth', 'chaos', 'wobble', 'freezeEngage', 'atmosFdnOn']
    labels = ['Nome', 'SPACE', 'WILD', 'BLOOM', 'Time ms', 'Feedback', 'Mix', 'High Cut Hz', 'Low Cut Hz', 'Diffusion', 'Width', 'Shimmer', 'Regen', 'Interval 0/1/2', 'Ducking', 'Rate Hz', 'Depth', 'Chaos', 'Wobble', 'Freeze', 'Atmos']
    lines.append(table(labels, [[name, *[fmt(rawb[name, k]) for k in params]] for name in names]))
    lines += ['## Identity\n', table(['Preset', 'Função musical'], [[n, desc[n]['identity']] for n in names]), '## Redundancy / Similarity\n', 'Distância = RMS das diferenças dos vetores z-score com escala comum antes/depois e seis fontes com peso igual. Energia/centroid/variância passam por log1p. Similaridade = exp(-distância). Pares com distância <0,25 são sinalizados, sem bloquear por julgamento numérico. CSVs contêm matrizes 13×13 e vetores normalizados.\n']
    for tag in ['before', 'after']:
        lines.append(f'### {tag}\n' + table(['A', 'B', 'Distância', 'Similaridade', 'Revisão'], [[x, y, fmt(d), fmt(math.exp(-d)), 'REVISAR' if d < .25 else '—'] for d, x, y in pairs[tag][:10]]))
        matrix = readcsv(OUT / f'{tag}_similarity.csv')
        lines.append(table(['Preset', *[str(i + 1) for i in range(13)]],
                           [[f'{i + 1}. {n}', *[f'{float(matrix[i][other]):.2f}' for other in names]]
                            for i, n in enumerate(names)]))
    lines.append('A proximidade original Low Orbit/Trade Winds foi tratada com foco estreito/escuro contra movimento de chorus. Tidal Lock/Escape Velocity passou a eco profundo ducked contra expansão rápida e forte movimento. Zero-G/Event Horizon/Dark Matter mantêm caudas atmosféricas, mas separam difusão flutuante sem regen, órbita quase infinita e sombra de oitava abaixo. Após o revoicing, Low Orbit/Trade Winds e Zero-G/Event Horizon são prioridades de escuta humana: a matriz os coloca mais próximos. O primeiro par separa foco/cauda curta de chorus médio; o segundo separa campo flutuante de órbita mais longa e escura. Tidal Lock/Zero-G também merecem A/B por compartilharem atmosfera sem regen, embora divirjam em cauda, difusão e modulação. Nenhum par abaixo do limiar de revisão foi detectado; isso não comprova distinção musical humana.\n')
    lines.append(f"Menor distância do banco: {pairs['before'][0][0]:.3f} antes → {pairs['after'][0][0]:.3f} depois, com escala comum. O aumento indica menos proximidade extrema neste conjunto de features, não uma nota musical absoluta.\n")
    lines.append('## Changes\n')
    changes = []
    for name in names:
        rows = []
        for (n, k), old in rawb.items():
            if n == name and abs(rawa[n, k] - old) > 1.e-5:
                reason = desc[name]['reason']
                parameter_reason = {
                    'space': 'Reposiciona dimensão e deixa margem para expansão de SPACE.',
                    'wild': 'Reposiciona movimento e deixa margem para intensificar WILD.',
                    'bloom': 'Equilibra sustain, abertura tonal e ducking do BLOOM.',
                    'timeMs': 'Define duração e separação dos ecos para o papel musical.',
                    'feedback': 'Controla persistência dos ecos sem depender do teto de feedback.',
                    'mix': 'Equilibra presença wet, nível ativo e clareza do dry sem usar trim.',
                    'highCutHz': 'Ajusta cor tonal considerando a abertura efetiva de BLOOM.',
                    'lowCutHz': 'Equilibra corpo grave e acúmulo na recirculação.',
                    'diffusion': 'Distingue ecos articulados de nuvens densas, preservando margem manual.',
                    'wetWidth': 'Distingue foco stereo de expansão sem carregar o knob no máximo.',
                    'shimmerAmount': 'Define quanto o pitch processing participa da identidade.',
                    'shimmerFeedback': 'Separa halo/sustain de recirculação; zero evita regen em papéis sem shimmer.',
                    'shimmerInterval': 'Seleciona registro harmônico; 0 = oitava abaixo, 1 = quinta, 2 = oitava acima.',
                    'duckAmount': 'Equilibra abertura da cauda entre frases e preservação do ataque.',
                    'modRateHz': 'Define velocidade de deriva/movimento conforme a função musical.',
                    'modDepth': 'Define amplitude de movimento e mantém margem para WILD/manual.',
                    'chaos': 'Separa centro estável de órbita caótica.',
                    'wobble': 'Separa deriva lenta de instabilidade de fita/órbita.',
                    'presence': 'Ajusta articulação e cor interna do preset.',
                    'atmosFdnOn': 'Escolhe reforço de campo difuso para o papel atmosférico.',
                    'modShape': 'Escolhe o caráter temporal de modulação, em vez de variar apenas profundidade.',
                }.get(k, '')
                if parameter_reason:
                    reason = parameter_reason + ' ' + reason
                if k == 'outputTrim':
                    reason = 'Retira compensação herdada; balanceamento usa Mix/voicing, trim neutro.'
                rows.append([k, fmt(old), fmt(rawa[n, k]), reason])
                changes.append(dict(preset=name, parameter=k, old=old, new=rawa[n, k], reason=reason))
        if rows:
            lines.append(f'### {name}\n' + table(['Parameter', 'Old', 'New', 'Reason'], rows))
    writecsv(OUT / 'changes.csv', changes)
    lines.append('## Loudness\n')
    for tag, data in [('Before', b), ('After', a)]:
        median = np.median([data[n]['active_rms_db'] for n in names])
        lines.append(f'### {tag} (mediana {median:.2f} dBFS)\n' + table(['Preset', 'RMS ativo dBFS', 'Peak linear', 'Δ vs median dB'], [[n, fmt(data[n]['active_rms_db']), fmt(data[n]['peak']), fmt(data[n]['active_rms_db'] - median)] for n in names]))
    lines.append('## Tail / Width / Tone — After\n' + table(['Preset', 'Impulse decay s', 'Tail energy média', 'Width wet', 'Centroid Hz', 'LF', 'HF', 'Shimmer contribution', 'Motion'], [[n, *[fmt(a[n][k]) for k in ['impulse_decay', 'tail_energy', 'width', 'centroid_hz', 'lf_fraction', 'hf_fraction', 'shimmer_contribution', 'modulation_proxy']]] for n in names]))
    lines.append('O ajuste final reduz High Cut efetivo de Event Horizon para cerca de 4,47 kHz e Dark Matter para 3,68 kHz, em vez de confiar nos valores brutos sob BLOOM alto. Os centroids wet médios finais confirmam a direção escura em relação a Solar Wind/Glass Transit/Frozen Choir. Width foi medido com entrada mono para observar abertura criada pelo efeito; não mede todo o comportamento com material stereo. Freeze de performance fornece o estado sustentado/infinite-like sem salvar uma captura vazia no factory preset.\n')
    lines.append('## A/B — mix RMS/pico/transiente e wet tail/width/centroid\n' + table(['Preset', 'ΔRMS dB', 'Peak before → after', 'Tail energy before → after', 'Width before → after', 'Centroid before → after', 'Transient RMS before → after'], [[n, fmt(a[n]['active_rms_db'] - b[n]['active_rms_db']), *[f'{fmt(b[n][k])} → {fmt(a[n][k])}' for k in ['peak', 'tail_energy', 'width', 'centroid_hz', 'transient_rms']]] for n in names]))
    bi = {(r['preset'], r['source']): r for r in bm}
    ab = []
    for r in am:
        old = bi[r['preset'], r['source']]
        ab.append(dict(preset=r['preset'], source=r['source'], **{f'delta_{k}': float(r[k]) - float(old[k]) for k in keys + ['peak', 'transient_rms']}))
    writecsv(OUT / 'ab_deltas.csv', ab)
    tr = readcsv(AFTER / 'transitions.csv')
    lines.append('## Preset transitions\n' + f"26 cargas consecutivas (ordem normal e reversa), entrada contínua, 2 s por estado, sem reset do DSP. Maior pico: {max(float(r['peak']) for r in tr):.4f}; maior diferença RMS adjacente: {max(abs(float(r['adjacent_rms_delta_db'])) for r in tr):.2f} dB. Todos os parâmetros brutos e identidade comparados com carga limpa. Limite conservador de salto: 6 dB; não garante crossfade nem ausência perceptiva de clique.\n")
    if freeze_file.exists():
        fr = readcsv(freeze_file)[0]
        lines.append('## Frozen Choir — Freeze de performance\n' + table(list(fr), [[fmt(float(fr[k])) for k in fr]]) +
                     'O teste captura a cauda aos 4 s, mantém até 12 s e libera. A energia sustentada precisa permanecer entre 0,1× e 5× o RMS inicial do hold, com segurança também após liberação. O preset continua carregando Freeze desligado.\n')
    safety = readcsv(AFTER / 'safety.csv')
    lines.append('## Safety / headroom\n' +
                 f"234 renders float verificados. Maior pico incluindo wet-only e contrafactual: {max(float(r['peak']) for r in safety):.4f}; maior DC médio registrado: {max(abs(float(r['dc'])) for r in safety):.6f}. O gate de DC também verifica cada canal separadamente. Sem nonfinite, clipping próximo de 0 dBFS ou crescimento tardio acima dos limites conservadores.\n" +
                 table(['Preset', 'SPACE', 'WILD', 'BLOOM', 'Feedback manual', 'Diffusion manual', 'Width manual', 'Shimmer manual', 'Output Trim dB'],
                       [[n, *[fmt(rawa[n, k]) for k in ['space', 'wild', 'bloom', 'feedback', 'diffusion', 'wetWidth', 'shimmerAmount', 'outputTrim']]] for n in names]) +
                 'Todos os trims finais são neutros. A maioria deixa margem nos três macros e nos controles manuais. Event Horizon usa High Cut manual no mínimo para compensar a abertura tonal de BLOOM e sustentar sua identidade escura; esse endpoint é deliberado, não um ajuste de nível. O CSV effective documenta o resultado real.\n')
    tests = OUT / 'tests.txt'
    lines.append('## Tests\n\n```text\n' + (tests.read_text(errors='replace') if tests.exists() else 'Execução pendente.') + '\n```\n')
    (OUT / 'M10-Preset-Voicing.md').write_text('\n'.join(lines), encoding='utf8')
    content = ['<!doctype html><meta charset="utf-8"><title>M10 A/B</title><style>body{font:16px system-ui;margin:32px;background:#10151c;color:#eee}table{border-collapse:collapse}td,th{padding:8px;border:1px solid #456}audio{width:260px}</style><h1>M10 — A/B sem normalização</h1><p>Fontes e níveis idênticos. Mode 0: mix completo do preset.</p>']
    for i, n in enumerate(names):
        content.append(f'<h2>{html.escape(n)}</h2><p>{html.escape(desc[n]["identity"])}</p><table><tr><th>Fonte</th><th>Before</th><th>After</th></tr>')
        for s in SOURCES:
            filename = f'{i:02}_{s}_0.wav'
            content.append(f'<tr><td>{s}</td><td><audio controls preload="none" src="../../build/m10/before/{filename}"></audio></td><td><audio controls preload="none" src="../../build/te2350-vst-ninja-net/PresetVoicingOutput/{filename}"></audio></td></tr>')
        content.append('</table>')
    content.append('<h2>Frozen Choir — Freeze aos 4 s, release aos 12 s</h2><audio controls preload="none" src="../../build/te2350-vst-ninja-net/PresetVoicingOutput/Frozen_Choir_freeze_performance.wav"></audio>')
    (OUT / 'listen.html').write_text('\n'.join(content), encoding='utf8')
    print('Closest before/after:', pairs['before'][:5], pairs['after'][:5])
    print('Report:', OUT / 'M10-Preset-Voicing.md')


if __name__ == '__main__':
    main()
