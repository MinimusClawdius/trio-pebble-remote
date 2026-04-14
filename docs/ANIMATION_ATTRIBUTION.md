# Animation and UI attribution (Trio Remote)

## Shipped assets and code

- **Tick / cross icons** on the confirm action bar come from **Pebble `ui-patterns`** (MIT-style Pebble examples). Repository: [pebble-examples/ui-patterns](https://github.com/pebble-examples/ui-patterns).

- **Bolus and carb confirm illustrations** in `src/modules/remote_send_ui.c` are **original procedural drawings** (no embedded bitmap frames for those scenes). Geometry is scaled from the art layer’s minimum side length so layouts stay readable on **aplite, basalt, chalk (round), diorite, and emery** without per-platform image packs.

## References consulted (patterns and docs)

These projects and guides informed timing and structure (AppTimer-driven `layer_mark_dirty`, action-bar flow); **logic and artwork in the confirm panel are not copied** from them:

- [pebble-examples/composite-animations-example](https://github.com/pebble-examples/composite-animations-example) — composite / property animation patterns (MIT).

- [Rebble — Vector animations tutorial](https://developer.repebble.com/tutorials/advanced/vector-animations/) — PDC / draw-command animation concepts. Trio Remote does **not** ship PDC sequences for confirm art so **aplite** and **diorite** stay supported with the same code path.

- [pebble-examples/pdc-sequence](https://github.com/pebble-examples/pdc-sequence) and [pdc-image](https://github.com/pebble-examples/pdc-image) — cited as future options if you add optional PDC assets for color platforms with monochrome fallbacks.

## If you add third-party art later

Do not commit stock vectors (e.g. marketplace “pixel syringe” packs) unless the license explicitly allows redistribution inside an app binary. Prefer original SVG → `svg2pdc`, original PNGs, or procedural code.
