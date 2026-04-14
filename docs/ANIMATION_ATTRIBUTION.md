# Animation and UI attribution (Trio Remote)

## Shipped assets and code

- **Tick / cross icons** on the confirm action bar come from **Pebble `ui-patterns`** (MIT-style Pebble examples). Repository: [pebble-examples/ui-patterns](https://github.com/pebble-examples/ui-patterns).

- **Optional confirm PDCS** (basalt / chalk / emery only): stub sequences under `resources/pdc/` are copied from [pebble-examples/pdc-sequence](https://github.com/pebble-examples/pdc-sequence) (`confirm_sequence.pdc`, `sent_sequence.pdc`) as **temporary placeholders** until custom bolus/carb vector animations replace them. See `docs/CONFIRM_PDC.md`.

- **Procedural fallback** in `src/modules/remote_send_ui.c` is **original code**: used on **aplite** and **diorite**, and on color watches if PDC load fails or is disabled (`TRIO_REMOTE_CONFIRM_PDC_ASSETS`). Geometry scales from the art layer’s minimum side length.

## References consulted (patterns and docs)

These projects and guides informed timing and structure (AppTimer-driven `layer_mark_dirty`, action-bar flow); **logic and artwork in the confirm panel are not copied** from them:

- [pebble-examples/composite-animations-example](https://github.com/pebble-examples/composite-animations-example) — composite / property animation patterns (MIT).

- [Rebble — Vector animations tutorial](https://developer.repebble.com/tutorials/advanced/vector-animations/) — PDC / draw-command concepts; Trio Remote uses PDCS on **color** targets and procedural art on **aplite** / **diorite**.

- [pebble-examples/pdc-sequence](https://github.com/pebble-examples/pdc-sequence) and [pdc-image](https://github.com/pebble-examples/pdc-image) — sequence playback and static PDC patterns; **stub** `.pdc` files in this repo come from `pdc-sequence` (see above).

## If you add third-party art later

Do not commit stock vectors (e.g. marketplace “pixel syringe” packs) unless the license explicitly allows redistribution inside an app binary. Prefer original SVG → `svg2pdc`, original PNGs, or procedural code.
