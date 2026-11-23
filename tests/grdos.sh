#!/bin/sh
#
# Test du.exe on GR-DOS itself (MS-DOS 4.00's kernel and COMMAND.COM),
# booted from its release's 1.44 MB floppy in headless QEMU, with the
# FAT image fixtures of tests/fixtures.sh as its other drives.
#
# Unlike DOSBox-X, which emulates DOS, this is a real DOS kernel on an
# emulated PC: its FAT driver decides what a directory search returns
# (DOSBox-X's returns hidden and system entries even when not asked
# for), its paths are limited to 64 characters, and an empty floppy
# drive raises a critical error (INT 24h) that would stop at
# "Abort, Retry, Fail?" if du did not answer it.
#
# Drives: A: is a copy of the GR-DOS floppy, with du, the batch files
# and the results; B: a 360 KB floppy image (1 KiB clusters); C: and D:
# hard disk images (FAT16, 2 and 16 KiB clusters).  AUTOEXEC.BAT runs
# the steps, then OFF.COM ends QEMU through its isa-debug-exit port.  A
# second boot leaves B: empty, for the critical error.
#
# The GR-DOS release is downloaded once into build/grdos and checked
# against the SHA-256 below.
#
# Usage: tests/grdos.sh [path/to/du.exe [path/to/int24.exe]]
# Environment: QEMU (default qemu-system-i386), KEEP=1 to keep the work
#              directory.

set -u

GRDOS_VERSION=0.06
GRDOS_IMAGE=gr-dos-$GRDOS_VERSION-1440k.img
GRDOS_SHA256=b584530434fedbbdf006ef621b24fc244923fa06966999f4f4786ae35cf1ce79
GRDOS_URL=https://github.com/gr-dos/gr-dos/releases/download/v$GRDOS_VERSION/$GRDOS_IMAGE

top=$(cd "$(dirname "$0")/.." && pwd)
exe=${1:-$top/du.exe}
int24=${2:-$top/build/int24.exe}
QEMU=${QEMU:-qemu-system-i386}

for t in "$QEMU" curl mformat mcopy mattrib minfo mdu mshortname mkfs.fat; do
    command -v "$t" >/dev/null || { echo "$t not found" >&2; exit 2; }
done
[ -f "$exe" ] || { echo "$exe not found" >&2; exit 2; }
[ -f "$int24" ] || { echo "$int24 not found" >&2; exit 2; }

cache=$top/build/grdos
mkdir -p "$cache"
if ! echo "$GRDOS_SHA256  $cache/$GRDOS_IMAGE" | sha256sum -c --status 2>/dev/null; then
    curl -fsSL -o "$cache/$GRDOS_IMAGE.tmp" "$GRDOS_URL" || exit 2
    echo "$GRDOS_SHA256  $cache/$GRDOS_IMAGE.tmp" | sha256sum -c --quiet || exit 2
    mv "$cache/$GRDOS_IMAGE.tmp" "$cache/$GRDOS_IMAGE"
fi

work=$(mktemp -d "${TMPDIR:-/tmp}/grdosdu.XXXXXX")
LC_ALL=C
export LC_ALL

. "$top/tests/fixtures.sh"
RES=res
. "$top/tests/checks.sh"

# ------------------------------------------------------------------
# fixtures

cd "$work" || exit 2
mkdir res

mkfloppy fd360.img 360
CS=$(cluster fd360.img)
begin fd360.img B
fill_tree

mkhdd hd2k.img 65 4
CS=$(cluster hd2k.img)
begin hd2k.img C
fill_tree
fill_deep

mkhdd hd16k.img 163 32
CS=$(cluster hd16k.img)
begin hd16k.img D
fill_tree

for i in fd360 hd2k hd16k; do
    bad=$(check_mdu $i.img)
    if [ -n "$bad" ]; then
        echo "fixture $i.img: $bad" >&2
        exit 2
    fi
    sha256sum $i.img > $i.sha
done

# crlf < text > text with DOS line ends
crlf() { sed 's/$/\r/'; }

# boot IMAGE STEPS: a copy of the GR-DOS floppy that runs STEPS
boot() {
    cp "$cache/$GRDOS_IMAGE" "$1"
    chmod u+w "$1"
    rc_bat a | crlf > rc.bat
    crlf > steps.bat < "$2"
    printf '@echo off\r\npath a:\\\r\ncall steps.bat\r\noff\r\n' > autoexec.bat
    # mov al,0; out 0f4h,al: QEMU exits with status 1; int 20h if not
    printf '\260\000\346\364\315\040' > off.com
    mcopy -o -i "$1" "$exe" ::DU.EXE
    mcopy -o -i "$1" "$int24" ::INT24.EXE
    mcopy -o -i "$1" rc.bat steps.bat autoexec.bat off.com ::
}

# run IMAGE [QEMU-ARGS...]: boot it, then fetch the results to res/
run() {
    img=$1
    shift
    timeout 180 "$QEMU" -display none -no-reboot -m 4 \
        -device isa-debug-exit,iobase=0xf4,iosize=0x01 \
        -drive "file=$img,if=floppy,index=0,format=raw" \
        "$@" -boot a >qemu.log 2>&1
    status=$?
    # 1 is OFF.COM's exit; anything else is a hang (124) or a crash
    if [ "$status" -ne 1 ]; then
        not_ok "$img: QEMU exit status $status, expected 1 (OFF.COM)"
    fi
    mcopy -n -i "$img" '::R*.TXT' '::O*.TXT' res/ 2>/dev/null
}

# disk IMAGE UNIT: QEMU arguments for a hard disk image as IDE UNIT,
# with the geometry its partition table was written for
disk() {
    g=$(hd_geometry "$1")
    echo "-drive file=$1,if=none,format=raw,id=hd$2" \
        "-device ide-hd,drive=hd$2,bus=ide.0,unit=$2,cyls=${g%%/*},heads=$HD_HEADS,secs=$HD_SECTORS"
}

cat > steps1.txt <<'EOS'
rem --- the trees, by directory, with files, and file lengths
du B:\ >o1.txt
call rc 1
du -a B:\ >o2.txt
call rc 2
du -b B:\ >o3.txt
call rc 3
du -a C:\TREE >o4.txt
call rc 4
du -a D:\ >o5.txt
call rc 5
du -b D:\ >o6.txt
call rc 6
rem --- grand totals, file operands
du -sc B:\ B:\TREE C:\TREE D:\HIDDIR >o7.txt
call rc 7
du B:\HSR.DAT B:\SYSDIR\Z.TXT D:\UNDER.DAT D:\LFN\LONGFI~1.TXT >o8.txt
call rc 8
rem --- deeper than DOS allows
du C:\DEEP >o9.txt
call rc 9
du -s C:\ >o10.txt
call rc 10
rem --- missing paths and drives, usage errors
du Q:\ >o11.txt
call rc 11
du B:\NOSUCH B:\EMPTY >o12.txt
call rc 12
du -x >o13.txt
call rc 13
du --version >o14.txt
call rc 14
rem --- relative to the current directory of another drive
b:
cd \TREE
a:
du -s b: b:L1 >o15.txt
call rc 15
b:
a:\du >a:\o16.txt
call a:\rc 16
cd \
a:
rem --- critical-error handler and DOS calls
int24 >o17.txt
call rc 17
rem --- DOS's own count of the space files take
du -s D:\ >o18.txt
call rc 18
chkdsk D: >o19.txt
EOS

# B: empty: DOS raises a critical error, which du answers Fail
cat > steps2.txt <<'EOS'
du B:\ >o21.txt
call rc 21
du -s B:\TREE C:\TREE >o22.txt
call rc 22
EOS

boot boot1.img steps1.txt
run boot1.img \
    -drive "file=fd360.img,if=floppy,index=1,format=raw" \
    $(disk hd2k.img 0) $(disk hd16k.img 1)

boot boot2.img steps2.txt
run boot2.img \
    -drive "if=floppy,index=1" \
    $(disk hd2k.img 0)

# ------------------------------------------------------------------
# checks

rc 1 0
totals 1 fd360.img dirs
postorder 1
rc 2 0
totals 2 fd360.img all
postorder 2
rc 3 0
totals 3 fd360.img bytes
rc 4 0
totals 4 hd2k.img all 'C:\TREE'
rc 5 0
totals 5 hd16k.img all
rc 6 0
totals 6 hd16k.img bytes

t=$(( $(value fd360.img alloc 'B:\') + $(value hd2k.img alloc 'C:\TREE') +
      $(value hd16k.img alloc 'D:\HIDDIR') ))
rc 7 0
expect 7 <<EOF
$(printf '%9s  B:\\' "$(value fd360.img dirs 'B:\')")
$(printf '%9s  B:\\TREE' "$(value fd360.img dirs 'B:\TREE')")
$(printf '%9s  C:\\TREE' "$(value hd2k.img dirs 'C:\TREE')")
$(printf '%9s  D:\\HIDDIR' "$(value hd16k.img dirs 'D:\HIDDIR')")
$(printf '%9s  total' $(( (t + 1023) / 1024 )))
EOF
rc 8 0
expect 8 <<'EOF'
        3  B:\HSR.DAT
        5  B:\SYSDIR\Z.TXT
       16  D:\UNDER.DAT
       16  D:\LFN\LONGFI~1.TXT
EOF

# Deeper than DOS lets a directory be made or entered (64 characters),
# but MS-DOS 4.00 searches longer paths, so du's own 128-byte limit
# stops the walk, as in DOSBox-X; what can be reached is counted, and
# the error is in the exit status
rc 9 1
has 9 ' +2  C:\\DEEP\\DDDDDDD1\\DDDDDDD2'
check "step 9: ends with C:\\DEEP" [ "$(out 9 | tail -n 1 | sed 's/^ *[0-9]*  //')" = 'C:\DEEP' ]
check "step 9: stopped by du's path limit" \
    [ "$(out 9 | awk -F'\\' 'NF > m { m = NF } END { print m }')" -le 15 ]
rc 10 1
deep=$(out 9 | tail -n 1 | awk '{ print $1 }')
expect 10 <<EOF
$(printf '%9s  C:\\' $(( $(value hd2k.img dirs 'C:\') - $(value hd2k.img dirs 'C:\DEEP') + deep )))
EOF

rc 11 1
expect 11 < /dev/null
rc 12 1
expect 12 <<'EOF'
        0  B:\EMPTY
EOF
rc 13 2
expect 13 < /dev/null
rc 14 0
expect 14 <<EOF
du (dos-du16) $(cat "$top/VERSION")
EOF
rc 15 0
expect 15 <<EOF
$(printf '%9s  b:' "$(value fd360.img dirs 'B:\TREE')")
$(printf '%9s  b:L1' "$(value fd360.img dirs 'B:\TREE\L1')")
EOF
rc 16 0
out 16 | sed 's/^ *\([0-9][0-9]*\)  /\1 /; s/ \./ B:\\TREE/' | sort -k 2 > got.txt
expected fd360.img dirs 'B:\TREE' > want.txt
compare 16 "no operand: the current directory"

rc 17 0
expect 17 <<EOF
ok 12
EOF

# CHKDSK's "bytes in N user files" and "bytes in N hidden files" are
# the clusters of every file on D:, which has nothing too deep
rc 18 0
expect 18 <<EOF
$(printf '%9s  D:\\' "$(value hd16k.img dirs 'D:\')")
EOF
chkdsk=$(out 19 | awk '/bytes in .* (user|hidden) files/ { t += $1 } END { print t + 0 }')
check "step 19: CHKDSK counts $chkdsk bytes in files, du $(value hd16k.img alloc 'D:\')" \
    [ "$chkdsk" = "$(value hd16k.img alloc 'D:\')" ]

# an empty drive: an error, no prompt, and the other operands reported
rc 21 1
expect 21 < /dev/null
rc 22 1
expect 22 <<EOF
$(printf '%9s  C:\\TREE' "$(value hd2k.img dirs 'C:\TREE')")
EOF

for i in fd360 hd2k hd16k; do
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
