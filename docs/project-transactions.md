# Project storage and controller (v0.9 checkpoint 3)

This checkpoint supplies internal Qt file/session APIs. It does not add menu
commands, dialogs, unsaved-change prompts, recent projects, or preferences.
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
  destination-required error for the future UI to resolve with Save As.
- `save_as(optional<QString>)` distinguishes a supplied path from cancellation.
  `nullopt` returns `SaveStatus::cancelled` without capture, state changes, or I/O.
- `workspace()`, `path()`, `dirty()`, `revision()`, `saved_revision()` and
  `ignored_fields()` expose state for checkpoint 4.

`ProjectResult<T>` is a value-or-`ProjectFailure` variant. No JSON implementation
or numerical runtime type crosses this boundary. `ProjectFileStore` operates
only on complete project DTOs and raw bounded file bytes; it knows no widgets.

Operations are synchronous on the GUI thread. Backends and preparation factories
must not pump events or re-enter the document. Qt notification slots must not throw.
The document owns its workspace; a future visual host must outlive the document
and must not independently delete that workspace. `workspaceReplaced(old,new)`
is emitted after the state swap while the old workspace is still alive, allowing
the host to detach it and display the replacement. `stateChanged()` exposes
bookkeeping changes. MainWindow is deliberately not wired to these APIs yet.

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
prompt policy in the controller: checkpoint 4 chooses Save/Discard/Cancel first.

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
encoding. Checkpoint 4 must show that warning; this API does not claim lossless
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
