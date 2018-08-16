Herein lies an exploratory toy ACID key-value store written using
LLFIO which lets you look up any BLOB value from some 128-bit key, and to
update as an atomic transaction up to 65,535 key-values at once.

It is purely to test the feasibility of one approach to implementing such
a store, and to test LLFIO's design. Nobody should use this store for
anything serious.

## Todo:
- [x] Add sparse file creation on Windows to LLFIO and see how the
benchmarks fare.
- [x] Add key-value deletion.
- [x] Atomic append should issue gather buffers of `IOV_MAX`
- [x] Optionally use mmaps to extend smallfile instead of atomic appends.
Likely highly racy on Linux due to kernel bugs :)
- [x] Use mmaps for all smallfiles
- [ ] Does this toy store actually work with multiple concurrent users?
- [ ] Online free space consolidation (copy early still in use records
into new small file, update index to use new small file)
  - [ ] Per 1Mb free space consolidated, punch hole
- [ ] Need some way of detecting and breaking sudden process exit during
index update.

## Benchmarks:
- 1Kb values Windows with NTFS, no integrity, no durability, read + append:
  ```
  Inserting 1M key-value pairs ...
  Inserted at 195312 items per sec
  Retrieving 1M key-value pairs ...
  Fetched at 612745 items per sec
  ```
- 1Kb values Windows with NTFS, integrity, no durability, read + append:
  ```
  Inserting 1M key-value pairs ...
  Inserted at 188572 items per sec
  Retrieving 1M key-value pairs ...
  Fetched at 542005 items per sec
  ```
- 1Kb values Windows with NTFS, no integrity, no durability, mmaps:
  ```
  Inserting 1M key-value pairs ...
  Inserted at 518403 items per sec
  Retrieving 1M key-value pairs ...
  Fetched at 2192982 items per sec
  ```
- 1Kb values Windows with NTFS, integrity, no durability, mmaps:
  ```
  Inserting 1M key-value pairs ...
  Inserted at 455996 items per sec
  Retrieving 1M key-value pairs ...
  Fetched at 1144164 items per sec
  ```

- 1Kb values Linux with ext4, no integrity, no durability, read + append:
  ```
  Inserting 1M key-value pairs ...
  Inserted at 698324 items per sec
  Retrieving 1M key-value pairs ...
  Fetched at 2544529 items per sec
  ```
- 1Kb values Linux with ext4, integrity, no durability, read + append:
  ```
  Inserting 1M key-value pairs ...
  Inserted at 660501 items per sec
  Retrieving 1M key-value pairs ...
  Fetched at 1934235 items per sec
  ```
- 1Kb values Linux with ext4, no integrity, no durability, mmaps:
  ```
  Inserting 1M key-value pairs ...
  Inserted at 695894 items per sec
  Retrieving 1M key-value pairs ...
  Fetched at 5988023 items per sec
  ```
- 1Kb values Linux with ext4, integrity, no durability, mmaps:
  ```
  Inserting 1M key-value pairs ...
  Inserted at 639795 items per sec
  Retrieving 1M key-value pairs ...
  Fetched at 458715 items per sec
  ```
  
- 1Kb values Windows with NTFS, integrity, durability, mmaps:
  ```
  Inserting 1M key-value pairs ...
  Inserted at 33387 items per sec
  Retrieving 1M key-value pairs ...
  Fetched at 1360544 items per sec
  ```
- 1Kb values Linux with ext4, integrity, durability, mmaps:
  ```
  Inserting 1M key-value pairs ...
  Inserted at 85397 items per sec
  Retrieving 1M key-value pairs ...
  Fetched at 781250 items per sec
  ```

- 16 byte values Windows with NTFS, no integrity, no durability, read + append:
  ```
  Inserting 1M key-value pairs ...
  Inserted at 214178 items per sec
  Retrieving 1M key-value pairs ...
  Fetched at 660066 items per sec
  ```
- 16 byte values Windows with NTFS, no integrity, no durability, mmaps:
  ```
  Inserting 1M key-value pairs ...
  Inserted at 938967 items per sec
  Retrieving 1M key-value pairs ...
  Fetched at 4739336 items per sec
  ```
- 16 byte values Linux with ext4, no integrity, no durability, read + append:
  ```
  Inserting 1M key-value pairs ...
  Inserted at 1400560 items per sec
  Retrieving 1M key-value pairs ...
  Fetched at 3816793 items per sec
  ```
- 16 byte values Linux with ext4, no integrity, no durability, mmaps:
  ```
  Inserting 1M key-value pairs ...
  Inserted at 1663893 items per sec
  Retrieving 1M key-value pairs ...
  Fetched at 26315789 items per sec
  ```
