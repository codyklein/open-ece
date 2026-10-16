# OpenECE 1.2.0 candidate — Read-Only Circuit Schematics

**Pre-release notes. Not published.** OpenECE 1.1.0 remains the latest published
release. This unsigned candidate needs its own Fedora 44/Windows 11 acceptance;
protected signing, exact signed-artifact acceptance and publication require
separate authorization. Existing published tags and assets remain immutable.

## Read the circuit as a schematic

Inside **Circuits → DC → Circuit and results**, **AC / Phasors → Circuit**, or
**Transient → Circuit**, choose **Circuit view: Tables / Schematic**. Tables remain
the default and the place to edit the circuit. Schematic is a derived, read-only
view of that same authoritative draft, not another circuit definition.

- Recognizable resistor, capacitor, inductor, independent voltage/current source
  and explicit-ground symbols, with terminal polarity and branch direction.
- Component names, IDs, raw values/units, AC phase and Transient source information.
- Deterministic structured arrangements for simple circuits; an explicitly labeled
  Connectivity layout with net rails for complex or incomplete circuits.
- Verified electrical identities, genuine junctions and gaps at nonconnecting
  wire crossings. Node ID 0 has no implicit ground meaning.
- Component/net selection and highlighting, literal-text inspection and tooltips.
- Fit to View, zoom, scrolling, empty-background drag-to-pan and keyboard navigation.

Empty values, invalid pending numeric text, unselected terminals, dangling IDs,
unused nodes and disconnected circuits remain editable and inspectable. The viewer
does not repair a draft or require a successful numerical solve. Presentation
failure clears the old diagram and shows an explicit diagnostic.

## Integration and accessibility

Switching views synchronizes exact pending editor text without numerical parsing.
The table widgets are retained. Visible circuit edits refresh an owned projection;
unchanged projections preserve the existing scene and navigation. A changed diagram
is fitted again, retaining a surviving typed selection. Viewing, fitting, zooming,
panning, selection and resizing are not project-data edits.

The canvas provides accessible names, visible focus frames and a textual entity
list/inspector. Tab and Shift+Tab enter and leave the six canvas controls in order.
Existing circuit tabs, result tables, plots, sweep controls and Transient execution
controls remain in place. Transient retains its scrolling editor/results splitter
and user-adjusted split; compact table presentation accommodates the view selector.

DC/AC solving, AC sweep cancellation, Transient worker execution and frozen traces
remain independent of schematic presentation. The viewer shows current draft
configuration, **not solved values or a live simulation overlay**. Existing
Signals/DSP, Digital/Timing and Communications numerical APIs remain unchanged.

## Projects and compatibility

The project format is unchanged: schema-1 import and schema-2 saves retain their
existing contracts. Supported editable state from v0.9/1.0.x schema-1 files and
1.1 schema-2 files remains supported, including incomplete drafts. In-place
schema-1 upgrade confirmation remains; use Save As to retain the older original.
**OpenECE 1.0.x cannot read schema 2.**

Schematic view choice, coordinates, zoom, pan, selection and rendering diagnostics
are not serialized. Reopening starts in Tables and remains inert. Numerical results
and running experiments are not saved. There is no new ABI, plugin, migration or
future-schema compatibility promise; unknown optional fields retain the existing
warning/discard-on-save behavior.

## Limits and validation

Circuits remain bounded to 32 nodes and 128 components. Dense Connectivity layouts
can contain thousands of crossings and require zoom plus textual inspection;
Fit is an overview, not a promise of textbook readability. Layout, verification,
coordinates, text and derived geometry retain their documented resource budgets.
Offscreen timing observations are not frame-rate or native-compositor guarantees.

Development checkpoints passed physical Fedora 44 and Windows 11 review. The new
candidate still requires [exact-build physical checks](v1.2-candidate.md).
Windows 10, MinGW, true mixed-monitor DPI and native screen-reader certification
are not newly claimed. The [readiness review](v1.2-readiness.md) records the known
Windows stress-paint and virtual-monitor geometry warnings and their scope.

CI retains GCC/Clang desktop/headless, Clang ASan/UBSan, MSVC Debug/Release,
four-scale GUI tests, compatibility fixtures, examples and fresh packaged-runtime
checks. No runtime dependency or dependency pin is added. Developer preview and
benchmark tools are not distributed in the portable application ZIP.

Deferred: graphical editing, dragging/rotation/wiring, saved positions, schematic
export, result overlays, undo/redo and unrelated engineering features. This is not
a schematic-capture or SPICE product.

The unsigned ZIP is **not** a signed distribution or Smart App Control-approved
release. A future public Windows release must use the existing reviewer-protected
OIDC signing process and pass physical testing of its own exact signed ZIP. Never
disable OS protections as a release workaround. See [signing](windows-signing.md)
and [release procedure](releasing.md).
