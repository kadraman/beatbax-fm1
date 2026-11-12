# Feature specifications

In-flight features live here as `NNN-slug/` folders. Each folder always contains `spec.md`, `plan.md`, and `tasks.md`.

See [../README.md](../README.md) for process and [../STATUS.md](../STATUS.md) for the index.

## Current Game Boy phases

| Folder | Title | Issue |
|--------|-------|-------|
| [001-platform-bring-up](001-platform-bring-up/) | Platform bring-up | [#1](https://github.com/kadraman/beatbax-fm1/issues/1) |
| [002-bax-round-trip](002-bax-round-trip/) | `.bax` round-trip on the host | [#2](https://github.com/kadraman/beatbax-fm1/issues/2) |
| [003-gameboy-renderer](003-gameboy-renderer/) | Game Boy renderer on the host | [#3](https://github.com/kadraman/beatbax-fm1/issues/3) |
| [004-device-player](004-device-player/) | Device player | [#4](https://github.com/kadraman/beatbax-fm1/issues/4) |
| [005-graphical-composer](005-graphical-composer/) | Graphical composer | [#5](https://github.com/kadraman/beatbax-fm1/issues/5) |
| [006-store-usb-transfer](006-store-usb-transfer/) | Store and USB transfer | [#6](https://github.com/kadraman/beatbax-fm1/issues/6) |
| [007-live-play-record](007-live-play-record/) | Live play and record | [#7](https://github.com/kadraman/beatbax-fm1/issues/7) |

Do not start implementing phase `N+1` until phase `N` is at least `planned`.
