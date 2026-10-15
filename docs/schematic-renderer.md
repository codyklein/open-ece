# Read-only schematic renderer and developer preview — checkpoint 3

This checkpoint adds an internal Qt Widgets renderer, not a workbench feature. The
DC, AC and Transient pages, numerical libraries, persisted schemas, application
version and release package contents are unchanged. Physical review is pending.

## Boundaries and APIs

The data flow remains:

```
Project draft → owned SchematicGraph → font measurements → verified LayoutDiagram
                                                        ↓
                                            SchematicCanvas (Qt Graphics View)
```

`openece_schematic_model` stays usable without Qt/Qwt. The separate internal
`openece_schematic_view` target uses only Qt Widgets plus that model. It is not
linked into `openece_workbench`. Headers are in `gui/schematic/qt/`:

- `measure_labels(graph, font)` returns owned display strings and `LayoutMetrics`.
- `symbol_geometry(placement)` returns vector strokes and polarity/arrow anchors.
- `wire_geometry(diagram)` returns one clipped path per verified original wire.
- `SchematicCanvas::setGraph(graph)` measures, lays out and independently verifies.
- `setDiagram(graph, diagram, metrics)` accepts externally prepared geometry only
  after independent connectivity verification and an exact current-font metrics
  check. A mismatched font requires measuring and laying out again.
- `select(LabelOwner)` uses analysis-scoped typed component/node keys. Names and
  item positions never resolve identity. `diagram()`, `selection()`, `inspection()`
  and `status()` expose read-only review information.

A rendering failure clears the old diagram and selection, and shows plain-text
context. It never displays unchecked or partial geometry as a complete schematic.
The renderer performs no numeric parsing, solver calls, ID allocation, draft
mutation, persisted navigation or document-edit notification.

## Symbols and connectivity

Symbols use diagram-space vector strokes: IEC rectangular resistors, capacitor
plates, four inductor loops, circular independent sources and a crossed-box generic
placeholder. Leads end at the supplied distinct positive/negative anchors.
Rotations follow the layout's positive-to-negative direction (right/down/left/up).
Voltage `+` is near the positive terminal and `−` near the negative; the glyphs stay
upright. Current arrows point from positive to negative. Ground bars are drawn only
for `LayoutDiagram::ground`, never inferred from ID 0 or a node name.

Every wire follows the original ordered polyline. Junctions are filled dots batched
by their actual net; unused-node anchors remain visible too. At each nonconnecting
crossing, **first_wire is gapped and second_wire stays continuous**. The gap extends
`marker_radius + 1` diagram units each side of the crossing. Selection redraws the
same clipped paths; it cannot fill a gap or create a connection. Color is additional
feedback, not the definition of connection. Open circles mean unselected terminals;
warning-colored crosses mean dangling IDs. Full references remain in inspection.

Routes and junctions are batched into two graphics items per declared net; symbols
use one item each. Labels are lightweight simple-text items. Crossings are path
cuts, not separate scene items. The maximum-density fixture has 7,936 crossings
and **611 scene items**, including all 416 labels, rather than thousands of crossing
objects. A connectivity layout is identified explicitly as labeled net rails,
not a conventional textbook arrangement.

## Text, fonts, DPI and original metadata

At view scale 1, one integer diagram unit maps to one **Qt logical pixel**. Qt's
view transform supplies zoom; Qt handles device pixels/DPR independently. Label
measurements never multiply by DPR. Consequently 100/125/150/200% scaling does not
apply scaling twice.

`QFontMetricsF` measures the actual canvas font and exact displayed string. Width
includes the greater of advance/ink width, negative left bearing and eight logical
units of padding. Height includes the greater of line/ink height plus eight units.
Both round **up** to integral diagram units. Rendering uses that same font/string,
with four units of inset; tests check actual item ink against the allocated boxes.
A canvas font change remeasures and reprojects its owned graph; no editing occurs.

Labels are single-line. CR/LF/tab display as `␍`/`↵`/`⇥`; Unicode line/paragraph separators display as `↵`; remaining ASCII controls
become a replacement glyph for display. Right elision uses Qt font metrics at
`max(260, 22 × font height)` logical units. This is an explicit display policy,
not a change to raw metadata. The layout retains its full original label strings.
Tooltips preserve the original label; the inspector preserves exact raw component
names, kinds, values, unit tokens, phase and every retained source point, including
inactive settings. Plain-text inspection also lists selected-net terminals,
junction coordinates/wire identities, nonconnecting crossings and diagnostics.
Tooltip markup is deliberately escaped for literal display; status labels use
`Qt::PlainText` and the inspector is `QPlainTextEdit`.

## Ownership and lifecycle

The widget owns a graph copy and a complete verified layout copy. Scene paths and
text own their Qt values; callbacks capture stable keys by value, not pointers into
drafts or vectors. The widget parents its `QGraphicsScene`, view and controls;
the scene owns all graphics items. Replacement blocks entity-list notifications,
clears old selection/lookup entries, destroys old scene items, then installs the new
owned snapshot. No worker, timer or deferred draft callback is involved. Repeated
replacement, failed installation, pending repaint and widget destruction are tested,
including under ASan/UBSan. Returned layout pointers are borrowed only until the
next replacement/font change or widget destruction.

## Navigation and textual access

- **Fit to View** / **F**: fit the complete diagram.
- **Zoom In** / **+**, **Zoom Out** / **−**, or **Ctrl+wheel**: bounded zoom.
- Ordinary wheel: scroll; arrow keys: scroll the focused canvas.
- Drag empty background: pan. Clicking a component/net inspects it.
- Tab: Fit to View → Zoom In → Zoom Out → schematic viewport → entity list →
  read-only inspector, then the next host control. Shift+Tab follows the exact
  reverse order. Focusable children are created in this order and linked explicitly
  after parenting the scroll-area controls, preserving entry/exit when the host
  adds more controls. Tab leaves the inspector rather than inserting text.
- Entity list: arrows select components/nets for textual inspection and highlighting.

The viewport, entity list and read-only inspector each reserve a three-logical-pixel
frame. The frame is neutral (`#46505e`) when unfocused and accent blue (`#96aef2`)
when focused, including keyboard focus. The outline surrounds the entire control;
electrical selection remains inside it as a row highlight or selected diagram paths.
Moving focus does not create, clear or change an electrical selection. Local,
object-scoped Qt `:focus` styling follows the scroll-area owner's focus (its viewport
delegates focus to that owner). Constant frame width prevents focus-induced layout,
scrollbar or transform changes. No scene items, global styles or focus filters are
involved. Rendered edge-color tests check all three outlines and their removal
with no/component/net selection, fixture replacement, resizing and four Qt scales.

The entity list provides an ordinary Qt keyboard/textual alternative to interpreting
plot pixels. Primary controls and views have accessible names/descriptions. This is
not a claim of native screen-reader certification. Fit is explicit and initial;
resizing does not repeatedly override the user's pan/zoom. Very dense diagrams
require zoom and scroll; full-diagram fit cannot make thousands of crossings legible.

## Build and run the developer preview

Use the repository's existing desktop dependencies and setup instructions. No new
packages, fonts, runtime libraries or numerical dependencies are added.

Fedora (from the repository root):

```sh
cmake --preset dev
cmake --build build/dev --target openece_schematic_preview --parallel 4
./build/dev/gui/openece_schematic_preview
```

Windows (Developer PowerShell after normal dependency bootstrap, `QT_ROOT` set):

```powershell
cmake --preset windows
cmake --build build/windows --config Release --target openece_schematic_preview --parallel 4
$env:PATH = "$env:QT_ROOT/bin;$env:PATH"
.\build\windows\gui\Release\openece_schematic_preview.exe
```

The preview embeds the four shipped project inputs it uses; no source-tree path is
needed at runtime. Controlled bridge/disconnected/incomplete/dense drafts are built
in. The chooser provides divider, RC, RL, RLC, Wheatstone, disconnected, incomplete
(long Unicode/markup-like names) and maximum-density fixtures. All use the actual
reusable canvas. `--smoke` constructs and paints all eight fixtures then exits.

In the preview, Tab cycles through **Review fixture → Fit to View → Zoom In →
Zoom Out → viewport → entity list → inspector**, then back to Review fixture.
Shift+Tab reverses that order. The preview-only dropdown uses the same fixed-width
neutral/blue focus border as the canvas panels. A small preview-only QComboBox
subclass paints that frame after native painting, without a stylesheet overriding
complex-control subparts or changing their geometry. It retains its native arrow, text,
popup, Up/Down selection and **Alt+R** label-buddy shortcut. The shared developer
control in `preview_controls.hpp` is tested directly; it is not a canvas or
workbench API. Switching fixtures is read-only navigation. Focus does not change
the selected fixture.

CI also uploads `OpenECE-schematic-developer-preview`, containing the **unsigned
non-shipping preview EXE**. To review without a Qt SDK, extract the ordinary unsigned
Windows CI package from the **same run** into a separate developer review directory,
then place the preview EXE beside its Qt runtime DLLs and run the preview. Do not
replace `openece.exe`, change a published ZIP, or treat this as a signed product
candidate. Local Windows security policy may reject unsigned developer tools;
this checkpoint neither signs the tool nor changes security policy. A locally
built developer preview is the alternative. No installer/install rule, normal
package entry or Azure signing change is added.

## Automated validation and measurements

`openece_schematic_gui_tests` covers symbol directions/polarity, measured bounds,
Unicode/elision/raw metadata, visible crossing gaps, exact selection keys, ground,
open/dangling terminals, failure clearing, keyboard/zoom/scroll, font changes,
scene replacement/destruction and maximum bounded layouts. It runs at four scale factors: offscreen on Fedora (fontconfig-backed), and with
the native Windows backend on MSVC runners. Qt 6.8.3’s Windows offscreen plugin
does not discover installed system fonts and is unsuitable for these font/paint
checks; no fonts are bundled or new font dependencies introduced. Optional developer screenshots can be captured by setting
`OPENECE_SCHEMATIC_SCREENSHOTS` to a directory when running this test executable;
these are review images, not pixel-golden tests or a product export API.

Build the optional Release benchmark:

```sh
cmake --build build/render-release --target openece_schematic_render_benchmark
QT_QPA_PLATFORM=offscreen ./build/render-release/tests/openece_schematic_render_benchmark
```

Configure `build/render-release` as a normal desktop Release build first. Windows
uses the same target in `build/windows`, `--config Release`, with the Qt runtime on
PATH and `QT_QPA_PLATFORM=windows` for native font measurement/painting. CI records
both Fedora and Windows logs. Timings use one warm-up followed by
five iterations; reported values are medians. They separate measured-font/layout,
scene replacement (including remeasurement/independent verification), CPU viewport
painting into an image, zoom+scroll+paint, and net selection/inspection. Peak process
memory includes Qt, all fixtures, scene, runtime and image buffers; it is not a
per-diagram allocation or a guaranteed memory bound. These are observations, not
frame-rate/native-compositor guarantees. No asynchronous renderer is justified by
the ordinary/dense scene construction measurements. Physical navigation remains a
review gate.

## Physical-review checklist (pending)

Record branch commit, OS/Qt version, font, screen scale and monitor configuration.
On Fedora and Windows:

1. Divider/RC/RL/RLC: confirm recognizable symbols, source polarity/current arrows,
   readable labels, selected explicit ground and exact component/net inspector data.
2. Wheatstone: confirm the explicit Connectivity label; zoom to normal scale and
   follow crossing gaps versus filled junctions. Select each rail and check its
   connected terminals; highlighting must retain crossing gaps.
3. Dense fixture: Fit, zoom, pan, scroll and select nets. All 32 nodes/128 components
   must remain inspectable. Report construction/interaction stalls; do not expect
   a legible textbook schematic at full-diagram fit.
4. Incomplete fixture: inspect unknown-kind placeholder, open circle versus dangling
   cross, missing ground, invalid `1e-`, Unicode, literal markup and full long names.
   Tooltips/inspector must expose original data, not an elided replacement.
5. Keyboard: Tab/Shift+Tab, toolbar activation, F/+/- and arrows; use entity-list
   selection and inspector without a mouse. Confirm the blue perimeter moves between
   viewport, entity list and inspector, disappears from the previous panel, and stays
   distinct from the retained selected row/net. Repeat after replacement/resizing;
   check inspector scrolling, visible toolbar focus and no traps.
6. Resize normally and small; test 100/125/150/200% and enlarged text as practical.
   Confirm usable scrolling, non-overlapping labels and visible crossing convention
   at normal zoom. True mixed-monitor DPI remains separate physical evidence.
7. Switch fixtures repeatedly, select then replace, and exit with repaints pending.
   No crashes/stale inspector or scene items should remain.

This preview cannot edit or run a circuit. Existing workbench behavior and project
persistence must remain unchanged. Checkpoint 4 should reuse this measured/verified
canvas through owned draft projections, with transient selection/zoom excluded from
project dirty state. Workspace integration and any further physical findings require
separate approval.
