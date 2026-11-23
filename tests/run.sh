#!/bin/sh
#
# Test du.exe under DOSBox-X.
#
# Drives: C: is the work directory's c/, mounted from the host; A: a
# 360 KB floppy image (FAT12, 1 KiB clusters); B: a 1.44 MB floppy image
# (FAT12, 512-byte clusters); D: a 32 MB hard disk image (FAT16, 2 KiB
# clusters) and E: an 80 MB one (FAT16, 16 KiB clusters), as for CF
# cards.  The images are made and filled on the host by
# tests/fixtures.sh, which also works out, independently of du, what du
# should report for them, and checks that against the FAT chains.
#
# DOSBox-X's directory search returns hidden and system entries even
# when they are not asked for, so that du asks for them is checked by
# the host unit tests and by tests/grdos.sh, on a real DOS kernel.
#
# One batch file runs every DOS-side step, and the results are checked
# on the host afterwards (tests/checks.sh).  Each step N records du's
# exit status in RN.TXT and its standard output in ON.TXT.  DOS cannot
# redirect standard error, so diagnostics are not checked here (the host
# unit tests check them).
#
# INT24.EXE (tests/int24.c) checks the critical-error handler in
# src/dosfs.c, which du's own steps cannot reach because DOSBox-X never
# raises a critical error for its drives.
#
# To add a test: add a step to steps.bat (command, then "call rc N")
# and the matching checks below.
#
# Usage: tests/run.sh [path/to/du.exe [path/to/int24.exe]]
# Environment: DOSBOX (default dosbox-x), KEEP=1 to keep the work
#              directory.

set -u

top=$(cd "$(dirname "$0")/.." && pwd)
exe=${1:-$top/du.exe}
int24=${2:-$top/build/int24.exe}
DOSBOX=${DOSBOX:-dosbox-x}

for t in "$DOSBOX" mformat mcopy mattrib minfo mdu mshortname mkfs.fat; do
    command -v "$t" >/dev/null || { echo "$t not found" >&2; exit 2; }
done
[ -f "$exe" ] || { echo "$exe not found" >&2; exit 2; }
[ -f "$int24" ] || { echo "$int24 not found" >&2; exit 2; }

work=$(mktemp -d "${TMPDIR:-/tmp}/dosdu.XXXXXX")
LC_ALL=C
export LC_ALL

. "$top/tests/fixtures.sh"
RES=c
. "$top/tests/checks.sh"

# ------------------------------------------------------------------
# fixtures

cd "$work" || exit 2
mkdir c c/T c/T/SUB
cp "$exe" c/du.exe
cp "$int24" c/int24.exe
head -c 1234 /dev/urandom > c/T/ONE.DAT
head -c 5678 /dev/urandom > c/T/SUB/TWO.DAT
: > c/T/SUB/EMPTY.DAT

mkfloppy fd360.img 360
CS=$(cluster fd360.img)
begin fd360.img A
fill_tree

mkfloppy fd1440.img 1440
CS=$(cluster fd1440.img)
begin fd1440.img B
fill_tree

mkhdd hd2k.img 65 4
CS=$(cluster hd2k.img)
begin hd2k.img D
fill_tree
fill_deep

mkhdd hd16k.img 163 32
CS=$(cluster hd16k.img)
begin hd16k.img E
fill_tree

for i in fd360 fd1440 hd2k hd16k; do
    bad=$(check_mdu $i.img)
    if [ -n "$bad" ]; then
        echo "fixture $i.img: $bad" >&2
        exit 2
    fi
    sha256sum $i.img > $i.sha
done

rc_bat c > c/rc.bat

cat > c/steps.bat <<'EOS'
@echo off
rem --- the trees, by directory, with files, and file lengths
du A:\ >o1.txt
call rc 1
du -a A:\ >o2.txt
call rc 2
du -b A:\ >o3.txt
call rc 3
du B:\ >o4.txt
call rc 4
du -a B:\ >o5.txt
call rc 5
du -A E:\ >o6.txt
call rc 6
du -b E:\ >o7.txt
call rc 7
du -a D:\TREE >o8.txt
call rc 8
rem --- summaries, grand totals, units
du -s A:\ B:\ D:\TREE E:\ >o9.txt
call rc 9
du -sc A:\TREE B:\ E:\HIDDIR >o10.txt
call rc 10
du -sc A:\ A:\TREE A:\TREE\L1 A:\ >o11.txt
call rc 11
du -c B:\HIDDIR >o12.txt
call rc 12
du -sh E:\ >o13.txt
call rc 13
du -sk -h -k A:\ >o14.txt
call rc 14
du --apparent-size -s E:\ >o15.txt
call rc 15
rem --- file operands, hidden and system too
du A:\EXACT.DAT A:\OVER.DAT A:\ZERO.DAT A:\HSR.DAT A:\SYSDIR\Z.TXT >o16.txt
call rc 16
du -b E:\UNDER.DAT E:\LFN\LONGFI~1.TXT >o17.txt
call rc 17
rem --- relative operands, and none
a:
cd \TREE
c:\du >c:\o18.txt
call c:\rc 18
c:\du -s . .. L1\L2 >c:\o19.txt
call c:\rc 19
c:
du -s A: A:L1 >o20.txt
call rc 20
a:
cd \
c:
rem --- a host directory, in bytes
du -b C:\T >o21.txt
call rc 21
du -ab \T\SUB >o22.txt
call rc 22
rem --- too deep for DOS and du
du D:\DEEP >o23.txt
call rc 23
du -s D:\ >o24.txt
call rc 24
rem --- missing paths and drives
du Q:\ >o25.txt
call rc 25
du A:\NOSUCH >o26.txt
call rc 26
du -s A:\TREE A:\NOSUCH\DIR Q: A:\EMPTY >o27.txt
call rc 27
du -sc Q: A:\TREE >o28.txt
call rc 28
rem --- usage errors
du -x >o29.txt
call rc 29
du -a -s A:\ >o30.txt
call rc 30
du A:\*.* >o31.txt
call rc 31
du --bogus >o32.txt
call rc 32
du - >o33.txt
call rc 33
du 1: >o34.txt
call rc 34
du A:\ FOO: >o35.txt
call rc 35
rem --- help and version
du --help >o36.txt
call rc 36
du -? >o37.txt
call rc 37
du --HELP >o38.txt
call rc 38
du --version >o39.txt
call rc 39
du --VERSION >o40.txt
call rc 40
rem --- critical-error handler
int24 >o41.txt
call rc 41
rem --- -- ends the options
du -s -- A:\TREE >o42.txt
call rc 42
EOS

SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout 300 \
    "$DOSBOX" -defaultconf -silent -nogui -nomenu \
    -set "cpu cycles=max" \
    -c "mount c \"$work/c\"" \
    -c "imgmount a \"$work/fd360.img\" -t floppy" \
    -c "imgmount b \"$work/fd1440.img\" -t floppy" \
    -c "imgmount d \"$work/hd2k.img\" -t hdd" \
    -c "imgmount e \"$work/hd16k.img\" -t hdd" \
    -c "c:" -c "steps.bat" -c "exit" >dosbox.log 2>&1

# ------------------------------------------------------------------
# checks

# trees: each image by directory, with files, and in bytes
rc 1 0
totals 1 fd360.img dirs
postorder 1
check "step 1: lines end in CR LF" \
    sh -c "[ \$(grep -c \"\$(printf '\r')\$\" c/O1.TXT) -eq \$(wc -l < c/O1.TXT) ]"
rc 2 0
totals 2 fd360.img all
postorder 2
rc 3 0
totals 3 fd360.img bytes
rc 4 0
totals 4 fd1440.img dirs
rc 5 0
totals 5 fd1440.img all
rc 6 0
totals 6 hd16k.img all
postorder 6
rc 7 0
totals 7 hd16k.img bytes
rc 8 0
totals 8 hd2k.img all 'D:\TREE'
# the same files take different space with different clusters
check "cluster sizes differ" [ "$(value fd360.img dirs 'A:\')" != "$(value hd16k.img dirs 'E:\')" ]

# summaries and grand totals
rc 9 0
expect 9 <<EOF
$(printf '%9s  A:\\' "$(value fd360.img dirs 'A:\')")
$(printf '%9s  B:\\' "$(value fd1440.img dirs 'B:\')")
$(printf '%9s  D:\\TREE' "$(value hd2k.img dirs 'D:\TREE')")
$(printf '%9s  E:\\' "$(value hd16k.img dirs 'E:\')")
EOF
# the total adds allocated bytes, then rounds up to KiB once
t=$(( $(value fd360.img alloc 'A:\TREE') +
      $(value fd1440.img alloc 'B:\') + $(value hd16k.img alloc 'E:\HIDDIR') ))
rc 10 0
expect 10 <<EOF
$(printf '%9s  A:\\TREE' "$(value fd360.img dirs 'A:\TREE')")
$(printf '%9s  B:\\' "$(value fd1440.img dirs 'B:\')")
$(printf '%9s  E:\\HIDDIR' "$(value hd16k.img dirs 'E:\HIDDIR')")
$(printf '%9s  total' $(( (t + 1023) / 1024 )))
EOF
# overlapping operands are each shown, and counted once
rc 11 0
expect 11 <<EOF
$(printf '%9s  A:\\' "$(value fd360.img dirs 'A:\')")
$(printf '%9s  A:\\TREE' "$(value fd360.img dirs 'A:\TREE')")
$(printf '%9s  A:\\TREE\\L1' "$(value fd360.img dirs 'A:\TREE\L1')")
$(printf '%9s  A:\\' "$(value fd360.img dirs 'A:\')")
$(printf '%9s  total' "$(value fd360.img dirs 'A:\')")
EOF
# a hidden directory; 512-byte clusters round up to KiB
rc 12 0
expect 12 <<EOF
$(printf '%9s  B:\\HIDDIR' "$(value fd1440.img dirs 'B:\HIDDIR')")
$(printf '%9s  total' "$(value fd1440.img dirs 'B:\HIDDIR')")
EOF
rc 13 0
check "step 13: -h" sh -c "out() { tr -d '\r' < c/O13.TXT; }; out | grep -Eq '^ *[0-9]+\.[0-9]KiB  E:\\\\\$' && [ \$(out | wc -l) -eq 1 ]"
rc 14 0
expect 14 <<EOF
$(printf '%9s  A:\\' "$(value fd360.img dirs 'A:\')")
EOF
rc 15 0
expect 15 <<EOF
$(printf '%9s  E:\\' $(( ($(value hd16k.img bytes 'E:\') + 1023) / 1024 )))
EOF

# file operands
rc 16 0
expect 16 <<'EOF'
        1  A:\EXACT.DAT
        2  A:\OVER.DAT
        0  A:\ZERO.DAT
        3  A:\HSR.DAT
        5  A:\SYSDIR\Z.TXT
EOF
rc 17 0
expect 17 <<'EOF'
    16383  E:\UNDER.DAT
     4000  E:\LFN\LONGFI~1.TXT
EOF

# relative operands keep their names
rc 18 0
out 18 | sed 's/^ *\([0-9][0-9]*\)  /\1 /; s/ \./ A:\\TREE/' | sort -k 2 > got.txt
expected fd360.img dirs 'A:\TREE' > want.txt
compare 18 "no operand: the current directory"
has 18 ' +[0-9]+  \.\\L1\\L2'
rc 19 0
expect 19 <<EOF
$(printf '%9s  .' "$(value fd360.img dirs 'A:\TREE')")
$(printf '%9s  ..' "$(value fd360.img dirs 'A:\')")
$(printf '%9s  L1\\L2' "$(value fd360.img dirs 'A:\TREE\L1\L2')")
EOF
rc 20 0
expect 20 <<EOF
$(printf '%9s  A:' "$(value fd360.img dirs 'A:\TREE')")
$(printf '%9s  A:L1' "$(value fd360.img dirs 'A:\TREE\L1')")
EOF

# a host directory: lengths do not depend on DOSBox-X's cluster size
rc 21 0
expect 21 <<'EOF'
     5678  C:\T\SUB
     6912  C:\T
EOF
# (files in the host's directory order, the directory last)
rc 22 0
out 22 | sort > got.txt
sort > want.txt <<'EOF'
     5678  \T\SUB\TWO.DAT
        0  \T\SUB\EMPTY.DAT
     5678  \T\SUB
EOF
compare 22 "output differs"
check "step 22: directory last" [ "$(out 22 | tail -n 1)" = '     5678  \T\SUB' ]

# too deep: an error, but what can be reached is counted
rc 23 1
has 23 ' +2  D:\\DEEP\\DDDDDDD1\\DDDDDDD2'
check "step 23: ends with D:\\DEEP" sh -c "tail -n 1 c/O23.TXT | tr -d '\r' | grep -Eqx ' +[0-9]+  D:.DEEP'"
# DOSBox-X allows paths longer than DOS's 64 characters, so there du's
# own limit stops the walk: "D:\DEEP" and 13 levels fit in 128 bytes,
# but not with "\*.*" to search the 13th, nor the 14th level
check "step 23: stopped by du's path limit" \
    [ "$(out 23 | awk -F'\\' 'NF > m { m = NF } END { print m }')" -le 15 ]
rc 24 1
deep=$(out 23 | tail -n 1 | awk '{ print $1 }')
expect 24 <<EOF
$(printf '%9s  D:\\' $(( $(value hd2k.img dirs 'D:\') - $(value hd2k.img dirs 'D:\DEEP') + deep )))
EOF

# missing paths and drives: an error, the rest still reported
rc 25 1
expect 25 < /dev/null
rc 26 1
expect 26 < /dev/null
rc 27 1
expect 27 <<EOF
$(printf '%9s  A:\\TREE' "$(value fd360.img dirs 'A:\TREE')")
        0  A:\\EMPTY
EOF
rc 28 1
expect 28 <<EOF
$(printf '%9s  A:\\TREE' "$(value fd360.img dirs 'A:\TREE')")
$(printf '%9s  total' "$(value fd360.img dirs 'A:\TREE')")
EOF

# usage errors: nothing read, nothing written
for n in 29 30 31 32 33 34 35; do
    rc $n 2
    expect $n < /dev/null
done

# help and version
rc 36 0
check "step 36: help" grep -q '^Usage: du' c/O36.TXT
for n in 37 38; do
    rc $n 0
    check "step $n: same as step 36" cmp -s c/O$n.TXT c/O36.TXT
done
for n in 39 40; do
    rc $n 0
    expect $n <<EOF
du (dos-du16) $(cat "$top/VERSION")
EOF
done

# critical-error handler (tests/int24.c)
rc 41 0
expect 41 <<EOF
ok 12
EOF

rc 42 0
expect 42 <<EOF
$(printf '%9s  A:\\TREE' "$(value fd360.img dirs 'A:\TREE')")
EOF

# du only reads: the images are as they were made
for i in fd360 fd1440 hd2k hd16k; do
    check "$i.img unchanged" sha256sum -c --quiet $i.sha
done

# ------------------------------------------------------------------

echo "$pass passed, $fail failed"
if [ "$fail" -ne 0 ] || [ "${KEEP:-0}" = 1 ]; then
    echo "work directory: $work"
else
    rm -rf "$work"
fi
[ "$fail" -eq 0 ]
