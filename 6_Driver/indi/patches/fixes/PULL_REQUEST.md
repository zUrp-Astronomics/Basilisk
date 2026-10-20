# Pinefeat CEF: reply handling, refresh rate and Busy states fixes

**Date** : 2026-10-19
**Dernière révision** : 2026-10-19 (ticket #645)
**Statut** : prêt — description de la pull request de `patches/fixes/` pour indilib/indi, non soumise (l'humain
contactera Pinefeat d'abord) ; le titre ci-dessus est celui de la PR, son corps est tout ce qui suit le trait
**Référencé par** : `6_Driver/indi/README.md`, `6_Driver/indi/UPSTREAM.md`

---

This series fixes seven defects of the Pinefeat CEF focuser driver (`drivers/focuser/pinefeat_cef.{h,cpp}`), one
commit per defect, and bumps the driver version to 1.2. It applies on `master` as of 1dd9b34f24 (v2.2.5). Nothing
else changes: the protocol, the properties, their names and the commands sent are the same, except where a fix says
otherwise.

## The fixes

1. **Read each reply as a string without its line end.** `tty_nread_section()` keeps the delimiter and writes no
   terminating zero when the reply fills the buffer, and `sendCommand()` handed those raw bytes over:
   - a reply of 15 characters and its `\n` fill `CEF_BUF` (16 bytes) with no terminating zero, and `strstr()`,
     `sscanf()`, `setText()` and the log read past the buffer;
   - the line end is kept in the texts shown: `FIRMWARE_VERSION` reads `1.3\n`, and so do `FOCUS_DISTANCE`,
     `RANGE_APERTURE` and the log (`Detected firmware version 1.3\n.`);
   - `ERR_NC()` tested `strcmp(res, "nc") > 0`: true for `nc\n` only because of its line end, and also for any reply
     sorting after `nc`, logged as "lens is not attached" in place of its own text.

   `sendCommand()` now replaces the line end (`\n`, and a `\r` before it) with the terminating zero, within `CEF_BUF`,
   and `ERR_NC()` compares the whole reply with `nc`.
2. **Format the focus moves as unsigned.** `moveFocusAbs()` and `moveFocusRel()` wrote a `uint32_t` with `%d`. No
   visible effect within the range of the properties; above `INT32_MAX` the command would go negative. Now `%u`.
3. **Show the aperture with one decimal.** `ABS_APERTURE` and `REL_APERTURE` were formatted `%.f`: f/5.6 showed as 6,
   f/1.8 as 2. The value sent was right; only the display rounded it. Now `%.1f`.
4. **Refresh the state once a second.** `TimerHit()` refreshes once a second has passed since `lastUpdate`, but
   `lastUpdate` was set only by the constructor and `CALIBRATE`: a second later the test held at every timer pass,
   every 50 ms, sending `e` twenty times a second and, while still, `e f r d a` a hundred commands a second.
   `lastUpdate` is now set at each refresh. `CALIBRATE` keeps its delay.
5. **End a Busy move or calibration when the state can't be read.** A move sets the position Busy, an accepted
   `CALIBRATE` the position and `FOCUS_MAX`. Only a refresh where `f`, `r`, `d` and `a` all answer ended them: a lens
   removed (`f` answering `nc`) left them Busy for ever. Once `e` reports the focuser still, a read that fails now turns
   each of them still Busy to Alert.
6. **Log a position that can't be read once, not at each refresh.** Without a lens, "Can't read focus position: lens
   is not attached." was logged at each refresh for as long as the lens was away. It is now logged when it first
   comes, and again only if the reply changes or after a position was read; each connection logs it anew.
7. **Don't take the rest of a stale line for the next reply.** `tcflush()`, before each command and after each reply,
   discards only what is already received: when it cut a late reply in two, the rest arrived after it and was read as
   the next command's reply. The pending input is now read and discarded instead, which shows where it ends; if it
   ends inside a line, the rest of that line is discarded before the reply. The input is no longer flushed after the
   reply. A late reply that arrives whole after the next command was sent is still taken for its reply: the protocol
   carries nothing that ties a reply to its command. The cost: if the rest of a stale fragment never arrives, the next
   command's reply is taken for it and discarded, and that command fails after `CEF_TIMEOUT`, as with no reply; the
   line is clean again for the command after it.
8. **Bump the driver version to 1.2.**

## How it was verified

- Built against libindi 2.2.5 (the INDI PPA, Ubuntu 26.04), with `-Wall -Wextra`: no warning.
- The lines each commit touches pass `astyle` with the options of INDI's README, as `scripts/check-codestyle.sh` does.
- The driver binary run on its own, spoken to in INDI XML on its standard input and output, against a scripted fake
  Pinefeat controller on a pty (`v f r e a d s c`, `er` to anything else). For each fix, a check shows the defect on
  the unpatched driver, failing there with the symptom it declares, and passes with the series (fix 2 has no visible effect within the properties' range, and fix
  1's read past the buffer has no deterministic symptom, so the unpatched driver is checked on the line end and on
  `ERR_NC()`). The same checks of the driver's existing behaviour pass on both: connection with firmware 1.3, 1.2, 0.9
  and 2.0 (`r` read only from minor 3, as before), absolute and relative moves, speed, aperture, `CALIBRATE`.
- **Not verified on a Pinefeat adapter**: we have none. The fake controller follows the replies the driver itself
  reads; its timings are made up.

## What this does not change

- The bounds of `r`: the low bound is still not used, and `FOCUS_MAX` still takes the high one only after
  `CALIBRATE`. Whether `r` can be trusted before a calibration is the controller's to say, not ours.
- `r` is still read only when the minor of the firmware version is 3 or more.
- No property, name, label or command is added, removed or renamed.
