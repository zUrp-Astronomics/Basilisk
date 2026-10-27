# Tamron E-mount lenses — what the body sees


## 0. What this document is

This is the Tamron supplement to [the common protocol](protocol.md): identity, init/stream admission, replies, focus behavior and known limitations for the seven images listed below. It uses the common offset and length conventions.

**All Tamron lens behavior here comes from static analysis, not a project hardware capture.** The F051 simulator is a model built from that analysis. It supplies no independent evidence about the physical lens. Detailed F051 findings are not automatically findings for the other six models.

Decoded interface fields, sizes, masks and model/version attribution are retained. Firmware images, opaque binary blocks, calibration data, internal continuation tables and factory adjustment procedures are not published. See [provenance](provenance.md). Unknown cells and untraced paths remain unknown, including EEPROM effects (§ 6).

## 1. The seven models

### 1.1 Identity

Static analysis, not observed, except the LensType2 names (ExifTool).

| Model code | LensType2, `0x07` offsets 9-10 | `0x3F` name (offsets 1-64) | ExifTool name for the LensType2 | Firmware |
|---|---|---|---|---|
| F051 | `0xC134` = 49460 | `E 24mm F2.8 F051` | Tamron 24mm F2.8 Di III OSD | 3.01 |
| F050 | `0xC135` = 49461 | `E 20mm F2.8 F050` | Tamron 20mm F2.8 Di III OSD | 3.01 |
| A036 | `0xC131` = 49457 | none: `0x3F` refused (§ 7.1) | Tamron 28-75mm F2.8 Di III RXD | 3.00 |
| A046 | `0xC132` = 49458 | none: `0x3F` refused (§ 7.1) | Tamron 17-28mm F2.8 Di III RXD | 3.00 |
| A056 | `0xC136` = 49462 | `E 70-180mm F2.8 A056` | Tamron 70-180mm F2.8 Di III VXD | 3.01 |
| A071 | `0xC137` = 49463 | `E 28-200mm F2.8-5.6 A071` | Tamron 28-200mm F2.8-5.6 Di III RXD | 4.01 |
| A057 | `0xC13A` = 49466 | `E 150-500mm F5-6.7 A057` | Tamron 150-500mm F5-6.7 Di III VC VXD | 4.01 |

The firmware version also names the generation used below: v3.00 (A036, A046), v3.01 (F051, F050,
A056), v4.01 (A071, A057).

- **Model code and firmware version** come from the update container Tamron publishes for each
  lens: its model field is `0xA0` followed by the LensType2, high byte first (`0xA0C134` for the
  F051), and its version field is the version given here (static analysis, not observed).
- **ExifTool** names the same LensType2 codes (`https://exiftool.org`, `Sony.pm`, `%sonyLensTypes2`;
  the generated copy in this repository: `4_Firmware/components/host/lens_names.c`, table `T`, codes
  49457 to 49466; names shortened here).
- **The `0x3F` names** are strings of the images. The handler that returns one is read on the F051
  only (§ 2.2); on the F050, A056, A071 and A057 the handler exists, it is reachable (§ 5.1), and
  the string is the only one of that form in the image (static analysis, not observed).
- **The brand.** No reply of a standard message carries the word "Tamron"; the `0x3F` name does not
  either (static analysis, not observed). The high byte `0xC1` of the LensType2 is shared: the
  ExifTool table gives codes `0xC1xx` that may also be Tokina or Viltrox lenses
  (`lens_names.c`, codes 49473 and 49474). **Recognise a Tamron by its exact LensType2**, from a
  closed list.
- `0x07` offsets 1-2 are those of the Samyang AF 135 on five models (`protocol.md` § 7.7): they
  identify no maker.

### 1.2 The decoded fields of the constant replies

Static analysis, not observed. Where the code of this repository uses a field, it is cited.

**`0x01` reply** (33 bytes). The bitmap of offsets 0-7 (rule of `protocol.md` § 7.1) lists:

| Models | Types listed |
|---|---|
| F051, F050, A056, A071, A057 | 01-0D, 10, 14, 15, 16, 17, 19, 1B, 1C, 1D, 1F, 22, 28, 2E, 2F, 34, 35, 3A, 3B, 3C, 3D, 3F |
| A036, A046 | the same without `3C` and `3F` |

- Offsets 8-31: not published (extracted from firmware, meaning unknown).
- The list is not the set of types the lens serves: all seven serve `0x26` and `0x3E` and do not list
  them; the v3.00 models serve `0x3C` and do not list it; `0x02`, `0x05`, `0x06` and `0x17` are
  types the lens sends, not receives (§ 2.2).
- The F051 keeps the request (static analysis, not observed); what it reads in it is not
  established.

**`0x07` reply** (35 bytes):

| Offset | Field | F051, F050, A056 | A071 | A057 | A036, A046 |
|---|---|---|---|---|---|
| 0 | `01`, a native lens (`protocol.md` § 7.7) | `01` | `01` | `01` | `01` |
| 1, bit 1 | the lens can switch to 1.5 Mbaud (§ 5.3) | set | set | set | clear |
| 1, other bits; 2; 3-4; 7 | not published (extracted from firmware, meaning unknown), except offsets 1-2 on the F051, below | | | the byte at offset 2 differs from the six others | |
| 5-6 | firmware version, u16 little-endian, `0x0301` = 3.01 | 3.01 | 4.01 | 4.01 | 3.00 |
| 8 | `0xA0`, the first byte of the container's model field (`protocol.md` § 7.7: `A0` on every device weiziqian measured) | `A0` | `A0` | `A0` | `A0` |
| 9-10 | LensType2 (§ 1.1) | | | | |
| 11-14, 19-33 | not published (extracted from firmware, meaning unknown) | | | | |
| 15-18 | the constant common to every lens of `protocol.md` § 7.7 | | | | |

- **Offsets 5-6 are the firmware version, read little-endian**: `0x0301`, `0x0300`, `0x0401` equal
  the container versions on all seven. The Samyang AF 135 puts its version the other way round
  (`01 05` = 1.05, `protocol.md` § 7.7).
- The F051 marks an internal flag on each `0x07`; that flag is also set by other paths, and its
  readers are not established (static analysis, not observed).
- **On the F051, offsets 1-2 read `03 70`**, the bytes of the Samyang AF 135 (`protocol.md` § 7.7;
  § 1.1). The fake answers them (`lens_std.c`, `R07`).
- Our board reads offsets 0-1 and 9-10 (`4_Firmware/components/session/lens_rx.c`, `lens_rx_id()`).

**`0x08` reply** (202 bytes). F051, read in its handler:

| Offset | Field |
|---|---|
| 0-1 | widest aperture, aperture code (`protocol.md` § 8.1): `0x1312` = f/2.9 |
| 2-3 | narrowest aperture: `0x1900` = f/22.6 |
| 12, bit 2; 13, bit 0 | forced to 0 by the handler, meaning unknown |
| 27-30 | a 32-bit value the lens computes at each request from eight ASCII digits held in its own memory; the fake F051 takes it for the unit's serial number and makes it a parameter (`4_Firmware/sim/lens_std.h`, `lstd_params_t`, `serial`, marked not established) |
| 85, bit 7 | a copy of bit 7 of the request's offset 0 (`4_Firmware/sim/lens_std.c`, `handle()`, case `0x08`) |
| every other byte | not published (extracted from firmware, meaning unknown) |

- Our board takes offsets 0-3 as the aperture range (`protocol.md` § 7.8; `lens_rx.c`, `ap_min`,
  `ap_max`). The fake F051 answers f/2.9 to f/22.6 (`lens_std.c`, `R08`).
- **The other models.** Each image holds a 202-byte block of the same form. Its offsets 0-3 read:
  F050, A036, A046 and A056 f/2.9 to f/22.6, A057 f/5.0 to f/22.6, A071 f/2.9 to f/15.7. On the
  zooms the range changes with focal length, and whether their handler rewrites these offsets at run
  time is not established (static analysis, not observed).
- **The request.** The F051 keeps it, clearing two bits of its offset 1 in its copy; it reads
  **bit 7 of offset 0** (§ 2.7). No read of the other bits of offset 0 has been found (static
  analysis, not observed). Our board sends `08` and eight zeros to a Tamron
  (`4_Firmware/components/session/init.c`, `body08_flags()`).

**`0x09` reply** (12 bytes). F051: a constant block, except bit 1 of offset 1, which carries an input
the F051 reads from a function that always answers 0: probably a barrel control this lens does not
have. Every other bit of offsets 0-10: not published (extracted from firmware, meaning unknown)
(static analysis, not observed). The fake answers `09` and eleven zeros (`lens_std.c`, `handle()`).

**`0x0A` reply** (17 bytes): the request ANDed with the lens's capability mask, whose fields are:
offsets 0-2, the 18 blocks of the `0x05` (bits 0-17 set, bits 18-23 clear); offset 8, the 8 blocks
of the `0x06` (all set); every other offset zero (`protocol.md` § 7.10; `lens_std.c`, `MASK0A`).
For the request `FF 7F … 3F` the reply is the request. Read on the F051; the six others have the
same handler structure (static analysis, not observed).

**`0x0B` reply** (3 bytes): `0B`, the request's offset 0, `00` (`lens_std.c`, `handle()`). The F051
also keeps whether that offset 0 was non-zero; the use is not established (static analysis, not
observed).

**`0x0D` reply** (2 bytes): `0D 00`, not `0D 01` as every Sony and Samyang lens of `protocol.md`
§ 7.13 (`lens_std.c`, `handle()`; static analysis, not observed). The F051 keeps the request.

**`0x3D` reply** (64 bytes, constant): offsets 0-62 not published (extracted from firmware, meaning
unknown). The F051 sets an internal flag when the request's offset 0 is zero; its use is not
established (static analysis, not observed).

**`0x3F` reply** (66 bytes): offset 0 **not written**: it holds what the previous transmission left
in the lens's send buffer, and must not be read; offsets 1-64: the name of § 1.1, padded with zeros
(`lens_std.c`, `handle()`, `NAME3F`; static analysis, not observed). The request's offset 0 is not
read.

**Other replies of fixed size** (F051; static analysis, not observed):

| Type | Reply | Known |
|---|---|---|
| `0x26` | 5 bytes | only the type byte is written; the four others are whatever the previous transmission left in the send buffer |
| `0x28` | 36 bytes | content not read |
| `0x35` | 40 bytes | content not read; side effect in § 5.5 |
| `0x3A` | 24 bytes, class 3 | Descriptor of an opaque block transfer; internal data and storage size are not published. |
| `0x3B` | 492 bytes, class 3 | the next 488-byte block of that zone; content not interpreted (not published) |
| `0x3E` | 12 bytes | content not read |

---

## 2. The F051 session, seen from the mount

Read on the F051. § 7 says what holds on the six others: the guards and the two state variables are
identical on all seven (static analysis, not observed). The fake F051 implements this section
(`4_Firmware/sim/lens_std.c`, `guard()`, `handle()`, `dispatch()`, `continuation()`).

### 2.1 Two state variables

**Static analysis, not observed.**

The body must distinguish init from stream phase and must wait for pending replies. Power-up starts in init with no pending reply on all seven studied models. `0x0A` changes phase (§ 2.4). Silence alone does not return the lens to init; no such timeout was identified. The admission conditions below describe the interface, without reproducing the lens's internal state storage.

### 2.2 The guards

Identical, type by type, on all seven models (static analysis, not observed; F051:
`lens_std.c`, `guard()`):

| Guard | Types |
|---|---|
| **init**: phase init **and** reply state free | `01 07 08 09 0B 0C 0D 10 14 15 16 19 1B 26 28 34 3D 3E`, and `3F` on the F051, F050, A056, A071, A057 |
| **free**: reply state free, any phase | `0A 35` |
| **stream**: phase stream | `03 1C 1D 1F 22 2E 2F 3C` |
| **none** | `04 3A 3B` |
| **always refused**: the handler answers nothing and consumes nothing | `00 02 05 06 0E 0F 11 12 13 17 18 1A 1E 20 21 23 24 25 27 29 2A 2B 2C 2D 30 31 32 33 36 37 38 39` |
| **refused before any handler** | every type ≥ `0x40` (F051, F050, A056, A071, A057); every type ≥ `0x3F` (A036, A046) |

- A message whose guard fails is refused with a `0x02` code 4 (§ 2.3), and nothing else happens.
- **Consumed sizes**: each handler consumes the size of `protocol.md` § 4.1, the same as the Samyang
  AF 135 for every type both serve (`lens_std.c`, `consumed()`). No handler reads the frame length:
  a message of the wrong size shifts the paving, and the next bytes are read as types until one is
  refused (static analysis, not observed).
- The handlers send their immediate reply from inside the frame processing, in class 2 (static
  analysis, not observed; `lens_std.c`, `reply()`).
- **Sub-messages before a refused one have been executed** (§ 2.3).

### 2.3 The `0x02` refusal

`02 <class> <type> <code> 00 00 00 00`, 8 bytes, **class 3, sequence 0** (`protocol.md` § 7.2;
`lens_std.c`, `send_err()`, `dispatch()`; static analysis, not observed):

| Code | Cause, checked in this order | Class field | Type field | Executed before |
|---|---|---|---|---|
| 1 | last byte not `55` | `FF` | `FF` | nothing |
| 2 | first byte not `F0` | `FF` | `FF` | nothing |
| 3 | wrong checksum | `FF` | `FF` | nothing |
| 4 | a message of type 0 or at or above the dispatch bound (§ 2.2), or a handler that refuses | the class received | the type of the **first** message of the frame; `FF` if the refused one is the first | every message before the refused one |

- **One `0x02` per frame**: the walk stops at the first refusal.
- **A `0x02` names the refused message only when it is alone or first.** A body that wants to know
  which message was refused sends it alone.
- **Every `0x02` sets the reply state back to free.** A deferred reply then pending is **lost** (the
  `0x1B` reply, the end of a homing, a `0x0C` in progress) (`lens_std.c`, `send_err()`; static
  analysis, not observed). This is also the only way the code offers out of a stuck *waiting* or
  *ready* state: any refused frame. The A036 does neither (§ 7.1).
- **What a refusal tells the body**: to an init message, the lens is not in init or a reply is
  pending; to a stream message, the lens is not in stream (static analysis, not observed).

`protocol.md` § 4 and § 6.1 point here.

### 2.4 The `0x0A`: the phase switch

- Accepted when the reply state is free, in **either** phase. In init, it moves the lens to stream.
  In stream, it moves it back to init and **stops the stream** (static analysis, not observed;
  `lens_std.c`, `handle()`, case `0x0A`).
- **The layout of the `0x05` and `0x06` blocks is set by the first `0x0A` the lens receives after
  it starts, and never recomputed**: a later `0x0A` gets a reply that announces the blocks it asks
  for ANDed with the capability mask, while the stream keeps the blocks granted the first time
  (`lens_std.c`, `layout()`: it lays out only while the type byte of the stream buffer is still
  zero; static analysis, not observed). The only path found that lays the blocks out again is a
  reinitialisation of the session that may follow a `0x16` (§ 5.6).
- **The reply** is § 1.2. It leaves in class 2, as an immediate reply.
- **A `0x0A` does not cancel a focus order** (§ 4.4): back in init, the order may run on, and its
  acknowledgement has no frame to leave in (static analysis, not observed).
- **A `0x0A` during a homing** is accepted: the stream starts before the homing ends (§ 3).
- **Consequence for a body**: a second `0x0A` puts the lens back in init while the body believes it
  streams; every later `0x03` is refused (`protocol.md` § 6.1, § 7.10; `init.c`, comment of
  `idempotent()`). Our board sends it once per session, and once more only as the "soft reset" of a
  lens that does not answer its `0x01` (§ 8).

`protocol.md` § 7.10 point here.

### 2.5 The stream

**Static analysis, not observed.**

The F051 emits `0x05` and `0x06` only in stream phase. Unlike the AF 135, continued streaming does not require a fresh `0x03`/`0x04` pair. The physical timing source has not been established on hardware: the simulation uses VD by default and exposes an optional internal period. Supply the normal body timing; do not infer that a clock-free session is supported.

The first `0x0A` selects the stream layout. Masks `FF 7F … 3F` yield 97 and 40 bytes, type included. Acknowledgements follow `0x06`; several completions of one kind before a transmitted frame may be coalesced. A body must parse the complete payload rather than assuming one fixed-size message.

| Message and offsets | Body-visible interpretation from analysis |
|---|---|
| `0x05`, 17–19 | No published aperture-ring value on F051 |
| `0x05`, 20–21 | Focus position in Sony distance units |
| `0x05`, 22 | Bit 6: focus moving; bit 7: focus module active |
| `0x05`, 24–27 | Focal-length-related values; the F051 prime's exact values are not established |
| `0x06`, 0 | Bits 0–2: inactive / active / referenced; bits 3–6: upper soft, lower soft, upper hard, lower hard limit |
| `0x06`, 1 | Bit 1: motion toward higher positions; bit 2: toward lower positions |
| `0x06`, 2–3 | Delayed commanded position, not a direct mechanical measurement |
| `0x06`, 7–10 | Published lower and upper soft limits |
| `0x06`, 20–21 and 32–38 | Selected by the `0x08` request; exact selector semantics remain unresolved (§ 2.7) |

Positions increase toward close focus. The model's own assumptions and differences are in § 9; its output does not turn these static findings into captures.

### 2.6 Deferred replies

Six replies leave after the handler has returned, from a background task of the lens (static
analysis, not observed; `lens_std.c`, `continuation()`, `homing_task()`):

| Reply | Set ready by | Class |
|---|---|---|
| `0C xx` | the `0x0C` handler; then the speed change runs over several passes (§ 5.3) | 2 |
| `10 00` / `10 01` | the end of the homing task (§ 3) | 2 |
| `19 00` | the `0x19` handler (§ 5.4) | 2 |
| `1B` + 10 bytes | a condition on the iris module after a `0x1B` (§ 5.4) | 2 |
| `34` + 23 bytes | the `0x34` handler (§ 4.7) | 2 |
| `17 xx yy` | an internal event whose source has not been read | 3 |

- **When.** At the first pass of that background task after the reply is ready; it depends on
  neither a frame nor a timer. The delay in time depends on the other tasks of the lens and is not
  readable (static analysis, not observed). The fake makes it a parameter (`lens_std.h`,
  `defer_us`).
- **While a reply is pending** (reply state *waiting* or *ready*), every init message and the `0x0A`
  are refused with a `0x02` — and that `0x02` loses the pending reply (§ 2.3). `0x04`, `0x3A` and
  `0x3B` still pass, and so do the stream messages while the lens streams; the `0x35` is refused.
  **A body waits for the deferred reply before sending anything that needs the free state.**
- **The end of a homing overwrites** whatever reply was pending: its reply takes the place of a
  pending `0x1B` or `0x19` reply (static analysis, not observed).
- The table entries that run these replies are the "`0x40` + n" entries of § 5.1.

`protocol.md` § 13 points here.

### 2.7 Bit 7 of the `0x08` request

Static analysis, not observed. The F051 keeps the `0x08` request; bit 7 of its offset 0:

- is copied into bit 7 of offset 85 of its `0x08` reply (§ 1.2; `lens_std.c`, `handle()`);
- chooses, at every stream frame, which of two internal quantities of the focus module the lens
  publishes at `0x06` offsets 20-21, and which of two sets of seven values it publishes at `0x06`
  offsets 32-38. The analyses read 20-21 as the encoder position and `protocol.md` § 7.6 gives
  32-38 as focus velocity samples (weiziqian); which quantity and which set each value of the bit
  selects is not established.
- The NEX-7 sends bit 7 set (`C0`), our board clear (`protocol.md` § 7.8). The fake does not publish
  offsets 20-38 of the `0x06` (§ 9).

`protocol.md` § 7.8 point here.

### 2.8 Physical layer, as the fake models it

**Static analysis, not observed.** The fake lens is not a physical-layer measurement.

The model receives while BODY_CS is high and transmits with the lens/body CS handshake. It uses minimum idle intervals before sending: 500 µs on LENS_CS and 50 µs on BODY_CS, plus a configurable turn-around delay. The relationship of the real lens's frame timing to VD is unresolved (§ 2.5).

These timings are analysis-derived simulation inputs. The real pin mappings, line transitions and timing tolerances still require a capture before they can be stated as measured values. Buffer layouts, interrupt implementation and busy-wait instruction counts are not part of this body-side reference.

## 3. Homing: `0x10`

**Static analysis of F051, not observed.**

`0x10` is accepted only in init with no pending reply. Its completion is deferred. `10 00` indicates the successful path, `10 01` a failure path; no observed duration is available. The simulated homing delay is a model parameter, not a measurement.

A `0x0A` can start streaming before homing finishes. Another request requiring a free reply state can cause a refusal and loss of the pending homing reply (§ 2.3, § 2.6). Wait for completion before advancing normal init. Do not infer the detailed F051 behavior for the six other models simply because they expose a `0x10` handler.

The lens's internal referencing algorithm, calibration constants and motor sequencing are outside the body-side contract.

## 4. Focus moves

Read on the F051 only (§ 7.2). Static analysis, not observed, except where the code is cited. The
fake F051 models the `0x1D` in steps only, and the `0x1C` (`lens_std.c`, header, simplification
S6; `pickup()`, `finish_check()`, `post_acks()`).

### 4.1 One focus job

**Static analysis of F051, not observed.**

A new focus order generally replaces the previous one; completion of the replaced order may never be reported (§ 4.4). `0x3C` is exceptional and has its own acknowledgement behavior. Motion can end at a target or limit, and `0x1C` requests a stop.

A `0x1D` request does not supply a motor speed. No physical motor-tick duration is established by a capture, so an extracted internal speed constant would not give a controller a reliable movement deadline. Use the external acknowledgement and state observations, with a controller-owned timeout.

### 4.2 `0x1D`: units refused, and the constants of bits 6-7

The flags of offset 3 have the layout of `protocol.md` § 7.4.2. On the F051:

| Bits 0-1, unit | Absolute (bit 2 = 0) | Relative (bit 2 = 1) |
|---|---|---|
| 0 | motor steps | motor steps |
| 1 | **refused** | **refused** |
| 2 | **refused** | multiples of a unit the lens computes from a value the body supplies; which value is not established |
| 3 | Sony distance unit (`protocol.md` § 8.3), converted as by `0x2E` | **refused** |

- **A refused unit**: the job ends without movement and is acknowledged `1D 01` (`lens_std.c`,
  `pickup()`). The Samyang lenses refuse unit 1 only, with `1D 00` (`protocol.md` § 7.4.2).
- **Relative**: offsets 0-1 are a signed 16-bit move from the current position; the result is
  bounded to 0-65535 before the target bounds apply. A relative move of 0 is the current position,
  whatever the unit (`pickup()`).
- **Bits 6–7, side of arrival**: on an absolute target, 2 selects approach from below and 1 from above; 0 or 3 selects a direct approach. The internal overshoot distance is model-specific and is not a body-side calibration constant.

- **Bit 3, wait for the cycle**: the job starts only at the motor tick after one where offset 11 of
  the `0x03` equalled offset 10 minus one (an offset 10 of 0 counts as 1), and its end is tested only
  at such ticks (`protocol.md` § 7.4.2). With the `0x03` of our board (offset 10 = 6, offset 11
  always 0) such a job never starts; our board never sets the bit (`drive.c`, `drive_goto()`).
- **Bits 4-5 and offset 2, oscillation**: read only when bit 3 is set; scale 0 = the raw byte,
  2 = 1/32 of the relative unit.
- Drivers' flag bytes: `0x00` absolute steps; `0x03` absolute Sony distance; `0x04` relative steps;
  `0x06` relative in the lens unit; plus `0x40` or `0x80` for a side of arrival.

`protocol.md` § 7.4.2 and § 13 point here.

### 4.3 Bounds and stops

**Static analysis, not observed.**

A target can be clamped without an error reply. The lower and upper soft limits are published at `0x06` offsets 7–10; read those values rather than copying calibration constants from a firmware image. The simulator's numerical defaults are model inputs, not measured limits for a physical lens.

An order can finish immediately at a soft limit; a hard-stop condition can prevent movement in either direction. Consequently `1D 00` alone does not prove arrival at the requested target. Compare the published position and limit flags. A subsequent `0x1D` replaces the previous job; a separate stop is not required merely to clear a completed limit stop.

### 4.4 Acknowledgements and eviction

Appended after the `0x06` (§ 2.5):

| Ack | When |
|---|---|
| `1C 01` | a stop (`0x1C`) done (`lens_std.c`, `post_acks()`) |
| `1C 03` | a stop that a `0x3C` interrupted, done later |
| `1D 00` | a `0x1D` done, decoded without error (`post_acks()`) |
| `1D 01` | a `0x1D` with a refused unit (`post_acks()`); a `0x1D` interrupted by a `0x3C` |
| `1F 00`, `1F 01` | a `0x1F` done; what decides between them is not established |
| `22 vL vH`, `2E vL vH` | a conversion ready (§ 4.8) |
| `3C xx 00 00` | a `0x3C` done, `xx` bits 0-1 = the direction received |

| A `0x1D` in progress, then | Effect |
|---|---|
| another `0x1D` | overwritten; one acknowledgement, the last one's |
| `0x1C`, `0x1F`, `0x34`, an internal job | overwritten; **the old one gets no acknowledgement** |
| `0x3C` | saved; it gets `1D 01`; the `0x3C` runs |
| `0x22`, `0x2E` | not overwritten; both acknowledgements come |
| `0x03` | nothing on focus; the iris target is updated |
| `0x0A` | the phase goes to init and the stream stops; the job is not cancelled, and its acknowledgement waits for a stream (§ 2.4) |

- The `0x34` has no acknowledgement in the stream: its reply is its deferred reply (§ 4.7).
- An internal job (homing, calibration) is never acknowledged.
- **A `0x1D` that never gets an acknowledgement**: refused outside stream (a `0x02` instead);
  overwritten by `0x1C`, `0x1F`, `0x34` or an internal job; bit 3 set with a cycle that never comes;
  a servo that never settles.

`protocol.md` § 7.4.1 and § 7.4.2 give these values in short.

### 4.5 `0x1C`

Stream only. It sets a stop job that holds the position, erases the pending acknowledgement of a
`0x1D`, and is acknowledged `1C 01` (`lens_std.c`, `handle()`, `start_job()`, `post_acks()`). Our
board accepts any `1C` value as the end of its stop (`protocol.md` § 7.4.1).

### 4.6 `0x3C`

Stream only. Its signed value at offsets 1-2 chooses a direction by its sign and a speed by its
magnitude; the target is one of two internal limits of the focus module, chosen by the sign. The
automaton analysis reads these limits as the soft limits; the fake's parameter comments read the
same two quantities as the hard stops less an offset (`lens_std.h`, `hard_low`, `hard_high`). Not
settled; the fake does not model `0x3C` (static analysis, not observed).

### 4.7 `0x34`

- **Init only** (§ 2.2): refused in stream.
- A focus job, with the fields of `protocol.md` § 7.18; offset 4 is a time in milliseconds over which
  the reply forecasts the position.
- The reply is deferred (§ 2.6) and leaves at the next pass of the background task, **without
  waiting for the end of the move**; 20 of its bytes are zero; offset 0 is a status of the unit
  conversion whose meaning is not established (`protocol.md` § 7.18, § 12).
- It evicts a `0x1D` without acknowledgement. Whether the move runs while the stream is stopped is
  not established.

### 4.8 `0x1F`, `0x22`, `0x2E`

- `0x1F`: stream only; a focus job, cut into the same fields as on the Samyang AF 135; what the lens
  does with each field is not established (`protocol.md` § 7.4.3, § 12).
- `0x22`, `0x2E`: stream only; conversions between motor steps and the Sony distance unit
  (`0x0700` = infinity, decreasing towards close focus); they do not touch the focus job; the
  result is appended to a later `0x06` (`protocol.md` § 7.4.4).

### 4.9 The `0x03` and `0x04`, as the F051 reads them

Static analysis, not observed (`protocol.md` § 7.3, § 7.4 give the common view).

| Message, offset | Read by the F051 |
|---|---|
| `0x03` 0-1, 10, 11, 16 (bits 0-3), 17 | kept; 10-11 serve the `0x1D` bit 3 (§ 4.2); the use of the others is not established |
| `0x03` 2 | bits 4-6 passed to the lens's control; use not established |
| `0x03` 3-4, 5-6, 7 bit 7 | the iris target: 3-4 if bit 7 of offset 7 is 0, 5-6 if it is 1; both zero sets an internal flag |
| `0x03` 7 bit 3 | passed to the iris module; use not established |
| `0x03` 12 bits 2-5 | kept; bit 3 (or offsets 13-14 = 7 or 9) puts the iris in one drive mode, otherwise the other; bit 4 selects a further iris setting |
| `0x03` 13-14 | a value tested against 7 and 9 (the iris drive mode, above), and against 3, 4, 10, `0x34`, `0x36` (a further iris setting) |
| `0x04` | no guard; seven fields kept, among them offset 3 bit 1: set, the manual focus ring is decoupled (`protocol.md` § 7.4) |

The fake does not model the iris: it accepts the `0x03` without effect (`lens_std.c`, header,
simplification S5).

---

## 5. Messages and table entries proper to Tamron

### 5.1 The "`0x40` + n" entries: continuation slots, not messages

**Static analysis, not observed.**

Values found in a firmware dispatch table are not automatically valid wire message IDs. In the studied Tamron applications, entries labelled above the wire dispatch bound are internal continuations of deferred operations; sending those indices as messages is refused. In particular, the table entries `0x4C` and `0x5A` do not establish mount commands on these Tamron models.

The public reports of `0x4C` on a TECHART adapter and Yongnuo lens, and `0x5A` on TECHART, concern different devices:

- [weiziqian, 0x4C](https://github.com/weiziqian/E-mount-protocol-RE/blob/main/docs/msg_0x4C.md)
- [weiziqian, 0x5A](https://github.com/weiziqian/E-mount-protocol-RE/blob/main/docs/msg_0x5A.md)

This Tamron analysis neither verifies nor disproves those observations. It gives no basis for probing their devices with internal indices. A036/A046 also lack the `0x3F` name request; the other five studied models expose it.

### 5.2 No service channel on the mount

**Static analysis, not observed.** None of the seven studied applications exposes the Samyang `0x40` service envelope as a mount command. Private factory adjustment facilities are not part of the supported body interface and are not documented here. Do not infer a service channel from the presence of factory strings in an image.

### 5.3 `0x0C`

- Init guard. The reply is deferred: `0C xx`, bit 0 = speed accepted, set only if bit 1 of offset 1
  of the lens's own `0x07` is set (`protocol.md` § 5). The speed change then runs over several
  passes, during which the reply state stays *ready* and every init message is refused (static
  analysis, not observed).
- A refusal during the change loses the reply (§ 2.3), and the step counter of the change is not
  reset: the next `0x0C` would resume where it stopped (static analysis, not observed).
- The A036 has no continuation for `0x0C` (§ 5.1): what it answers is not established. The A046 has
  one, and its `0x07` has bit 1 clear (§ 1.2).
- Our board never sends it (`protocol.md` § 5).

### 5.4 `0x19` and `0x1B`

Static analysis, not observed.

- **`0x19`**: init guard. If bit 0 of the request's offset 0 is set, it changes an internal mode the
  lens derives from the `0x03` (§ 4.9) to another value; its meaning is not established. Reply `19 00`,
  deferred (§ 2.6; `lens_std.c`, `continuation()`). weiziqian lists `0x19` as implemented by a TECHART
  adapter, contents unknown (`https://github.com/weiziqian/E-mount-protocol-RE/blob/main/docs/msg_0x19.md`).
- **`0x1B`**: init guard; it drives the iris (`protocol.md` § 7.17). It puts the reply state in
  *waiting*; a condition on the iris module then makes the reply ready. Whether that condition is the
  end of the iris move or a timeout is not established. While waiting, init messages and the `0x0A`
  are refused and lose the reply (§ 2.6). The reply's offset 9 keeps only its bits 0-2. The fake
  replaces the condition by a delay and the reply by parameters (`lens_std.h`, `iris_1b_us`, `r1b`;
  `lens_std.c`, `continuation()`; simplification S5).

### 5.5 `0x35`

Guard: reply state free, any phase. It stops the two timer interrupts that queue the stream,
**without changing the phase**; in stream, the next frame routine may re-arm them. Reply 40 bytes,
content not read (static analysis, not observed). The fake does not model it (`lens_std.c`,
`handle()`). weiziqian reads `0x35` as a carrier of optical rows (`protocol.md` § 7.21).

### 5.6 `0x16`

**Static analysis, not observed.** `0x16` is admitted in init, refused in stream, and replies `16 00` immediately. That reply does not prove that power-down, motor parking or an end-of-session transition has completed.

Some session reinitialisation paths can permit a new stream layout at the next `0x0A`; the conditions are not fully established. Whether the lens then sleeps, repeats a handshake or remains reachable is unresolved. The six other models have the same admission rule, but their detailed shutdown paths were not read.

Basilisk does not send this command; the fake F051 does not implement it. Persistent effects are discussed in § 6.

### 5.7 `0x17`, `0x2F`, `0x14`, `0x15`

- **`0x17`**: `17 xx yy`, class 3, sent by the lens on an internal event; the code that raises the
  event has not been read. In stream, while it is pending, the `0x0A` and the `0x35` are refused
  (static analysis, not observed). `protocol.md` § 7.16.
- **`0x2F`**: stream only; the lens keeps its two bytes and does nothing visible (`protocol.md`
  § 7.4.6).
- **`0x14`** and **`0x15`**: § 6.

---

## 6. Writes to non-volatile memory

### 6.1 F051

**Static analysis, not observed.**

No mount-message path to flash programming was identified in the reviewed F051 application paths. The EEPROM access indirection was not fully traced: absence of a persistent write from every standard message is an inference, not established fact.

The update-related `0x14` and `0x15` requests are outside Basilisk's allowed interface. `0x14` can leave the application and stop normal replies until power is removed. The behavior beyond that transition is not established here. The factory adjustment interface and its memory map are intentionally outside this reference.

### 6.2 All seven

**Static analysis, not observed.**

| Scope | Public conclusion |
|---|---|
| F051 | Reviewed flash paths do not establish a mount write; EEPROM coverage is incomplete (§ 6.1). |
| F050, A036, A046, A056, A071, A057 | Persistent-write paths were not exhaustively followed; unknown is not “none”. |
| All seven | Update-related handlers exist; the application contains adjustment facilities outside the ordinary mount interface. Their reachability from a real physical setup has not been measured. |

Basilisk never sends `0x14`, `0x15` or `0x16`, or any message above `0x3F` to an unrecognised lens (`bench_core.c`, `listed_size()`). This is the board's policy, not a claim that the lens can never write its own memory.

## 7. How each model departs

### 7.1 From the F051

Static analysis, not observed. Same on all seven: the guards and categories of types `0x00`-`0x3E`,
the two state variables and their power-up values, the `0x0A` as the only writer of the phase, the
stopping of the stream by the `0x0A`, the deferred-reply mechanism, the gate of § 6.2, the stream
block sizes.

| Model | Departs from the F051 |
|---|---|
| F050 | no additional body-visible difference established beyond identity |
| A036 | **its `0x02` leaves in class 2, with a sequence number, and does not set the reply state back to free**: a refusal neither loses a pending reply nor unblocks a stuck one (its full state machine has not been traced); no `0x3F` (dispatch bound `0x3F`); no `0x0C` continuation; `0x07` offset 1 bit 1 clear; `0x01` without `3C` and `3F`; one adjustment command fewer in two families |
| A046 | no `0x3F` (dispatch bound `0x3F`); `0x07` offset 1 bit 1 clear; `0x01` without `3C` and `3F` |
| A056 | none seen by the body, beyond identity |
| A071 | none seen by the body, beyond identity |
| A057 | `0x07` offset 2 differs (§ 1.2); no dedicated stabilisation command has been established |
| the five zooms | the code that fills the stream differs from the primes'; the field where a zoom publishes its current focal length has not been isolated |

### 7.2 What has been read on the F051 only

The detail of the focus job (§ 4), of the iris, of the homing steps (§ 3), of the `0x16` and of the
codes of the `0x02`: on the six others, their structure (guards, handlers, continuations) matches
the F051's, and their detail is unknown. The `0x1D` handler of the F050 matches the F051's; on the
others, and between the three motor families of the range, the `0x1D` semantics are unknown.

### 7.3 From `protocol.md`

| Subject | Common view (`protocol.md`) | Tamron |
|---|---|---|
| when init messages are served | Samyang: any time (§ 6.1) | init phase only, and reply state free (§ 2.2) |
| bad frame | Samyang: silent (§ 3.2) | a `0x02` (§ 2.3) |
| `0x02` class | Sony: class 2 (§ 7.2) | class 3; A036 class 2 |
| `0x0A` | lays out at every `0x0A` on the Samyang (§ 7.10) | a toggle; layout fixed at the first one (§ 2.4) |
| `0x0D` reply | `0D 01` (§ 7.13) | `0D 00` |
| stream pacing | Samyang: one `0x05` per `0x03`, one `0x06` per `0x04` (§ 6.2) | its own frame routine; no `0x03`/`0x04` needed (§ 2.5) |
| `0x1D` units | unit 1 refused by both makers (§ 7.4.2) | units 1, 2 refused in absolute; 1, 3 in relative; `1D 01` (§ 4.2) |
| `0x1D` replaced | Samyang acknowledges the old one (§ 7.4.2) | only the last one (§ 4.4) |
| `0x1C` ack | `1C 00` on Sony and Samyang (§ 7.4.1) | `1C 01` (§ 4.5) |
| `0x07` offsets 5-6 | the version, high byte first on the Samyang AF 135 (§ 7.7) | the version, little-endian (§ 1.2) |
| `0x3F` | answered by Sony and Samyang (§ 7.19) | init only; absent on A036 and A046 |
| `0x16` | Samyang: reply after both motors stop (§ 7.15) | init only, `16 00` at once (§ 5.6) |

---

## 8. Our board with a Tamron

What the code of this repository does; none of it has met a real Tamron.

- **Not a Samyang.** Our board recognises a Samyang by its `0x3F` name or a closed list of
  LensType2; a Tamron is declared "other" to `bench_core` (`init.c`, `init_recognize()`). It then
  sends `08` and eight zeros (`body08_flags()`), and starts the `0x03`/`0x04` loop only after the
  `0x0A` reply, so it sends no `0x03` during the homing, which a Tamron would refuse
  (`init.c`, `loop_start()`, `init_reply()`).
- **`0x3F`** right after the `0x07`, in the init phase: the F051 answers it; the A036 and A046 answer
  a `0x02`, which our board does not decode (`protocol.md` § 7.2): the request runs to its deadline,
  is re-sent, three sends at most, and the init goes on without a name (`init.c`, `idempotent()`).
- **The `0x0A` is never re-sent** within a session, because it toggles a Tamron (`init.c`,
  comment of `idempotent()`).
- **A Tamron left powered in stream** while the board restarts: it refuses the `0x01` with a `0x02`,
  so the `0x01` runs to its deadline three times; the board's soft reset, a lone `0x0A`, then returns
  the lens to init, and the init is served (`init.c`, comment of `INIT0A`; `4_Firmware/sim/test/test_std.c`,
  scenario `reste_alimente_reset`). Against the fake, the first `0x01` after the board's handshake is
  misread: the handshake's last BODY_CS pulse makes the fake close an empty frame slot, and what the
  real F051 does with one is not established (`4_Firmware/sim/test/constats_std.txt`, scenario
  `reste_alimente`).
- **Moves.** The board sends `1D lo hi 00 00`: absolute, steps, no wait, direct arrival
  (`drive.c`, `drive_goto()`), a combination the F051 serves (§ 4.2). It reads `0x05` offset 60 as
  movement and `0x06` offsets 2-3 as the position (`protocol.md` § 11, points 1 and 2); on a Tamron,
  by the analyses, offset 60 is not set by commanded moves (`protocol.md` § 7.5) and offsets 2-3 are
  a commanded position (§ 2.5). The suite `test_std` plays the board against the fake F051, which
  publishes the mechanical position at 2-3 and leaves offset 60 at zero (§ 9).

---

## 9. Where the fake F051 and the analyses diverge

| Subject | Analyses | Fake F051 (`4_Firmware/sim/lens_std.c`) |
|---|---|---|
| `0x06` offsets 2-3 | the commanded position of two motor ticks before | the mechanical position (header, S4; `build_content()`) |
| fields of the stream | the lens fills `0x05` offsets 20-29 and 60, and `0x06` offsets 11-38 in part (§ 2.5), and writes a constant byte in block 15 of the `0x05`, not published | only `0x05` offset 22 and `0x06` offsets 0-10 are computed; every other byte stays zero, block 15 included (S4) |
| the handshake and the physical layer | not read in the Tamron firmware | read in the image and marked established (header, "Couche physique"; § 2.8) |
| the class 1 sequence | `protocol.md` § 3.4: what the lens does with it is unknown | the stream counter is set from it (§ 2.5; `dispatch()`, `frame_tick()`) |
| the `0x3C` target | the soft limit on the side of the sign | the parameter comments call the same quantities the hard stops less an offset (`lens_std.h`, `hard_low`, `hard_high`); `0x3C` not modelled |
| the frame routine's trigger | VD or an internal 16 666 µs clock, not established | VD, with the internal clock as a parameter (`frame_period_us`) |
| homing | referencing moves the modules | no movement; after a delay the position jumps to `home_pos` (S8) |
| focus motion | servo behavior; no measured speed or tick duration | constant speed `steps_per_s`, no ramp (S3) |
| `0x1D` | units 3 (absolute) and 2 (relative), bit 3, bits 4-5, bits 6-7 served | counted "not modelled", the job set then dropped without movement or acknowledgement (S6) |
| iris, `0x1B` | a condition on the iris module | a delay, `iris_1b_us`, and a parameter reply `r1b` (S5) |
| link mode at start-up | chosen from the level of an input pin; the level a body imposes is not readable | the lens is in the serial link mode (S2), and the same pin is the lens's serial receive line (header, "Couche physique") |
| an empty frame slot | not read | misreads the next frame (`constats_std.txt`, `reste_alimente`) |

---

## 10. What stays unknown

- Everything Tamron on a wire: no Tamron has been connected; no Tamron delay is known (reply, homing,
  stream phase against VD).
- **Which edge paces the stream** (VD or an internal clock), and so whether a lens without VD streams
  (§ 2.5).
- **The link mode at start-up.** The analyses read it from the level of an input pin when the lens
  configures its serial port: low, the serial link with the body; high, the test link, where the
  lens serves no mount message. The fake places that configuration in the handshake, after LENS_CS
  rises, and identifies the same pin as the lens's serial receive line (§ 2.8, § 9). If both readings
  hold, a body whose TXD line is high at that moment puts the lens in its test link; our board holds
  TXD high from before the handshake (`protocol.md` § 2). Neither the moment of the reading nor this
  consequence has been checked.
- What the F051 does with an empty frame slot (§ 8).
- The `0x08` bit 7: which quantity and which set each value selects (§ 2.7).
- The meaning of `0x05` offsets 24-27 on a prime, and the focal-length field of the zooms (§ 2.5,
  § 7.1).
- The source of the `0x17`; what a `0x10` that fails leaves; the `0x1B` closing condition; the
  `0x19` mode; the `0x3C` target (§ 4.6); the `0x1F` fields; the `0x34` reply offset 0.
- What the lens does after a `0x16` (§ 5.6).
- What the A036 answers to `0x0C` (§ 5.3).
- The detail of the focus job, iris and homing on the six models other than the F051 (§ 7.2), and
  whether the A057's stabiliser is driven by a field of an existing message.
- Whether any standard message writes the EEPROM (§ 6.1), and what the six other models write.
- That `0x4C` is an artefact in the TECHART and Yongnuo firmwares too, and `0x5A` in the TECHART one
  (§ 5.1).
- Which image the second decompilation held under the F051's name decompiles (§ 0).
