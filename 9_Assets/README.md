# 9_Assets

The showcase of Basilisk: what the [zUrp Astronomics website](https://zurp-astronomics.github.io/basilisk/) and this repository's
README display. Its poster and other images belong to the zUrp Astronomics organisation, which keeps them consistent
across all its products. The product-page manual (`manual/`) belongs to this project, in the format the website sets;
the product sheet follows the website's format, and its `status` changes only on the explicit decision of the human.

| File | What it is | Shown on |
|---|---|---|
| `zurp.yml` | the product sheet: name, tagline, slogan, section, status, description, accent colour | the website, the organisation README |
| `basilisk.webp` | the poster, 1254 × 1254 | the website, the README header |
| `basilisk-board.webp` | main board v1.0, 3D view | the README |
| `basilisk-board-top.webp` | main board v1.0, top | the README |
| `basilisk-board-bot.webp` | main board v1.0, bottom | the README |
| `basilisk-enclosure.webp` | enclosure v1.0b, CAD render | the README, the Assembly tab |
| `manual/` | the product manual, in English: `overview.en.md`, `assembly.en.md`, `setup.en.md`, `usage.en.md` | the website, one tab each: Overview, Assembly, Setup, Usage |

`basilisk-board.webp` is also shown in the Overview tab.

**The manual** (`manual/`) is the product page's assembly and user guide; the README presents the project and the
repository. The website reads only these four file names — it does not list the folder — and fixes the order and
the titles of the tabs; it places the install block (flash button, downloads, Bench button) in the Setup tab
itself. When `overview.en.md` exists, it replaces the `description` of `zurp.yml` in the page body; `description`
stays mandatory in the sheet, short and consistent with the start of the overview. The format, agreed with the
website's architect:

- no H1, sections start at `##`; no raw HTML (shown as text);
- the file header goes in a `---` … `---` front matter, never rendered;
- a relative link to a repository file leads to that file on GitHub; a link to another tab's file leads to that tab;
- images relative to the `.md` file, under `9_Assets/` only, in PNG, JPG or WebP, each well under 600 KB.

English only for now; the French versions (`.fr.md`) will come with the website's language switch. The facts come
from the repository, and each tab links to where they live.

Images here are WebP copies sized for display. Their full-size originals stay where they are produced: `1_Board/Basilisk-v1.0_2-view_3D.png`, `1_Board/Basilisk-v1.0_2-view_top.png`, `1_Board/Basilisk-v1.0_2-view_bot.png`, `2_Hardware/Basilisk_v1.0b_enclosure.png`.

The sheet is read and checked by the website at each build: its format is described in
[`content/README.md`](https://github.com/zUrp-Astronomics/zurp-astronomics.github.io/blob/main/content/README.md)
of the website's repository.
