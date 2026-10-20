---
date: 2026-10-20
last-revision: 2026-10-20 (ticket #661, the site's review: one voice, no dated text, links between tabs)
status: active — the Overview tab of the product page, read by the zUrp website
referenced-by: 9_Assets/README.md
derived-from: README.md, 7_Docs/MANUEL.md, 7_Docs/PROTOCOL.md, 2_Hardware/README.md, 6_Driver/indi/README.md
---

## What Basilisk is

Basilisk is a Sony E adapter for astro cameras. It sits between your astro camera and a Sony E-mount lens, and plays
the part of the camera body: it powers the lens, runs its start-up, then drives it on request — focus, aperture, and
a remembered focus position, the **mark**. One USB-C cable carries power and data; there is no camera body and no
other supply.

On the computer side, the lens becomes a focuser for **Ekos/INDI** (Linux) or **ASCOM** (Windows). Basilisk speaks
the Pinefeat serial protocol, so the Pinefeat INDI and ASCOM drivers work with it unchanged; the INDI driver of this
repository adds what Basilisk serves beyond Pinefeat.

![Basilisk main board v1.0, 3D view](../basilisk-board.webp)

## What it does

- **Focus**, absolute or relative, within the limits the lens itself publishes — nothing is measured or coded per
  lens model.
- **Aperture**: read the range, set it, read back what the lens reports.
- **The mark**: a focus position stored in the board, per lens and per focal length. The button on the lens
  barrel, on a lens that has one, sets it and goes back to it, and the board returns to it by itself every time the lens starts.
- **The lens ring**: when the lens switch gives the ring to the aperture, turning it sets the aperture in third
  stops.
- **Temperature and humidity**: a SHT40 sensor on the board, read once a second, with the dew point (or frost point,
  below zero) the board computes from them. Ekos can refocus on a temperature change.
- **A status LED** that tells the state of the board and the lens at a glance.
- **A bench page**: a single web page that drives the board from the browser, nothing to install.

Details: [the manual](../../7_Docs/MANUEL.md) and [the serial protocol](../../7_Docs/PROTOCOL.md) (both in French).

## Lenses

Basilisk drives Sony E-mount lenses through their own contacts. Its **first target is the Samyang AF 135 mm
F1.8 FE**. The focus limits and the aperture range come from the lens itself: nothing is coded per lens model.

The Samyang AF 135 gets one more tool: its M1/M2 custom switch can be read and set from the computer, without
Samyang's software (see [Setup](setup.en.md)).

The project documents how a Sony E body talks to its lens — Sony, Samyang and Tamron — from bench captures of a Sony
FE 24-105 mm F4 G OSS and a Samyang AF 135 F1.8 FE: [Sony E-mount interoperability](../../7_Docs/E-Mount/README.md).

## Cameras

**Compatible**: the uncooled Player One planetary cameras, in the hexagonal format, as listed on Player One's website
on 2026-10-20:

- Mars-C, Mars-C II, Mars-M II, Mars 662M;
- Neptune-C, Neptune-M, Neptune-C II, Neptune 664C;
- Uranus-C, Uranus-M;
- Saturn-C SQR, Saturn-M SQR.

The Player One solar cameras have the same format, so they should fit too; what we doubt is their use with a
photographic lens, not their compatibility.

**Not compatible, for now**: the cooled cameras — a real camera in hand is needed to check the mechanics.
**Not checked**: ZWO and ToupTek.

The list lives in [`2_Hardware/README.md`](../../2_Hardware/README.md).

## Inside

| | |
|---|---|
| Board | ring-shaped, 50.87 × 36.3 mm, parts on one side only |
| Link | one USB-C cable for power and data |
| Brain | ESP32-C3, native USB |
| Lens power | two switched, current-limited rails (5 V motor, 3.3 V logic), each reporting its faults |
| Lens interface | 10-contact Sony E-mount on single pogo pins, with lens detection |
| Protection | ESD on every lens line and on USB |
| Control | Ekos/INDI, ASCOM — Pinefeat-compatible |

## Nothing writes into your lens

A lens whose memory is damaged cannot be repaired. Everything the board sends to the lens goes through one filter, a
white list compiled behind every transmission, with no option to lift it: lens firmware updates and every service
command that writes the lens's memory are refused.

One exception, our choice: writing the M1/M2 switch configuration of the **Samyang AF 135**, and only to
a recognised AF 135. See [the manual](../../7_Docs/MANUEL.md), § Sécurité.

## Open hardware, open software

The board, the mechanics and the 3D models are under the [Open Community License v1.1](../../LICENSE-HARDWARE); the
firmware, the software and the documentation under the [GNU GPL v3.0](../../LICENSE); the INDI driver under the LGPL
2.1. Based on [Pinefeat](https://github.com/pinefeat/cef135), whose serial protocol Basilisk speaks.
