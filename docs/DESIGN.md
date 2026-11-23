# du16: design

`du16` is `DU.EXE`, an 8086-compatible MS-DOS utility that reports the
disk space used by files and directory trees, in the manner of Unix
`du`. It follows the `gr-dos/*16` conventions of
[hello16](https://github.com/gr-dos/hello16) (layout 1: C only, Open
Watcom and libc16) and the structure of its sibling
[df16](https://github.com/gr-dos/df16). This document records the
decisions [issue #1](https://github.com/gr-dos/du16/issues/1) left open.
The README describes the behaviour.

## Decisions

| Question | Decision | Why |
| --- | --- | --- |
| What is counted | allocated space: each file's length rounded up to its drive's cluster size | what the file costs on the disk, which is what a disk usage report is for; matches CHKDSK |
| Apparent sizes | `-b` (bytes) and `--apparent-size`, as GNU du | lengths are useful too, and exact on any drive |
| Default unit | 1 KiB blocks, rounded up (`-k`) | Unix du's and df16's; `-h` for binary units, `-b` for bytes |
| Directories' own clusters | not counted, nor the root directory area | DOS's directory search gives a directory's size as 0, and the alternative, reading the FAT through INT 25h, fails on redirected drives and is a lot of code for a small part of the total; a deliberate, documented undercount, equal to CHKDSK's "bytes in directories" for a whole drive |
| Switches | `-a`, `-s`, `-c`, `-h`, `-k`, `-b`, `--apparent-size`, `-?`, `--help`, `--version`, `--` | the Unix ones the issue asked for, plus df16's `-k`; `-h` is human-readable as in df16, never help |
| Case | options case-insensitive, short ones groupable | as df16 |
| Output | size right-aligned in 9 columns, two spaces, path; post-order | DOS `SORT` sorts right-aligned numbers by size; fits 80 columns; post-order is Unix du's |
| Paths shown | the operand as typed, then the DOS names below it | as Unix du; scripts get back what they gave |
| No operand | `.` | as Unix du |
| Wildcards | a usage error | Unix shells expand them, DOS's does not; refusing is predictable, expanding them is future work |
| Multiple and overlapping operands | each reported in full, in order; `-c` counts an operand inside another, or repeated, once | the report is per operand, the grand total is of the union, as GNU du's would be |
| Errors | reported on stderr with the path, rest still processed, ERRORLEVEL 1 | an incomplete total must not look complete; scripts see ERRORLEVEL |
| Exit statuses | 0 ok, 1 something not read, 2 usage or write error | df16's and crcsum16's convention |
| Arithmetic | 64-bit totals; per-file rounding without `size + cluster - 1` | no overflow for anything DOS can store |
| Path length | 128 bytes, checked before every copy into the path | DOS's own paths are 64 characters; 128 covers anything a DOS can search, and bounds depth |
| Depth | at most 62 levels (from the path limit); one static search record per level | a looping, damaged directory tree cannot run forever or overrun the 2 KiB stack |
| Critical errors | Fail during each disk-reading DOS call, previous handler back after | as df16: unattended runs never stop at `Abort, Retry, Fail?` |

## Structure

The DOS calls are behind a small interface, `struct du_ops` in
`src/du.h`: the current drive, a drive's cluster size (INT 21h AH=36h),
`_fullpath`, and the directory search (`_dos_findfirst` and
`_dos_findnext`). `src/dosfs.c` implements it for DOS; the host tests
(`tests/test_du.c`) implement it as a mock disk with DOS's rules, so
the walk, the totals, the messages and the exit statuses are tested
without an emulator.

The walk (`walk` in `src/du.c`) keeps the absolute path in one static
128-byte buffer and one DOS search record per level in a static array,
so it needs a few bytes of stack per level. A directory is searched
with `*.*` and hidden, system and directory attributes; entries with
the volume-label attribute (which includes long-name entries) and `.`
and `..` are skipped. Its line is written after its contents.

Each operand is resolved with `_fullpath`; a root is a directory, and
anything else is looked up with a directory search for the path
itself, which says whether it is a file or a directory and gives a
file's length. Cluster sizes are asked for once per drive.

## Arithmetic

A file's allocation is `(size / cluster + (size % cluster != 0)) *
cluster`, with `size` 32-bit and the result 64-bit, so a file of
4 GiB - 1 in 32 KiB clusters is 4 GiB exactly. Totals are
`unsigned long long` (Open Watcom implements it on the 8086 with
helper routines). KiB and human-readable values are rounded up from
the total, not per file.

## Repository

As hello16: GitHub Actions `ci.yml` builds and runs `make test` and
`make test-grdos` on every pull request and push to `main`, and checks
that a rebuild gives the same `du.exe`; its job, `test`, is the required
check. `release.yml` releases when `VERSION` names an untagged version
(see the README).

Branch protection on `main`, as on hello16 (checked with
`gh api repos/gr-dos/hello16/branches/main/protection`):

- a pull request is required before merging (no approving review
  needed),
- the `test` status check from GitHub Actions (app 15368) must pass,
- administrators are included (`enforce_admins`),
- force pushes and deletion are not allowed.

It is not in the repository's files, so it is applied with:

    gh api -X PUT repos/gr-dos/du16/branches/main/protection --input - <<'EOF'
    {"required_status_checks":{"strict":false,"checks":[{"context":"test","app_id":15368}]},
     "enforce_admins":true,
     "required_pull_request_reviews":{"required_approving_review_count":0},
     "restrictions":null,"allow_force_pushes":false,"allow_deletions":false}
    EOF

and checked with `gh api repos/gr-dos/du16/branches/main/protection`.
The repository merges with merge commits only and deletes merged
branches, as hello16 does.

## GR-DOS

`help/DU.txt` is a topic for GR-DOS's `HELP` in the format of its
`help/topics` (header, then syntax, examples and notes, ASCII, lines
of at most 79 characters), attached to each release. Adding du16 to
GR-DOS (`dist16/lock`, `C:\DOS`) is a separate change to
`gr-dos/gr-dos` after a release; whether it also goes on a floppy is
decided there.

## Testing

See the README's "Tests". Two things the fixtures had to do for
MS-DOS 4.00, found while writing `tests/grdos.sh`: its IO.SYS assumes
one reserved sector on a hard disk partition, and believes the boot
sector's BPB only when the OEM name starts with `MSDOS`, `IBM` or
`OS2`, otherwise assuming a layout from the partition's size. So the
hard disk images are laid out as DOS FORMAT would (`mkfs.fat -a -R 1`,
OEM name `MSDOS5.0`). A CF card formatted on another system may meet
the same problem on GR-DOS, whatever du does.

## Future work

- Wildcard operands (`du *.EXE`).
- A maximum depth for the lines shown (GNU `-d`).
- Counting directories' own clusters, by reading the FAT.
- Exact FAT32 cluster sizes on DOS 7 (INT 21h AX=7303h).
