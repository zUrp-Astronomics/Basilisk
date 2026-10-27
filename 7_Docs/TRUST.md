# The firmware trust report

> **Historical scope.** This evaluation applies to the commit and validation records stated below. It predates the closure review of 26 October 2026 and does not validate fixes still awaiting integration. A revised report must identify the fixes and checks actually delivered.

**Date** : 2026-10-25
**Dernière révision** : 2026-10-25
**Statut** : actif — the English translation, complete, of `CONFIANCE.md` (self-assessment of the firmware of `main` at commit `0bff024`, firmware 1.3)
**Référencé par** : `README.md` (§ Trust, § Documentation), `7_Docs/CONFIANCE.md`, `7_Docs/MANUEL.md` (§ Arborescence), `7_Docs/README.md`, `CLAUDE.md` (§ Doc)
**Dérivé de** : `7_Docs/CONFIANCE.md`

This document says how Basilisk's firmware was produced and checked, what is proven, what is judged sound and what
remains weak, so that one can decide, on evidence, whether to plug a lens worth several hundred euros into the board.
It is a **self-assessment**: it is written by an agent of the fleet that did not write the firmware, and judged like
any delivery. It claims no outside label. Its external source is the audit by **Codex**, another LLM, with biases
different from the producer's; its reports (2026-10-23) are kept in the workshop and are not published: we quote their
conclusion (`audit-basilisk-conclusion-qualifiee.md`) and the register of 44 observations (`audit-basilisk-registre.csv`),
whose fate the appendix gives.

It describes the code of `main`, not the audited version. Neither the v1.0.0 release (2026-10-20, built from `be2e877`,
as recorded in the workshop) nor the board validation of 2026-10-19 (firmware 0.9) carries the fixes cited here.

## 1. What the firmware promises

**The functions.** Power up and initialise the lens as soon as it is mounted; drive focus and aperture; publish what
the lens reports (position, limits, aperture, name); keep one position, the mark, in the board; handle removal, link
loss and restart. The contract is `7_Docs/PROTOCOL.md`; the letters `f r e a d s c` stay Pinefeat-compatible byte for
byte.

**The meaning of results.** Three stages are kept apart:

- **command accepted**: `ok` to `f<n>` says the goto is admitted, within the limits, nothing more (`PROTOCOL.md:59`);
- **move finished**: read afterwards, `e` then `g`: `ok` arrived, `aborted`, `stall`, or `unconfirmed` when the stop
  was neither acknowledged nor followed by stillness (`PROTOCOL.md:63`);
- **mark stored**: `js` answers `ok` only once the mark is written to NVS, `er store` otherwise (`PROTOCOL.md:73`,
  ticket #747).

`q` answers once the `0x1C` has left, or with an error: a stop sent, not yet confirmed (`PROTOCOL.md:80`, #743).

**The compatibility scope.**

| Level | Lenses | Evidence |
|---|---|---|
| Plugged into the real board | Samyang AF 135 F1.8 FE, Sony FE 24-105 F4 G OSS | maintainer's record, 2026-10-19, unpublished: init, homing, mark, goto, button, switch, `p0`/`p1`, hot removal, lens swap; firmware 0.9 |
| Taken from traces | the 135 (`4_Firmware/traces/`, 2026-09-20), the Sony (`7_Docs/E-Mount/traces/`, 2026-10-08, #544) | `replay` replays each traced pair, 100 % expected; the Sony frames feed the scripted PHY |
| Simulated only | Tamron F051 (`4_Firmware/sim/lens_std.c`) | never plugged; one declared gap, `sim/test/constats_std.txt` |

The other Samyang lenses recognised by signature (`PROTOCOL.md:92`) are neither plugged nor simulated.

## 2. How it is organised to keep it

| Choice | Implementation | Exceptions |
|---|---|---|
| A single task decides | the loop of `app_main`, `main/main.c:86-95`: the session, then HOST | the interrupts of `phy.c` (the cut on removal), the receive, USB, log, store and sensor tasks: they carry, they do not decide (O36) |
| A single path to the lens | `bench_core` alone calls the transmit, on an allowlist (`components/bench_core/bench_core.c`, `PROTOCOL.md:35-46`) | `CUSTOM WRITE`, to the 135 only, the human's decision (`svc135_allowed`); the init's `0x09`, which every body sends, touches an accessory flag of the V-AF 24, a reservation written at the head of `listed_size` |
| A single decoder, which validates | `session/lens_rx.c`: an out-of-domain value is published unknown (#728) | none |
| One owner per datum | the `p0` hold (#709), the admission of gotos and of the return (#763), the move cycle (#767) | HOST still predicts admission outside `ready` (B4 (a), § 4) |
| Storage, USB, sensor outside control | separate tasks and queues; a failed NVS leaves focus usable (#676) | — |
| Bounded queues | `phy.c:808-811`, `store.c:127-128`, the 8 KB log ring; each loss counted (`evq_drop`, `dropped` in `DEBUG`) | — |
| Limits measured, never coded | those of the `0x06`, narrowed by 5 steps (`lens_rx.c:80-81`); a goto outside them is refused (`motion.c:198`) | X = 200 approach steps on the 135 (`PROTOCOL.md` § 7) |

## 3. What protects the lens

Three distinct properties: **lens protection** (nothing written, no line driven without its rails, cut on removal),
**correct execution of the move** (arriving where it said), **focuser availability** (returning to service).

| Situation | The board commands | It observes | If the observation is missing | Checked by |
|---|---|---|---|---|
| Plugging in | nothing before insertion is confirmed | the XDETECT contact, debounced | no start | `test_phy` (debounce), `test_session135`; record of 2026-10-19 |
| Start | logic rail, motor rail at least 50 ms later, lines at least 50 ms later, handshake | LENS_CS, init replies | three `0x01`, soft reset, `recovering`; at the 4th failure, `fault` (`PROTOCOL.md` § 7) | init compared with `astro-normal.txt:222-254` (`test_session135`); delays under lag (#689, #691) |
| Move | a `0x1D` within the limits | position and movement from `0x06`/`0x05` | start seen within 1 s, stillness within 10 s (`motion.c:20-21`), resends, then `stall` and `0x1C` | `test_session_script`, `test_stop_goto135` |
| Stop | `0x1C` | acknowledgement, stillness | three sends in 4.5 s, then `unconfirmed` | #743, #787 |
| Removal | rails and lines cut in interrupt, outside the session (`phy.c:243-253`, `278-293`) | XDETECT | nothing: the cut depends on the contact alone | logic played in `test_phy`; `phy.c` by reading; duration not measured (`phy.c:240-242`); hot removal in the record |
| Silent lens | nothing new | the `0x05`/`0x06` streams | 2 s without one: its data unknown; without both: loss, retry 3 s later, rails kept (`session.c:254-261`) | `t_loss`, `t_resume_*` of `test_session_script`; Codex's six failure scenarios (O38) |

`test_session135` checks after each command that no line is driven without both rails (`SUITES.md:174`). What no host test plays
(`phy.c`, the real NVS, `host_usb.c`, `sensor_board.c`, `led_board.c`, `main.c`) is demonstrated by reading in its
header or on the bench (`7_Docs/SUITES.md:206-216`). No record of the firmware of `main` on the board is known: the last one, on 2026-10-19, carried firmware 0.9.

## 4. What the trust rests on

**Mechanised checks** (a script fails, in CI, `.gitea/workflows/ci.yml`):

- the allowlist, `test_bench_core`, and `garde_emission.sh`, which rejects a copy where another caller transmits;
- **1,565 mutants** (`mutations.txt` 1,464, `mutations_std.txt` 101, recounted at commit `0bff024`): each must be
  killed;
- the 15 C programs replayed under ASan and UBSan, `-Werror` everywhere (`4_Firmware/run.sh`);
- the replay of real traces (`replay`);
- at build, the silent console, the watchdog in panic mode and the UART ISR in IRAM (`4_Firmware/build.sh:33-53`), the
  NVS guard (`garde_nvs.py`);
- the INDI driver built and played under libindi against a fake Pinefeat and a fake Basilisk (`6_Driver/indi/test.sh`).

**Checks that rest on an agent's reading**: expected values written by hand from the reference, never by the code under
test (`SUITES.md:104`); `PROTOCOL.md` changed in the same commit as the code; fake lenses that declare what they
simplify; demonstrations by reading; the judges' verdicts.

**Selected demonstrations.**

- **Stop then goto.** Codex reproduces an old `0x1C` sent after a new goto, 14873 acknowledged for a target of 16623
  (O01-O02). #680 fixes the frame order (`t_stop_goto_order`). #730 plays the stop and the goto at 167 phases of a VD
  cycle, judged on the frames sent and on the fake 135's position (`test_stop_goto135.c`): it finds a defect the audit
  had not seen, a goto dropped just after a stop never carried out. #787 fixes it: the `0x1D` waits for the end of the
  stop, and every case ends on its target (`t_stop_wait_q`).
- **Delays under lag.** Codex measures 49, 45, 30 then 0 ms of wait instead of 50 for 1 to 50 ms of lag (O04). #689 and
  #691 count each delay from the actual action; `t_lag_audit` replays the audit's case, 150 ms without a step, and
  requires the lines 50 ms after the motor rail, BODY_CS 15 ms after the lines.
- **Unknown data.** An out-of-domain aperture, an inverted range, limits empty after the margin are published unknown,
  never corrected (#728: `t_aperture_domain`, `t_aperture_inverted`, `t_limits_invalid`).
- **Resume.** After a loss, the board returns to the last position, even if the loss happens during that return
  (`t_restore_recovery`, `t_resume_restoring_loss`).
- **Mark stored.** `ok` only after the write; a failure or the 2 s deadline gives `er store` (#747: `t_mark_stored`,
  `t_mark_store_fail`, `t_mark_deadline`).

**Known limits** (§ 4 of the source material, up to date on 2026-10-25):

| Condition | Consequence | Handling | Why accepted |
|---|---|---|---|
| FAULT on a rail | counted and logged, nothing is cut | cut decided, not coded | waits for a bench measurement of a held FAULT (O29) |
| A defect that crashes every start | restart loop through the watchdog | backlog | the human's decision, 2026-10-23 |
| Deferred homing reply (`0x10`) during a resume | can end a request of its type (`PROTOCOL.md:424-426`) | left to fuzzing and to the bench | judged theoretical; the CUSTOM case is fixed (#789) |
| The 135 stops a few steps from the target (±5) | `f` publishes the position where it stopped | left to Ekos | the human's decision, 2026-10-23 |
| Three internal contracts without a guard: `vline` (`journal.c:252`), `bsk_txn_held_end` (`txn.c:212`), `session_ack_last` (`session.c:161`) | none reachable today | wait for fuzzing, planned, not started | the human's decision, 2026-10-24 |
| B4 (a): HOST predicts admission outside `ready`, CMD's refusal is not logged | no divergence today | open, not settled | fixing it changes the contract |
| No record of the fixed firmware on the board | real behaviour not measured | next bench tests | not a choice: waiting for the bench |

## 5. What it says about the fleet

The firmware is produced by **LCARS**, a fleet of **Claude** agents:

- the human decides, and does not code;
- the architect frames each ticket: a result, checkable criteria, a scope;
- the scoper reviews each brief against the code and sends it back if a fact is wrong or a decision is missing;
- the engineer codes;
- two judges judge each delivery: the qualifier, the evidence (do the tests bite?); the reviewer, the deliverable
  against its criteria;
- Codex audits from outside.

Figures from the fleet's register, recorded on 2026-10-23: **351 brief judgements** by the scoper, 192 accepted as
written, 157 sent back (45 %), 2 abandoned; **610 delivery verdicts** on **182 tickets**. The producer does not see
this register and cannot recount them.

A dated fact: the fleet's test-relevance probe returned no verdict for most of the project; the judges measured how
the tests bite through the mutants, by hand.

**Scope.** This case shows that, on a real embedded firmware, this process produced code that is understandable,
testable and correctable: an external audit found two priority defects, each was understood, fixed and locked by a
test, and the sweep written for the check found a third. It does not show that all code from this process will be
reliable. Trust rests on the code and its tests, not on its provenance.

## Appendix — the fate of Codex's register

Mapping from the source material (`plans/rapport-confiance.md` § 6, workshop, commit `0c6111a`). Each ticket can be
checked on `main` by its merge message (`from lcars/issue-<n>-engineer`); the tests cited are in `4_Firmware/sim/test/`.
"Kept": on the auditor's own recommendation (`audit-basilisk-conclusion-qualifiee.md`, § Observations conservées),
unless stated.

| Obs. | Subject | Fate | Ticket, evidence |
|---|---|---|---|
| O01-O02 | old stop after new goto; success off target | fixed | #680 (`t_stop_goto_order`, `t_stop_goto_variants`); sweep #730 (`test_stop_goto135.c`); its finding fixed by #787 (`t_stop_wait_q`) |
| O03 | stop acknowledged before it was sent | fixed | #743 (`t_stop_held`, `t_stop_held_host`) |
| O04 | delays shortened by lag | fixed | #689 (`t_lag_audit`, `t_lag_waits`), #691 (`t_lag_hs_up`, `t_lag_hs_down`) |
| O05-O06 | NVS failure masked; error taken for absence | fixed | #676 (`t_nvs`, `t_restore_store_error`); the error sorting of `store.c` played since by `test_store` (#747) |
| O07 | mark announced before the write | fixed | #747 (`t_mark_stored`, `t_mark_store_fail`, `t_mark_deadline`) |
| O08-O11 | inconsistent aperture and limits published | fixed | #728 (`t_aperture_domain`, `t_ring_domain`, `t_aperture_inverted`, `t_limits_invalid`), completed by #776 (`jg` on invalid limits) |
| O12-O13 | old reply taken for the new one | fixed | #783 (`t_custom_late`, `t_custom_desync_kept`, `t_init_not_echo`); CUSTOM abandoned by a loss: #789 (`t_custom_lost`) |
| O14 | presence and queue head swapped under saturation | fixed, proven by reading | #738: demonstration at the head of `bsk_phy_poll` (`phy.c`); no test, the simulated PHY has no concurrent producer |
| O15 | lines briefly re-enabled after a cut | kept | known window, written in `phy.c`; no pulse measured |
| O16 | USB semaphores not checked | fixed, proven by reading | #734; `host_usb.c` is played by no host test |
| O17 | gesture accumulator not saturated | fixed | #734 (`t_ring_bound`, `t_ring_bound_rest`) |
| O18 | `p0` hold duplicated | fixed | #709 (`t_hold_d2`, `t_hold_session`) |
| O19-O20 | 135 recognition repeated; two admissions | fixed | #763 (mutants in `mutations.txt`, no test added) |
| O21 | repeated predicates | partly fixed | #767 (`t_thirds`); the admission predicted by HOST stays open: B4 (a) |
| O22 | two tables of aperture thirds | fixed | #767 (`t_thirds`) |
| O23 | TXN mutations open to callers | kept | callers disjoint today |
| O24 | partial `pair_late`, `tx_wait_us` measures | fixed (documented) | #738: `PROTOCOL.md:143` and the header of `bsk_phy_send` say what is counted |
| O25 | step duration unbounded | kept | no UART failure demonstrated |
| O26 | starvation under sustained PHY activity | kept | real load not established |
| O27 | 8-bit log generation | fixed | #734 (`t_gen_wide`) |
| O28 | number wrap-around | fixed | #734 (`t_seq_wrap`) |
| O29 | FAULT without a cut | kept, the human's decision | cut waiting for the bench measurement (§ 4) |
| O30 | watchdog ≠ availability | kept | limit of the conclusion; see the restart loop (§ 4) |
| O31 | F051 already powered | kept | `constats_std.txt`, `reste_alimente`; model or firmware, not settled |
| O32 | INDI timeout not global | fixed | #672 (`6_Driver/indi/test/test_driver.py`) |
| O33 | RAM and stacks not measured on target | kept | margins readable through `DEBUG`; no shortage established |
| O34 | official suite not run by the auditor | kept | limit of the audit; the suite runs in CI |
| O35 | oracles and firmware share the codec | kept | expected values written by hand; oracles on observable outputs |
| O36-O42, O44 | supported points: serialisation, publication, convergence, state machines, acknowledgements, multiple paths, memory bounds, distinct data | kept | positive results of the audit |
| O43 | application tests outside the firmware | kept | outside the firmware's scope |
