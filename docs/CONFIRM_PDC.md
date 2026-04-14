# Confirm screen — optional PDC vector animation

## What is wired up

- On **color** platforms (`PBL_COLOR`: Pebble Time, Time Round, Time 2), `remote_send_ui.c` tries to draw **`CONFIRM_BOLUS_PDCS`** or **`CONFIRM_CARB_PDCS`** (raw Pebble Draw Command **sequences**) before falling back to the built-in procedural animation.
- **Aplite** and **diorite** never ship these resources; they always use the procedural path (same code as today).

Resources are declared in `package.json` with `targetPlatforms: ["basalt", "chalk", "emery"]` only.

## Stub files in the repo

Until you replace them with custom art, the project vendors MIT-licensed sequences from [pebble-examples/pdc-sequence](https://github.com/pebble-examples/pdc-sequence):

| Resource name        | Current file                 | Source (example)   |
|----------------------|------------------------------|--------------------|
| `CONFIRM_BOLUS_PDCS` | `resources/pdc/confirm_bolus_stub.pdc`  | `confirm_sequence.pdc` |
| `CONFIRM_CARB_PDCS` | `resources/pdc/confirm_carb_stub.pdc`   | `sent_sequence.pdc`    |

They are **placeholders** so the draw-command path compiles and runs; swap the `.pdc` files (keep the same `package.json` names or update paths) when you have final bolus/carb animations.

## Replacing with your own PDCS

1. Create compatible SVG frames (see [Rebble vector tutorial](https://developer.repebble.com/tutorials/advanced/vector-animations/)).
2. Run Pebble’s `svg2pdc.py --sequence <frames_dir/>` to produce a single `.pdc` sequence file.
3. Overwrite `resources/pdc/confirm_bolus_stub.pdc` / `confirm_carb_stub.pdc` **or** add new filenames and update `package.json` `file` entries.
4. Rebuild all color targets and confirm framing on **chalk** (round) and **emery** (tall).

## Temporarily disable PDC (procedural only on color too)

In `remote_send_ui.c`, before the `TRIO_REMOTE_CONFIRM_PDC_ASSETS` block, you can compile with:

```c
#define TRIO_REMOTE_CONFIRM_PDC_ASSETS 0
```

or pass `-DTRIO_REMOTE_CONFIRM_PDC_ASSETS=0` from your build flags / CloudPebble “compiler flags” if supported.

## Centering

Frames are drawn with an offset so the sequence’s reported `gdraw_command_sequence_get_bounds_size()` is centered inside the white art panel. Very small or zero bounds fall back to top-left of the panel.
