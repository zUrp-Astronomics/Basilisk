---
date: 2026-10-20
last-revision: 2026-10-20 (ticket #661, the site's review: one voice, no dated text, links between tabs)
status: active — the Usage tab of the product page, read by the zUrp website
referenced-by: 9_Assets/README.md
derived-from: 7_Docs/MANUEL.md, 7_Docs/PROTOCOL.md, 4_Firmware/components/led/led.c, 6_Driver/indi/README.md
---

## Plug in

Mount the lens, plug the USB-C cable. The board starts the lens **by itself** as soon as the lens's presence contact
says it is there (held 300 ms): it powers the lens, initialises it as a camera body would, identifies it, runs its
homing, then **returns to the mark** if there is one, and is ready. During that start, the commands that need the
lens answer "busy".

The board never speaks first: it only answers what it is asked.

Remove the lens, and the board cuts its power and puts its lines at rest.

## Focus

Drive the focus from Ekos, ASCOM or the bench page. The focus limits are **the ones the lens publishes itself**,
narrowed by 5 steps; nothing is measured, and there is nothing to calibrate. A target outside them is refused before
anything is sent to the lens. With the INDI driver of this repository, these limits become the range of the focus
position, and Calibrate simply reads them again.

## The mark

The mark is a focus position remembered **in the board**, per lens and per focal length — nothing is written into
the lens.

- **The lens barrel button**, on a lens that has one, sets it and uses it, decided when you release it:
  - **less than 1 s**: go to the mark;
  - **1 to 4 s**: set the mark at the current position (the long press avoids overwriting it by accident);
  - **longer**: nothing.

  The button works only when the board is ready, and is ignored during a move.
- **At the end of every start** (power-up, lens restart, recovery after a lost lens), the board goes back to the mark
  by itself, arriving from the side it came from when the mark was set, to take up the mechanical play.
- From the computer: in Ekos, with the INDI driver of this repository, the mark controls (set, go to, clear) and the
  mark read back; on the bench page, **Mark** and **Go to mark**.

## The lens ring and the aperture

When the lens's switch gives its ring to the aperture, **turning the ring sets the aperture**, in third stops, three
per second at most. On the Samyang AF 135, which position of its M1/M2 switch does that is a setting of the lens
([Setup](setup.en.md) tab, *The M1/M2 switch*).

From the computer, the aperture is set within the range the lens reports; the value shown is the one the lens reports
back.

## The status LED

| What you see | What it means |
|---|---|
| steady for 1 s, at power-up | the board has just powered up |
| a soft swell every 10 s | no lens |
| one long flash | a lens is detected |
| a slow, faint blink | the lens is starting |
| a quick rise | the lens is ready |
| a fast flicker | going back to the mark |
| steady for 1 s, after the fast flicker | arrived at the mark |
| slow breathing | ready |
| short, fast pulses | focus position increasing |
| long, slow pulses | focus position decreasing |
| one brief flash | the aperture changed |
| three flashes | mark set |
| a fade-out | mark cleared |
| a double flash every 2 s | a start failed or the lens was lost: the board is retrying |
| 1 flash, then a pause, repeated | stopped: the lens was lost |
| 3 flashes, then a pause, repeated | stopped: the lens's homing failed |

The patterns and their exact timings are at the top of
[`4_Firmware/components/led/led.c`](../../4_Firmware/components/led/led.c).

**Brightness**: 0 to 100 %, 0 turns it off. The board forgets it at each power-up and comes back to 100 %; the INDI
driver of this repository stores it in its INDI configuration and sends it again at each connection (Options tab,
LED brightness).

## Temperature, humidity, dew or frost point

The board reads its SHT40 sensor once a second: the temperature in °C, the relative humidity in %RH, and the
condensation threshold on the optics that the board computes from them — **the dew point** when it is zero or
above, **the frost point** when it is below zero.

- **Unknown is never a number.** While the board does not know a value (not read yet, sensor silent, wrong reading,
  last reading older than 3 s), it says so; the bench page shows "—", and the INDI driver of this repository removes
  the property rather than publish an old value. The threshold is also undefined at 0 % humidity, above 60 °C, or for
  a frost point below -65 °C.
- **Ekos reads the temperature** (`FOCUS_TEMPERATURE`) to refocus on a temperature change. The humidity and the dew
  or frost point are shown by the driver, for information.
- **The board does nothing else with them**: it does not compensate the focus — Ekos decides — and it does not act on
  the dew or frost point, it reports it.
- The sensor is on the board, against the back of the lens: what it measures of the air depends on the enclosure.

## The bench page

The bench page ([Setup](setup.en.md) tab) shows and drives everything above, from the browser:

- **Lens**: its name and the board's state; restart the lens, cut and restore its power.
- **Board sensor**: temperature, humidity, dew or frost point, with or without a lens.
- **Focus**: ±10 / ±100 steps, a slider between the limits with the mark on it, Go to, Stop, Mark, Go to mark.
- **Aperture**: a third-stop slider between the lens's limits, ±⅓, wide open; the value shown is the one the lens
  reports.
- **Log**: the board's journal on demand, with each frame exchanged with the lens decoded; a free command line; the
  log exports to a text file.

It reads the position and the motion five times a second, the rest once a second.

## When something goes wrong

- **The lens is lost** (no frame for 2 s): the board restarts it by itself. After four failed attempts in a row it
  stops (the LED's repeated flashes); restart the lens from the computer (Ekos, the bench page), or unplug and replug
  the lens.
- **A power rail of the lens trips** (over-current, over-heating): the board counts it and reports it in its
  diagnostic line (`DEBUG`, from the bench page's command line); it cuts nothing itself.
- **To report a problem**, the bench page's log, with the journal on (`LOG ON`, or `LOG ALL` for every frame), exported
  to a text file, is what tells what happened.

Every command, reply and error is in [`7_Docs/PROTOCOL.md`](../../7_Docs/PROTOCOL.md); the French manual,
[`7_Docs/MANUEL.md`](../../7_Docs/MANUEL.md), says the same as this tab in more detail.
