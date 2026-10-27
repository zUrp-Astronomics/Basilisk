# Samyang E-mount lenses — what the body sees


## 0. What this document is

This is the Samyang supplement to [the common protocol](protocol.md). It covers model identity, the AF 135 session, motion, controls and the bounded service subset used by Basilisk. Offsets start after the message type; sizes include it. Multi-byte fields are little-endian unless stated otherwise.

The direct bench evidence concerns **AF 135 firmware 1.05**. Static analysis concerns **1.06** and eleven other images listed in § 1. A disagreement between those versions remains a disagreement; a capture is evidence for the device that produced it. Code references establish the board or simulator's behavior, not an independent measurement of the lens.

Paragraphs or tables labelled **static analysis, not observed** retain that status throughout the section. Captures are cited by original filename and line. See [provenance](provenance.md) for the source matrix and publication boundary. Internal calibration algorithms, memory maps and factory write procedures are omitted; decoded interface fields and their limitations remain.

The numerical section IDs are retained for references from firmware comments, notably § 6.3 and § 7.2. Those sections now state the public service contract and persistent-effect conclusions rather than a factory command inventory.

## 1. The twelve models

### 1.1 Identity

Static analysis, not observed, except the AF 135 row (captures cited) and the ExifTool column.

| Model | Firmware read | Microcontroller | LensType2, `0x07` offsets 9-10 | `0x3F` name (offsets 1-64) | ExifTool name for this LensType2 |
|---|---|---|---|---|---|
| AF 135 F1.8 FE | 1.06 (bench lens: 1.05) | PIC32 | 8 | `SAMYANG AF 135mm F1.8` | none (ExifTool lists the 135 under 51518) |
| V-AF 24 T1.9 | 1.06 | PIC32 | 12 | `SAMYANG VAF 24mm T1.9`, or `SAMYANG ANA 24mm T1.9` with an anamorphic accessory | none |
| AF 35-150 F2-2.8 | 7.03 | STM32 | 13 | `SAMYANG AF 35-150mm F2-2.8` | Samyang AF 35-150mm F2-2.8 |
| AF 16 F2.8 | 3.02 | STM32 | 23 | `LK SAMYANG AF 16mm F2.8 P FE` | none |
| AF 14 F2.8 (STM32, "14 A" below) | 3.27 | STM32L451 | `0xC931` = 51505 | `SAMYANG AF 14mm F2.8` | Samyang AF 14mm F2.8 or Samyang AF 35mm F2.8 |
| AF 14 F2.8 (PIC32, "14 P" below) | 2.27 | PIC32 | `0xC931` = 51505 | `SAMYANG AF 14mm F2.8` | the same |
| AF 24 F1.8 (two files, "24" and "24 A") | 2.05 and 3.05 | PIC32 | `0xC93A` = 51514 | `SAMYANG AF 24mm F1.8` | Samyang AF 24mm F1.8 |
| AF 75 F1.8 | 1.07 | PIC32 | `0xC938` = 51512 | `SAMYANG AF 75mm F1.8` | Samyang AF 75mm F1.8 |
| AF 85 F1.4 II | 1.03 | PIC32 | 9 | `SAMYANG AF 85mm F1.4 II` | none |
| AF 35 F1.4 P FE | 1.02 | STM32L451 | 20 | `SAMYANG AF 35mm F1.4 P FE` | Samyang AF 35mm F1.4 P FE |
| AF 14-24 F2.8 | 1.03 | STM32L4 | 21 | `SAMYANG AF 14-24mm F2.8` | Samyang AF 14-24mm F2.8 |

- **The AF 135, on the wire**: LensType2 `08 00` = 8 (`4_Firmware/traces/sy135-2026-09-20-full.txt`
  line 12); the name `SAMYANG AF 135mm F1.8`, padded with zeros (`traces/firmware2_ring.txt`
  lines 17-18); the version 1.05, in the `0x07` and in the `'V'` reply of the service channel
  (`4_Firmware/traces/sy135-2026-09-20-full.txt` lines 12 and 43).
- **The version** is the `'V'` reply of the service channel (§ 6.3), two bytes, major then minor.
  The AF 135 also puts it at `0x07` offsets 5-6, high byte first (`01 05` = 1.05, `protocol.md`
  § 7.7); so do the 24 (`02 05`), the 24 A (`03 05`) and the 14 A (`03 27`) (static analysis, not
  observed). The first digit names the electronic generation on the 14 and 24 pairs: 2.xx for the
  original board, 3.xx for the new one (static analysis, not observed).
- **ExifTool** names: `https://exiftool.org`, `Sony.pm`, `%sonyLensTypes2`; the generated copy in this
  repository is `4_Firmware/components/host/lens_names.c`, table `T`. LensType2 8, 9, 12 and 23 are
  not in it. The ExifTool code 32823 ("Sony FE 85mm F1.4 GM or Samyang AF 85mm F1.4") is not what
  the AF 85 II sends.
- **The microcontroller family** matters to the body in one place: the STM32 generation of the 14
  differs from its PIC32 twin and from the AF 135 in the handshake, the homing replies, the `0x0A`
  and the sleep (§ 8.1).

### 1.2 Recognising a Samyang

Static analysis, not observed, except where a trace or the code is cited.

- **`0x07` offsets 1-2 identify no maker.** All twelve send `03 70`, and so does the Tamron F051
  (`protocol.md` § 7.7). The AF 135 sends it on the wire (`4_Firmware/traces/sy135-2026-09-20-full.txt`
  line 12).
- **The `0x3F` name contains `SAMYANG` on all twelve**; the AF 16 prefixes it with `LK `, so a rule
  "starts with SAMYANG" misses it. The six older models serve the `0x3F` in every state (§ 2.1); on
  the six newer ones the name does not vary with the state of the lens.
- **LensType2 collisions.** The 14 A and 14 P share `0xC931`, and ExifTool also gives that code to an
  AF 35 F2.8 the project has not read. The 24 and 24 A share LensType2, name and internal design;
  only the version separates them. No LensType2 of the twelve is that of the Tamron F051 (`0xC134`)
  or the Sony FE 24-105 G (`0x8025`).
- **Our board's rule** (`4_Firmware/components/session/init.c`, `init_recognize()`): a name that
  starts with `SAMYANG ` or `LK SAMYANG `, or a LensType2 in the closed list 8, 9, 12, 13, 20, 21, 23,
  `0xC938`, `0xC93A`. `0xC931` is not in the list: the two AF 14 are recognised by their name only.
  The AF 135 (LensType2 8) is declared apart to `bench_core` (§ 7.4).
- **A message outside the standard set goes to a recognised Samyang only** (`CLAUDE.md` of the
  repository; `bench_core.c`, `svc_ok()`): a Sony FE 24-105 G that receives a `'V'` floods the line
  with `0x02` until its power is cut.

### 1.3 The decoded fields of the constant replies

Static analysis, not observed, except where a capture or the code is cited.

**`0x01` reply** (33 bytes).

- AF 135, on the wire: `01 FF 9F 38 5D A2 60 18 5E` followed by 24 zeros
  (`4_Firmware/traces/sy135-2026-09-20-full.txt` line 9); the types it lists are in `protocol.md`
  § 7.1.
- 14 A and 14 P (static analysis, not observed): the bitmap of offsets 0-7 lists 01-0D, 10-19, 1B,
  1C, 1D, 1F, 22, 23, 24, 26, 27, 28, 2E, 2F, 34, 35, 3A-40. Offsets 8-31: not published (extracted
  from firmware, meaning unknown). **The list names types these lenses have no size for** (0x11,
  0x12, 0x13, 0x17, 0x18, 0x23, 0x24, 0x27): a body that sends one blocks the lens (§ 2.6). The list
  is not "send it to me", as `protocol.md` § 7.1 says for the AF 135.
- The ten other models: not read.

**`0x07` reply** (35 bytes).

| Offset | Field | AF 135 (`4_Firmware/traces/sy135-2026-09-20-full.txt` line 12) | Other models (static analysis, not observed) |
|---|---|---|---|
| 0 | `01`, a native lens (`protocol.md` § 7.7) | `01` | 14 A: `01`; others not read |
| 1-2 | `03 70` (§ 1.2) | `03 70` | `03 70` on all eleven |
| 3-4 | unknown | `01 00` | 14 A: not published (extracted from firmware, meaning unknown) |
| 5-6 | firmware version, high byte first | `01 05` | 24 `02 05`, 24 A `03 05`, 14 A `03 27`; others not read |
| 7 | unknown | `00` | 14 A: not published (extracted from firmware, meaning unknown) |
| 8 | weiziqian's `A0` on every device (`protocol.md` § 7.7) | `00` | 14 A: `A0` |
| 9-10 | LensType2 (§ 1.1) | `08 00` | § 1.1 |
| 11-14, 19-33 | unknown | zeros | not published (extracted from firmware, meaning unknown) |
| 15-18 | the constant common to every lens (`protocol.md` § 7.7) | `60 92 86 5E` | not read |

- The AF 75 builds offset 1 with a bit 7 inherited from RAM whose value is not established; the six
  newer models build the reply once, at start-up (static analysis, not observed).
- Our board reads offsets 0-1 and 9-10 (`4_Firmware/components/session/lens_rx.c`, `lens_rx_id()`).

**`0x08` reply** (202 bytes). AF 135, on the wire: offsets 0-3 `BF 11 00 19`, f/1.8 to f/22.6
(`4_Firmware/traces/sy135-2026-09-20-full.txt` lines 15-18; `protocol.md` § 7.8); the 198 other bytes
are in the same lines, meaning unknown. One condition changes the reply (static analysis, not
observed): a body that announces itself with the `0x01` signature of § 5.3 named after the a6000,
while the switch position is configured APERTURE at that moment, gets the narrowest aperture equal to
the widest (offsets 2-3 = offsets 0-1) and three other fields changed (offsets 20, 25, and the low
nibble of 26). Other models: not read.

**`0x09` reply** (12 bytes). AF 135: eleven zeros (`4_Firmware/traces/sy135-2026-09-20-full.txt`
line 23). On the six newer models the handler only answers; its content is not published (static
analysis, not observed). On the V-AF 24 the handler also reloads an accessory flag from its EEPROM
(§ 7.2, note 4).

**`0x0B` reply** (3 bytes): `0B <offset 0 of the request> <x>`. AF 135: `x` = `00`, `0B 60 00`
(`4_Firmware/traces/sy135-2026-09-20-full.txt` line 21). 14 A and 14 P: `x` is not the 135's `00`;
its value is not published (extracted from firmware, meaning unknown) (static analysis, not observed).

**`0x0D` reply**: `0D 01` (`4_Firmware/traces/sy135-2026-09-20-full.txt` line 25; 14 A: static
analysis, not observed). What the request selects: § 2.4.

**`0x10` reply**: `10 00` (`4_Firmware/traces/sy135-2026-09-20-full.txt` line 35). It is `10 00`
whatever the homing's outcome (§ 3).

**`0x3F` reply** (66 bytes): offset 0 = `00`, the name of § 1.1 from offset 1, padded with zeros
(`traces/firmware2_ring.txt` lines 17-18 for the AF 135). On the six newer models the name is not
terminated, and the rest of the 66-byte buffer is zero on the 24 A, 85, 35 P and 14-24, and not
written by any code on the 24 and 75 (static analysis, not observed). A reader stops at the first
zero or at offset 64, as our board does (`lens_rx.c`, `name_3f()`).

**The `0x3B` blocks** (L→B, 492 bytes, class 3): after a `0x3A` whose offset 0 was zero, the AF 135
copies one of seven 488-byte blocks of a table, chosen by the `0x3B` request's offsets 0-1; the 14 A
has six such blocks. The blocks' content is not published (static analysis, not observed). Our board
sends neither `0x3A` nor `0x3B`.

---

## 2. The AF 135 session, seen from the mount

Static analysis of the 1.06 firmware, not observed, except where a capture or the code is cited.
The 14 A, read the same way, keeps this automaton with the departures of § 8.1; the other models
have not been read as automatons. The fake AF 135 implements this section
(`4_Firmware/sim/lens135.c`, `l135_vd()`, `l135_byte()`, `l135_body_cs()`, `l135_advance()`, and
the simplifications S1 to S15 in its header).

### 2.1 Three machines, and no guard

**Static analysis of AF 135 firmware 1.06, not observed unless a capture is cited.**

The AF 135 does not impose the Tamron init/stream admission rules: supported init messages can be served while streaming. A capture contains 29 `0x05` messages during init (`4_Firmware/traces/sy135-2026-09-20-full.txt`, lines 8–41). Do not take acceptance of an init request as proof that the lens is no longer streaming.

After power-up, the link starts at 750 000 baud, streaming and service mode are off, and focus is not referenced until homing. Link negotiation, stream selection and ongoing motion are separate conditions: switching the stream does not reset the entire lens. Their observable effects are described below.

### 2.2 The link

**Static analysis, not observed, except the cited captures.**

Use the handshake in `protocol.md` § 2. AF 135 receive becomes available during that handshake; the 14 A differs (§ 8.1). A lens left powered may answer `0x01` without repeating the handshake (`4_Firmware/traces/sy135-2026-09-20-full.txt`, lines 7–9; `sy135-2026-09-20-reboots-0x10-stall.txt`, line 108).

`0x0C` requests a speed switch followed by a short CS handshake. The selected 1.5 Mbaud rate persists through sleep until power is removed. Basilisk does not issue this command.

`0x16` requests shutdown and can wait indefinitely for motion to stop before `16 00`. Sleep is not a cold reset; a subsequent wake-up does not establish the default speed or clear service mode. The wake input's mapping to a mount contact is not established. Basilisk refuses `0x16` because its persistent effects differ across models (§ 7.2).

Do not overlap link transitions: acceptance of another request is not proof that a previous transition finished.

### 2.3 The `0x0A`: layout, motors, and the stream switch

**Static analysis, not observed, except the cited captures.**

`0x0A` can stop both motors, cancel an active `0x1D` without its acknowledgement, suspend the status stream and recompute its layout. The reply echoes the request. Nonzero masks enable the selected stream blocks; all-zero masks leave the stream off. This is not the Tamron phase toggle.

The AF 135 capture shows a reply 6 ms after the request (`4_Firmware/traces/sy135-2026-09-20-full.txt`, lines 38–39). The requested masks `FF 7F … 3F` produce a 97-byte `0x05` and 40-byte `0x06` (`sy135-2026-09-20-dump05.txt`, lines 5 and 11). The 109-byte layout before a first `0x0A` is a static finding, not an observed alternative in this capture corpus.

During homing, `0x0A` may interrupt reference motion without establishing a valid reference. Repeating it while a previous `0x0A` is pending restarts its processing. Treat it as a state-changing command, not an idempotent query. Basilisk uses it once in normal init and as the explicitly defined soft-reset attempt after a silent `0x01` (`init.c`, `INIT0A`).

### 2.4 What paces the lens: VD, slots, and the `0x03`/`0x04` pair

**Static analysis, not observed.**

The body must supply VD and the `0x03`/`0x04` pair. While streaming, one `0x03` permits a `0x05`, and one `0x04` permits a `0x06`; transmission is scheduled relative to VD. Missing a scheduling window can defer a response to the next frame. Without VD, a body cannot rely on receiving either stream.

While streaming, aperture processing needs `0x03` and focus/homing processing needs `0x04`. With streaming off, VD also advances the corresponding processing. An already-started movement can run to its target after the body stops supplying VD. Therefore stopping the clock is not a focus-stop command. One iris homing movement can start without VD; a blanket statement that no motor can move without it is false.

`0x0D` offset 0 bits 0–1 select 60, 50, 48 and 60 Hz respectively. The lens does not establish the actual VD rate from these bits: supply a clock consistent with the declared rate. At 60 Hz the response slots are spaced approximately 2083 µs.

Bit 1 of `0x04` offset 3 affects the motion stopping policy: when set, a movement is capped at approximately 121 VD periods. This is a body-visible timing dependency, not a general guarantee that every focus operation terminates within two seconds. The relevant fields are in `protocol.md` § 7.4.

### 2.5 Replies, and the transmit lock

**Static analysis, not observed, except the cited timings.**

Serialize requests. Overlapping class 2/3 replies can be dropped permanently; two init messages packed into one frame may yield only the first reply. Standard replies and `0x40` replies or notifications compete for transmission. Keeping BODY_CS high can withhold both replies and streams.

| Reply | Body-visible completion condition |
|---|---|
| `0A …` | Motors stopped and layout selected; static analysis indicates a bounded wait of up to 200 ms |
| `10 00` | End of a homing path, not proof of a successful reference (§ 3) |
| `16 00` | Shutdown motion finished, potentially unbounded |
| `34 …` | Move launched, not necessarily arrived |
| `40 …` | Deferred; some operations wait for motion |
| `17 xx yy` | Error notification; absence is not proof of success |

In the older service-mode captures, ordinary replies took 5–6 ms, the 202-byte `0x08` took 9 ms, and service replies took at least 61–62 ms. A service focus move took 141 ms (`4_Firmware/traces/sy135-2026-09-20-full.txt`, lines 8–25, 42–43, 178–187, 387–393). These are observations from that session, not universal deadlines. The sender waits for BODY_CS low and signals transmission with LENS_CS (`protocol.md` § 1.4).

### 2.6 Bad frames and unknown types

**Static analysis, not observed.**

Malformed frames can be ignored without a refusal reply. An unsupported message type or incorrectly sized sequence of sub-messages can leave the studied firmware unresponsive until power is removed. No project trace demonstrates that failure, and the public reference does not provide a recipe for provoking it.

Use the exact sizes and directions in `protocol.md` § 4; do not infer that a lens can accept a type merely because it transmits it. Unknown types are not extension points. Silence cannot distinguish a lost frame, an overlapping reply, a different link speed or an unresponsive lens. The controller needs its own bounded timeout and recovery policy.

### 2.7 No session timeout; a lens left powered

- **Nothing in the lens reacts to a silent body**: no timeout brings it out of the stream, back to
  the handshake or to sleep. With the stream on and no `0x03`/`0x04`, it sends nothing and processes
  nothing; it waits.
- **Facing a full init while it streams** (a body that restarted without cutting its power):
  the handshake gets nothing; every init message is served; the `0x10` starts a homing that stalls
  without `0x03`/`0x04` (§ 3.1); the final `0x0A` stops the motors and switches the stream back on with
  the new layout. Captures: `4_Firmware/traces/sy135-2026-09-20-full.txt` lines 7-41 and
  `4_Firmware/traces/sy135-2026-09-20-reboots-0x10-stall.txt` lines 108-132.
- **Only a power cut** resets the link speed, the service mode (§ 6.5) and a blocked dispatcher.
- **"`0x01` without reply"** has been captured: the `0x01` gets nothing while the next messages are
  answered (`4_Firmware/traces/sy135-2026-09-20-reboots-0x10-stall.txt` lines 537-543;
  `4_Firmware/traces/sy135-2026-09-20-astro-normal.txt` lines 123-127). A block is excluded, since the
  lens answers afterwards. The code offers two transient causes, a reply dropped by the lock and a
  frame swallowed by a desynchronised parser; the captures do not tell which.

### 2.8 What a body can observe of the state

- `0x05`/`0x06` coming back for its `0x03`/`0x04`: the stream is on.
- The echo of a `0x0A`: that `0x0A` was served; it says nothing of the stream before.
- No reply: undecidable (§ 2.6).
- No reply publishes the state of a homing; only `10 00` marks its end, and it does not say whether
  it succeeded (§ 3).

---

## 3. Homing: `0x10`

### 3.1 The AF 135

**Static analysis of firmware 1.06, not observed except the captures below.**

`0x10` requests homing; offset 0 bit 2 selects the iris path and bit 3 the focus path. Neither bit set can produce no action and no reply. Reissuing `0x10` during homing restarts it. During streaming, keep sending `0x03`/`0x04` and VD so the homing can progress (§ 2.4).

Observed `10 1F` → `10 00` delays are 527, 663, 675 and 757 ms (`4_Firmware/traces/sy135-2026-09-20-full.txt`, lines 27–35; `sy135-2026-09-20-reboots-0x10-stall.txt`, lines 332–333 and 854–855; `sy135-2026-09-20-astro-normal.txt`, lines 241–251). Other captured sessions have no completion in eight seconds after a restart with the lens still powered; those logs do not record all VD/pair activity, so they support but do not alone prove the pacing explanation.

Published positions can sweep or jump while the lens references itself. `10 00` is not an unconditional success indication: static analysis finds failure paths that still send it and other failures that produce no completion. Check subsequent position, limits and motion state; do not copy the lens's internal homing procedure into a body controller.

A service focus homing can also end with `10 00` without a preceding `0x10` (`sy135-2026-09-20-reboots-0x10-stall.txt`, lines 547–549). Avoid unrelated commands during referencing; `0x0A` can interrupt it (§ 2.3).

### 3.2 Where the focus rests after a homing

- **Without service mode**, the 1.06 analysis puts the focus at 16384 (`0x4000`) at the end of the
  homing, before the `10 00`; **after an `'M'`** (service mode, § 6.5), at 14623 (`0x391F`), which is
  also the infinity origin of its distance table (static analysis, not observed; the fake:
  `4_Firmware/sim/lens135.c`, `POS_REST`, `POS_THRESH_LO`).
- **On the wire** both values appear: 14623 after a `10 1F` (`4_Firmware/traces/sy135-2026-09-20-full.txt`
  line 34; `traces/firmware1_ring.txt` line 44), 16384 after an `'F'` `0x32`
  (`4_Firmware/traces/sy135-2026-09-20-reboots-0x10-stall.txt` line 149). These sessions ran on a lens
  left powered by a board firmware that sent `'M'` at each boot, so the service mode was probably
  set in all of them; the 1.05 and the 1.06 may differ here, or the position has another writer. Not
  established.
- **In the astro mode** (§ 5.6) the homing poses nothing, and the lens returns to its mark on its own
  at the 61st VD after the `10 00`: on the wire, about 1.0 s after the reply, from 14550 up to 14820
  then down to the mark 14620, an overshoot of 200 steps
  (`4_Firmware/traces/sy135-2026-09-20-astro-normal.txt` lines 264-287).

### 3.3 The other models

**Static analysis, not observed.**

Homing behavior is model-specific. Some studied models make their rest move before the reply; the AF 85 II has a failure path that replies while motion continues, and the 14–24 can move after the reply to maintain its zoom-dependent focus distance. Waking from sleep can restore a previous position instead of the cold-start rest position.

Do not encode per-model rest constants or regard `0x10` completion as proof of mechanical stillness. Read the published state and limits. The detailed AF 135 sequence and its timings do not transfer to the other eleven images.

## 4. Focus moves

Static analysis, not observed, except where a capture or the code is cited. The AF 135 is the
reference; § 4.2 and § 8 give the other models.

### 4.1 One flag per order

Each focus order has its own job flag, picked up at the frame processing (§ 2.4); there is no single
job overwritten by every order as on a Tamron (`tamron.md` § 4.1).

| Order | Picked up | End and acknowledgement |
|---|---|---|
| `0x1D` | computes a target, clamps it (§ 4.3), arms the move, which starts at the next VD | `1D 00` when the motor stops |
| `0x1F` | a state machine (`protocol.md` § 7.4.3) | `1F 00` at the end, or on a 1.2 s deadline |
| `0x3C` | drives to the soft limit on the side of the sign of offsets 1-2; the direction byte comes from offset 0, bits 2-3 | `3C xx 00 00` when the limit is reached |
| `0x1C` | stop, then wait for stillness | `1C 00` once still; after 1.2 s, a forced stop, a `0x17`, and `1C 00` |
| `0x34` | an absolute or relative move (§ 4.6) | its own reply `34 …`, no acknowledgement in the stream |
| `0x22`, `0x2E` | converted at once, no job touched | `22 vL vH`, `2E vL vH` after a later `0x06` |

### 4.2 `0x1D`: units refused, and the constants of bits 6-7

The flags of offset 3 have the layout of `protocol.md` § 7.4.2, on all twelve models.

| Field | Samyang (AF 135; the others where stated) |
|---|---|
| bits 0-1, unit | 0 = motor steps; 2 = the value times a constant the lens recomputes (a "depth" unit); 3 = Sony distance unit (`protocol.md` § 8.3), converted; **1 = refused**: the order ends without movement and is acknowledged **`1D 00`**, in absolute and in relative mode. In unit 3, the value `0x1C0` is treated as a special target on the 14 P, 14 A, 24, 24 A and 75; its meaning is not established |
| bit 2 | 0 = absolute, 1 = relative: target = position + the signed value ± half the oscillation amplitude, then clamped. On the STM32 models the base of a relative move is **the last target commanded**, not the encoder |
| bit 3 | wait: while offset 11 of the last `0x03` is not zero the order stays pending, frame after frame; the test is reached only when offset 10 of the `0x03` is 1 or a cycle is running. Same on the STM32 models. With our board's `0x03` (offset 11 always 0) the order would start at once (`drive.c`; `protocol.md` § 7.3) |
| bits 4-5 and offset 2 | oscillation amplitude, **in relative mode only**: scale 0 = the raw byte, 2 = scaled by the depth constant, divided by 16 or by 32 depending on the body profile (§ 5.3). The 85 II and the V-AF 24 have three scales chosen by the aperture |
| bits 6-7 | Side-of-arrival behavior is model-dependent; do not copy internal compensation constants into the body. Captured AF 135 target/readback differences are listed below. |
| speed | Not carried by `0x1D`; affected by `0x03` fields and the body profile. No physical speed is established by an internal numeric constant. |

- **A `0x34` changes the speed profile of later `0x1D`** (§ 4.6).
- **On the wire** the AF 135 moves on `1D lo hi 00 00`, the order our board sends (`drive.c`,
  `drive_goto()`), and stops a few steps from the target: 15359 → 15354, 25726 → 25731, 26995 → 27000,
  15354 → 15349 (`traces/samyang.txt` lines 36-38, 47-49, 76-78, 85-87), 24384 → 24381
  (`traces/firmware2_ring.txt` lines 42-44). The compensation of bits 6-7 = `00` is the probable
  cause; it is not modelled by the fake (`lens135.c`, header, S6).

`protocol.md` § 7.4.2 point here.

### 4.3 Bounds and limits

**Static analysis, not observed except the captures below.**

Targets can be silently clamped to internal limits. The useful body interface is the **published** lower/upper pair at `0x06` offsets 7–10. On the bench AF 135 those fields read 13873 and 30738 (`4_Firmware/traces/sy135-2026-09-20-dump05.txt`, line 11; `traces/samyang.txt`, line 38). Homing and ring motion can reach further: 13723 and 30988 appear in the captures (`sy135-2026-09-20-full.txt`, lines 30 and 32; `traces/195_samyang.txt`, line 186).

These are observations of one lens, not constants to encode for a model. A limiter can further restrict motion under model-specific conditions. Read both position and status; neither a target being accepted nor an acknowledgement proves arrival. Basilisk narrows the published range by another five steps (`lens_rx.c`, `LIMIT_MARGIN`).

### 4.4 Acknowledgements and eviction

- **Place**: after the `0x06`, in the same class 1 frame (`protocol.md` § 7.6.1). On the wire:
  `1D 00` after the 40 bytes of the `0x06` (`traces/samyang.txt` lines 38, 49, 58;
  `traces/firmware2_ring.txt` line 44).
- **Three at most per frame**, in the order 1D, 1F, 22, 2E, then 1C and 3C if room is left; a fourth
  `1C` or `3C` waits for the next frame, a fourth among `1D`, `1F`, `22`, `2E` is lost. Several ends of
  the same kind before one `0x06` give one acknowledgement.
- **Values**: `1D 00`, `1F 00`, `1C 00`, always (the byte is written only at start-up); `3C xx 00 00`,
  `xx` bits 0-1 = the direction received. **There is no failure acknowledgement**: a refused unit is
  `1D 00` too.
- **Eviction**:

  | In progress | Received | Effect |
  |---|---|---|
  | `0x1D` | `0x1D`, `0x1F`, `0x3C`, `0x1C` | the old one is acknowledged `1D 00`, the new one takes its place |
  | `0x1D` | `0x22`, `0x2E` | nothing evicted; both acknowledgements come |
  | `0x1D` | `0x34` | the flag is untouched: the `0x1D` is acknowledged at the end of the `0x34` move |
  | `0x1D` | `0x0A` | **motors stopped, `0x1D` cancelled without acknowledgement** (§ 2.3) |
  | `0x3C` | `0x1D`, `0x1F`, `0x3C`, `0x1C` | the old one gets its acknowledgement |
  | `0x1F` | `0x1D`, `0x3C` (and a new `0x1F` on the 14 A) | **the `0x1F` is lost, without acknowledgement** |
  | `0x1F` | `0x1C` | `1F 00` |

- **A `1D 00` proves neither arrival nor success**: it may acknowledge an order replaced while the
  next one still runs. Compare the published position with the target.
- **No `0x1C` is needed between two `0x1D`.** On the wire the AF 135 receives a lone `0x1C` in a class
  1 frame from an earlier board firmware (`4_Firmware/traces/sy135-2026-09-20-full.txt` line 391).
- The 14 A acknowledges and evicts the same way (§ 8.1).

### 4.5 `0x1C`

It evicts the orders in progress (§ 4.4), stops, waits for stillness up to 1.2 s, and acknowledges
`1C 00`; past 1.2 s, a forced stop, a `0x17` and `1C 00`. A `0x1D` received in the same pass of the
lens's loop as the start of the stop is probably dropped and acknowledged `1D 00` (inferred from the
order of the calls).

### 4.6 `0x34`

- Fields: `protocol.md` § 7.18; offset 4 bounds an absolute target to offset 4 × 384 / 17 steps of the
  current position.
- Launched at the next frame processing (the next `0x04` in stream); the reply `34 …` leaves **as
  soon as the move is launched**, not when it ends.
- **It sets a mode that changes the speed profile of every later `0x1D`.** The mode is cleared only
  when a `0x05` is built while bit 4 of the last `0x03`'s offset 12 is clear; with the stream off no
  `0x05` is built and the mode stays. Our board never sends `0x34`, and its `0x03` carries bit 4 for
  30 pairs after each `0x1D` (`drive.c`, `AF_FRAMES`). `protocol.md` § 7.18 point here.

### 4.7 `0x1F`, `0x3C`, `0x22`, `0x2E`

- `0x1F`: `protocol.md` § 7.4.3 gives the AF 135's reading of every field.
- `0x3C`: to the soft limit on the side of the sign; the acknowledgement is sent when the limit is
  reached (§ 4.4).
- `0x22`, `0x2E`: conversions between motor steps and the Sony distance unit through the lens's
  distance table (not published); `0x0700` = infinity (`protocol.md` § 7.4.4, § 8.3).

### 4.8 What the AF 135 publishes of its focus in the `0x06`

On the wire, with the masks `FF 7F … 3F`:

| Offset | Field | Captures |
|---|---|---|
| 0 | bits 0-2 = 2; bit 3 = position ≥ 29988; bit 4 = position ≤ 14623; bit 5 = at or above the upper soft limit; bit 6 = at or below the lower one | `12` at rest at 14623 (`4_Firmware/traces/sy135-2026-09-20-dump05.txt` line 11); `02` at 15354 (`traces/samyang.txt` line 38); `52` at 13723 (`traces/195_samyang.txt` line 186); `0A` at 30473 (same file, line 336). The meaning of the thresholds of bits 3-4 is not established |
| 1 | bit 1 = the position increased since the previous `0x06`, bit 2 = it decreased | `02` while the ring drives the focus up (`traces/195_samyang.txt` lines 126-146) |
| 2-3 | position, motor steps | above |
| 7-8, 9-10 | the published lower and upper limits (§ 4.3) | 13873, 30738 |
| 20-21 | the position of the previous frame, in these captures: equal to offsets 2-3 at rest; one `0x06` late while the focus moves | `traces/195_samyang.txt` lines 126 and 131 |
| 32-38 | non-zero while the focus moves, zero at rest (`protocol.md` § 7.6) | `traces/195_samyang.txt` lines 126-146 and 161 |
| 39 … | acknowledgements (§ 4.4) | `traces/samyang.txt` line 38 |

- Which sample offsets 20-21 and 32-38 carry also depends on the body profile (§ 5.3): the previous
  one with the profile of a NEX-7 or of a `0x08` without bits 1-2, the current one otherwise (static
  analysis, not observed). The body profile of the capture `traces/195_samyang.txt` is not in it.
- In the `0x05` the AF 135 sets offset 22 bit 6 while the focus moves, here moved by the ring
  (`traces/195_samyang.txt` line 124: `C0`; line 4: `80` at rest), and publishes the distance at
  offsets 20-21 (same lines). `protocol.md` § 7.5 gives the other fields.

### 4.9 The iris

- **Continuous target**: each `0x03` stores offsets 3-4 as the aperture target (offsets 5-6 are not
  read); the iris processing converts it to steps and moves if the step index changes; offset 12 bit 3
  chooses the movement profile. The movement starts at a VD (§ 2.4).
- **`0x1B` drives the iris**, at once, and answers at once with 11 bytes, before the iris arrives
  (`protocol.md` § 7.17).
- **No iris acknowledgement** in the stream.
- With the native aperture of § 5.5 and the a6000 profile only, the lens takes its target from the
  ring instead.

---

## 5. The ring, the switches, and what the lens reads of the body

### 5.1 Barrel controls, model by model

Static analysis, not observed, except where a capture is cited.

| Model | Button | Custom switch | Limiter | How the body sees them |
|---|---|---|---|---|
| AF 135 | yes | 2 positions, M1 (up) and M2 (down), configurable | 3 positions | `0x05` offset 64 bit 3, offset 62 bit 1, offset 60; `'W'` notifications after `'M'` (§ 6.4) |
| V-AF 24 | yes | yes | no | `0x05`; `'W'` 01, 02, 03 |
| AF 35-150 | two (two presets) | a 3-position switch, and a physical MF switch | not read | `0x05`; no `'W'` |
| AF 16 | not reported | yes | no | `0x05`; `'W'` 02, 03 |
| 14 A, 14 P | no | no | no | offsets 62 bit 1 and 64 bit 3 always 0; no `'W'` |
| 24, 24 A | yes | 2 positions | not found | `0x05` offset 64 bit 3, offset 62 bit 1, offset 60; no `'W'` |
| 75 | not found | 2 positions | not found | `0x05`; no `'W'` |
| 85 II | yes | 2 positions | no | `'W'` 01, 02, 03 and the `0x05` |
| 35 P | none notified | none (MF mode, LED mode and custom mode are settings in flash) | no | `0x05` |
| 14-24 | yes | AF/MF | not found | `0x05`; the zoom is read by an analogue input |

"Not found" means no input read and no string points to it: an absence seen, not proven.

- **AF 135, on the wire**: the button is `0x05` offset 64 bit 3, `08` while pressed
  (`traces/195_samyang.txt` lines 865 and 875; `traces/samyang.txt` lines 52-53); the switch is
  offset 62 bit 1, `03` in a position configured MF, `01` otherwise
  (`4_Firmware/traces/sy135-2026-09-20-dump05.txt` lines 5 and 7; `traces/195_samyang.txt` line 395).
- **The button bit** follows 16 consecutive samples of the lens's loop; the `'W'` 01 notification
  leaves on the first raw sample, in every mode.

### 5.2 The Custom switch of the AF 135, and its forcing

**Static analysis of AF 135 firmware 1.06, not observed.**

The persistent switch byte has M1 in the high nibble and M2 in the low nibble, each 0 = APERTURE, 1 = AF, 2 = MF. The documented factory assignment is M1 = AF, M2 = APERTURE. Reading and writing it is the restricted `P/FA` and `P/38` interface (§ 6.3).

A body profile can force a temporary assignment independently of the stored byte. `0x08` offset 0 bit 2 affects that behavior; enabling it later does not necessarily restore the stored configuration until power is cycled. Basilisk sends `08 06` followed by seven zero bytes to recognised Samyang lenses (§ 5.3).

`0x05` offset 62 bit 1 reports the effective MF condition; it does not distinguish AF from APERTURE. The analysis associates the upper switch position with M1; that physical assignment still needs checking. Verify the actual ring behavior after a Custom change and power-cycle, rather than treating a stored byte alone as proof of the physical result.

### 5.3 What the lens reads of the body: "old" and "modern" bodies

Static analysis of the AF 135, not observed; the codes are those the AF 135 compares.

**The `0x01` request.** Three signatures, compared byte for byte:

| Request offsets | Value | Effect | Name in the firmware's strings |
|---|---|---|---|
| 6-8 | `FF 7F 00` | the "a6000" profile (below) | `a6000` |
| 6-8 | `FF 01 00` | arms the forcing of § 5.2 | none |
| 8-10, if 6-8 are not `FF 01 00` | `FF EF FF` | the "a7r4" profile: the `0x1D` speed becomes `0x100` | `a7r4` |

- The `0x01` of the NEX-7 and of our board carries `FF 01 00` at offsets 6-8 (`protocol.md` § 6.1).
- The a6000 and `FF 01 00` flags are cleared only by a `0x08` with bit 2 of offset 0; the a7r4 flag is
  never cleared. **They survive from one session to the next while the lens stays powered.**
- `protocol.md` § 7.1 point here.

**The `0x08` request.**

| Field | Effect |
|---|---|
| offset 0, bit 2 (`0x04`) | clears the a6000 and `FF 01 00` flags of the `0x01`; makes the `0x05` fill its offsets 96-107 (meaning unknown) |
| offset 0, bit 1 or bit 2 | with no `0x01` flag left, **lifts the forcing of § 5.2**; `0x06` offsets 20-21 and 32-38 carry the current sample instead of the previous one; the oscillation scale of a relative `0x1D` divides by 32 instead of 16 (§ 4.2) |
| offset 0, bit 1 (`0x02`) | read only on paths taken when the `0x03` sets offset 12 bit 3, which our board never does |
| offset 1, low nibble | a shift, in entries, of the table of apertures that feeds `0x05` offsets 0-3 (the NEX-7's `21` shifts it by one); meaning not established |
| offsets 4-5 | a body model identity: the V-AF 24 compares it with `0x0933` (the NEX-7) and `0x0C0C`; the AF 135 keeps it unread |
| offset 5 | a body family: the AF 135 scales a depth-of-field value by it (used by the "depth" unit of `0x1D` and `0x1F`, and published at `0x06` offsets 13-14); the V-AF 24 derives body family names from it |

**The resulting profiles** (columns: what the body sends):

| | `0x08` offset 0 = `00` (our board to a non-Samyang) | NEX-7 (`C0 21 00 00 33 09 62 02`) | `02` | `06` (our board to a Samyang) | `04` | a6000 signature |
|---|---|---|---|---|---|---|
| switch forced (§ 5.2) | yes | yes | yes | **no** | **no** | no with `02`, yes with `00` |
| `0x06` offsets 20-21, 32-38 | previous sample | previous sample | current | current | current | current with `02` |
| `0x05` offsets 96-107 | zero | zero | zero | **filled** | filled | zero |
| depth-of-field value | 0 | **not 0** (offset 5 = 9) | 0 | 0 | 0 | not established |
| ring aperture applied to the iris by the lens (§ 5.5) | — | — | — | no | no | **yes** |

- **The firmware does not sort bodies into "old" and "modern"**: it names two `0x01` signatures, and
  bit 2 of the `0x08` is its only hierarchy. "Modern" is our board's word for a `0x08` with bits 1
  and 2 set.
- **With the `0x03` our board sends** (offset 12 bit 3 never set, `0x1D` unit 0, absolute, no side of
  arrival), the profiles `02` and `06` differ only by the forcing and by `0x05` offsets 96-107: focus
  speeds, focus by wire, ring sensitivity, homing, mark and astro mode are the same.
- **Our board sends `08 06` + 7 zeros to a recognised Samyang**, `08` + 8 zeros otherwise
  (`init.c`, `body08_flags()`), and logs it (`7_Docs/PROTOCOL.md` § 2, "La bague"). Earlier firmwares
  sent `02` (`4_Firmware/traces/sy135-2026-09-20-full.txt` line 14) and once `04`
  (`4_Firmware/traces/sy135-2026-09-20-reboots-0x10-stall.txt` line 115).
- **The other PIC32 models** (24, V-AF 24) and the 35-150 have the same `0x01` signatures (the 24
  and the 35-150 without a7r4) and the same role for bit 2 of the `0x08`; the V-AF 24 identifies the
  body by offsets 4-5 and reads, in the a6000 profile, a setting of its EEPROM.

`protocol.md` § 7.8 and § 13 ("`0x08` flags") point here.

### 5.4 The focus ring: focus by wire

**Static analysis, not observed unless a capture is cited.**

Ring behavior depends on the lens's effective switch assignment and body profile. In MF it can command focus internally, while reporting motion and position to the body. In aperture mode it can publish a requested aperture (§ 5.5). A controller must distinguish lens-driven motion from its own in-flight request.

For AF 135, `0x05` offset 60 carries ring-motion information; offsets 17–19 carry the native aperture request and validity. These are different signals, not interchangeable measurements of ring position. Their common decoding is in `protocol.md` § 7.5. The body must continue supplying its normal stream/timing inputs for the session.

The lens's acceleration curves, ring-edge accumulation, calibration scaling and internal motor target computation are not required to consume those fields and are omitted. Their effects are already represented by reported state; do not reproduce the proprietary ring-control algorithm in the body.

### 5.5 The native aperture (APERTURE position)

**Static analysis, not observed except where the original captures are cited by `protocol.md` § 7.5.**

With the ring assigned to APERTURE, the AF 135 publishes its requested aperture at `0x05` offsets 17–18, valid when offset 19 bit 0 is set. The actual reported aperture remains at offsets 0–3. A controller must not confuse a ring request with the aperture already achieved.

The effective behavior depends on the body profile (§ 5.3). Ring pulses can coexist with a published aperture change; applying both as independent changes would double-count one gesture. Basilisk prioritises a changed published aperture and mutes pulse interpretation for one second (`ring.c`, `aperture()`, `PUB_US`). This is a board policy, not a lens-protocol requirement.

The proprietary index-to-aperture table and its internal update algorithm are omitted. The transmitted aperture code and its validity bit are sufficient to consume this interface.

### 5.6 The button and the astro (preset) mode

**Static analysis, not observed except the cited capture.**

The lens's native preset feature is separate from Basilisk's mark. On the AF 135, a native preset mode can hide the barrel button from the usual `0x05` button bit, recall a position autonomously and store that position persistently in the lens. A capture shows a `'W'` button notification while the usual bit is clear, followed by a recall (`4_Firmware/traces/sy135-2026-09-20-astro-normal.txt`, lines 296–303).

No standard reply identified by this project reports the native mode or its stored mark. Native preset and calibration gestures differ across models and may write persistent data without any mount command. Their factory gestures, erase sequences and implementation algorithms are outside this interface reference; use the lens manufacturer's operating instructions for native features.

Basilisk's own mark is stored in the board's NVS and has independent controls. Its approach policy is defined by the board code (`mark.c`, `restore.c`), not by an assumption that a native preset was read or changed.

## 6. The service channel `0x40`

Static analysis, not observed, except where a capture or the code is cited.

### 6.1 The message

The service request and reply are both 19 bytes including the type: `40 <main> <sub> <16 data bytes>`, class 2. The main command is an ASCII letter. This envelope is observed in `4_Firmware/traces/sy135-2026-09-20-full.txt`, lines 42–43.

An absent capability bit does not by itself mean this channel is absent: the AF 135 serves it without advertising `0x40`. The envelope does not make subcommands portable between models. Replies can be delayed, lost through overlap with standard replies, or be replaced by unsolicited notifications (static analysis, not observed). Serialize the operations and establish success from the operation's actual readback.

### 6.2 Main commands, model by model

**Static analysis, not observed.**

The command families and their persistent effects vary by model and firmware. This reference publishes the bounded subset used by Basilisk (§ 6.3 and § 6.6), not a factory adjustment command catalogue. Recognising a Samyang family is insufficient to enable an AF 135 configuration operation.

### 6.3 The commands

**Scope:** static analysis, not observed unless the capture is cited. These are protocol facts for the studied models, not permission to probe arbitrary lenses.

| Main / subcommand | Interface meaning | Evidence and limit |
|---|---|---|
| `'V'` / `00` | Version in data bytes 0–1, major then minor | AF 135 returns `01 05` in `4_Firmware/traces/sy135-2026-09-20-full.txt`, lines 43–44. On the 14–24, analysis also finds a service-mode side effect. |
| `'F'` / `FA` | Focus position, little-endian u16 in data bytes 0–1 | Same capture, lines 178–188. A deferred sample is not necessarily a fresh arrival measurement. |
| `'F'` / `FB` | Focus target, little-endian u16 in data bytes 0–1 | Same capture, lines 387–394. Targets are clamped by the lens. Use reported limits, never constants extracted from an image. |
| `'F'` / `32` | Focus-only homing | Completes with standard `10 00`, not necessarily a `0x40` reply (§ 3.1). |
| `'M'` / `00`, `31` | Service-mode/notification controls | Historical captures use service mode; it is not needed by the current session (§ 6.5–6.6). |
| `'P'` / `FA` | AF 135 switch configuration in reply data byte 7 | Static analysis of 1.06; the request also has transient side effects. Power-cycle after use. |
| `'P'` / `38` | AF 135 persistent M1/M2 assignment | Static analysis of 1.06. Data byte 0 is `0x30 + ((M1 << 4) | M2)`, each assignment 0 = AP, 1 = AF, 2 = MF. |

For the AF 135 `P/FA` and `P/38`, only request data byte 0 is read by the studied handlers. Basilisk rejects `P/FA` with that byte equal to `0x53` and accepts `P/38` only for the nine valid assignment pairs. The other fifteen bytes are not a channel for additional settings. A `P/38` echo with zero data does **not** establish that storage succeeded; read the setting back and verify after power-cycling.

This `P/38` contract does not transfer to other Samyang lenses. The board recognises the AF 135 by LensType2 8 and does not check the lens firmware version. The public tool is `5_App/tool_station/tool_station.py`; the USB interface is `7_Docs/PROTOCOL.md` § 3.1.

Every unlisted service subcommand remains outside the allowed interface. Factory calibration, firmware update and reset operations can write persistent memory. The standard message `0x34` for focus is distinct from a service subcommand numbered `0x34`.

### 6.4 Notifications

- **`'W'`** (`0x57`), spontaneous `0x40` frames after an `'M'`: data byte 0 = `01` the button, `02` the
  switch moved to M1 (up), `03` to M2 (down), `04` to `06` the limiter (AF 135 only). On the wire:
  `'W'` 02 and 03 (`4_Firmware/traces/sy135-2026-09-20-astro-normal.txt` lines 35-46), `'W'` 01
  (`4_Firmware/traces/sy135-2026-09-20-full.txt` line 382). The AF 135 sends `'W'` 02/03 only if an
  input read at start-up allows it.
- **`'H'` and `'L'`** (`0x48`, `0x4C`): the focus fork, at each edge, in service mode outside a homing,
  until an `'M'` `0x31`; on the wire with a position in data bytes 0-1
  (`4_Firmware/traces/sy135-2026-09-20-astro-normal.txt` lines 28-29). The 14 A sends them from an
  interrupt, and no `'W'`.
- Which models send which: § 5.1.

### 6.5 The service mode

**Static analysis, not observed, except the historical capture context.**

Service mode can change homing rest positions, limiter behavior, iris behavior and byte timing. Sleep is not enough to clear it; remove lens power. The effects differ between models, including entry through commands that look like reads.

The four `4_Firmware/traces/` captures were made with service mode enabled by the old board firmware (`sy135-2026-09-20-full.txt`, lines 52–56). Their timings and rest positions therefore do not establish ordinary-mode behavior. The current session does not send `'M'` at startup. No per-model calibration or internal motor-profile constants are required to use that distinction.

### 6.6 What our board sends on `0x40`

- `bench_core` lets through, **to a recognised Samyang only**: `'V'` `00`, `'F'` `FA`, `FB`, `0x32`,
  `'M'` `00` and `0x31` (`4_Firmware/components/bench_core/bench_core.c`, `svc_allowed()`), with no persistent write identified in the reviewed paths (§ 7.2); and, **to the AF 135 only**, `'P'`
  `0x38` and `'P'` `FA` (§ 7.4).
- The session sends only `'P'` `FA` and `'P'` `0x38`, through the lab command `CUSTOM`
  (`4_Firmware/components/session/cmd.c`, `CMD_LENS_CUSTOM`; `7_Docs/PROTOCOL.md` § 3.1). It sends no
  `'M'`: a 135 sends `'W'` only if another master armed them since its last power cut.

---

## 7. Writes to non-volatile memory

### 7.1 The memories

The studied lenses can persist configuration, native presets and factory calibration. Those effects can originate from mount commands, startup or the lens's own controls. A body-side transmit allowlist limits what the body requests; it cannot prohibit every internal write a lens may perform.

The storage layout, flash pages, recovery copies and factory-console implementation are not part of this public interface reference.

### 7.2 The map: model × path

**Static analysis, not observed.** This section retains the persistent-effect conclusions needed to review the board's allowlist; it does not publish the factory write catalogue.

| Path | Finding and scope |
|---|---|
| Standard messages emitted by Basilisk | No direct persistent write identified in the reviewed ordinary paths. The V-AF 24 `0x09` accessory-state path remains an explicit uncertainty; see `bench_core.c`, `listed_size()` comment. |
| Allowed legacy service subset `V/00`, `F/FA`, `F/FB`, `F/32`, `M/00`, `M/31` | No non-volatile write identified in the reviewed handlers across the twelve images. Transient service-mode effects still exist. This is analysis, not proof of universal absence. |
| AF 135 `P/38` | Writes the M1/M2 configuration; deliberately allowed only for this model and the valid values of § 6.3. |
| AF 135 `P/FA` | Reads the setting, with transient mode effects. The permitted request excludes the factory-bench selector. |
| Other service operations | Adjustment, reset or update paths can write flash or EEPROM. Unknown/unlisted operations remain refused; no cross-model permission is inferred. |
| Standard `0x14` / `0x15` | Update-related operations, outside the board's allowed set. |
| Standard `0x16` | Shutdown is not uniformly free of writes: the V-AF 24 analysis reports a persistent accessory/body setting. Basilisk refuses it for every lens. |
| Startup and barrel controls | Some models can persist defaults, recover settings or save native presets without a mount command. A blocked transmit command does not prevent these autonomous paths. |

The positive `P/38` exception and the negative default are the relevant public contract. Keeping the factory command list private does not turn unknown cases into harmless ones.

### 7.3 The limit of the proof

The evidence covers the application images and the paths actually read. It is not a formal proof and does not cover bootloaders. Coverage varies: newer-model write paths were followed more broadly; several older-model conclusions rely on individual handler reads. Some accessory paths remain unresolved (§ 7.2).

A static finding is tied to its listed firmware version. A successful board simulation only demonstrates the behavior encoded in that model. Neither extends the conclusion to an unexamined lens version.

### 7.4 Our defence

- **A whitelist, last before the wire.** `bench_core` is the only caller of the board's transmit
  function; a frame passes only if its messages pave it exactly with the sizes of its list:
  `0x01 03 04 07 08 09 0A 0B 0D 10 1C 1D 3F`, and `0x40` to a recognised Samyang with the sub-commands of
  § 6.6. `0x0C`, `0x14`, `0x15`, `0x16`, `'X'` `0x34` and every unlisted type or sub-command are refused,
  counted and reported (`4_Firmware/components/bench_core/bench_core.c`, `listed_size()`, `allowed()`,
  `svc_allowed()`; `4_Firmware/include/bsk_bench_core.h`). Its tests stay
  (`4_Firmware/sim/test/test_bench_core.c`, `4_Firmware/sim/test/garde_emission.sh`).
- **One exception, decided by the project owner**: `'P'` `0x38` with a data byte
  `0x30 + (h << 4 | b)`, `h` and `b` from 0 to 2, and `'P'` `FA` with a data byte other than `0x53`, **to
  the AF 135 only** (`bench_core.c`, `svc135_allowed()`; the session declares it by its LensType2 8,
  `init.c`, `init_recognize()`, `BSK_BENCH_SAMYANG135`). The same commands write other Samyang lenses
  with a meaning nobody has reread.
- **Its contract**: `7_Docs/PROTOCOL.md` § 3.1, `CUSTOM READ` / `CUSTOM WRITE <h><b>`: one byte of the
  AF 135's flash, its switch configuration, and nothing else; the format is established on the 1.06,
  and the board does not read the lens's version.
- **Its client**: `5_App/tool_station/tool_station.py`, which reads the configuration, asks for a
  confirmation, writes, and reads back.
- **What a body-side driver should refuse**, from the map: `0x14`, `0x15`, `0x16` (on the V-AF 24),
  `'X'` `0x34` whatever its data, and every unlisted factory service operation;
  and, since unknown cells are not "none", every `0x40` sub-command not shown harmless on the model at
  hand.

---

## 8. How each model departs

### 8.1 From the AF 135

**Static analysis, not observed.** Differences below are reasons to keep model/version scope, not additional compatibility claims.

| Model or group | Difference relevant to a body |
|---|---|
| AF 14, STM32 generation (14 A) | Receive is available earlier in the handshake; homing reply/error behavior differs, and timing from the AF 135 cannot be copied. |
| AF 14, PIC32 generation (14 P) | Some failed homing paths send an error without `10 00`. |
| V-AF 24 | The name can depend on an accessory; native presets and the body-dependent persistent effect of `0x16` differ. |
| AF 35–150 | Zoom-dependent limits and focus; turning the zoom ring can move focus autonomously. The MF condition is tied to a physical switch. |
| AF 16 | Barrel notifications and available preset features differ. |
| AF 24 / 24 A | Different hardware/firmware generations; native preset behavior is not Basilisk's mark. |
| AF 75 | No barrel button; service mode does not change the rest position in the studied image. |
| AF 85 II | Service focus completion may have no deadline; a homing failure path can reply while the motor is still moving. |
| AF 35 P | No physical button or switch in the studied configuration. |
| AF 14–24 | A version query can enter service mode; focus can follow zoom and move after a homing reply. |

Do not infer identical motor behavior from a matching wire format. Bits 4–7 of `0x1D` were not fully read on AF 35 P and AF 35–150. Per-model internal timing, calibration constants, firmware similarity counts and factory interfaces are omitted.

### 8.2 From `protocol.md`

Static analysis, not observed, except where the section cited in the right-hand column cites a capture.

| Subject | Common view (`protocol.md`) | Samyang |
|---|---|---|
| when init messages are served | Samyang: any time (§ 6.1) | confirmed, every known type in every state (§ 2.1) |
| bad frame | dropped in silence (§ 3.2) | the same; never a `0x02` (§ 2.6) |
| an unknown type | blocks (§ 4) | blocks until power cut; type 0 not established on the AF 135, not a block on the 14 A (§ 2.6, § 8.1) |
| VD | a movement never starts without VD (§ 1.3) | true except step 4 of the iris homing (§ 2.4) |
| `0x16` sleep | waits for an input line low for 50 ms (§ 7.15) | low, 50 ms, low again; the 14 A without the 50 ms (§ 2.2, § 8.1) |
| `0x1D` beyond a limit already reached | no movement and an acknowledgement (§ 7.4.2) | contested between the analyses (§ 4.3) |
| `0x06` limits | 13873-30738 on the AF 135 (§ 7.6) | published; the lens clamps to 13723-30988 (§ 4.3) |
| `0x0D` | `0D 01`; 50 or 48 Hz on the AF 135 (§ 7.13) | offset 0 bits 0-1; no 30 Hz; the VD period is not measured (§ 2.4) |
| `0x1D` speed | — | from the `0x03` offset 12 and the body profile (§ 4.2) |

---

## 9. Our board with a Samyang

What the code of this repository does; it moves the AF 135 on the bench.

### 9.1 The session

- **Recognition** by the name or a closed list of LensType2 (§ 1.2), at the `0x07` and `0x3F` replies
  of the init (`init.c`, `init_recognize()`).
- **Init order** `01 07 3F 08 0B 09 0D 10 0A` (`protocol.md` § 6.1); `08 06` + 7 zeros to a Samyang
  (§ 5.3); **the `0x03`/`0x04` loop starts with the `0x10`** for a recognised Samyang (§ 3.1); the
  `0x0A` and the `0x10` are never re-sent within a session (`init.c`, `idempotent()`).
- **A homing of its own**, `0x1C`, 100 ms, then `10 08` (focus only), when the init did not finish
  (`4_Firmware/components/session/session.c`, `to_homing()`, `HOME10`).
- **Moves**: `1D lo hi 00 00`, absolute, steps, no wait, no side of arrival, inside the published
  limits narrowed by 5 steps (`drive.c`, `drive_goto()`; `lens_rx.c`, `LIMIT_MARGIN`); offset 12 bit 4
  of the `0x03` set for 30 pairs after each `0x1D` (`drive.c`, `AF_OFF`, `AF_BIT`, `AF_FRAMES`); a
  stop is a lone `0x1C` in class 1, re-sent every 1.5 s, three sends at most, until a `1C` of any value
  or a still lens (`motion.c`, `STOP_MS`).
- **Ring role** from offset 62 bit 1 (§ 5.4); the published aperture of offsets 17-19 copied into the
  `0x03` (§ 5.5); the button from offset 64 bit 3 (`mark.c`, `BTN_BIT`).
- **The `0x40`**: only `CUSTOM`, to the AF 135 (§ 6.6).
- **A re-init without a power cut** (loss of link, soft reset) loses the AF 135's switch configuration
  until it is unplugged (§ 5.2); a recovery never cuts the board's rails, by decision of the human on
  2026-10-23 (`session.c`, `on_due()`, `S_REC_WAIT`: the wait, then a new start).

### 9.2 The fake AF 135

The suite plays the board against a fake AF 135 (`4_Firmware/sim/lens135.c`), whose replies are byte
templates copied from the traces (`4_Firmware/sim/sources135.c`) and whose mechanisms follow § 2 to
§ 5. Its declared simplifications (header, S1 to S15) include: the `0x1D` in unit 0 only, without the
compensation of bits 6-7 (S6); only `1D` and `1C` acknowledgements (S11); no astro mode, no limiter,
no `'W'` of the switch (S7); the iris as an abstract motor (S4). Its limits are those its traced `0x06`
publishes, 13873 and 30738, and its rest position after a homing is 16384, or 14623 in service mode
(`POS_REST`, `POS_THRESH_LO`).

### 9.3 Contested points that concern a Samyang

Reported, not corrected; the full list is `protocol.md` § 11.

- **Movement read at `0x05` offset 60** (`lens_rx.c`, `move`; `motion.c`; `still.c`): on the AF 135
  offset 60 is a ring flag, or the direction of a focus-by-wire move (§ 5.4), and neither a `0x1D` nor an
  `'F'` `FB` move sets it (static analysis, not observed); the AF 135 sets offset 22 bit 6 while its focus moves (§ 4.8).
- **Arrival tested on `0x06` offsets 2-3**: on the AF 135 these are its position; offsets 20-21 lag by
  one frame in the capture of § 4.8.
- **The appendix of the `0x06` walked two bytes at a time**: harmless with a Samyang while the board
  sends no `0x22`, `0x2E` or `0x3C`; a Samyang appends at most three acknowledgements (§ 4.4).

---

## 10. What `protocol.md` points here for

| Common-reference topic | Samyang section |
|---|---|
| Service envelope and bounded commands | § 6.1–6.3 |
| Homing while already streaming | § 3.1 |
| `0x0A` side effects | § 2.3 |
| Stream pacing and frame-rate selection | § 2.4 |
| `0x1D` units, arrival and acknowledgement | § 4.2–4.4 |
| Body profiles and ring behavior | § 5.2–5.5 |
| Persistent effects and allowlist scope | § 7 |

References use sections rather than line numbers because editorial revisions move the prose. Capture citations still use their original line numbers.

## 11. What stays unknown

- Everything on a wire for the eleven models other than the AF 135; their delays.
- On the AF 135: which mount contacts the extra lines of the handshake and of the sleep are; the
  meaning of `0x06` offset 0 bits 3-4 and of the wait of `0x1D` bit 3; the duration of the per-byte
  wait in service mode; the effect of type 0 and of types at or above `0x4D`; how a `0x1D` received
  during a homing interacts with the reference; the cause of the captured "`0x01` without reply"
  (§ 2.7); which rest position the 1.05 takes after a homing (§ 3.2); what happens when a target beyond
  a limit is given at that limit (§ 4.3); the meaning of `0x05` offsets 96-107 and of `0x08` offset 1.
- What the 1.05 firmware of the bench lens does differently from the 1.06 that was read.
- The cells marked unknown in § 7.2, and what the boot loaders do with an update flag.
- The link between the serial consoles of the lenses and the mount contacts.
- On the 14 A: what a reply sent during the handshake does; the effect of a `0x0A` that leaves the
  slot timer running; whether a hardware watchdog is armed.
- The `0x1D` target computation of the models other than the AF 135; bits 4-5 and 6-7 of the 35 P and
  35-150; the special unit-3 target of the 14 P, 14 A, 24, 24 A and 75.
- The 75's sense of increasing steps; the angle of a ring edge on the AF 135.
- Whether the 24 or the 24 A is the product Samyang sells as "astro": the firmware does not say.
