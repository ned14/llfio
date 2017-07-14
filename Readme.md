This is the beginnings of the post-peer-review AFIO
v2 rewrite. You can view its documentation at https://ned14.github.io/boost.afio/

<b>master branch test status</b> Linux: [![Build Status](https://travis-ci.org/ned14/boost.afio.svg?branch=master)](https://travis-ci.org/ned14/boost.afio) Windows: [![Build status](https://ci.appveyor.com/api/projects/status/ox59o2r276xbmef7/branch/master?svg=true)](https://ci.appveyor.com/project/ned14/boost-afio/branch/master) Coverage: Boost.KernelTest support for coveralls.io still todo <b>CMake dashboard</b>: http://my.cdash.org/index.php?project=AFIO

Tarballs of source and prebuilt binaries for Linux x64 and Windows x64: http://my.cdash.org/index.php?project=AFIO (click on the little package icon to the right of the Build Name, obviously choose one passing all tests)


### Immediate todos in order of priority:
- [x] Audit Outcome and AFIO for all uses of `std::error_code::value()` and replace where
appropriate with `std::errc::whatever`.
- [x] Get Outcome to work perfectly with exceptions and RTTI disabled, this makes
Outcome useful in the games/audio world.
  - [x] Add a new Boost.Test emulation, one which is noexcept capable
  - [ ] Move AFIO to being tested with exceptions and RTTI disabled. Where AFIO 
throws, have it detect __cpp_exceptions and skip those implementations.
  - [x] Add macro helpers to Outcome for returning outcomes out of things
which cannot return values like constructors, and convert said exceptions/TLS
back into outcomes.
   - Make use of std::system_error(errno, system_category, "custom error message");
- [x] Port AFIO v2 back to POSIX
- [ ] Move handle caching into native_handle_type? Overlapped flag is especially needed.
- [ ] Need to split out the path functions from io_handle into a pathed_handle
  - [ ] directory_handle extends pathed_handle
  - [ ] symlink_handle extends pathed_handle
- [ ] Fat `afio::path`
  - [ ] Relative path fragment + a pathed_handle as the base
  - [ ] POSIX, NT kernel and win32 path variants
- [ ] `virtual handle::path_type pathed_handle::path(bool refresh=false)` should be added using
`GetFinalPathNameByHandle(FILE_NAME_OPENED)`. `VOLUME_NAME_DOS` vs `VOLUME_NAME_NT` should
depend on the current afio::path setting.
- [ ] Once the fat path is implemented, implement the long planned ACID key-value BLOB store
with a very simple engine based on atomic renames and send it to Boost for peer review.

- [ ] All time based kernel tests need to use soak test based API and auto adjust to
valgrind.
- [ ] Raise the sanitisers on per-commit CI via ctest.
- [ ] Rename all ParseProjectVersionFromHpp etc to parse_project_version_from_hpp etc
- [ ] In DEBUG builds, have io_handle always not fill buffers passed to remind
people to use pointers returned!
- [ ] DLL library edition appears to not be encoding extended error code detail because
it's not sharing a single ringbuffer_log. Hard to fix given Outcome could be being
used by multiple libraries as a header only library, need to figure out some global
fix e.g. named shared memory. Make log disc stored while we are at it.
- KernelTest needs to be generating each test kernel as a standalone DLL so it can be
fuzzed, coverage calculated, bloat calculated, ABI dumped etc
  - Easy coverage is the usual gcov route => coveralls.io or gcovr http://gcovr.com/guide.html
- [ ] Single include generation
- [x] Make updating revision.hpp updated by the pre-commit git hook
- [ ] Add missing functions on handle/file_handle from AFIO v1


### clang AST parser based todos which await me getting back into the clang AST parser:
- [ ] Implement [[bindlib::make_free]] which injects member functions into the enclosing
namespace.
- [ ] C bindings for all AFIO v2 APIs. Write libclang parser which autogenerates
SWIG interface files from the .hpp files using custom attributes to fill in the
missing gaps.
- Much better coverage is to permute the *valid* parameter inputs of the kernel
  deduced from examining the kernel parameter API and
  figure out what minimum set of calling parameters will induce execution of the
  entire potential call graph. This approach is a poor man's symbolic execution
  SMT solver, it uses brute force rather than constrained solution. Idea is that
  when someone releases a proper C++ capable KLEE for open source use we can use
  that instead.
    - `-fsanitize-coverage=trace-pc -O3` will have clang (works also on winclang)
    call a user supplied `__sanitizer_cov_trace_pc()` and `__sanitizer_cov_trace_pc_indirect(void *callee)`
    function. This would simply atomically append the delta of the program counter
    from the previously stored one to a memory mapped file. You should use a tagged
    encoding, so leading byte specifies length and type of record.
    - One also needs to replace syscalls with stubs (easy on Windows DLLs) and
    permute their potential return codes and effects to maximise edge execution.
    Either monkey patching or a custom DLL loader would work.
  - One then generates a default KernelTest permutation set for that kernel
  which can be freshened (probably via commenting out stale entries and appending
  new entries) by an automated tooling run
  - A freebie feature from this work is an automatic possible error returns
  calculator for the documentation
  - Another freebie feature is automatic calculation of the number of malloc + free
  performed.
  - Can we even figure out O type complexities from the call graph? e.g. doubling
  an input parameter quadruples edges executed? Could generate an O() complexity
  per input parameter for the documentation?



### Known bugs and problems:
- [ ] algorithm::atomic_append needs improvements:
  - Trap if append exceeds 2^63 and do something useful with that
  - Fix the known inefficiencies in the implementation:
    - We should use byte range locks when concurrency*entities is low
    - We have an extra read() during locking between the scanning for our lock
    request and scanning for other lock requests
    - During scan when hashes mismatch we reload a suboptimal range
    - We should use memory maps until a SMB/NFS/QNX/OpenBSD lock_request comes
    in whereafter we should degrade to normal i/o gracefully
- [ ] Add native BSD kqueues to POSIX AIO backend as is vastly more efficient.
  - http://www.informit.com/articles/article.aspx?p=607373&seqNum=4 is a
very useful programming guide for POSIX AIO.
- [ ] Port to Linux KAIO
  - http://linux.die.net/man/2/io_getevents would be in the run() loop.
pthread_sigqueue() can be used by post() to cause aio_suspend() to break
early to run user supplied functions.
- [ ] Don't run the cpu and sys tests if cpu and sys ids already in fs_probe_results.yaml
  - Need to uniquely fingerprint a machine somehow?
- [ ] Add monitoring of CPU usage to tests. See GetThreadTimes. Make sure
worker thread times are added into results.
- [ ] Configurable tracking of op latency and throughput (bytes) for all
handles on some storage i.e. storage needs to be kept in a global map.
  - Something which strongly resembles the memory bandwidth test
  - [ ] Should have decile bucketing e.g. percentage in bottom 10%, percentage
  in next 10% etc. Plus mean and stddev.
  - [ ] Should either be resettable or subtractable i.e. points can be diffed.
  - [ ] Add IOPS QD=1..N storage profile test
  - [ ] Add throughput storage profile test
- [ ] Output into YAML comparable hashes for OS + device + FS + flags
so we can merge partial results for some combo into the results database.
- [ ] Write YAML parsing tool which merges fs_probe_results.yaml into
the results directory where flags and OS get its own directory and each YAML file
is named FS + device e.g.
  - results/win64 direct=1 sync=0/NTFS + WDC WD30EFRX-68EUZN0


### Algorithms library `AFIO_V2_NAMESPACE::algorithm` todo:
- [ ] Add an intelligent on demand memory mapper:
  - Use one-two-three level page system, so 4Kb/2Mb/?. Files under 2Mb need just
one indirection.
  - Page tables need to also live in a potentially mapped file
  - Could speculatively map 4Kb chunks lazily and keep an internal map of 4Kb
offsets to map. This allows more optimal handing of growing files.
  - WOULD BE NICE: Copy on Write support which collates a list of dirtied 4Kb
pages and could write those out as a delta.
    - LATER: Use guard pages to toggle dirty flag per initial COW
- [ ] Store in EA or a file called .spookyhashes or .spookyhash the 128 bit hash of
a file and the time it was calculated. This can save lots of hashing work later.
- [ ] Correct directory hierarchy delete
  i.e.:
  - Delete files first tips to trunk, retrying for some given timeout. If fail to
  immediately delete, rename to base directory under a long random hex name, try
  to delete again.
  - Only after all files have been deleted, delete directories. If new files appear
  during directory deletion, loop.
  - Options:
    - Rename base directory(ies) to something random to atomically prevent lookup.
    - Delete all or nothing (i.e. rename all files into another tree checking
    permissions etc, if successful only then delete)
- [ ] Correct directory hierarchy copy
  - Optional backup semantics (i.e. copy all ACLs, metadata etc)
  - Intelligent retry for some given timeout before failing.
  - Optional fixup of any symbolic links pointing into copied tree.
  - Optional copy directory structure but hard or symbolic linking files.
    - Symbolic links optionally are always absolute paths instead of relative.
  - Optional deference all hard links and/or symbolic links into real files.
- [ ] Correct directory hierarchy move
- [ ] Correct directory hierarchy update (i.e. changes only)
- [ ] Make directory tree C by cloning tree B to tree B, and then updating tree C
with changes from tree A. The idea is for an incremental backup of changes over
time but saving storage where possible.
- [ ] Replace all content (including EA) duplicate files in a tree with hardlinks.
- [ ] Figure out all hard linked file entries for some inode.
- [ ] Generate list of all hard linked files in a tree (i.e. refcount>1) and which
are the same inode.


## Commits and tags in this git repository can be verified using:
<pre>
-----BEGIN PGP PUBLIC KEY BLOCK-----
Version: GnuPG v2

mDMEVvMacRYJKwYBBAHaRw8BAQdAp+Qn6djfxWQYtAEvDmv4feVmGALEQH/pYpBC
llaXNQe0WE5pYWxsIERvdWdsYXMgKHMgW3VuZGVyc2NvcmVdIHNvdXJjZWZvcmdl
IHthdH0gbmVkcHJvZCBbZG90XSBjb20pIDxzcGFtdHJhcEBuZWRwcm9kLmNvbT6I
eQQTFggAIQUCVvMacQIbAwULCQgHAgYVCAkKCwIEFgIDAQIeAQIXgAAKCRCELDV4
Zvkgx4vwAP9gxeQUsp7ARMFGxfbR0xPf6fRbH+miMUg2e7rYNuHtLQD9EUoR32We
V8SjvX4r/deKniWctvCi5JccgfUwXkVzFAk=
=puFk
-----END PGP PUBLIC KEY BLOCK-----
</pre>
