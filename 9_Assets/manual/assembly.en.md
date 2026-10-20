---
date: 2026-10-20
last-revision: 2026-10-20 (ticket #661, the site's review: one voice, no dated text, links between tabs)
status: active — the Assembly tab of the product page, read by the zUrp website
referenced-by: 9_Assets/README.md
derived-from: 2_Hardware/README.md, 3_3D-Models/README.md, README.md (§ Build), 1_Board/README.md
---

## What you need

- **The Basilisk board v1.0** — see *The board* below.
- **The printed enclosure**: the core (top and bottom) and the USB surround, from
  [`3_3D-Models/`](../../3_3D-Models/README.md).
- **One half kit of Meike MK-S-AF3A.** The kit is a pair of 10 mm and 16 mm macro extension tubes: only the male
  metal barrel, its screws and its latch are used.
- **Three M2.5×12 screws**, for the camera (see *The camera screws*).
- **A compatible camera** — the list is in the [Overview](overview.en.md) tab and in
  [`2_Hardware/README.md`](../../2_Hardware/README.md).

The bill of materials outside the board is [`2_Hardware/Basilisk_v1.0_BoM.csv`](../../2_Hardware/Basilisk_v1.0_BoM.csv);
the board has its own, in [`1_Board/`](../../1_Board/README.md).

## The board

**The easy way — assembled by JLCPCB.** Send the Gerber, BoM and pick-and-place files from
[`1_Board/`](../../1_Board/README.md) and order the board assembled, top side only. The ten lens contacts are single
pogo pins (P1–P10), listed with the other top-side parts. The PCB options are in
[`1_Board/Basilisk-v1.0_0-README.txt`](../../1_Board/Basilisk-v1.0_0-README.txt): 2 layers, 1.0 mm, 1 oz copper,
black solder mask, white silkscreen, HASL RoHS.

**The hard way — do it all yourself.** Everything is in [`1_Board/`](../../1_Board/README.md). Expect fine-pitch work:
a QFN microcontroller, 0402 passives and tiny exposed-pad power parts.

## Printing the enclosure

| File | Parts |
|---|---|
| [`Basilisk_v1.0_enclosure.3mf`](../../3_3D-Models/Basilisk_v1.0_enclosure.3mf) | the enclosure core: the top and the bottom |
| [`Basilisk_v1.0_USB.3mf`](../../3_3D-Models/Basilisk_v1.0_USB.3mf) | the USB surround |

These two files are not yet checked on a final assembly. Their CAD source, all the parts in one STEP file, is
[`2_Hardware/Basilisk_v1.0_enclosure.step`](../../2_Hardware/Basilisk_v1.0_enclosure.step).

### Materials

**Core (top and bottom)**

- **PC-CF: validated.** Stable in temperature, strong and stiff; the material of our finished parts.
- **PETG: should do the job**, slightly less durable.
- **ABS, ASA: should hold**; we have not tried them.
- **PLA: not to be used.**

**USB surround**

- **Clear PETG: the chosen material**, to let the light of the LEDs through.
- **Clear PLA: should be suitable**; we have not tried it, so we have no opinion.

The USB surround is not the most critical part.

### Tolerances

They work for us. Depending on your material and your printer, they may need a marginal touch-up: to the tenth of a
millimetre for the latch, for example.

## The camera screws

The three M2.5×12 screws replace the three original M2.5×8 screws of the camera's front plate: they compensate the
added height and keep the threads engaged.

## Putting it together

**The step-by-step procedure is not written yet.** The final assembly is not yet checked; the procedure will be
published here once it is. Until then, the parts, the screws and the materials above are all the repository states.

![Basilisk enclosure v1.0b, CAD render: the E-mount ring with its pogo-pin contacts and the USB-C port](../basilisk-enclosure.webp)

*The enclosure v1.0b, the previous revision: no render of the v1.0 enclosure is provided yet.*
