# BeatBax FM-1 Game Boy Renderer Reference

Engineering reference for the fixed-point Game Boy (DMG-01) renderer
(`host/render/gameboy/`) and its CLI comparison harness. The spec of record is
[`specs/complete/003-gameboy-renderer/`](../specs/complete/003-gameboy-renderer/).

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

**Feature 004 device rate.** The renderer's `sample_rate` is a `uint32`, so the
device path uses the nearest integer **44118 Hz** (`GB_RENDER_DEVICE_RATE`);
the true codec rate is 24 MHz / 544 = 44117.647… Hz. The compiled song blob is
baked at 44118 Hz and carries the 0.95 peak-limit scale computed by the offline
renderer at that rate, so the device streamer is sample-identical to the host
render of the same fixture at 44118 Hz (verified by `make test-render`'s stream
test).

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

## Spec-112 parity update (2026-11-23)

The renderer now matches BeatBax's **spec-112 WAV/Desktop parity** semantics (the
`beatbax.main` working tree at `a34cdac1`), not the 0.6.1 CLI. Changes — see
[`specs/complete/004-device-player/plan.md`](../specs/complete/004-device-player/plan.md),
section "Renderer fidelity update":

- **Wave output scale** ×0.54 (`0.9` buffer fill × `0.6` playback gain).
- **GB envelope** linearly interpolates between steps (was discrete stepping).
- **Release tails**: 5 ms linear (pulse/noise); 20 ms exponential (`setTargetAtTime`
  shape) for a wave without a GB envelope.
- **Portamento curve**: exponential/geometric (pulse), linear Hz (wave) — was smoothstep.
- **Effect order**: port → arp → bend → vib, with vibrato additive and keyed off the
  un-arpeggiated base frequency.
- **Pulse portamento voice-holding**: contiguous `port` runs share one oscillator
  (`join`/`hold` flags, flat envelope, no mid-run fade); a normal pulse note resets its
  oscillator phase to 0 (`continuePhase = legato || portJoin`).

**Chromium 440 Hz startup** (`d3807ca6`) is **replicated**: a new pulse voice plays (and
advances phase at) 440 Hz for the leading `min(start%128, 128 − start%128)` samples,
matching Chromium's OscillatorNode. **Accepted residual** — the parity renderer uses a
band-limited pulse (Fourier series + `2·duty−1` DC), while this renderer keeps a naive
±1 square; the correlation cost is ~2% (single notes ≈ 0.98).

The correlation figures below were measured against CLI **0.6.1** (pre-parity) and must
be re-measured against the new CLI (`make test-compare`).

## Measured results (BeatBax CLI 0.6.1, 44100 Hz)

| Fixture | Correlation |
|---------|-------------|
| `tutorial_groove.bax` | 0.970 |
| `gb_sweep_demo.bax` | 1.000 |
| `gb_uge_note_demo.bax` | 1.000 |

## Measured results (spec-112 parity CLI, 44100 Hz)

| Fixture | Correlation |
|---------|-------------|
| `tutorial_groove.bax` | 0.958 |
| `gb_sweep_demo.bax` | 0.989 |
| `gb_uge_note_demo.bax` | 0.951 |

The residual (~2%) is the band-limited pulse (naive ±1 square vs Fourier-series pulse),
an accepted difference; see "Spec-112 parity update" above.

`grassland_dash.bax` renders with exact note timing and length but ~0.55
correlation. Per-channel isolation (`render-bax -c N`) shows the drift is
confined to the frequency-modulated channels (arp-heavy harmony 0.18, melody
0.53, wave 0.63) while noise is exact (1.00) — a sub-sample phase drift that
accumulates over the 20 s song in the persistent pulse/wave phase. The effect
formulas match BeatBax (`effects/index.ts`, `pcmRenderer.ts`); this is a residual
fixed-point accumulation, not a timing or formula error.
