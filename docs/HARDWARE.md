# du16: real-hardware smoke test

For an XT-class PC (8088) or a NEC V20 machine, with MS-DOS 6.22 or
GR-DOS, a floppy drive and, if it has one, a hard disk or CF card. It
takes about ten minutes. The emulator tests cover the details; this
checks that the real machine, its BIOS and its disks agree.

## Prepare

1. Copy `DU.EXE` from the release to the machine (`C:\DOS` or a
   floppy). Check it against `du.exe.sha256` on the host first.
2. Have a formatted floppy with a few files on it, including one
   hidden file (`ATTRIB +H FILE`) and a subdirectory.
3. Note the DOS version (`VER`) and CPU.

## Steps

| # | Do | Expect |
| --- | --- | --- |
| 1 | `DU --VERSION` | `du (dos-du16) VERSION`, ERRORLEVEL 0 |
| 2 | `DU --HELP` | the usage, no disk access |
| 3 | `DU -S C:\` and `CHKDSK C:` | DU's size times 1,024 is CHKDSK's "bytes in user files" plus "bytes in hidden files", rounded up to a KiB |
| 4 | `DU -S A:\` with the floppy in, and `CHKDSK A:` | the same agreement for the floppy, with its own cluster size (CHKDSK's "bytes in each allocation unit") |
| 5 | `DU -A A:\` | the hidden file and the subdirectory's files are listed |
| 6 | `DU -C -S A:\ C:\DOS` | two lines and a total that is their sum (rounded up) |
| 7 | `DU -B A:\FILE` for a file | the length `DIR` shows |
| 8 | `DU C:\ \| SORT` | lines in order of size, the largest last |
| 9 | `DU A:\` with the floppy drive empty (door open) | after the drive gives up, no `Abort, Retry, Fail?`, an error message, ERRORLEVEL 1 (check with `IF ERRORLEVEL 1 ECHO 1`) |
| 10 | `DU Q:\` (no such drive) | an error, ERRORLEVEL 1 |
| 11 | `DU -X` | a usage message, ERRORLEVEL 2 |
| 12 | `DU C:\ >NUL` on the largest tree, timed | it finishes; note the time |
| 13 | `DIR` on the drives used, before and after | nothing changed (dates, sizes, free space) |

On a machine with a write-protected floppy, step 4 also shows that du
never writes. With a CF card, step 3 is the larger-cluster case: CF
cards formatted by DOS usually have 2 KiB to 32 KiB clusters.

## Record

Note in the pull request or release: the machine, CPU, DOS version,
drives and their cluster sizes, the time of step 12, and anything
that did not match.
