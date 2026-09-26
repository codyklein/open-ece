# Project storage, controller and workflows (v0.9 checkpoints 3–4)

Checkpoint 3 supplies internal Qt file/session APIs. Checkpoint 4 wires the File
menu through a workflow controller, with injectable dialogs and preferences.
The schema and Qt-independent DTO/codec remain in [project-format.md](project-format.md).
Numerical APIs and all engineering execution paths are unchanged.

## Ownership and API

`ProjectDocument` owns a `ProjectWorkspace`, which owns the sole editable
`ProjectSnapshot`. Views borrow its subdrafts. The document additionally owns the
current absolute QString path, revision/clean bookkeeping, and unknown-field
warnings. No second editable snapshot is kept. Captured and staged snapshots are
independent owned transaction values, not live parallel editors.

- `prepare_open(path)` returns `ProjectResult<PreparedProject>`.
- `prepare_new(snapshot, DocumentState::clean|dirty)` prepares a new session with
  explicit initial dirty state and no file path.
- `install(PreparedProject)` consumes an eligible move-only candidate belonging
  to that document. Abandoning a candidate simply destroys its private workspace.
- `save()` targets the current path; an unnamed document returns a structured
  destination-required error; the workflow controller routes unnamed Save to Save As.
- `save_as(optional<QString>)` distinguishes a supplied path from cancellation.
  `nullopt` returns `SaveStatus::cancelled` without capture, state changes, or I/O.
- `workspace()`, `path()`, `dirty()`, `revision()`, `saved_revision()` and
  `ignored_fields()` expose document state.

`ProjectResult<T>` is a value-or-`ProjectFailure` variant. No JSON implementation
or numerical runtime type crosses this boundary. `ProjectFileStore` operates
only on complete project DTOs and raw bounded file bytes; it knows no widgets.

Operations are synchronous on the GUI thread. Backends and preparation factories
must not pump events or re-enter the document. Qt notification slots must not throw.
The document owns its workspace; its visual host must outlive the document
and must not independently delete that workspace. `workspaceReplaced(old,new)`
is emitted after the state swap while the old workspace is still alive, allowing
the host to detach it and display the replacement. `stateChanged()` exposes
bookkeeping changes. MainWindow detaches its old central widget during the signal;
the document destroys it after the notification, including cooperative runtime timers.

## Loading transaction

1. Resolve the user-supplied QString to an absolute path. Use QFile in binary
   read-only mode; reject missing/non-regular files and sequential devices.
2. Check reported size against the 8 MiB project-file limit before allocating
   the byte buffer. Read in bounded 64 KiB chunks, checking returned counts and
   QFile error state. Check the running limit before appending. Verify final
   byte count and reported size; truncation/growth produces an actionable error.
3. Pass the unchanged UTF-8 bytes to the strict codec, including its duplicate-key,
   UTF-8, nesting, schema and aggregate-resource validation.
4. Construct a complete temporary snapshot, validate storage structure, and create
   a hidden, parentless, inert replacement workspace. Check that its capture is
   exactly equal to the decoded snapshot. No solver, FFT, simulator, sweep, link,
   or BER operation runs; timers, results and plots start empty.
5. Return the prepared candidate. The live workspace, path, dirty state and
   notifications are untouched. Failure destroys candidate resources.
6. Only an explicit `install` swaps the whole owned session and its bookkeeping.
   Open installs clean state. New uses its explicitly supplied disposition.

A prepared Open remembers a successful-save token. If any successful save occurs
before its installation, installation re-reads and re-prepares the source before
swapping. This conservative rule handles Save As overwriting a pending Open target,
including path aliases, without installing stale bytes. A re-stage failure retains
the current session, including any successful save already performed. There is no
prompt policy in the file/session layer: ProjectWorkflow resolves Save/Discard/Cancel first.

External same-size concurrent file modifications are not locked or detected
universally. The codec still validates the bytes actually read. Cross-process
conflict detection and file watching are outside this milestone.

## Saving transaction

1. Synchronize active editor text exactly, then capture the authoritative model.
   Synchronization does not parse, normalize, convert units, validate engineering
   inputs, or execute the project.
2. Validate storage limits and encode bounded UTF-8 through the project codec.
   An unrunnable engineering draft remains saveable.
3. Open QSaveFile for the supplied destination with `setDirectWriteFallback(false)`.
4. Write every encoded byte, checking each count and file error state. Positive
   partial writes continue; zero/negative writes or error state abort the save.
5. Call `commit()` as the sole finalization operation. No explicit flush/close,
   destination deletion, ordinary QFile overwrite, or direct-write fallback occurs.
6. Only successful commit updates the document path, saved revision, save token,
   and clean state. Post-commit notifications are outside the failure-catching
   transaction, so a notification cannot be misreported as a file-save failure.

QSaveFile creates its temporary file beside the destination. Its commit replaces
the destination atomically; destroying an uncommitted writer discards the temporary
file. An open/write/commit failure preserves previous destination bytes and the
previous document path. Dirty/clean bookkeeping is unchanged by a failed save
except for real pending user edits synchronized before capture. A failed save of
an already-clean document remains clean. Cancellation before destination selection
has no effects. Save As only changes the path after successful commit.

These guarantees follow [QSaveFile's atomic replacement contract](https://doc.qt.io/qt-6/qsavefile.html),
not a universal guarantee against every filesystem or power-loss failure.
Parent directories are not created implicitly; missing/unwritable directories are
reported to the caller. Successful preference writes are not involved.

## Identity, revisions and diagnostics

Path identity uses QFileInfo canonical existing paths. For a new destination it
combines the nearest canonical existing ancestor with the remaining normalized
path. Fedora comparison is case-sensitive; Windows uses QFileInfo equality for
existing paths and case-insensitive comparison for prospective paths under the
standard Windows filesystem convention. QString paths preserve spaces and Unicode.
Separate hard-link names are separate replacement destinations: atomic replacement
of one directory entry need not update the other. This is not a cross-process file
identity/locking protocol. Per-directory case-sensitive Windows configurations are
not claimed for prospective-path comparison; conservative re-staging does not depend
on that comparison. Empty/embedded-NUL paths are invalid.

Only `draftEdited` advances revisions and marks dirty. Derived results and automatic
result navigation do not. Successful save records the captured revision; successful
Open/New installation starts new session bookkeeping. The counter saturates instead
of wrapping; a separate dirty bit prevents saturation from implying cleanliness.

`ProjectFailure` contains code, operation, absolute path, actionable message,
optional Qt/OS detail, JSON field path, byte offset, and underlying codec cause.
The existing project error codes remain available; added storage codes are
`read_failed`, `file_too_large`, `restore_failed`, `encode_failed`,
`save_open_failed`, `save_write_failed`, and `save_commit_failed`. Codec errors
retain their exact code/path/offset on load. Encoding errors carry `encode_failed`
and their structural cause. Unexpected implementation exceptions are replaced by
bounded generic messages, not exposed verbatim to GUI consumers.

Ignored optional JSON fields are returned with the prepared candidate, then exposed
by the installed document until a successful re-save. They will be discarded by
encoding. The workflow shows that warning after installation; this API does not claim lossless
forward compatibility or byte-identical round trips.

## Validation seams

`ProjectFileIo` supplies owned reader/writer handles; production uses QFile and
QSaveFile. Test decorators force open/read/size changes, partial/failed writes,
write error state, and commit failure. Failed writers are exercised around a real
QSaveFile, with assertions on existing destination bytes and temporary-file cleanup.
A preparation factory forces exceptions and capture mismatches. The transaction
suite also uses real temporary directories, Unicode/spaces/nested paths, overwrite,
Save As, missing parents, path aliases, unknown fields and a complete invalid-draft
fixture. Read-only directory tests run when permissions are enforced; deterministic
injected failures remain mandatory on every platform.

## File workflows (checkpoint 4)

`ProjectWorkflow` borrows the document and owns no editable project copy. It
coordinates New/Open/Save/Save As/Close decisions through `ProjectDialogs` and
`ProjectPreferences` interfaces. MainWindow only composes the active workspace,
wires actions with Qt standard platform shortcuts, and updates presentation.
Modal workflow reentry is rejected. The file/session transaction itself remains
synchronous and never pumps events.

- **New** prepares `default_project()` inertly, resolves unsaved changes, and installs
  a clean unnamed session. Default examples are editable; all derived results start
  empty. Old execution state dies with the old session. Recent projects stay intact.
- **Open / Recent Projects** stages the complete file before asking about current
  unsaved changes. Invalid/unreadable files leave the active session untouched and
  do not reorder history. Cancellation abandons only the candidate. A successful
  install starts clean without running anything. Same-file Open follows these rules,
  including re-staging after an intervening successful Save or Save As.
- **Save** targets the current path or asks for Save As when unnamed. **Save As**
  preserves the old path until commit. Pending text is synchronized exactly before
  modal destination/unsaved dialogs; serialization still reads only the draft model.
- Save As appends `.openece` unless the name already ends in that extension
  (case-insensitive): `example` becomes `example.openece`, `example.txt` becomes
  `example.txt.openece`, and `example.OPENECE` is unchanged. Overwrite confirmation
  checks the final, extended destination, including dangling symlinks. Cancelling
  either dialog does not write or change the document path/history.
- Dirty New/Open/Close present **Save / Discard / Cancel**. Save continues only
  after a successful file commit; failed save or cancelled Save As aborts the
  requested replacement/close. Discard explicitly abandons current edits. Cancel
  preserves the session. Clean documents do not prompt. Close/Exit share the same
  path and an accepted close stops cooperative execution timers immediately.

The title is `Untitled — OpenECE` or `filename.openece — OpenECE`, with `*` immediately
following the name only when persisted draft edits are dirty. The window tooltip,
window file path and status bar expose the absolute path. Generate/Solve/Run,
progress, cancellation, results and automatic result navigation do not dirty a
project. User changes to persisted domain/tab selections do; restoration does not.

Recent projects use **QSettings organization `OpenECE`, application `OpenECE`, key
`projects/recentPaths`**, outside the project file. At most ten absolute paths are
retained, de-duplicated through the same path-equivalence policy as the document.
Successful Open/Save/Save As moves its path to the front. A successfully saved
current project still enters history if a later pending Open re-stage fails.
Preference failure is reported separately and cannot undo or misreport a successful
project commit. No recent project opens automatically on startup.

Missing paths remain listed with `(missing)`. Opening one reports a normal read
error, preserves the active document and keeps history order. The submenu
**Remove from recent projects** explicitly removes a selected entry without
removing its file. There is no moved-file search or path substitution. Menu labels
escape ampersands so filesystem names are not interpreted as accelerators.

Structured errors are presented as plain text with the file path, JSON field path,
byte offset and Qt/OS detail when available. Unknown optional fields produce a
separate warning that re-saving discards them. No exception stack or rich-text
interpretation of project-controlled strings is exposed.

`project_file_workflow` scripts every dialog choice and preference operation. It
covers unsaved decisions for New/Open/Close, atomic-save failure through a real
QSaveFile wrapper, exact pending line/delegate text across five save routes,
same-file re-staging, extension/overwrite policy, title/dirty behavior, missing
recent paths, settings recreation, preference failure, and session destruction.
No undo/redo, autosave, recovery, startup reopening, external assets or results
are included in persistence.
