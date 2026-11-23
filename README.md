# du16

A Unix `du`-style disk usage report for 16-bit MS-DOS, built with Open
Watcom and [libc16](https://github.com/gr-dos/libc16). It shows the
disk space used by files and directory trees, totalled by directory. It
complements [df16](https://github.com/gr-dos/df16), which reports each
drive's free and used space.

The resulting `du.exe` is a small-model, real-mode 8086 executable
(about 8 KB on disk and 14 KB of memory) that runs on any DOS from
an original PC or NEC V20 up. It is tested under DOSBox-X and on
GR-DOS (MS-DOS 4.00's kernel), and it only ever reads the disk.

## Usage

    du [OPTION]... [FILE-OR-DIRECTORY]...

| Option | Meaning |
| --- | --- |
| `-a` | also a line for each file, not only for directories |
| `-s` | only one line, the total, for each operand |
| `-c` | a grand total line at the end |
| `-k` | sizes in 1 KiB blocks (the default) |
| `-h` | human-readable sizes (`512B`, `1.5KiB`, `122.0MiB`, ...) |
| `-b` | apparent sizes (file lengths), in bytes |
| `--apparent-size` | count file lengths instead of allocated space |
| `-?`, `--help` | show help |
| `--version` | show version |
| `--` | treat the remaining arguments as operands |

Options are case-insensitive and short ones may be grouped (`-sch`).
`-h` always means human-readable, never help. Of `-k`, `-h` and `-b`,
the last one given sets the unit; `-b` also turns on apparent sizes,
so `-b -k` gives file lengths in KiB. `-a` and `-s` together are an
error.

With no operand, du reports the current directory (`.`). An operand is
a file or a directory, with or without a drive: `C:\DOS`, `A:`,
`D:GAMES`, `..`, `README.TXT`. Wildcards are not expanded (`*` and
`?` are a usage error), and paths use DOS 8.3 names.

### Examples

    A:\TREE> du
            0  .\L1\L2\L3\L4
           13  .\L1\L2\L3
           14  .\L1\L2
           22  .\L1
           46  .\MIXED
           71  .
    C:\> du -a A:\TREE\MIXED
            1  A:\TREE\MIXED\M10.BIN
            1  A:\TREE\MIXED\M100.BIN
            1  A:\TREE\MIXED\M1000.BIN
           10  A:\TREE\MIXED\M10000.BIN
           33  A:\TREE\MIXED\M33333.BIN
           46  A:\TREE\MIXED
    C:\> du -s A:\ D:\
           98  A:\
          416  D:\
    C:\> du -sh -c A:\TREE D:\TREE
      71.0KiB  A:\TREE
     176.0KiB  D:\TREE
     247.0KiB  total
    C:\> du -b -s A:\TREE
        67566  A:\TREE
    C:\> du C:\ | SORT

`A:` is a 360 KB floppy (1 KiB clusters) and `D:` a hard disk with
16 KiB clusters, holding the same files: `A:\TREE` and `D:\TREE` hold
67,566 bytes, which take 71 KiB on the floppy and 176 KiB on the hard
disk.

### Output

One line per directory (and, with `-a`, per file), each after
everything inside it, as Unix du prints them, in the order DOS lists
the directory. Each line is the size, right-aligned in 9 columns, two
spaces and the path. Because sizes are right-aligned, DOS `SORT` puts
the lines in order of size (`du | SORT`). Lines end in CR LF.

Paths start with the operand as it was typed, followed by the names
below it: `du games` prints `games\DOOM`, and `du A:` prints `A:SUB`
(A:'s current directory). With `-c` the last line is the grand total,
`total`.

## What is counted

**Allocated space.** By default a file counts for the space DOS
allocated to it: its length rounded up to a whole number of the
drive's clusters. A zero-length file counts 0, a file of one byte one
cluster, and a file of exactly one cluster one cluster. The cluster
size of each operand's drive comes from DOS (INT 21h AH=36h, sectors
per cluster times bytes per sector), so it is right for each drive:
1 KiB on a 360 KB floppy, 512 bytes on a 1.44 MB one, 2 KiB to 32 KiB
on a hard disk or CF card.

**Apparent size.** With `-b` or `--apparent-size`, a file counts for
its length in bytes instead, as `DIR` shows it.

**Which files.** Every file in the tree, including hidden, system and
read-only ones. Volume labels and the `.` and `..` entries are not
counted, and du never goes into `.` or `..`.

**Directories' own space is not counted.** The clusters that hold a
subdirectory's entries are not included in any total, nor is the root
directory's area. DOS does not report them (a directory's size in its
parent's entry is 0), and du does not read the FAT itself. So du's
total for a whole drive is the space in files: `CHKDSK`'s "bytes in
user files" plus "bytes in hidden files", which the GR-DOS tests check.
It is less than the drive's used space by what `CHKDSK` reports as
"bytes in directories".

**Units.** `-k` sizes are in KiB (1,024 bytes), and `-h` sizes in
binary units (1 KiB = 1,024 bytes); both are rounded up, so nothing
non-empty shows as 0. A total is added up in bytes first and rounded
once, so a directory of two 512-byte files on a 512-byte-cluster
floppy is 1 KiB, not 2.

**Totals are 64-bit**, so they cannot overflow for anything DOS can
store, even over several operands or files of 4 GB.

**Overlapping operands** are each reported in full. The grand total
(`-c`) counts each file once: an operand that is inside another one
(`du -c C:\ C:\DOS`), or the same as an earlier one, is left out of
it. Operands are compared by their full path (`_fullpath`), so `C:\DOS`
and `c:\dos\..\DOS` are the same, but a drive reached through `SUBST`
or `JOIN` is not recognised as the same directory.

## ERRORLEVEL

| ERRORLEVEL | Meaning |
| --- | --- |
| 0 | everything was read |
| 1 | an operand was not found, a drive was invalid or not ready, or part of a tree could not be read (the totals printed leave it out) |
| 2 | usage error, out of memory, or a write error on standard output |

Each problem is reported on standard error, naming the path, and du
goes on with the rest: the other operands, and the rest of the tree. So
a total printed with ERRORLEVEL 1 is a lower bound.

## Limits and safety

- du only reads: it finds files with DOS's directory search
  (INT 21h AH=4Eh/4Fh) and opens nothing.
- **Paths are limited to 128 bytes.** DOS makes and enters directories
  only up to 64 characters, so this is only reached on a disk made
  elsewhere: a directory too deep is reported (`path too long`,
  ERRORLEVEL 1) and skipped, and the levels above it are counted. The
  limit also bounds how deep du goes (at most 62 levels), so a damaged
  disk whose directories loop cannot make it run forever, and du uses a
  fixed 2 KiB stack whatever the tree.
- **Critical errors.** On DOS 3.0 and later, a disk error while du
  reads (an empty floppy drive, a bad sector) does not stop at
  `Abort, Retry, Fail?`: du answers Fail, reports the path and goes on,
  with ERRORLEVEL 1. At any other time, such as writing the report to a
  floppy, DOS prompts as usual. DOS 2.x has no Fail answer, so there du
  leaves critical errors to DOS.
- Cluster sizes come from INT 21h AH=36h, whose 16-bit fields are exact
  for FAT12 and FAT16. On FAT32 under DOS 7 and later, that call may
  report adjusted values, so allocated sizes there may be wrong;
  `-b` is still exact.
- A network or other redirected drive that does not answer AH=36h is
  reported as invalid or not ready.
- A tree is counted with the cluster size of its operand's drive. A
  drive attached into it with `JOIN` is walked, but its files are
  rounded to the outer drive's clusters; name the joined drive itself
  to count it exactly.

## Building

`du.exe` links [libc16](https://github.com/gr-dos/libc16), a C runtime
for small-model DOS programs, instead of Open Watcom's own, and is built
with the Open Watcom toolchain that libc16 pins. By default `make` runs
`scripts/fetch-libc16`, which downloads the libc16 release pinned in
that script (`libc16-VERSION.zip`), checks it against the SHA-256
written there before unpacking it, and unpacks it into `build/libc16`.
The release's `tools/fetch-watcom` then fetches the Open Watcom snapshot
libc16 was built with into `build/tools`, also checked by SHA-256. This
needs `curl`, `unzip`, `sha256sum` and `xz`. To move to a newer libc16,
set `version` and `sha256` in `scripts/fetch-libc16` to the new
release's (its `.sha256` asset), both in the same change (libc16's
`docs/CONSUMERS.md`, "Using libc16").

    make                         # fetches libc16 if needed, builds du.exe
    make LIBC16_ROOT=../libc16   # or use a built local libc16 checkout
    make test                    # host unit tests, then DOSBox-X tests
    make unit                    # host unit tests only
    make dostest                 # tests/run.sh under DOSBox-X only
    make test-grdos              # tests/grdos.sh: GR-DOS in QEMU
    make clean

The build is reproducible: the same sources, libc16 and Open Watcom give
a byte-identical `du.exe`, which CI checks.

## Tests

- **Host unit tests** (`tests/test_*.c`, a C99 compiler, `HOSTCC`):
  cluster rounding at and around boundaries and near 2^32, number
  formats, option parsing, and the whole report against a mock disk
  (`tests/test_du.c`): nesting, hidden and system entries, volume
  labels, `.` and `..`, empty directories and roots, files over 4 GB,
  overlapping operands, relative paths, every error and the message
  for it, too-deep trees and write errors.
- **DOSBox-X** (`tests/run.sh`, `dosbox-x`, mtools, dosfstools): du on
  FAT image fixtures, a 360 KB and a 1.44 MB floppy and 32 MB and 80 MB
  hard disks with 2 KiB and 16 KiB clusters, holding zero-length,
  one-byte, exact- and over-cluster files, hidden, system and read-only
  files and directories, a long file name, empty and nested directories
  and a tree too deep for DOS (`tests/fixtures.sh`). The expected totals
  are worked out on the host from the files' lengths and each image's
  boot sector, and checked against the clusters mtools' `mdu` finds in
  each file's FAT chain. Also exit statuses and output for missing
  paths, invalid drives and usage errors, `--help` and `--version`, the
  critical-error handler (`tests/int24.c`), and that the images are
  unchanged afterwards.
- **GR-DOS** (`tests/grdos.sh`, `qemu-system-i386`): the GR-DOS 0.06
  release's 1.44 MB floppy, pinned by SHA-256, booted in QEMU with the
  same kind of fixtures. A real DOS kernel decides what a directory
  search returns (DOSBox-X returns hidden files even when not asked
  for them), `CHKDSK` cross-checks a whole drive's total, and a second
  boot with an empty B: drive checks that a critical error gives
  ERRORLEVEL 1, not a prompt.
- **Real hardware**: [docs/HARDWARE.md](docs/HARDWARE.md) is a smoke
  test for an XT-class PC or NEC V20 with a floppy or CF card.

Set `KEEP=1` to keep a DOS test's work directory, `DOSBOX=` or `QEMU=`
to use another emulator.

## Releases

The version is the one line in `VERSION`, DOS style (0.01, 0.02, ...
1.00), not semantic versioning. When a PR that changes it is merged to
`main`, the release workflow builds and tests that merge commit, tags
it `v<VERSION>` and publishes a GitHub release with `du.exe`,
`du.exe.sha256` and `DU.txt`, the `HELP DU` topic for GR-DOS
(`help/DU.txt`).

`main` changes only through pull requests whose `test` check (from
`ci.yml`) has passed; see [docs/DESIGN.md](docs/DESIGN.md#repository)
for the branch protection.

## Files

- `src/main.c`: the command-line program
- `src/options.c`, `src/options.h`: option and operand parsing
- `src/du.c`, `src/du.h`: the walk, the totals and the report
- `src/format.c`, `src/format.h`: cluster rounding and number formats
- `src/dosfs.c`, `src/dosfs.h`: the DOS calls, with the critical-error
  handler
- `src/out.c`, `src/out.h`: writing strings without `printf`
- `tests/test_*.c`, `tests/unit.h`: host unit tests
- `tests/run.sh`: DOS tests under DOSBox-X
- `tests/grdos.sh`: DOS tests on GR-DOS in QEMU
- `tests/fixtures.sh`: the FAT image fixtures and their expected totals
- `tests/checks.sh`: checks shared by the DOS tests
- `tests/int24.c`: DOS test of the critical-error handler
- `help/DU.txt`: the `HELP DU` topic for GR-DOS
- `docs/DESIGN.md`: the design decisions
- `docs/HARDWARE.md`: the real-hardware smoke test
- `Makefile`, `scripts/fetch-libc16`: the build
- `VERSION`: the version, compiled in and printed by `--version`

## License

MIT. See `LICENSE`.
