


/*! Instead of placing locks on the file directly, use a shadow file which
allows concurrent Windows and POSIX users on a networked drive to interoperate correctly. On
Windows this makes locks advisory instead of mandatory, hence matching POSIX
semantics. On POSIX this enables non-insane byte range locks, so instead of
all locks being dropped on any handle close as per POSIX requirements, every
byte range lock is kept per thread per handle as on Windows.
*/
constexpr unsigned flag_use_shadow_lock_file = (1 << 2);
/*! Will the *last* close of this handle by any process in the system delete the file?
On Windows this asks the operating system to delete the file on last close which
simply works. On POSIX on open this locks delete_on_close_lock_range() in the file
(or shadow lock file if enabled) for shared access
and tries to upgrade to exclusive on handle close - if successful, it assumes it is
the only read-writer and deletes the file.
*/
constexpr unsigned flag_delete_on_close = (1 << 3);
/*! Will byte range locking be performed around i/o? This passes through operating
system semantics as-is, so on Windows you get per thread per handle byte range locking,
on Linuces since 3.15 you get per handle byte range locking and on all other POSIX
you get per file byte range locking with all locks being dropped on any handle close.
Also specifying flag_use_shadow_lock_file can give you sane byte range lock semantics
on all platforms.

FIXME: Supply feature flags indicating the type of locking this provides via an API
*/
constexpr unsigned flag_lock_around_io = (1 << 4);
