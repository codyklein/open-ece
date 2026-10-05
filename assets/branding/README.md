# OpenECE — final branding asset package

Approved identity: **Refined Waveform O / A + Technical cobalt + Noto Sans
SemiBold**. Icon geometry, native 16/20/24 px variants, palette and typography
direction are frozen. The approved artwork is integrated unchanged for OpenECE 1.0.1.
The published v1.0.0 tag, release and Windows package remain immutable.

## Palette

| Role | Color |
| --- | --- |
| Primary icon on light backgrounds | `#2854C5` |
| Icon on dark backgrounds | `#96AEF2` |
| Light-lockup wordmark | `#1A2738` |
| Dark-lockup wordmark | `#FFFFFF` |
| Official monochrome black | `#000000` |
| Official monochrome white | `#FFFFFF` |
| Demonstration dark background | `#121B2A` |

Use flat solid colors. Do not introduce gradients, glossy effects, shadows or
additional palettes. The two blue treatments belong to the same identity; they
are chosen for background contrast and do not imply automatic theme switching.

## Asset inventory

| Asset | Filename/location |
| --- | --- |
| Primary icon SVG master | `openece-icon.svg` |
| Dark-background icon | `openece-icon-dark.svg` |
| Black/white monochrome icons | `openece-icon-monochrome.svg`, `openece-icon-white.svg` |
| Outlined neutral wordmark | `openece-wordmark.svg` |
| Cobalt/black/white outlined wordmarks | `openece-wordmark-cobalt.svg`, `openece-wordmark-monochrome.svg`, `openece-wordmark-white.svg` |
| Horizontal light lockup | `openece-lockup-light.svg`; `openece-lockup.svg` is an identical convenience alias |
| Horizontal dark lockup | `openece-lockup-dark.svg` |
| Black/white lockups | `openece-lockup-monochrome.svg`, `openece-lockup-white.svg` |
| Native small-size SVG artwork | `optical/openece-icon-{16,20,24}.svg`, with dark/monochrome/white alternatives |
| Transparent icon PNGs | `png/openece-{size}.png`, plus `-dark`, `-monochrome`, `-white` variants |
| Wordmark/lockup PNGs | `png/openece-wordmark*.png`, `png/openece-lockup*.png` |
| Primary Windows ICO | `openece.ico` |
| Alternative Windows ICOs | `openece-dark.ico`, `openece-monochrome.ico`, `openece-white.ico` |
| GitHub social preview | `openece-social-preview.png`, with an exact copy in [`docs/images`](../../docs/images/openece-social-preview.png); 1280×640 |
| Frozen identity specification | `FROZEN-BRAND.json` |
| Validation results | `VERIFICATION.json` |

The 48 transparent icon PNGs cover **16, 20, 24, 32, 40, 48, 64, 96, 128, 256,
512 and 1024 px** in all four official color treatments. PNGs are RGBA with an
sRGB profile and no background matte. Export sizes refer to the entire square
canvas, including the existing optical padding.

Each ICO has ten frames: **16, 20, 24, 32, 40, 48, 64, 96, 128 and 256 px**.
Frames below 256 use conventional 32-bit alpha DIB encoding; 256 uses PNG
encoding. All frames were decoded and compared exactly with their respective
PNG pixels. The writer embeds the actual small-size artwork; it does not create
those frames by shrinking the largest icon. The executable-resource test compares all ten embedded Windows frames against
the approved ICO bytes. Physical icon appearance is checked on each candidate.

## Usage and spacing

- The icon works independently. Use it for application/window/executable icons;
  use a lockup when there is enough space to present the name.
- Preserve the square canvas and its existing approximately 10% optical padding.
  Do not crop the icon tighter, distort it, or change ring/wave spacing.
- Prefer one of the supplied lockups rather than recreating icon-to-wordmark
  spacing. At its source scale the icon canvas is 96 units and the wordmark
  begins at x=108, has a height of 60, and starts at y=18.
- Keep clear space outside the complete lockup of at least one quarter of the
  visible icon diameter where the surrounding layout permits it. This is a
  presentation spacing rule, not additional padding to bake into app-icon PNGs.
- Use the light treatment on light surfaces and the dark treatment on dark
  surfaces. The monochrome versions are official alternatives, not fallbacks
  requiring a different geometry.
- A Windows ICO is one fixed artwork set. Choose the appropriate supplied
  variant deliberately; ICO files do not switch colors automatically with theme.
- Do not substitute the O inside the written name with the icon or alter letters.

## Wordmark

Use **Noto Sans SemiBold, weight 600, upright, normal width**, spelling `OpenECE`.
Keep ordinary kerning and a consistent weight. No decorative letter changes,
tracking effects or separate styling of `ECE` are part of this identity.

The distributed wordmarks are glyph outlines, not SVG text. All logo SVG files
are self-contained, with no external fonts, scripts, images or resource links.
There is no font-installation requirement. Qt uses the embedded PNG exports instead of runtime SVGs, avoiding a QtSvg
dependency. The primary cobalt set is used for the application and executable;
no automatic light/dark icon switching is introduced.

## Small-size artwork

The 16/20/24 px vectors have separately tuned ring/wave weights, shoulder
positions and join spacing. Use each at its named size. Do not use one tiny
optical variant as the large master. The 32 px and larger icons use the approved
large geometry.

| Native size | Ring weight | Wave weight |
| --- | --- | --- |
| 16 px | 1.50 px | 1.40 px |
| 20 px | 1.75 px | 1.65 px |
| 24 px | 2.00 px | 1.90 px |

`source/frozen-geometry/` retains the eight approved source SVG bytes and their
hashes. `source/coverage/` retains pinned alpha-coverage masks for repeatable PNG
and ICO export. This keeps the approved tiny-pixel work intact across recoloring.
All color variants share the same alpha coverage at a given size.

## Ownership, licensing and provenance

Brand artwork was prepared for the OpenECE project and project owner Cody Klein.
The icon began as an AI-assisted concept study and was reconstructed/refined as
explicit vector geometry; the approved geometry is recorded in the source
hashes. The wordmark is outlined from Noto Sans SemiBold without customized
letterforms.

The original OpenECE artwork, layout and export utilities are supplied under
OpenECE's MIT license, included as `LICENSE`. Copyright (c) 2026 Cody Klein.

Noto Sans attribution: Copyright 2022 The Noto Project Authors. The exact font
used identifies itself as **Noto Sans SemiBold, version 2.015**, licensed under
SIL Open Font License 1.1. The matching license is included in
`notices/Noto-Sans-OFL.txt` verbatim, including upstream whitespace. No font binaries are included; generated wordmark
documents are separate from the font software. Font provenance and its SHA-256
are recorded in `FROZEN-BRAND.json`.

Official font/license source:
https://github.com/google/fonts/blob/main/ofl/notosans/OFL.txt

## Export procedure

The package is self-contained for rebuilding its vector, PNG and ICO assets.
Requirements: Python 3, Pillow with ImageCms, and ImageMagick 7 (`magick` on PATH).
No network access or OpenECE source checkout is required.

```sh
python3 build_assets.py --assets-only
```

This reuses pinned vector, glyph-outline and pixel-coverage sources; it verifies
their hashes, emits sRGB transparent exports, assembles ICOs frame-by-frame and
runs the asset checks. It leaves the supplied social/review PNGs unchanged.

To regenerate the social preview and review sheet as well, use the regular and
SemiBold Noto Sans fonts for their explanatory text. If the fonts are not at the
documented Fedora paths, set these environment variables to their files:

```text
OPENECE_BRAND_FONT_REGULAR=/path/to/NotoSans-Regular.ttf
OPENECE_BRAND_FONT_SEMIBOLD=/path/to/NotoSans-SemiBold.ttf
```

Then run `python3 build_assets.py`. The logo's glyph geometry still comes from
the pinned outlines; the fonts are used only for explanatory/social body text.
For consistent text wrapping use the recorded 2.015 font family.

The source optical renders used 8× coverage sampling with BOX area reduction,
not sharpening. Existing approved masks remain authoritative for final exports.
ICO creation uses those exact pixels at each size. The GitHub preview is an
ordinary solid-background PNG below 1 MB; it is not uploaded by this package.

## Validation and review boundary

Checks cover frozen geometry, outlined glyph source, transparent PNG dimensions,
shared light/dark coverage, exact ICO decoding, SVG structure and social-preview
dimensions/size. `SHA256SUMS.json` covers every delivered file except itself.

The imported artwork is checked against `IMPORTED-ASSET-SHA256SUMS.json` by the
GUI branding test. `APPROVED-SOURCE-SHA256SUMS.json` records the complete external
approved source package, including its original README and review image; those
review-only files are not part of this repository's asset inventory. Repository
usage documentation is maintained separately from frozen artwork.

Do not run export utilities during normal CMake configuration or packaging.
Re-export only for a demonstrated technical export problem and review any hash
changes before accepting them. The approved social preview is copied to
`docs/images/`; the export script's optional review/social outputs are for manual
inspection, not automatic replacement of that approved image.

Integration changes no plot colors, themes, installers, file associations or
repository settings. See [integration and candidate checks](../../docs/branding.md).
