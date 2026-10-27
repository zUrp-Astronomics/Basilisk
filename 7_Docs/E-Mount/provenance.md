# Evidence and publication boundary

## Evidence is scoped

| Source | What it establishes | What it does not establish |
|---|---|---|
| Original project capture | Bytes and events observed with the named lens and board session | All revisions, all lenses, physical arrival inferred from a commanded position |
| Project firmware | What Basilisk implements at the cited revision | What a proprietary lens necessarily does |
| Static analysis | A version-specific interpretation of an application image | A bench measurement, bootloader behavior, paths not examined |
| Simulator | Behavior of our model and controller under the exercised conditions | Independent confirmation of the source used to write the model |
| Public prior work | The author's published observations or interpretation | A new verification by this project |

A conflict stays visible. A newer static image does not erase an older observed trace, and a simulator cannot settle their disagreement by repeating one side.

## Corpus

- Sony FE 24–105 mm F4 G OSS: the project's bench captures in `traces/`; no private Sony firmware analysis is used here.
- Samyang AF 135 F1.8 FE: real captures, including firmware 1.05; static application analysis of 1.06. The other eleven Samyang images contribute static findings, with versions listed in `samyang.md` § 1.
- Tamron F051 and six other models: static application analysis only; model/version list in `tamron.md` § 1. The fake F051 is derived evidence.
- Additional lens/body combinations belong to the explicitly attributed public captures in `protocol.md`, not to Basilisk's own bench record.

## What this edition publishes

The wire interface: contacts, frame structure, message sizes, meaningful fields, timing dependencies, sequencing, refusal behavior, acknowledgements, persistent-effect conclusions and unresolved points. Static findings remain labelled. Fields with independent interface meaning may include an identifier, version, mask or decoded range; opaque extracted blocks are omitted.

The restricted Samyang AF 135 M1/M2 setting is retained because it is an implemented interoperability function. Its model and version limits, persistent write, transient side effects and readback requirement remain explicit. Other factory adjustment command catalogues, memory maps, calibration algorithms, erase recipes and firmware-update entry procedures are omitted.

No proprietary firmware image, disassembly, decompilation, executable extract or private analysis file accompanies this reference. The public text states the conclusions needed at the boundary in its own words. The private evidence remains with the maintainer; a reader can see where a conclusion is not independently reproducible from the public corpus.

## Original captures remain original

The eleven files in `traces/` are retained **byte for byte**, with SHA-256 hashes and line counts in [their manifest](traces/manifest.json). They record observations made through the mount, including constant replies and undecoded bytes. They are not binary blocks copied from an application image. Keeping them whole preserves frame checksums, citations and the possibility of finding a better interpretation later.

This is an explicit editorial choice, not a rule that anything observed on a wire is automatically publishable. No raw private capture or extra workshop export has been added. The four historical captures in `4_Firmware/traces/` are also cited; they were made in service mode and remain in their existing location. Filtering this documentation does not claim to have sanitised the firmware's test data or the complete repository history.

## Public sources

The documents retain the following attribution:
   - the Google Doc "Sony E Mount Lens Protocol" (bostwickenator, Entropy512):
     `https://docs.google.com/document/d/1iw54nzrF0bzQgLINpcP9F8Odd0N5cd7LjlwCDPTNZK0/edit`
     (`protocol.md` § 1.1, § 10);
   - weiziqian, `https://github.com/weiziqian/E-mount-protocol-RE` (`protocol.md`), and the pages of
     it the documents name under `https://github.com/weiziqian/E-mount-protocol-RE/blob/main/`:
     `docs/frame_format.md`, `docs/msg_0x01.md`, `docs/msg_0x0A.md` (`protocol.md`),
     `docs/msg_0x19.md`, `docs/msg_0x4C.md`, `docs/msg_0x5A.md` (`tamron.md`);
   - LexOptical: the emulator `https://github.com/LexOptical/E-Mount` and the NEX-7 captures
     `https://github.com/LexOptical/E-Mount-Traffic-Samples`, cited file by file under
     `https://github.com/LexOptical/E-Mount-Traffic-Samples/blob/master/`
     (`nex7-sony-28-70mm-at-70mm.txt`, `nex7-sony-55mm-init.txt`, `nex7-clocks-and-sync-lines.txt`,
     `nex7-voigtlander-15mm-f4.5-inf.txt`, `nex7-voigtlander-15mm-4.5-30cm-to-inf.txt`,
     `nex7-zeiss-28mm-at-2.8-init.txt`) (`protocol.md`);
   - Entropy512's thread `https://www.dpreview.com/forums/thread/3872069`, tools
     `https://github.com/Entropy512/emount_tools` and decoder
     `https://github.com/Entropy512/libsigrokdecode` (`protocol.md` § 10);
   - the logic captures `https://github.com/EliasOenal/sigrok-dumps/tree/master/lens_mounts/sony_emount`
     (`protocol.md` § 10);
   - ExifTool, `https://exiftool.org`, for the lens names (`samyang.md` § 1.1, `tamron.md` § 1.1).

   `protocol.md` § 10 also names, without a URL, the Dyxum forum topic where Leegong published
   message sizes. `protocol.md` § 10 says what each of these sources published first, and what
   this project adds.


