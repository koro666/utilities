Utilities
=========

Random small utilities that don't warrant their own Git repository.

Build
-----

Build using `make`, or `make STATIC=1` to make `musl` static binaries.

Description
-----------

- `setlogcons`: lifted from [busybox](https://git.busybox.net/busybox/tree/console-tools/setlogcons.c) and rewritten to build standalone.
- `sleepuntil`: sleeps until a defined time, up to 24 hours in the future.
- `uidmapshift`: lifted from [nsexec](https://bazaar.launchpad.net/~serge-hallyn/+junk/nsexec/view/head:/uidmapshift.c) but fixed to actually work and be more verbose on errors.
- `vipcheck`: checks if binary files passed as arguments end with the bytes `\n[0-9]+^`; prints matching names.
