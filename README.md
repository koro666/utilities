Utilities
=========

Random small utilities that don't warrant their own Git repository.

Build
-----

Build using `make`, or `make STATIC=1` to make `musl` static binaries.

Description
-----------

- `sleepuntil`: sleeps until a defined time, up to 24 hours in the future.
- `vipcheck`: checks if binary files passed as arguments end with the bytes `\n[0-9]+^`; prints matching names.
