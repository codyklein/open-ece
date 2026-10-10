# Your first OpenECE session

1. Launch `openece.exe` from the fully extracted Windows package, or
   `./build/dev/gui/openece` after a Fedora build. The sidebar selects Signals / DSP,
   Digital Logic, Circuits or Communications. Digital and Circuits have sub-tabs.
2. Choose **File → Open…** (`Ctrl+O`) and select `examples/dc-divider.openece`.
   Source checkouts and portable packages include `examples/`. Opening
   leaves every result empty; nothing computes just because a project is opened.
3. In **Circuits → DC**, press **Solve DC**. The 10 V supply and two 1 kΩ resistors
   give a 5 V midpoint and −5 mA voltage-source current. Current is positive from
   the source's positive terminal to its negative terminal; this source delivers power.
4. Edit the supply to 12 V. Results are cleared or marked **stale** until you solve
   again; the new midpoint is 6 V. **Failed** means the current operation was not
   accepted. **Complete** marks finished computation; partial, paused and cancelled
   runs are explicitly labelled. A title `*` means saved editable state changed,
   not that a computation is running.
5. Use **File → Save As…** to save your own `My first circuit.openece`. A missing
   extension is appended automatically. Do not overwrite the shipped example
   unless you intend to change it. Save preserves even invalid numeric text or
   missing references, provided the project structure meets storage limits.
6. Close and reopen your saved file. The 12 V edit remains; results are empty.
   Press Solve again. Solver results, plots and in-progress simulations are never
   project content. Seeds/configuration are saved, BER progress is not.
7. Try another [example](../examples/README.md). If prompted about unsaved edits,
   **Save** saves then continues, **Discard** abandons edits, and **Cancel** keeps
   the current project. Failed Save or cancelled Save As also stops replacement.

Keyboard: Tab/Shift+Tab move between controls, arrow keys navigate tables,
F2 edits a selected cell, Enter commits and Escape abandons that active edit.
Use standard File menu shortcuts for New/Open/Save/Save As. Domain and workspace
selection changes are saved; automatic navigation to results is transient.

Timing has Run/Pause/Step/Reset; AC sweep Cancel retains incomplete points;
BER Cancel retains counts and Run resumes. Editing execution inputs invalidates
those results. Replace-example/copy-to-Timing actions replace drafts and warn
before substantial work is discarded. There is no undo or autosave: save work
before experimenting. [Project documentation](projects.md) explains safety and limits.

For transient circuits, open `examples/transient-rc-step.openece` and select
**Circuits → Transient**. Run produces a charging curve (~9.93 V at 5 ms); Step
advances one interval and pauses. The example uses an explicit zero capacitor
voltage; Operating point instead starts at 10 V. Pause/Resume keeps the run,
Cancel retains a terminal accepted prefix, and Reset results clears derived data.
Use the numerical trace for exact timestamps and ordered before/after breakpoint
rows. Voltages/currents are instantaneous SI quantities, not RMS phasors. See
[transient examples](../examples/README.md#transient-examples-schema-2).

New saves use schema 2, which OpenECE 1.0.x cannot read. Importing a schema-1 file
preserves supported editable state and adds an empty Transient workspace. Save As
to keep the original; an in-place upgrade requires confirmation. Opening either
schema remains inert.
