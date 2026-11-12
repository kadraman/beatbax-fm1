# Song model contract

The structured song model is the stable core shared by host tools and firmware. `.bax` text is the interchange format with BeatBax.

## MUST

- Represent songs as a structured model: metadata, chip directive, instruments, patterns, sequences, channel bindings, and effects that the active chip surface allows.
- Phase 001–007 support **`chip gameboy` only** (DMG-01 four voices: pulse1, pulse2, wave, noise), quoting BeatBax’s Game Boy surface.
- Parse supported `.bax` into the model and write **canonical `.bax`** back. Round-trip fixtures MUST remain accepted by `beatbax verify` (and playable by the CLI) for the supported subset.
- Refuse unsupported constructs with a **named reason** (construct name or directive). Do not drop or rewrite them silently.
- Keep pattern and sequence meaning **chip-agnostic**. Chip-specific data lives on instruments, channel voice types, and effect parameters the BeatBax Game Boy surface already defines.
- Keep expansion / ordering semantics aligned with BeatBax for the supported subset. When BeatBax documents a rule, follow it; do not invent a parallel rule.

## MUST NOT

- Invent new `.bax` syntax or directives in this repository.
- Change AST/ISM meanings that BeatBax owns; if BeatBax changes, update a compatibility feature here rather than forking the language.
- Embed chip-renderer state into the persisted song model beyond what `.bax` can express.
- Soften validation so an invalid or unsupported song “loads anyway.”

## Persistence

- On device, the editable form is the structured model in RAM; durable form is `.bax` in the song store (feature 006).
- Host libraries that implement parse/write stay **Apache-2.0** and MUST NOT link Felucca.

## Later chips

- Additional chips (`nes`, `sms`, `gg`, AY family, …) arrive as new feature specs.
- A multi-chip song on one project is out of scope for 001–007.

## References

- BeatBax language and Game Boy docs: https://beatbax.com/docs
- BeatBax compatibility rules: [compatibility.md](compatibility.md)
