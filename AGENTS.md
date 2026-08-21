# Agentic coding guidelines

1. All source and header files MUST be kept compatible with the 2017 ISO
C++ standard.
2. Run `clang-format` on every changed header and source file. Do NOT run
`clang-format` on cmake files.
3. When building and testing, extract what to do for the current platform
from `.github/workflows/ci.yml`.
4. Polyfills, usually from `quickcpplib`, are used to substitute in features
from newer C++ standards. Make sure when using those to also use the
polyfill in the same way as existing code in this library.
5. Never, EVER use sleeps alone to synchronise between threads. These
cause flaky tests. ALWAYS use a proper synchronisation between threads;
sleeps within proper synchronisation are permitted.
6. When a defect or work item tracked in `plans/` is fixed, update the
document to remove that item. By remove, I mean all mention of it in the
plan file.
7. For every public API, if its doyxgen API documentation comment contains
`THREADSAFE`, that means that function must be thread-safe. Analyse the
implementation of those APIs for every possible cause of thread-unsafety.
Be exhaustive and report your findings as severe bugs.
9. NULL inputs to public APIs causing an immediate crash SO LONG as no
data gets unexpectedly mutated or causing a potential security
vulnerability is OK - we WANT to fail fast if users supply a NULL
to an argument which is mandatory.
10. Never, EVER run `git commit` by yourself.
11. If not running on Windows, try to test the Windows only code using
`wine`. If you need to examine the
source code for Windows, consider examining the source code for Reactos
(https://github.com/reactos/reactos) which is a binary compatible
reproduction of Windows.
11a. Do NOT run the `shared_fs_mutex` tests under wine: they hang or fail
unreliably in that environment. Skip them when running the wine test
suite.
11b. Under wine, ignore failures in the noisy `utils` tests that measure
cpu usage, timings and the like (e.g. `current_process_cpu_usage`): they
are unreliable in that environment.
12. If not running on Linux, try to test the Linux only code using
docker or another VM. Similarly for Mac OS only code, and FreeBSD only
code, if VMs for those are available on the local system.
13. If you need more data about why CI failed, https://my.cdash.org/index.php?project=Boost.AFIO
can be useful to consult.
