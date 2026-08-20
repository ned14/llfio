# Analysis: `include/llfio/v2.0/detail/impl/windows/directory_handle.ipp`

Date: 2026-08-20
Scope: Exhaustive review of the Windows implementation of `directory_handle` only
(`include/llfio/v2.0/detail/impl/windows/directory_handle.ipp`), cross-checked against the
doxygen API documentation in `include/llfio/v2.0/directory_handle.hpp`, the Windows utility
code it calls (`detail/impl/windows/import.hpp`, `fs_handle.ipp`, `file_handle.ipp`,
`path_handle.ipp`, `handle.ipp`), the POSIX backend for behavioural comparison
(`detail/impl/posix/directory_handle.ipp`), the `ntkernel-error-category` mapping table, and
the ReactOS sources (`ntoskrnl/io/iomgr/iofunc.c`, `drivers/filesystems/fastfat/dirctrl.c`,
`drivers/filesystems/ntfs/dirctl.c`).

Build/configuration dimensions considered: `LLFIO_DIRECTORY_HANDLE_ENUMERATE_LESS_INFO`,
`LLFIO_EXPERIMENTAL_STATUS_CODE`, `LLFIO_HEADERS_ONLY`, `LLFIO_USE_LEGACY_FILESYSTEM_SEMANTICS`,
`NDEBUG`, 32-bit vs 64-bit, x64 vs ARM64, MinGW vs MSVC, Windows 10 pre-1709 / 1709 / 1803+
feature gating, NTFS vs non-NTFS (FAT/exFAT/network) volumes, `flag::multiplexable`
(OVERLAPPED) vs synchronous handles, all `mode`/`creation`/`caching` combinations, user-supplied
vs internally allocated kernel buffers, empty/glob/filtered enumeration requests, and the
racy directory corner cases (issue #137 paths).

Items are ordered by priority. Each item has a unique eight character identifier.

---

## High

### `DEADLNDR` — `read()`'s deadline is ignored for the actual i/o waits

Location: `directory_handle.ipp:345-348 and 402-405`.

Both passes call `ntwait(_v.h, isb, deadline())` with an empty (no-deadline) deadline
instead of the caller's `d`. For synchronous handles `NtQueryDirectoryFile` never returns
`STATUS_PENDING`, so the deadline is only enforced by `LLFIO_DEADLINE_TO_TIMEOUT_LOOP(d)`
in the retry paths; but for `flag::multiplexable` (OVERLAPPED) handles the syscall *can*
pend, and then `ntwait` blocks without bound. This violates the documented contract
(`directory_handle.hpp:417`: "`d` An optional deadline by which the i/o must complete,
else it is cancelled") and is inconsistent with `byte_io_handle.ipp:148,290`, which passes
the real deadline. Fix: pass `d` to both `ntwait` calls.

### `REOPNDIR` — `reopen()` with a changed `mode` or `caching` always fails on directories

Location: `directory_handle.ipp:212-218` (`reopen()`), root cause `import.hpp:2100-2118`
(`do_clone_handle(..., isdir=true)`).

`directory_handle::reopen(mode_, caching_)` calls `do_clone_handle(..., isdir=true)`. The
slow path (any change of `mode` or `caching`) builds `ntflags` and unconditionally sets
`ntflags |= 0x040 /*FILE_NON_DIRECTORY_FILE*/` ("do not open a directory"), even when
`isdir` is true. `NtOpenFile` on a directory object with `FILE_NON_DIRECTORY_FILE` fails
with `STATUS_FILE_IS_A_DIRECTORY` (0xC00000BA, mapped to `EACCES`), so the slow path can
never succeed for a directory. Only the fast path (`mode::unchanged` +
`caching::unchanged`, plain `DuplicateHandle`) works.

This breaks the documented contract in `directory_handle.hpp:379-391`:
"optionally race free reopening the handle with different access or caching … `\errors` Any
of the values POSIX dup() or DuplicateHandle() can return." The POSIX backend implements
this correctly by path re-open and inode comparison (`posix/directory_handle.ipp:200-252`).
The `isdir` parameter already exists and strips `DELETE` (`import.hpp:2100-2106`), so the
fix is to make the `CreateOptions` conditional: `FILE_DIRECTORY_FILE` when `isdir` is true,
`FILE_NON_DIRECTORY_FILE` otherwise. The misleading error (`EACCES`-equivalent rather than
"invalid argument"/ENOTSUP) makes this hard to diagnose.

### `ABSPTHBS` — absolute Win32 paths fail when a valid `base` handle is supplied

Location: `directory_handle.ipp:65, 104-115`.

The NT branch is selected when `base.is_valid() || path.is_ntpath()`. A caller passing a
valid `base` together with an absolute Win32 path such as `C:\foo` (or an extended-length
path `\\?\C:\foo`, which `is_ntpath()` deliberately does not match) lands in the NT branch,
where `C:\foo` is treated as a *relative* NT object name resolved against
`RootDirectory = base`. The object manager looks for a child named `C:` inside `base` and
fails with `STATUS_OBJECT_PATH_NOT_FOUND`/`STATUS_OBJECT_NAME_NOT_FOUND` (ENOENT).

The POSIX backend ignores `base` for absolute paths (`openat`/`open` semantics), so
portable code that passes both an absolute path and a base handle works on POSIX but fails
on Windows. The same pattern exists in `file_handle::file()` and `path_handle::path()`;
the fix is to treat `is_absolute()`/drive-letter or `\\?\`-prefixed paths as absolute in
the NT branch (e.g. detect a drive letter or UNC prefix and null out `RootDirectory`), or
route absolute Win32 paths through the `CreateFileW_` path.

### `ALWNEWDD` — `creation::always_new` is not implemented and returns the wrong error code

Location: `directory_handle.ipp:81-85, 122-125 (NT), 161-165, 172-175 (Win32)`.

`handle::creation::always_new` is documented (`handle.hpp:92`) as "If filesystem entry
exists, it is atomically replaced with a new inode, else a new entry is created." The
Windows implementation degrades it to `FILE_CREATE`/`CREATE_NEW` (the comments at
lines 81-85 and 161-165 admit that replacement is unimplemented because DELETE was
stripped), and maps the resulting `STATUS_OBJECT_NAME_COLLISION` (NT) /
`ERROR_ALREADY_EXISTS` (Win32) to `errc::directory_not_empty`.

This has three defects:
1. The documented replace semantics are simply not provided (the POSIX backend *does*
   implement them, via `rename_random_dir_over_existing_dir`, `posix/directory_handle.ipp:89-134,125-133`),
   so behaviour differs between backends for the same documented operation.
2. `errc::directory_not_empty` is the wrong code whenever the existing entry is a plain
   file, or an empty directory — the correct code for a name collision is
   `errc::file_exists` (the NTSTATUS table already maps 0xC0000035 → EEXIST; this code
   overrides that correct mapping).
3. `uniquely_named_directory()` in the header (`directory_handle.hpp:315-334`) documents
   its loop by matching `errc::file_exists`, which can never match `always_new` failures.

Fix: implement replacement by the same duplicate-with-DELETE + `FILE_SUPERSEDE`-style
trick used by `relink()`, or at minimum return `errc::file_exists` on collision.

### `ENUMODES` — enumeration fails for `mode::none`/`attr_read`/`attr_write`/`append` handles, unlike POSIX

Location: `directory_handle.ipp:54` (access build) and `286` (`read()`).

`access_mask_from_handle_mode` (`import.hpp:1916-1952`) grants `SYNCHRONIZE` only for
`mode::none`, `FILE_READ_ATTRIBUTES` for `attr_read`, etc. — none of these include
`FILE_LIST_DIRECTORY`, which `NtQueryDirectoryFile` requires, so `read()` on such a handle
fails with `STATUS_ACCESS_DENIED`. Confirmed against ReactOS: the I/O manager performs
`ObReferenceObjectByHandle(FileHandle, FILE_LIST_DIRECTORY, IoFileObjectType, ...)`
(`ntoskrnl/io/iomgr/iofunc.c`) before issuing the query IRP. The POSIX backend silently
converts every mode to `O_RDONLY` for directories (`posix/directory_handle.ipp:58-66`)
and enumeration works for all modes. The Windows doc comment in `directory_handle.hpp`
documents no such mode restriction, so `directory(dirh, name, mode::none)` + `read()` is
portable on paper but fails on Windows only. Fix: always add `FILE_LIST_DIRECTORY` to
the access mask for directories (it is harmless), or document the restriction.

---

## Medium

### `CASEFLAG` — `win_create_case_sensitive_directory` silently ignored for all modes except `mode::write`

Location: `directory_handle.ipp:196-208`.

The flag documentation (`handle.hpp:189-194`) says it applies "when creating a directory",
with no mode restriction. The implementation gates the `NtSetInformationFile
(FileCaseSensitiveInformation)` call on `_mode == mode::write`; with `mode::read` (the
default in `uniquely_named_directory`/`temp_directory` call paths), or
`mode::attr_write`/`mode::append` (which both carry `FILE_WRITE_ATTRIBUTES` and would
work), the request is silently dropped — the user believes they created a case-sensitive
directory and it is not. Either honor the flag whenever the access mask contains
`FILE_WRITE_ATTRIBUTES` (and fail loudly when it cannot be honoured for a new directory),
or document the `mode::write` restriction. Note also the API requires Windows 10 1803+
NTFS; older Windows logs a warning and proceeds, which is acceptable but undocumented in
the flag description.

### `CASESENS` — the two open branches have opposite case-sensitivity semantics

Location: `directory_handle.ipp:109` (`oa.Attributes = 0`, `OBJ_CASE_INSENSITIVE`
commented out) vs. `import.hpp:2351-2354` (`CreateFileW_` sets `OBJ_CASE_INSENSITIVE`).

The NT branch (valid `base`, or `\!!\`/`\??\` paths) performs case-*sensitive* lookups,
while the Win32 branch (`CreateFileW_`) is case-*insensitive*. Consequences: code that
opens `directory({}, "C:\\Users\\Public")` succeeds, but the "equivalent" refactor to
`directory(base, "Public")` fails with `STATUS_OBJECT_NAME_NOT_FOUND` if the on-disk case
differs. On NTFS this is a genuine behavioural difference between the two branches of the
same function, and neither matches the Win32 semantics a Windows user would expect; the
POSIX backend is case-sensitive, so the NT branch happens to match POSIX, but then the
Win32 branch is the odd one out. Pick one documented behaviour per call — preferably
matching the Win32 branch, or honouring `OBJ_CASE_INSENSITIVE` when the path is not
`\!!\`-prefixed (the prefix implies device-path semantics where case matters).

### `ERRMAPNG` — error codes for common open failures deviate from POSIX

Location: `directory_handle.ipp:122-126, 172-178`, plus `ntkernel-table.ipp`.

- Opening a path that is a regular file: NT branch returns `STATUS_NOT_A_DIRECTORY`
  (0xC0000103), mapped to `EINVAL`/`errc::invalid_argument`; POSIX `open(O_DIRECTORY)`
  returns `ENOTDIR`/`errc::not_a_directory`. User code testing `errc::not_a_directory`
  silently fails to match on Windows.
- `creation::truncate_existing` returns `errc::is_a_directory` even when the path does
  not exist, whereas POSIX `open(O_TRUNC|O_DIRECTORY)` on a missing path returns
  `ENOENT` (both backends share this shortcut, but it still violates the documented
  `\errors` contract "Any of the values POSIX open() or CreateFile() can return").
- `creation::always_new` collision returns `errc::directory_not_empty` (see `ALWNEWDD`).

### `GLBMATCH` — kernel-side glob matching deviates from POSIX: always case-insensitive, matches 8.3 short names, leaf-only

Location: `directory_handle.ipp:312-318, 344, 401` (glob passed to `NtQueryDirectoryFile`).

Confirmed against ReactOS sources: fastfat upcases the pattern
(`RtlUpcaseUnicodeString`, or the a-z uppercase loop in `FatQueryDirectory`) and matches
case-insensitively; NTFS honours `SL_CASE_SENSITIVE` on the query IRP, a flag which the
classic `NtQueryDirectoryFile` API can never set (only `NtQueryDirectoryFileEx` can) —
so on NTFS the query is always case-insensitive as well, regardless of how the
directory handle itself was opened (case-sensitive NT branch of `directory()`).
Additionally, fastfat's `FatLocateDirent` matches the query expression against both the
long name and the 8.3 short name (`FileNameDos`), so a glob such as `*.htm` can match
`foobar.html` (via its `FOOBAR~1.HTM` short name) on FAT/NTFS with 8.3 generation
enabled; and `\`-containing globs match nothing (names are leaf-only).

Consequences: a glob such as `*.TXT` matches `foo.txt` on Windows but not on POSIX
where `traverse()`-style user-side filtering via `fnmatch` is case-sensitive; portable
code cannot rely on glob case or short-name semantics. Document this.

---

## Low

### `SNAPSHOT` — `buffers_type::is_snapshot()` can overclaim at the two-syscall boundary

Location: `directory_handle.ipp:406-433` (the `STATUS_NO_MORE_FILES` path).

The snapshot contract (`directory_handle.hpp:409-411`) promises "a **snapshot**, without
races" unless `flags::permit_racy_reads` is set. The implementation marks
`_snapshot = false` only when `count > 1` (three or more `NtQueryDirectoryFile` calls).

When `count == 1` (the first pass-2 call returned all of the data and the second call
returns `STATUS_NO_MORE_FILES`), `_snapshot` stays `true`. Note: a kernel *under*-fill
with more entries remaining always produces `count >= 2` (the data spans more than one
call), so the overclaim is confined to the narrow race where an entry is added between
the data call and the confirming `NO_MORE_FILES` call — the enumeration then misses that
entry yet reports `done() == true` and `is_snapshot() == true`. The documented contract
("an enumeration which fits into a single call is still an atomic snapshot, one which
does not reports `is_snapshot()` false") slightly overclaims for this boundary case.

### `INOFETCH` — `st_dev()`/`st_ino()` are 0 after `directory()` until a lazy fetch

Location: `directory_handle.ipp:44` (`directory_handle(native_handle_type(), 0, 0, flags)`)
and `215` (`reopen`).

The POSIX backend eagerly fills `_devid`/`_inode` at open time (unless
`flag::disable_safety_unlinks` is set, in which case the flag is cleared if the fetch
fails — `posix/directory_handle.ipp:170-177`). Windows never fetches them, so
`unique_id()`, `st_dev()`, `st_ino()` return `(0,0)` until something calls
`fs_handle::parent_path_handle()`/`_fetch_inode()` lazily. Code that compares directory
handle identities right after `directory()` behaves differently per backend. Either fetch
at open time like POSIX, or document the laziness.

### `BKPNTINT` — NT branch never requests `FILE_OPEN_FOR_BACKUP_INTENT`

Location: `directory_handle.ipp:115` (NtCreateFile call).

The Win32 branch gets backup semantics via `FILE_FLAG_BACKUP_SEMANTICS` →
`FILE_OPEN_FOR_BACKUP_INTENT` in `CreateFileW_` (`import.hpp:2291-2312`), but the NT branch
passes no equivalent, so opening a directory the caller may traverse but not list (or
opening with restricted privileges on some network shares) can fail in the NT branch while
succeeding in the Win32 branch. Add `FILE_OPEN_FOR_BACKUP_INTENT` (0x40) to `ntflags` in
the NT branch for consistency.

### `ALIGNKBF` — user-supplied `kernelbuffer` alignment requirement is undocumented

Location: `directory_handle.ipp:391-393`, and the pass 1 stack buffer at line 341.

`FILE_ID_FULL_DIR_INFORMATION` contains `LARGE_INTEGER` fields requiring 8-byte
alignment of the buffer. ReactOS confirms the filesystem drivers store these fields
directly (`fastfat/dirctrl.c` writes `IdFullDirInfo->FileId.QuadPart`; NTFS likewise),
while the I/O manager's user-mode probe (`iofunc.c` `ProbeForWrite(..., sizeof(ULONG))`)
only checks 4-byte alignment — so on ARM64 an 8-byte-misaligned buffer can fault in the
driver. LLFIO's internally allocated kernel buffer (`operator new[]`) is `max_align_t`
aligned and safe; a user-supplied `span<char>` kernelbuffer is not checked. Additionally,
the pass 1 stack buffer is a `char[65536 + ...]` (alignment 1) cast to
`FILE_NAMES_INFORMATION *`, whose `ULONG` members the drivers write directly — fine on
x86/x64 (and in practice on ARM64 stacks which are usually 16-byte aligned, but not
guaranteed by the C++ standard). The `io_request` documentation
(`directory_handle.hpp:212-227, 228-247`) does not mention any of this. Document the
requirement, or align/validate user buffers before use.

### `STACKBUF` — 64 KiB stack allocation inside `read()`

Location: `directory_handle.ipp:337` (`char _buffer[65536 + sizeof(FILE_NAMES_INFORMATION)]`).

A 64 KiB+ frame is fine on a default 1 MiB thread stack but can overflow deliberately
small user-created thread stacks (e.g. 64 KiB or 128 KiB), and `read()` may be called from
any thread. Consider a heap buffer or a smaller first-pass buffer (the kernel will
iterate), or document the stack requirement.

### `UNLNKOLD` — `unlink()` on pre-1709 Windows defers deletion and reports success

Location: `directory_handle.ipp:275-284` (delegation to `fs_handle::unlink`,
`fs_handle.ipp:288-405`).

When `FILE_DISPOSITION_INFORMATION_EX` with `FILE_DISPOSITION_POSIX_SEMANTICS` is
unsupported (Windows 10 pre-1709, non-NTFS), `fs_handle::unlink` falls back to marking the
directory delete-on-close on the duplicated handle. The deletion then happens only when
the *last* handle closes — which includes the caller's still-open `directory_handle` —
yet `unlink()` returns success. On POSIX, `unlink()` removes the directory immediately.
The duplicated-handle dance also means `directory_handle::unlink` performs two
duplications (once in `directory_handle.ipp:282`, again inside `fs_handle::unlink` for
directories). Consider documenting the deferred semantics on old Windows or returning a
distinct error when immediate deletion is impossible.

### `UNCMAXLN` — `UNICODE_STRING` sizes are fabricated and can truncate

Location: `directory_handle.ipp:96` and `316-317`.

`_path.MaximumLength = (_path.Length = size*2) + 2` is set on a
`not_zero_terminated_rendered_path` that does not guarantee a NUL terminator (or spare
capacity), so `MaximumLength` overstates the actual allocation; harmless today because the
kernel only reads `Length` bytes for `ObjectName`, but it is a latent lie. Separately,
`static_cast<USHORT>(size * 2)` silently truncates paths longer than 32 767 wchar_t (and
`MaximumLength` wraps to 0 at the maximum path length), producing a corrupted open request
rather than an error. Guard the length or check for overflow.

### `MODUNCHG` — `directory()` rejects `mode::unchanged`; POSIX accepts it

Location: `directory_handle.ipp:54` via `access_mask_from_handle_mode`
(`import.hpp:1922-1923` returns `errc::invalid_argument`).

The POSIX backend treats `mode::unchanged` as a no-op (it "can be called by reopen()").
`directory()` is not reopen, but portable code passing `mode::unchanged` through from a
wrapper gets `invalid_argument` on Windows and a working open on POSIX. Minor, but the
two backends should agree (or the docs should forbid it).

### `DUPBEHAV` — duplicated delete-privs handle's `behaviour` can misrepresent actual access

Location: `directory_handle.ipp:234-261` (`duplicate_handle_with_delete_privs`).

The returned `file_handle` copies the *original* handle's `behaviour` while the duplicated
kernel handle actually carries `GENERIC_READ|SYNCHRONIZE|DELETE`; e.g. a directory opened
with `mode::none` yields a duplicate that reports `is_readable() == false` despite
granting read access (and `relink`/`unlink` then rely on `fs_handle`'s own
`is_directory()`-driven duplication logic). Cosmetic today, but the `behaviour` should
reflect the new access mask (and the duplicate could request only
`SYNCHRONIZE|DELETE`, matching `fs_handle.ipp:298-318`).

### `ROSTNTFS` — ReactOS NTFS lacks `FileIdFullDirectoryInformation` and has other query gaps (informational)

Location: pass 2's `what_to_enumerate_type` (`FILE_ID_FULL_DIR_INFORMATION`).

ReactOS `ntfs/dirctl.c:NtfsQueryDirectory` supports only `FileNamesInformation`,
`FileDirectoryInformation`, `FileFullDirectoryInformation` and
`FileBothDirectoryInformation` — `FileIdFullDirectoryInformation` (LLFIO's pass 2 class)
returns `STATUS_INVALID_INFO_CLASS`, so the full-info path fails on ReactOS NTFS (the
ReactOS fastfat driver supports it, so FAT volumes are fine). ReactOS NTFS additionally
returns `STATUS_NOT_IMPLEMENTED` for compressed directories. None of these affect
Windows, whose NTFS supports all of LLFIO's classes; noted so that any future ReactOS
based CI/runtime validation does not misreport LLFIO defects.

### `SGLHDRPR` — generated single-header/ABI headers not regenerated

Location: `single-header/llfio.hpp`, `single-header/abi.hpp`.

The repository ships pre-generated `single-header/llfio.hpp`/`abi.hpp`; the
`directory_handle.hpp`/`read()` changes are not reflected in them, so any consumer of the
checked-in single header silently misses the fixes (and would also miss any future
`enumeration_flags`-style API addition). Regenerate the single header as part of any
change to `directory_handle.hpp` or its implementation.

---

## Considered and dismissed (checked, found correct)

Validated against the ReactOS sources (`ntoskrnl/io/iomgr/iofunc.c`,
`drivers/filesystems/fastfat/dirctrl.c`, `drivers/filesystems/ntfs/dirctl.c`):

- **No 64 KiB kernel limit on directory queries.** The I/O manager probes and forwards
  the full caller `Length`; fastfat/NTFS fill entries until the next entry does not fit
  and return `STATUS_SUCCESS` with the used byte count in `IoStatus.Information`. The
  64 KiB in `read()` is purely a pass-1 stack-frame choice (documented in the code).
- **`STATUS_BUFFER_OVERFLOW` semantics.** Both drivers return it only when even one
  entry cannot fit (fastfat copies as much of the name as fits on the *first* record and
  returns `BUFFER_OVERFLOW`; NTFS writes nothing and returns `BUFFER_OVERFLOW`); any
  partial multi-entry fill returns `STATUS_SUCCESS`. In pass 2 it can only mean the
  directory grew since pass 1 sized the buffer — consistent with the `read()` handling.
- **`STATUS_NO_MORE_FILES` implies `IoStatus.Information == 0`.** fastfat leaves
  `Information` at 0 when no entry was written; NTFS sets it to `Written` (0). The
  `assert(isb.Information == 0)` on the `count <= 1` NO_MORE_FILES path is valid.
- **Per-handle serialisation is required.** NTFS caches the search pattern and scan
  position in the CCB (`Ccb->DirectorySearchPattern`, `Ccb->Entry`) and fastfat caches
  the query template in the CCB, updated under an exclusive FCB lock — so concurrent
  queries on one handle with different patterns/positions corrupt each other. This
  validates serialising `read()` per handle, with the lock held across retries.
- **`RestartScan`/`ReturnSingleEntry` usage.** LLFIO passes `RestartScan` only on the
  first call of each pass (`first`, `count == 0`) and `ReturnSingleEntry = FALSE`;
  drivers restart from position 0 on `SL_RESTART_SCAN` and continue from the cached
  position otherwise. Correct.
- **NULL/empty pattern means match-all** in both drivers; LLFIO passes a NULL `FileName`
  when no glob is supplied. Correct.
- **`FILE_NAMES_INFORMATION` (pass 1) and `FILE_DIRECTORY_INFORMATION`
  (less-info mode)** are supported by both drivers. Correct.
- **`STATUS_PENDING`** is a legitimate async result (fastfat `FatFsdPostRequest`, NTFS
  `NtfsMarkIrpContextForQueue`); LLFIO's `ntwait` handling is correct (only the ignored
  deadline is an issue, `DEADLNDR`).
- **`.` and `..` are never returned** by the drivers; LLFIO's defensive skip is harmless.
- **The `IoStatus.Information`-based sizing (`isb.Information > bytes - 1024` heuristic)**
  is coherent with drivers filling until the next entry does not fit: a first pass-2 call
  that fills the buffer to the top strongly implies more entries (or change).
- The per-entry `(FileNameLength + 7) & ~7` pass-1 sizing matches the drivers' `QuadAlign`
  record padding (fastfat `NextEntry += QuadAlign(BaseLength + BytesConverted)`). Correct.

Other:

- `unlink_on_first_close` rejection in `directory()` matches the POSIX backend and
  `close()`'s fallback path (`directory_handle.hpp:362-377`).
- The two-pass buffer size estimate (`+1024` slop and per-entry
  `sizeof + (len+7)&~7` overestimates by up to 8 bytes/entry) is always an over-allocation
  — safe.
- The leafname zero-termination write (`ffdi->FileName[length] = 0`) is bounds-safe: the
  guard checks against `NextEntryOffset`, and the last entry in a buffer is never
  zero-terminated.
- `entries_parsed >= req.buffers.size()` guard cannot overflow the span (the `!done`
  condition prevents writing the entry that would overflow).
- `stat_t::want` metadata claims match what is filled for both the
  `FILE_ID_FULL_DIR_INFORMATION` and `LLFIO_DIRECTORY_HANDLE_ENUMERATE_LESS_INFO`
  (`FILE_DIRECTORY_INFORMATION`) variants; `st_ino` is correctly omitted in the less-info
  variant, and `to_st_type` honours `LLFIO_USE_LEGACY_FILESYSTEM_SEMANTICS`.
- `\!!\` stripping (4-char prefix → 3 wchar adjustment) is correct for the LLFIO-only
  prefix; `\??\` paths are valid NT paths and correctly pass through unmodified.
- Empty-path-with-valid-base opens the base itself, matching the POSIX `path = "."`
  special case.
- `flag::multiplexable` handles pend correctly via `ntwait` on the file object (the
  standard LLFIO pattern) — the only issue is the ignored deadline (`DEADLNDR`).
- `clone_to_path_handle()` correctly avoids double-close on the `DuplicateHandle` failure
  path and matches POSIX behaviour.
- The `filter::fastdeleted` skip does not corrupt `entries_parsed` or the buffer walk.
