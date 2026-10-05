from pathlib import Path
import hashlib
import io
import json
import os
import shutil
import struct
import subprocess
import sys
import xml.etree.ElementTree as ET
import zipfile

from PIL import Image, ImageCms, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent
REVIEW = ROOT.parent / 'brand-review-v1'
APPROVED = ROOT.parent / 'waveform-o-v2'
FROZEN = ROOT / 'source' / 'frozen-geometry'
SIZES = (16, 20, 24, 32, 40, 48, 64, 96, 128, 256, 512, 1024)
ICO_SIZES = (16, 20, 24, 32, 40, 48, 64, 96, 128, 256)
COLORS = {'primary': '#2854C5', 'dark': '#96AEF2',
          'monochrome': '#000000', 'white': '#FFFFFF'}
TEXT = '#1A2738'
DARK_BG = '#121B2A'
FONT_REGULAR = os.environ.get('OPENECE_BRAND_FONT_REGULAR', '/usr/share/fonts/google-noto/NotoSans-Regular.ttf')
FONT_SEMIBOLD = os.environ.get('OPENECE_BRAND_FONT_SEMIBOLD', '/usr/share/fonts/google-noto/NotoSans-SemiBold.ttf')
ICC = ImageCms.ImageCmsProfile(ImageCms.createProfile('sRGB')).tobytes()


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def freeze_copy(source, target, expected):
    assert sha(source) == expected, f'Approved source changed: {source.name}'
    target.parent.mkdir(parents=True, exist_ok=True)
    if target.exists():
        assert sha(target) == expected, f'Frozen copy changed: {target.name}'
    else:
        shutil.copyfile(source, target)


# Copy approved sources once, then refuse any subsequent source alteration.
approval_path = ROOT / 'source' / 'geometry-approval.json'
if not approval_path.exists():
    shutil.copyfile(REVIEW / 'FROZEN-GEOMETRY.json', approval_path)
approval = json.loads(approval_path.read_text())
for name, expected in approval['sha256'].items():
    source = REVIEW / 'frozen-geometry' / name
    target = FROZEN / name
    if target.exists():
        assert sha(target) == expected, f'Frozen geometry changed: {name}'
    else:
        freeze_copy(source, target, expected)

shape_path = ROOT / 'source' / 'wordmark-outline.json'
if not shape_path.exists():
    shutil.copyfile(REVIEW / 'wordmark-shape.json', shape_path)
shape = json.loads(shape_path.read_text())
brand_path = ROOT / 'FROZEN-BRAND.json'
brand = {
    'status': 'approved/frozen', 'approval_date': '2026-10-05',
    'name': 'OpenECE / Refined Waveform O A / Technical cobalt',
    'geometry_source_sha256': approval['sha256'],
    'wordmark': {'family': 'Noto Sans', 'weight': 600, 'style': 'SemiBold',
                 'case': 'OpenECE', 'outline_sha256': sha(shape_path),
                 'font_version': '2.015',
                 'font_sha256': (json.loads(brand_path.read_text())['wordmark']['font_sha256']
                                  if brand_path.exists() else sha(Path(FONT_SEMIBOLD)))},
    'colors': {**COLORS, 'light_wordmark': TEXT, 'dark_wordmark': '#FFFFFF',
               'review_dark_background': DARK_BG},
    'lockup': {'icon_canvas': 96, 'wordmark_height': 60,
               'wordmark_origin': [108, 18]},
}
if brand_path.exists():
    assert json.loads(brand_path.read_text()) == brand, 'Frozen branding specification changed'
else:
    brand_path.write_text(json.dumps(brand, indent=2) + '\n')


def geometry(text):
    root = ET.fromstring(text)
    return [(e.tag, {k: v for k, v in e.attrib.items()
                     if k not in ('stroke', 'fill', 'aria-labelledby', 'role')})
            for e in root.iter()
            if e.tag.rsplit('}', 1)[-1] in ('svg', 'g', 'circle', 'path')]


master = (FROZEN / 'openece-icon.svg').read_text()
group = ET.tostring(ET.fromstring(master).find('{http://www.w3.org/2000/svg}g'), encoding='unicode')
group = group.replace('ns0:', '').replace(':ns0', '')
word_width, word_height = shape['width'], shape['height']


def document(width, height, body, title):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width:.6f} {height:.6f}" '
            f'width="{width:.6f}" height="{height:.6f}" role="img" aria-labelledby="title">'
            f'<title id="title">{title}</title>{body}</svg>\n')


def wordmark(color):
    return document(word_width, word_height,
                    f'<path fill="{color}" fill-rule="nonzero" d="{shape["path"]}"/>',
                    'OpenECE outlined wordmark / Noto Sans SemiBold')


def lockup(icon_color, text_color):
    scale = 60 / word_height
    return document(108 + word_width*scale, 96,
        f'<g transform="scale(0.09375)">{group.replace("#161616", icon_color)}</g>'
        f'<path fill="{text_color}" fill-rule="nonzero" transform="translate(108 18) scale({scale})" '
        f'd="{shape["path"]}"/>', 'OpenECE horizontal lockup')


def render_vector(source, width, height=None):
    width = int(width)
    temporary = ROOT / 'render-intermediate.png'
    resize = f'{width*8}x{int(height)*8}' if height else f'{width*8}x'
    subprocess.run(['magick', '-background', 'none', '-density', '576',
                    str(source), '-resize', resize, str(temporary)], check=True)
    image = Image.open(temporary).convert('RGBA')
    target_height = int(height) if height else round(image.height*width/image.width)
    output = image.resize((width, target_height), Image.Resampling.BOX)
    temporary.unlink()
    return output


def save_png(image, path):
    image.save(path, icc_profile=ICC)


def recolor(alpha, color):
    image = Image.new('RGBA', alpha.size, color)
    image.putalpha(alpha)
    return image


def suffix(variant):
    return '' if variant == 'primary' else '-' + variant


# Exact approved optical SVG paths; palette is the only change.
for variant, color in COLORS.items():
    text = master.replace('#161616', color)
    assert geometry(text) == geometry(master)
    (ROOT / f'openece-icon{suffix(variant)}.svg').write_text(text)
    for size in (16, 20, 24):
        source = (FROZEN / 'optical' / f'openece-icon-{size}.svg').read_text()
        output = source.replace('#161616', color)
        assert geometry(source) == geometry(output)
        (ROOT / 'optical' / f'openece-icon-{size}{suffix(variant)}.svg').write_text(output)

alphas = {}
coverage_root = ROOT / 'source' / 'coverage'
coverage_root.mkdir(exist_ok=True)
coverage_manifest = ROOT / 'source' / 'coverage-sha256.json'
if coverage_manifest.exists():
    for name, expected in json.loads(coverage_manifest.read_text()).items():
        assert sha(coverage_root/name) == expected, f'Frozen pixel coverage changed: {name}'
for size in SIZES:
    approved_png = APPROVED / 'png' / f'openece-icon-{size}.png'
    coverage_file = coverage_root / f'coverage-{size}.png'
    if coverage_file.exists():
        alphas[size] = Image.open(coverage_file).convert('L')
    elif approved_png.exists():
        alphas[size] = Image.open(approved_png).convert('RGBA').getchannel('A')
    else:
        alphas[size] = render_vector(FROZEN / 'openece-icon.svg', size, size).getchannel('A')
    if not coverage_file.exists():
        alphas[size].save(coverage_file)
    for variant, color in COLORS.items():
        save_png(recolor(alphas[size], color), ROOT / 'png' / f'openece-{size}{suffix(variant)}.png')
coverage_record = {file.name: sha(file) for file in sorted(coverage_root.glob('*.png'))}
if coverage_manifest.exists():
    assert json.loads(coverage_manifest.read_text()) == coverage_record
else:
    coverage_manifest.write_text(json.dumps(coverage_record, indent=2)+'\n')

for name, color in (('', TEXT), ('-cobalt', COLORS['primary']), ('-monochrome', COLORS['monochrome']), ('-white', COLORS['white'])):
    file = ROOT / f'openece-wordmark{name}.svg'
    file.write_text(wordmark(color))
    save_png(render_vector(file, 1024), ROOT / 'png' / f'openece-wordmark{name}.png')
for name, ic, tc in (('-light', COLORS['primary'], TEXT), ('-dark', COLORS['dark'], '#FFFFFF'),
                    ('-monochrome', '#000000', '#000000'), ('-white', '#FFFFFF', '#FFFFFF')):
    file = ROOT / f'openece-lockup{name}.svg'
    file.write_text(lockup(ic, tc))
    save_png(render_vector(file, 1280), ROOT / 'png' / f'openece-lockup{name}.png')
shutil.copyfile(ROOT / 'openece-lockup-light.svg', ROOT / 'openece-lockup.svg')


def make_ico(images, destination):
    # Preserve each optical frame. DIB32 for 16–128; PNG for 256, avoiding a
    # largest-image downsampling ICO writer and supporting ordinary RC tooling.
    payloads = []
    for size in ICO_SIZES:
        image = images[size]
        if size == 256:
            data = io.BytesIO()
            image.save(data, format='PNG')
            payloads.append(data.getvalue())
        else:
            pixels = image.tobytes('raw', 'BGRA', 0, -1)
            stride = ((size+31)//32)*4
            mask = bytearray(stride*size)
            alpha = image.getchannel('A')
            for row, y in enumerate(range(size-1, -1, -1)):
                for x in range(size):
                    if alpha.getpixel((x, y)) == 0:
                        mask[row*stride+x//8] |= 1 << (7-x%8)
            header = struct.pack('<IiiHHIIiiII', 40, size, size*2, 1, 32, 0, len(pixels), 0, 0, 0, 0)
            payloads.append(header + pixels + mask)
    offset = 6 + 16*len(ICO_SIZES)
    directory = bytearray(struct.pack('<HHH', 0, 1, len(ICO_SIZES)))
    for size, data in zip(ICO_SIZES, payloads):
        directory += struct.pack('<BBBBHHII', 0 if size == 256 else size,
                                 0 if size == 256 else size, 0, 0, 1, 32, len(data), offset)
        offset += len(data)
    destination.write_bytes(directory + b''.join(payloads))


for variant, color in COLORS.items():
    make_ico({n: recolor(alphas[n], color) for n in ICO_SIZES}, ROOT / f'openece{suffix(variant)}.ico')


def font(size, semibold=False):
    return ImageFont.truetype(FONT_SEMIBOLD if semibold else FONT_REGULAR, size)


def paste_fit(board, image, box):
    x, y, w, h = box
    image = image.copy()
    image.thumbnail((w, h), Image.Resampling.LANCZOS)
    board.paste(image, (x+(w-image.width)//2, y+(h-image.height)//2), image)


if '--assets-only' not in sys.argv:
    social = Image.new('RGB', (1280, 640), '#FFFFFF')
    draw = ImageDraw.Draw(social)
    hero = Image.open(ROOT/'png'/'openece-lockup-light.png').convert('RGBA')
    hero.thumbnail((760, 174), Image.Resampling.LANCZOS)
    social.paste(hero, (74, 76), hero)
    draw.text((92, 278), 'Electrical & computer engineering workbench', font=font(36), fill=TEXT)
    draw.text((92, 369), 'Signals / DSP  ·  Digital Logic / Timing', font=font(28), fill=TEXT)
    draw.text((92, 413), 'DC & AC Circuits  ·  Digital Communications', font=font(28), fill=TEXT)
    draw.text((92, 493), 'Open source  ·  C++20 / Qt 6  ·  Project save / load', font=font(23), fill='#596579')
    draw.line((92, 552, 1188, 552), fill=COLORS['primary'], width=3)
    draw.text((92, 574), 'github.com/codyklein/open-ece', font=font(22), fill='#596579')
    save_png(social, ROOT / 'openece-social-preview.png')

    board = Image.new('RGB', (1440, 1790), '#F4F6F9')
    draw = ImageDraw.Draw(board)
    draw.text((36, 18), 'OpenECE — final branding review', font=font(34, True), fill=TEXT)
    draw.text((36, 70), 'Frozen Waveform O / A  ·  Technical cobalt  ·  Noto Sans SemiBold', font=font(21), fill='#596579')
    for x, bg, color, label in ((36, '#FFFFFF', COLORS['primary'], 'Light icon · #2854C5'),
                               (738, DARK_BG, COLORS['dark'], 'Dark icon · #96AEF2')):
        draw.rounded_rectangle((x, 116, x+666, 368), radius=10, fill=bg)
        mark = recolor(alphas[256], color).resize((188, 188), Image.Resampling.LANCZOS)
        board.paste(mark, (x+239, 126), mark)
        draw.text((x+22, 326), label, font=font(20), fill='#FFFFFF' if bg == DARK_BG else TEXT)
    draw.text((36, 385), 'Outlined wordmark', font=font(22, True), fill=TEXT)
    draw.rounded_rectangle((36, 422, 1404, 511), radius=8, fill='#FFFFFF')
    paste_fit(board, Image.open(ROOT/'png'/'openece-wordmark.png').convert('RGBA'), (380, 432, 680, 67))
    draw.text((36, 530), 'Light and dark horizontal lockups', font=font(22, True), fill=TEXT)
    for x, bg, variant in ((36, '#FFFFFF', 'light'), (738, DARK_BG, 'dark')):
        draw.rounded_rectangle((x, 569, x+666, 690), radius=8, fill=bg)
        paste_fit(board, Image.open(ROOT/'png'/f'openece-lockup-{variant}.png').convert('RGBA'), (x+30, 585, 606, 90))
    draw.text((36, 709), 'Official black and white monochrome alternatives', font=font(22, True), fill=TEXT)
    for x, bg, variant in ((36, '#FFFFFF', 'monochrome'), (738, DARK_BG, 'white')):
        draw.rounded_rectangle((x, 748, x+666, 860), radius=8, fill=bg)
        paste_fit(board, Image.open(ROOT/'png'/f'openece-lockup-{variant}.png').convert('RGBA'), (x+40, 764, 586, 82))
    draw.text((36, 879), 'Actual-size icon exports: 16 / 20 / 24 / 32 / 48 px', font=font(22, True), fill=TEXT)
    for x, bg, variant in ((36, '#FFFFFF', 'primary'), (738, DARK_BG, 'dark')):
        draw.rounded_rectangle((x, 921, x+666, 1014), radius=8, fill=bg)
        for size, offset in zip((16,20,24,32,48), (72,202,332,462,592)):
            image = recolor(alphas[size], COLORS[variant])
            board.paste(image, (x+offset-size//2, 967-size//2), image)
    draw.text((36, 1034), 'GitHub social preview — 1280 × 640', font=font(22, True), fill=TEXT)
    board.paste(social, (80, 1076))
    draw.text((36, 1746), 'Review package only · no repository integration · published v1.0.0 unchanged', font=font(20), fill='#596579')
    save_png(board, ROOT / 'review' / 'final-brand-sheet.png')

# Pixel-level ICO verification proves the native optical artwork survives.
for variant in COLORS:
    file = ROOT / f'openece{suffix(variant)}.ico'
    ico = Image.open(file)
    assert ico.ico.sizes() == {(n,n) for n in ICO_SIZES}
    for size in ICO_SIZES:
        actual = ico.ico.getimage((size,size)).convert('RGBA')
        expected = recolor(alphas[size], COLORS[variant])
        assert actual.tobytes() == expected.tobytes(), (file.name,size)
for size in SIZES:
    for variant in COLORS:
        file = ROOT / 'png' / f'openece-{size}{suffix(variant)}.png'
        image = Image.open(file).convert('RGBA')
        assert image.size == (size,size)
        assert image.getchannel('A').tobytes() == alphas[size].tobytes()
        assert image.getpixel((0,0))[3] == 0
for file in list(ROOT.glob('*.svg')) + list((ROOT/'optical').glob('*.svg')):
    root = ET.fromstring(file.read_text())
    for element in root.iter():
        assert element.tag.rsplit('}',1)[-1] not in ('script','image','text','foreignObject','style')
        assert not any(k.rsplit('}',1)[-1] in ('href','src') or k.startswith('on') for k in element.attrib)
assert Image.open(ROOT/'openece-social-preview.png').size == (1280,640)
assert (ROOT/'openece-social-preview.png').stat().st_size < 1_000_000
for name, expected in approval['sha256'].items():
    assert sha(FROZEN/name) == expected
assert sha(shape_path) == brand['wordmark']['outline_sha256']

verification = {
    'icon_geometry': '8 approved source SVG hashes verified; recoloring only',
    'png_icon_files': 48, 'png_sizes': list(SIZES), 'color_variants': list(COLORS),
    'optical_sizes': [16,20,24],
    'ico_files': 4, 'frames_per_ico': 10, 'ico_sizes': list(ICO_SIZES),
    'ico_verification': 'All 40 frames exactly match RGBA PNG pixels, including optical variants',
    'svg_validation': 'All logo SVGs self-contained; no fonts, external references or scripts',
    'social_dimensions': [1280,640], 'social_bytes': (ROOT/'openece-social-preview.png').stat().st_size,
    'runtime_validation': 'Qt/Windows Explorer integration deferred; not a packaged-app build',
}
(ROOT/'VERIFICATION.json').write_text(json.dumps(verification,indent=2)+'\n')
print(json.dumps(verification,indent=2))
