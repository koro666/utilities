Utilities
=========

Random small utilities that don't warrant their own Git repository.

Build
-----

Build using `make`, or `make STATIC=1` to make `musl` static binaries.

Description
-----------

- `hexx`: generates hex dumps in the right format.
- `iphm`: takes IPv4 addresses/ranges on stdin and outputs an heatmap on stdout in PPM format; similar to [xkcd](https://xkcd.com/195/) with a slightly different order.
- `setlogcons`: lifted from [busybox](https://git.busybox.net/busybox/tree/console-tools/setlogcons.c) and rewritten to build standalone.
- `sleepuntil`: sleeps until a defined time, up to 24 hours in the future.
- `takeover`: allows taking ownership of arbitrary files by passing the file descriptor via a UNIX socket to an elevated server; server sets the file owner to the caller's UID after some security checks.
- `tsvstat`: outputs a `.tsv` on stdout with the `stat(2)` information from all the files in the directories passed as arguments, recursively.
- `uidmapshift`: lifted from [nsexec](https://bazaar.launchpad.net/~serge-hallyn/+junk/nsexec/view/head:/uidmapshift.c) but fixed to actually work and be more verbose on errors.
- `vipcheck`: checks if binary files passed as arguments end with the bytes `\n[0-9]+^`; prints matching names.
