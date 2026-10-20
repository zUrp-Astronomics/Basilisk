---
date: 2026-10-20
last-revision: 2026-10-20 (ticket #661, the site's review: one voice, no dated text, links between tabs)
status: active — the Setup tab of the product page, read by the zUrp website, which places the install block (flash button, downloads, Bench button) in this tab
referenced-by: 9_Assets/README.md
derived-from: README.md (§ Flash), 7_Docs/MANUEL.md, 6_Driver/indi/README.md, 5_App/tool_station/tool_station.py
---

## Flash the firmware

The install button on this page writes the latest released firmware onto the board, from your browser.

1. Use **Firefox, Chrome or Edge on a computer** (not Safari, not a phone).
2. Plug the board into the computer over USB-C, **with a data cable**. Nothing else is needed: the flashing tool puts
   the board into download mode over USB, and no driver is needed.
3. Click the install button and pick the board's port: `/dev/ttyACM0` on Linux, `/dev/cu.usbmodem…` on macOS, `COMx`
   on Windows ("USB Serial Device").

The button writes the single image of the release at `0x0`: **it resets your focus mark.** On a new board that does
not matter. To update a board later **and keep your mark**, use one of the other ways in the
[README, § Flash](../../README.md#flash): esptool-js in the browser, or the `flash.sh` / `flash.cmd` scripts of the
release's `_flash.zip`. They write only the bootloader, the partition table and the application, and leave the mark
alone. The French manual details all three ways: [`7_Docs/MANUEL.md`](../../7_Docs/MANUEL.md), § Le flasher.

The firmware reports its own version with the `v` command. That number is the version of the board's firmware, not
the release tag.

## Check it with the bench page

The Bench button on this page opens the bench page of the same release: a single web page that talks to the board
over Web Serial. Nothing to install.

- **Browser**: Firefox 151 or later, Chrome or Edge, on a computer.
- **Connect** and pick the board's port. Opening the port may restart the board; the page sends `v` until the board
  answers, then reads its state.
- **Language**: French or English — the browser's language first, then the **FR | EN** switch at the top of the page,
  remembered between visits.

With a lens mounted, the page shows the lens identity and the board's state, the sensor readings, the focus and
aperture controls, the mark, and a log you can export. See the [Usage](usage.en.md) tab.

The same page is in each release as `basilisk_<tag>_bench.html`, to open from your disk. If the browser refuses Web
Serial on a `file://` page, serve its folder with `python3 -m http.server` and open
`http://localhost:8000/` followed by the file name.

## Ekos and INDI (Linux)

**The driver of this repository** is INDI's own Pinefeat driver, with a set of fixes and the Basilisk extension:
the marks, the aperture read back, stop, the LED brightness, restarting the lens and its name, the lens's focus
limits as the position range, and the temperature, humidity and dew or frost point of the board. It keeps the
upstream names: the binary `indi_pinefeat_cef_focus`, "Pinefeat CEF" in an Ekos profile, the device "Pinefeat EF Lens
Controller" in the INDI panel. It needs **libindi 2** (INDI's PPA, `ppa:mutlaqja/ppa`).

From a clone of the repository:

```sh
sudo apt install cmake make g++ pkg-config libindi-dev
cmake -S 6_Driver/indi -B build && make -C build
sudo make -C build install
```

It installs under `/usr/local`, next to the driver of INDI's package, without overwriting it.
`command -v indi_pinefeat_cef_focus` tells which one Ekos will start; `sudo rm /usr/local/bin/indi_pinefeat_cef_focus`
goes back to the package's. Do not install it with `-DCMAKE_INSTALL_PREFIX=/usr`: it would silently replace the
package's file.

**In Ekos**: in the profile, Focuser → **Pinefeat CEF**; in the INDI panel, the board's serial port. The baud rate has
no effect: it is a USB port.

**INDI's stock Pinefeat driver** (`indi_pinefeat_cef_focus`, shipped with INDI) works too, on position, motion and
aperture, with nothing to configure on the board.

Everything about the driver — its properties, how it is built and tested — is in
[`6_Driver/indi/README.md`](../../6_Driver/indi/README.md) (French).

## ASCOM (Windows)

Use **Pinefeat's ASCOM driver** as is ([github.com/pinefeat/cef135](https://github.com/pinefeat/cef135)): Basilisk
serves everything it sends, with the replies it expects. Choose the board's COM port.

## Other software

A program that only knows the Moonlite protocol can drive the focus and read the temperature too; it reads 0 °C while
the board does not know the temperature, because Moonlite has no way to say "unknown". Any serial terminal works as
well: one command per line, described in [`7_Docs/PROTOCOL.md`](../../7_Docs/PROTOCOL.md) (French).

## The M1/M2 switch of the Samyang AF 135

The Samyang AF 135 F1.8 FE has a two-position custom switch, M1 and M2. What each position does — AF, MF, or the ring
on the aperture (AP) — is a configuration stored in **the lens's flash memory**; from the factory, M1 = AF and
M2 = AP. `5_App/tool_station/tool_station.py` reads and writes it through the board, without Samyang's tool:

```sh
pip install -r 5_App/tool_station/requirements.txt
python 5_App/tool_station/tool_station.py --read --port /dev/ttyACM0
python 5_App/tool_station/tool_station.py --M1 AF --M2 AP --port /dev/ttyACM0
python 5_App/tool_station/tool_station.py --man
```

- Before writing, the tool shows the configuration it read and the one it will write, and asks for confirmation;
  afterwards it reads it back.
- **After a read or a write, unplug and replug the lens.**
- The board refuses any other lens, other Samyang lenses included.
- The format is established on the lens's firmware 1.06; on another version it is not checked. **Writing is at your
  own risk.**
- That M1 is the upper position is read in the lens's code, not yet checked on a lens. On a first try, write for
  example `--M1 MF --M2 AP`, replug the lens, and turn the ring with the switch on M1: it should focus. If M1 and M2
  are swapped, go back to the factory setting (`--M1 AF --M2 AP`).

The full instructions: `python 5_App/tool_station/tool_station.py --man` (French), and
[`7_Docs/MANUEL.md`](../../7_Docs/MANUEL.md).
