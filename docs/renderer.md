# BeatBax FM-1 Game Boy Renderer Reference

Engineering reference for the fixed-point Game Boy (DMG-01) renderer
(`host/render/gameboy/`) and its CLI comparison harness. The spec of record is
[`specs/features/003-gameboy-renderer/`](../specs/features/003-gameboy-renderer/).

## Rates

| Context | Sample rate | Source |
|---------|-------------|--------|
| Host comparison | **44100 Hz** | BeatBax CLI `wav` exporter default (`beatbax export wav …`) |
| Device codec | **44117.6 Hz** | [`specs/global/audio.md`](../specs/global/audio.md) |

The renderer is **rate-parameterized**: one `sample_rate` value drives phase
increments and envelope/effect timing. The same shared algorithm runs at
44100 Hz for host comparison and at 44117.6 Hz on-device in feature 004, so the
host and device paths stay bit-identical by construction.

The comparison harness renders **both sides at 44100 Hz** and compares
sample-for-sample — no resampling. This avoids the WAV header's integer-rate
limitation (a `-r 44117.6` request stores `44117` in the header).

## Reference commands (BeatBax CLI 0.6.1)

```text
# Reference WAV (44100 Hz, 16-bit, stereo)
beatbax export wav <song.bax> -o <ref.wav> -d <seconds> [-b 16]

# Reference events (ISM JSON — expanded song model)
beatbax export json <song.bax> -o <ref.ism.json>
```

The CLI `wav` exporter emits **stereo** (2ch, 16-bit). The harness downmixes
the reference to mono (mean of L/R) before correlating against the renderer's
mono output.

## Tolerances (SC-003)

- **Musical events:** note-on times within **1 ms** of CLI-derived reference
  events (same score expansion), sourced from the ISM JSON export.
- **Resampled WAV:** normalized Pearson correlation **≥ 0.95** on a trimmed
  non-silent region. This is the **primary** WAV gate. Correlation is
  self-normalizing and robust to overall gain differences between the
  fixed-point renderer and the CLI's float path.

The harness also prints the RMS dB difference as a **diagnostic** (not a
pass/fail gate) to aid triage of envelope/amplitude bugs when correlation is
near the threshold. `--normalize` is NOT applied to the reference export.

## How to run

```text
make test-render     # build + run host renderer unit tests
make test-compare    # render fixtures + compare to BeatBax CLI (>= 0.95)
```

`make test-compare` runs `tools/run-compare.py`, which renders each fixture with
`tools/render-bax`, exports the reference with `beatbax export wav`, and checks the
normalized correlation over the trimmed non-silent region against the 0.95 gate.

## Measured results (BeatBax CLI 0.6.1, 44100 Hz)

| Fixture | Correlation |
|---------|-------------|
| `tutorial_groove.bax` | 0.970 |
| `gb_sweep_demo.bax` | 1.000 |
| `gb_uge_note_demo.bax` | 1.000 |

`grassland_dash.bax` renders with exact note timing and length but ~0.55
correlation. Per-channel isolation (`render-bax -c N`) shows the drift is
confined to the frequency-modulated channels (arp-heavy harmony 0.18, melody
0.53, wave 0.63) while noise is exact (1.00) — a sub-sample phase drift that
accumulates over the 20 s song in the persistent pulse/wave phase. The effect
formulas match BeatBax (`effects/index.ts`, `pcmRenderer.ts`); this is a residual
fixed-point accumulation, not a timing or formula error.
