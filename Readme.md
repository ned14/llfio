This is the post-peer-review AFIO v2 rewrite. You can view its documentation at https://ned14.github.io/afio/

<b>master branch test status</b> Linux & OS X: [![Build Status](https://travis-ci.org/ned14/afio.svg?branch=master)](https://travis-ci.org/ned14/afio) Windows: [![Build status](https://ci.appveyor.com/api/projects/status/ox59o2r276xbmef7/branch/master?svg=true)](https://ci.appveyor.com/project/ned14/afio/branch/master) <b>CMake dashboard</b>: http://my.cdash.org/index.php?project=Boost.AFIO

Tarballs of source and prebuilt binaries for Linux x64, MacOS x64 and Windows x64:
- https://dedi4.nedprod.com/static/files/afio-v2.0-source-latest.tar.xz
- https://dedi4.nedprod.com/static/files/afio-v2.0-binaries-linux64-latest.tgz
- https://dedi4.nedprod.com/static/files/afio-v2.0-binaries-darwin64-latest.tgz
- https://dedi4.nedprod.com/static/files/afio-v2.0-binaries-win64-latest.zip


### Immediate todos in order of priority:
- [x] Implement new custom C++ exception synthesis support from Outcome.
- [x] Finish trivial vector, which is unfinished and currently segfaults.
- [x] Run clang-tidy fix pass, it's got a bit untidy recently.
- [ ] Add OS X support to `storage_profile`, this should complete the Mac OS port.
- [x] Fix all known bugs in Outcome, plus reorg source code in prep for `status_code`.
- [x] Scatter-gather buffers to use https://github.com/martinmoene/byte-lite
- [ ] Implement SG14 `status_code` as a standalone library and test in AFIO.
- [ ] Single include generation now we're on `status_code` and it's safe.
- [ ] `atomic_append` isn't actually being tested in shared_fs_mutex
- [ ] Implement a non-toy ACID key-value BLOB store and send it to Boost for peer review.
  - [ ] For this need to implement a file-based B+ tree. And for that, need to
  implement a page allocator out of a single file. Some notes:
  
  B+ trees would be based on the 4Kb page for memory mapping, and thus allocate and release whole 4Kb pages.

  A simple page allocator from a file might simply keep some magic at the top, and then a list of offsets to free pages for the remainder of the page. That might be (4096 – 12) / 4 = 1021 slots (remember these are 42 bit offsets, so simply limit to 32 bit offsets << 12).
  
  Each free page links to its next free page. Freeing a page means modulus its address >> 12 with 1021, and CASing it into its “slot” as its linked list.

  Allocating pages involves iterating a round robin index, pulling free pages off the top. Only if no free pages remain do we atomic append 1021 * 4096 = 4,182,016 bytes and refill the free page index.
- [ ] All time based kernel tests need to use soak test based API and auto adjust to
valgrind.
- [ ] In DEBUG builds, have io_handle always not fill buffers passed to remind
people to use pointers returned!
- KernelTest needs to be generating each test kernel as a standalone DLL so it can be
fuzzed, coverage calculated, bloat calculated, ABI dumped etc
  - Easy coverage is the usual gcov route => coveralls.io or gcovr http://gcovr.com/guide.html


### clang AST parser based todos which await me getting back into the clang AST parser:
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
- [ ] Output into YAML comparable hashes for OS + device + FS + flags
so we can merge partial results for some combo into the results database.
- [ ] Write YAML parsing tool which merges fs_probe_results.yaml into
the results directory where flags and OS get its own directory and each YAML file
is named FS + device e.g.


### Algorithms library `AFIO_V2_NAMESPACE::algorithm` todo:
- [ ] Add `vector<T>` which adapts a `mapped_view<T>`.
- [ ] Add some primitive which intelligently copies/moves between views and vectors.
Specifically, if resizing, if type is trivially copyable, skip memory copying during
resize via remapping.
- [ ] Add an intelligent on demand memory mapper:
  - Use one-two-three level page system, so 4Kb/2Mb/1Gb. Files under 2Mb need just
one indirection.
  - Page tables need to also live in a potentially mapped file
  - Could speculatively map 4Kb chunks lazily and keep an internal map of 4Kb
offsets to map. This allows more optimal handing of growing files.
  - WOULD BE NICE: Copy on Write support which collates a list of dirtied 4Kb
pages and could write those out as a delta.
    - Perhaps Snappy compression could be useful? It is continuable from a base
if you dump out the dictionary i.e. 1Mb data compressed, then add 4Kb delta, you can
compress the additional 4Kb very quickly using the dictionary from the 1Mb.
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


### Eventual transactional key-blob store:
- What's the least possible complex implementation based on files and directories?
  - `store/index` is 48 bit counter + r/w mutex + open hash map of 128 bit key to blob
  identifier (64 bits). Blob identifier is top 6 bits file id:
    - 0 means large file, values 1-15 are reserved for future use (large file deltas).
    - Values 16-63 are the smallfile. 
  
    1. `store/small/01-3f` for blobs <= 65512 bytes (8 bytes tail, 16 bytes key).
    Each blob is padded to 64 byte
    multiple and tail record with 16 bytes of key, 6 byte (48 bit) counter + 2 byte size aligned
    at end + optional 16 byte hash. There are 48 of these used to maximise write concurrency
    (use the thread id to select a smallfile on open, use exclusive lock probing to
    figure out a small file not in use, hold shared lock on chosen small file until exit).

    Remaining 58 bits of blob identifier is the offset into the smallfile of the end of
    the tail record (shifted left 6 bits, all records in smallfiles are at 64 byte multiples).
  
    2. `store/large/*` for blobs > 65512 under the assumption that one day we'll
    implement 4Kb dirty delta page with compression support.
      - `store/large/hexkey/48bithexcounter` stores each blob
      - Last 64 bytes contains magic, size, optional hash.
      
    Blob identifier is top 6 bits zero. Next 10 bits is 4 bits mantissa shifted left
    6 bits of shift (0-63) for approx size. Remaining 48 bits is counter.
    
  - `store/config` keeps:
    - transactions enabled or not.
    - mmap enable or not (i.e. can be used over network drive)
    - content hash used e.g. SpookyHash
    - compression used e.g. pithy
    - dirty flag i.e. do fsck on next first open
      - `O_SYNC` was on or not last open (affects severity of any fsck).
    - shared lock kept on config so we know last user exit/first user enter
- Start a transaction = atomic increment current 48 bit counter
  - Write the changes making up this transaction under this counter
  - When ready, lock the open hash map's r/w mutex for exclusive access.
  - Check that all the keys we are modifying have not been changed since the
  transaction began.
  - If all good, update all the keys with their new values and unlock the r/w mutex
  QUESTION: Can forcing all map users to lock the mutex each access be avoided?
  e.g. atomic swapping in a pointer to an updated table? One could COW the 4Kb pages
  being changed in the current table, then update the map, then swap pointers
  and leave the old table hang around for a while.
- Garbage collecting in this design is easy enough, write all current small values
into a single file, then update the map in a single shot, then hole punch all the
other small files.
- Live resizing the open hash map I think is impossible however unless we use that
atomic swapping design.
- You may need compression, https://github.com/johnezang/pithy looks easily convertible
into header-only C++ and has a snappy-like performance to compression ratio. Make sure
you merge the bug fixes from the forks first.

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
