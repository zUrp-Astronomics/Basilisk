# Pinefeat CEF: optional extended set for the Basilisk E-mount lens controller

**Date** : 2026-10-19
**Dernière révision** : 2026-10-23 (ticket #694 : `BOARD_RESTART`, le dernier redémarrage de la carte, `x`) ;
2026-10-23 (ticket #672 : les lignes sautées lues dans l'échéance de la réponse, et
contrôlées sans fin) ; 2026-10-20 (ticket #648 : la version du driver reste 1.2) ; 2026-10-19 (ticket #645, reprise de
la revue de la PR #646 : la réponse à `v` hors forme déclarée, les contrôles ajoutés)
**Statut** : prêt — description de la pull request de `patches/basilisk/` pour indilib/indi, non soumise (l'humain
contactera Pinefeat d'abord) ; elle s'applique après celle de `patches/fixes/` ; le titre ci-dessus est celui de la
PR, son corps est tout ce qui suit le trait
**Référencé par** : `6_Driver/indi/README.md`

---

The Basilisk board (zUrp Astronomics, open hardware) drives a Sony E-mount lens from a PC, mainly as a focuser for
astrophotography. It serves the Pinefeat CEF protocol on `v f r e a d s c`, so the Pinefeat driver already focuses it
as is. This change lets the same driver use what the board serves beyond that, as an **optional extended set**: it is
enabled only for a controller that reports it, and a Pinefeat adapter gets the driver as before. It applies on top of
the Pinefeat CEF fixes series (PR #…), and keeps the driver version that series sets, 1.2.

## What it adds

At the connection, after `v`, the driver sends `t`. A Pinefeat adapter answers `er`. A controller whose reply carries
the token `ext=1` gets:

| Property | Letters | What |
|---|---|---|
| `APERTURE` (`FNUMBER`, RO) | `o` | the current f-number, read once a second while still; follows the lens's own ring |
| `FOCUS_ABORT_MOTION` | `q` | the standard abort (`FOCUSER_CAN_ABORT`, set at the connection) |
| `FOCUS_MARK` (RO), `FOCUS_MARK_CTL` | `j`, `js` `jg` `jx` | a focus position stored in the board: read, set here, go to, clear |
| `LED_BRIGHTNESS` (Options) | `k` | the status LED, 0-100 %; the board keeps none, so it is saved in the config and sent at each connection |
| `LENS_REBOOT` | `b` | restart the lens |
| `LENS_INFO` (`LENS_NAME`, RO) | `i` | the lens's name |
| `FOCUS_TEMPERATURE`, `FOCUS_HUMIDITY`, `FOCUS_DEW_POINT` (RO) | `z` | the board's sensor, read once a second, while the focuser moves too; each property exists only while the board reports its value, so Ekos never takes an unknown temperature for a fresh one |
| `BOARD_RESTART` (`CAUSE`, RO, Info) | `x` | the cause of the board's last restart; a crash, a watchdog, the supply or an unknown cause is logged as a warning, once per restart, at the connection and while connected (below) |
| end of a move | `g` | Ok, Idle when aborted, Alert when stalled or when the stop is not confirmed |
| position range, `CALIBRATE` | `r` | both bounds of `r` are the range of the position and of `FOCUS_MAX`, at each read; `CALIBRATE` reads them again, with no `c` and no Busy: the board's bounds are the lens's own, there is nothing to calibrate |

The board may restart while the port stays open, and after a crash or a watchdog it resumes its lens, so the focus
moves: `x` answers the cause and the time since the restart, which starts from 0 at each one, so that two restarts of
the same cause differ, however close. A reply is a new restart when its cause differs from the last one, or when its
time is less than it can be without a restart: the last one, plus the host's time from that reply's arrival to this
command's sending (the board reads its clock between the command and its reply, however late either comes), less
10 ms and 0.1 % of that time (the board truncates to the ms, and the two clocks don't run at quite the same rate). It
is read at the connection, then once a second, while the focuser moves too: the board reports itself moving while it
resumes, and the warning comes during that, not after.

Every Busy the set sets ends once `e` reports the focuser still. Without `ext=1`, no property of the set is defined
and none of its letters is sent, even when a client writes one of its properties by name: libindi 2 refuses a property
the driver has not defined before the driver sees it, as it does without this change.

## What a Pinefeat adapter sees

Before `t` answers, the driver can't know which controller it talks to, so three things apply to every controller:

- one `t` per connection, which a Pinefeat adapter answers `er`;
- a line starting with `*` is never a reply: it is skipped up to its line end (the board's debug log, when its bench
  page left it on). No Pinefeat reply starts with `*`;
- at `v`, only a line of the form of a version (`<major>.<minor>`, what `readFirmwareVersion()` already reads) is the
  reply: opening the port can restart the board, and the lines of its ROM come after `v`. A Pinefeat adapter answers
  `v` with its version.

Both skips are read within the reply's deadline, which the fixes series sets: a command fails `CEF_TIMEOUT` after it
was sent, whatever the controller keeps sending, lines or a line that never ends.

**One behaviour does change for any controller**: a reply to `v` that is not of the form `<major>.<minor>` (say
`abc`) was taken as the version, with the minor read as 0, and the controller connected; with this change it is
skipped as a ROM line, the three tries of the handshake run out, and the controller is not detected. We know of no
Pinefeat firmware that answers so: the driver already parses the reply with `%d.%d` to decide whether to read `r`.
If one does, this is the change to discuss.

## How it was verified

- Built against libindi 2.2.5 (the INDI PPA, Ubuntu 26.04), with `-Wall -Wextra`: no warning; the touched lines pass
  `astyle` with INDI's options.
- The driver binary run on its own, spoken to in INDI XML on its standard input and output, against fake controllers
  on a pty:
  - a fake Pinefeat adapter: the same checks pass with and without this change (connection at several firmware
    versions, moves, speed, aperture, `CALIBRATE`, and the fixes of the previous series); no property of the set is
    defined and no letter of it is sent, `t` once per connection; a reply `abc` to `v` connects without this change
    and is not detected with it (above);
  - a fake Basilisk board: the connection through a restart (ROM lines instead of the first reply to `v`) with the
    log on, a log line cut around a command; the set defined; the bounds of `r` and `CALIBRATE`; the end of absolute
    and relative moves (`ok`, `stall`, `aborted`, then `unconfirmed`), on both position properties; a move whose
    position can't be read at its end, and a reboot after which it can't, both Alert; the mark, the LED (saved in the
    config and sent again to a new board at the next connection), the reboot;
    `o`, `i`, `z` and its unknown forms, and `z` new then unknown during a move of 10 s, each within 2 s; each restart
    cause at the connection, a warning for the six that are not normal only, and again at a new connection to a board
    that has not restarted since; a restart with the port open, then a resume of 4 s: the warning within 3 s, during it,
    once, and again for a second restart of the same cause; two restarts of the same cause a second apart (`panic 300`,
    then `panic 900`), a warning each; 30 s without a restart, each reply delayed at random up to 0.4 s, no warning;
    lines without end, non-version lines at `v` for ever and a log line that never ends, each command failing within
    `CEF_TIMEOUT` (and a margin for the error to reach the test); then a Pinefeat adapter on the same driver, without
    the set and with the original range.
- Not verified on a Pinefeat adapter: we have none.

## What this does not change

- Anything a Pinefeat adapter gets, but the three points above and the reply to `v` that is not a version.
- The rule that reads `r` only from firmware minor 3: the board reports its version as it is; a board under minor 3
  loses the bounds too.
- The driver's name, device name and executable.
