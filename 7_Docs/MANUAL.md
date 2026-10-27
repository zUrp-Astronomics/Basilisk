# Basilisk — build and use

[Français](MANUEL.md)

## Project

### Eighteen millimetres. Make it fit.

Sony E gives you 18 mm between the mount and the sensor. Basilisk puts the barrel, contacts, controller and USB port into that space, with room left for the astro camera. A questionable idea. A very satisfying object.

It plays the camera body for an E-mount lens: power, focus and aperture over one USB-C cable. Drive it from Ekos/INDI, ASCOM or the web bench on the product page. The lens supplies its own focus limits; the board remembers a focus mark. No motor strapped to the barrel, no belt to tension.

### What you build

An open hardware adapter around an ESP32-C3, a custom ring-shaped PCB, a printed enclosure and the metal barrel of a Meike extension tube. The design starts from Pinefeat; the board, firmware and build files live in this repository.

The enclosure is designed for the uncooled, hexagonal Player One planetary cameras: Mars-C, Mars-C II, Mars-M II, Mars 662M; Neptune-C, Neptune-M, Neptune-C II, Neptune 664C; Uranus-C and Uranus-M; Saturn-C SQR and Saturn-M SQR. Cooled bodies are outside this design. ZWO and ToupTek fit has not been established.

The bench work in this repository uses a Samyang AF 135 F1.8 FE and a Sony FE 24–105 mm F4 G OSS. The [E-mount reference](../7_Docs/E-Mount/README.md) records what is observed and what is inferred for other lenses. A protocol description is not a compatibility certificate.

### From parts to stars

[Assembly](#assembly) covers the board, printed parts and camera interface. [Setup](#setup) gets the firmware and your software connected. [Usage](#usage) covers focus, aperture, the mark and diagnostics.

The following sections also cover the design and offline workflow. The files are yours to build, inspect and adapt. The finished object is the point; the workshop is the price of admission.

## Assembly

### Parts on the bench

| Part | Source |
|---|---|
| Basilisk v1.0 PCB and components | [Board fabrication package](../1_Board/README.md) |
| Enclosure, upper and lower halves | [Enclosure 3MF](../3_3D-Models/Basilisk_v1.0_enclosure.3mf) |
| USB surround | [USB 3MF](../3_3D-Models/Basilisk_v1.0_USB.3mf) |
| Male metal barrel, screws and latch | One half of a Meike MK-S-AF3A 10/16 mm extension-tube kit |
| Camera fasteners | Three M2.5×12 screws |
| Camera | A body listed in [The beast](#project) |

The [mechanical bill of materials](../2_Hardware/Basilisk_v1.0_BoM.csv) is separate from the PCB BoM. The [STEP assembly](../2_Hardware/Basilisk_v1.0_enclosure.step) contains the enclosure parts for adaptation.

### Order the board

Use the Gerber, BoM and pick-and-place files in the board package. The fabrication settings are **2 layers, 1.0 mm thickness, 1 oz copper, black solder mask, white silkscreen, HASL RoHS**. Assembly is on the top side only. P1–P10 are ten individual pogo pins; CN2 is not populated as a separate connector.

JLCPCB assembly is the documented route. Hand assembly means a QFN microcontroller, 0402 passives and small exposed-pad power components.

### Print and fit

For the structural upper and lower halves, **PC-CF is the material used and validated on the project's parts**. PETG is an expected alternative; ABS and ASA have not been tried. Do not use PLA for these structural parts. The USB surround uses clear PETG so the LED remains visible.

Check the latch fit with your own material and printer: a correction of a few tenths of a millimetre may be needed. The three M2.5×12 screws replace the camera front plate's M2.5×8 screws, compensating for the added height while keeping the threads engaged.

### Assembly status

The final v1.0 assembly from the supplied 3MF files has not yet been checked, and there is no verified step-by-step assembly sequence. The files and material guidance above are available; a complete reproduction guide still needs that final fit check and its photographs. No torque, spacer thickness or fastening order is specified without that evidence.

![Basilisk enclosure, previous v1.0b CAD revision](../9_Assets/basilisk-enclosure.webp)

The illustration is **v1.0b**, the previous revision; use the v1.0 fabrication files linked above.

Continue with [Setup](#setup) once assembly is checked.

## Setup

### Firmware and first connection

The [product page](https://zurp-astronomics.github.io/basilisk/) provides the web installer. It uses the merged release image: **installing this way erases the board's saved focus mark**. For an update that keeps it, use the release's `_flash.zip` package and its `flash.sh` or `flash.cmd` script; see [Offline kit](#offline-kit).

Open **Bench** on the product page, or the standalone HTML from the release, select the board's USB port and connect. The bench and firmware come from the same release. Mount a lens and wait for the ready state. Its identity, focus limits and aperture range should appear before you ask it to move. Only one application can hold the serial port: disconnect the bench before opening your astronomy software.

For a night without internet, download the release's `_bench.html` file in advance. It is the standalone bench. If local-file access is refused, serve its folder with `python3 -m http.server` and open it through `http://localhost:8000/`. The browser must provide Web Serial.

### Ekos / INDI

Select **Pinefeat CEF** as the focuser and choose the board's serial port. The stock INDI driver handles focus and aperture. The driver supplied in this repository adds the focus mark, aperture readback, lens identity, lens restart, LED brightness, reported focus limits and sensor properties.

The supplied driver needs **libindi 2**. Build it from the repository root:

```sh
cmake -S 6_Driver/indi -B build/indi
cmake --build build/indi
sudo cmake --install build/indi
```

Install the C/C++ build tools, CMake, pkg-config and libindi development package first. The default installation goes to `/usr/local`; retain that prefix to keep the distribution's driver intact. `command -v indi_pinefeat_cef_focus` shows which binary will be found. The INDI device retains its upstream name, **Pinefeat EF Lens Controller**. The serial baud setting has no effect on native USB.

The [driver reference](../6_Driver/indi/README.md) records its properties, provenance and patch series.

### ASCOM and other clients

Use the [Pinefeat ASCOM driver](https://github.com/pinefeat/cef135) and choose the board's COM port. Basilisk implements its USB command contract.

Moonlite clients can drive focus and read temperature. That protocol reports **0 °C when the temperature is unknown**; the bench and Basilisk INDI extension can represent the missing value explicitly. Custom clients should follow the [USB contract](../7_Docs/PROTOCOL.md).

### Samyang AF 135 custom switch

For this recognised model, the optional [tool station](../5_App/tool_station/) reads and sets the M1/M2 assignments: AF, MF or aperture ring control (AP). This setting lives **in the lens's flash**, unlike Basilisk's focus mark.

[Optional M1/M2 configuration](#optional-m1m2-configuration) below gives the commands and scope. The tool asks before writing and reads the setting back. **Power-cycle the lens after either a read or a write.** This is optional configuration, not part of normal focus and aperture use.

## Usage

### Plug in, let it start

Mount the lens and connect USB-C. Basilisk powers and identifies the lens, lets it home, then returns to a stored mark if one exists. Commands needing the lens report busy during startup. Removing the lens cuts its power.

Drive focus from the bench or your astronomy software. The allowed range comes from the lens, with a margin of five steps at each end. There is no manual calibration. An out-of-range target is refused. Set aperture within the reported range; the displayed value comes back from the lens.

### Keep a focus mark

The mark belongs to **the board**, keyed by lens identity and nominal focal length rounded to millimetres. It stores a focus position and approach direction. Set it at the position you want to recover; return to it from the bench or the supplied INDI driver. A normal startup recalls it automatically, with the stored approach direction to take up mechanical play.

On a lens with a supported barrel button, the action is decided on release:

| Press duration | Action |
|---|---|
| Less than 1 second | Go to the mark |
| At least 1, less than 4 seconds | Set the mark here |
| 4 seconds or more | No action |

The button is used only while ready, with no move in progress. The board mark is separate from any native preset function of the lens.

During recovery from a lost lens or a warm board restart, Basilisk attempts to restore the interrupted move's target, otherwise the last known position, for the same lens and focal length. This recovery information lives in memory, not flash; removing board power clears it. A recovery can move and re-home the lens. It is not uninterrupted tracking of physical focus.

### Ring and aperture

When the lens reports aperture control for its ring, Basilisk translates ring motion into third-stop changes, at most three per second. On the Samyang AF 135, the M1/M2 assignment determines that role; see [Setup](#setup).

### Read the LED

| Pattern | Meaning |
|---|---|
| Steady for 1 s at power-up | Board powered |
| Gentle swell every 10 s | No lens |
| One long flash, then faint slow blinking | Lens detected, starting |
| Quick rise, then slow breathing | Ready |
| Fast flicker, then steady for 1 s | Returning to the mark, then arrived |
| Short fast pulses / long slow pulses | Focus position increasing / decreasing |
| One brief flash | Aperture changed |
| Three flashes / fade-out | Mark set / cleared |
| Double flash every 2 s | Retrying after a failed start or lost lens |
| Repeated groups of 1 / 3 flashes, with a pause | Stopped after lens loss / homing failure |

Brightness is adjustable from 0 to 100%; zero switches the LED off. The board starts at 100% after power-up. The supplied INDI driver can save the chosen value in its configuration and restore it on connection.

### Temperature and humidity

The SHT40 reports local temperature and relative humidity once a second. The board also computes a dew or frost point. **The sensor's thermal relationship to the air, lens and electronics has not been characterised**: these readings do not measure condensation on the optics.

Missing, invalid or older-than-three-seconds measurements are unavailable in the bench and Basilisk INDI extension. The computed threshold is also unavailable at zero humidity, above 60 °C or below a frost point of −65 °C. Moonlite has the different fallback described in [Setup](#setup).

The firmware reports these values; it does not compensate focus or control a heater. Ekos can use the temperature property for its own refocusing policy.

### When a session goes wrong

If neither status stream arrives for two seconds, the board tries to restart the lens. After four consecutive failed attempts it stops. Restart the lens from the bench or the supplied INDI driver, or remove and remount it.

For a useful problem report, open the bench log, enable `LOG ON` (`LOG ALL` for all frames), reproduce the event and export the text. Include `DEBUG`, the board firmware version and lens identity. A power-rail fault is counted in diagnostics; a silent board alone does not identify its cause. The board sends journal events only after you enable them.

The [USB contract](../7_Docs/PROTOCOL.md) defines every command, response and error. The following sections add the offline workflow and design notes.

## Offline kit

Download the artifacts from the [repository releases](https://github.com/zUrp-Astronomics/basilisk/releases). The `_bench.html` file opens the standalone controls; no Astro checkout is needed.

Before leaving, keep the release's `_flash.zip` and `_bench.html`, this repository and the chosen driver on the computer. The site is a convenient entry point; using the device does not require it.

Extract the flash package to a folder and run `bash flash.sh /dev/ttyACM0` on Linux, or `flash.cmd COM3` on Windows, with the board's actual port. These scripts write the separate bootloader, partition table and application images listed in `flash_args`, leaving NVS intact. The merged `_esp32c3.bin` image supplied to the web installer writes from address zero and resets NVS. Keep the files of one release together. The scripts require Python and esptool; follow their own dependency check.

The release tag and the board's `v` response are different identifiers: the release names the delivered bundle; `v` returns the board firmware's major/minor version. Record both when reporting a problem. Downloading the current bench alone does not update a board.

## Optional M1/M2 configuration

From the repository root:

```sh
pip install -r 5_App/tool_station/requirements.txt
python 5_App/tool_station/tool_station.py --read --port /dev/ttyACM0
python 5_App/tool_station/tool_station.py --M1 AF --M2 AP --port /dev/ttyACM0
python 5_App/tool_station/tool_station.py --man
```

Use the actual serial port. `AF`, `MF` and `AP` select autofocus, manual focus and aperture ring roles. The tool reads the existing value, asks before writing and reads it back. This is the explicitly supported write to the recognised Samyang AF 135's non-volatile memory. Its format was established on lens firmware 1.06; the board does not check the lens firmware version. The assignment of M1 to the upper physical switch position comes from analysis rather than a bench check. Verify the physical result after power-cycling the lens; the factory assignment is M1 = AF, M2 = AP. Power-cycle after a read as well.

## Design notes

### Space and mechanics

The E-mount register is 18 mm. The ring-shaped board is 50.87 × 36.3 mm, with components on one side. The printed parts position the electronics and camera around the metal mount and latch recovered from the Meike tube. The [STEP assembly](../2_Hardware/Basilisk_v1.0_enclosure.step) is the dimensional source; the supplied v1.0b illustration is not a drawing of the final v1.0 parts. No alternative camera fit is implied by a similar-looking front plate.

### Power and signals

USB-C supplies both power and native USB data. The lens has separate switched 5 V motor and 3.3 V logic rails, with current limiting and fault reporting. Ten pogo contacts implement the mount interface. Presence detection controls the lens session; removal cuts the rails. The [schematic](../1_Board/Basilisk-v1.0_1-Schematics.pdf) defines the electronics, and the [E-mount reference](E-Mount/protocol.md) gives contact numbering, timing and frames.

### Board v1.0 signal map

| GPIO | Signal |
|---|---|
| 0 | XDETECT |
| 1 / 2 | TXD / RXD |
| 3 / 5 | LENS_CS / BODY_CS |
| 4 | VD |
| 6 / 9 | FAULT_MOTOR / FAULT_LOGIC |
| 7 / 8 | EN_MOTOR / EN_LOGIC |
| 10 | LED |
| 18 / 19 | USB |
| 20 / 21 | SCL / SDA |

The [pin table](../4_Firmware/components/phy/pins.h) and schematic define wiring: presence, rail enables and fault inputs are active low; the LED is active high. No free GPIO. VD runs at 60 Hz with an approximately 60 µs low pulse. Presence is confirmed for 300 ms before startup; removal is confirmed for 2 ms before power is cut and lines idled.

### Two protocols, one device

The PC speaks the line-oriented [USB contract](PROTOCOL.md). The lens speaks the binary [mount protocol](E-Mount/protocol.md). A USB acknowledgement can mean an operation was admitted; it does not by itself prove focus arrived. Motion outcome and readback are separate observations. The board's transmit allowlist restricts which lens messages can leave it; normal focusing does not require factory calibration or firmware-update commands.

The board mark resides in NVS. Warm-recovery state is separate and does not survive loss of board power. The optional Custom switch setting belongs to the lens. Keeping these three lifetimes distinct matters when choosing an update method or diagnosing a restart.

### Where implementation knowledge lives

The [doctrine](DOCTRINE.en.md) records design principles. Contracts, ownership and hardware-specific constraints belong in the headers and nearby comments of [the firmware](../4_Firmware/). The manual describes interfaces and construction; it is not a second implementation specification. Measurements, simulations and static analysis have different evidential scope, recorded in the [confidence report](TRUST.md) and [E-mount provenance](E-Mount/provenance.md).

## Build from source

Use ESP-IDF **5.3.2** for the single supported target, **ESP32-C3**. From the repository root, with `IDF_PATH` pointing to the installed SDK:

```sh
. "$IDF_PATH/export.sh"
bash 4_Firmware/build.sh
```

Output is in `4_Firmware/build/esp32c3/artefact/`: application, bootloader, partition table, merged image, `flash_args`, `flash.sh` and `flash.cmd`. The build checks that its normal flash arguments do not overlap NVS. A source build is not required to use release binaries.

For development, [SUITES.md](SUITES.md) is the maintained reference for checks and prerequisites. Firmware tests use the host compiler and Python; bench tests use Node and the repository's pinned Playwright setup; the tool-station tests have their own entry point. INDI build and patch verification require libindi 2 and the tools named in [its README](../6_Driver/indi/README.md). Tests against a simulated lens establish behavior against that model, not new hardware compatibility.
