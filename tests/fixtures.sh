# FAT image fixtures for du's DOS tests, sourced by tests/run.sh
# (DOSBox-X) and tests/grdos.sh (GR-DOS in QEMU).
#
# Images are made on the host with mkfs.fat and filled with mtools.  As
# each file and directory is added, it is written to a manifest,
# IMAGE.lst ("D PATH" or "F PATH SIZE NAME": PATH as DOS shows it, such
# as A:\TREE\A.TXT, and NAME as it was given to mtools, which may be a
# long name).  The expected totals are then worked out from the
# manifest and the cluster size the image's boot sector gives (minfo),
# independently of du, and each file's allocation is checked against the
# clusters mtools' mdu finds in its FAT chain, so the expectations are
# known to match the disk.
#
# Needs mtools (mformat, mmd, mcopy, mattrib, minfo, mdu, mshortname)
# and mkfs.fat (dosfstools).

MTOOLS_SKIP_CHECK=1
MTOOLSRC=/dev/null
export MTOOLS_SKIP_CHECK MTOOLSRC

# Hard disk images have the geometry of 16 heads and 63 sectors a track,
# which QEMU's BIOS is told too (hd_geometry), and their one FAT
# partition starts at the second track, as DOS FDISK puts it: MS-DOS 4.00
# finds it by the partition table's cylinder, head and sector numbers.
HD_HEADS=16
HD_SECTORS=63
HD_OFFSET=$((HD_SECTORS * 512))

# mkfloppy IMAGE KB: a blank FAT12 floppy (360 or 1440) with a label
mkfloppy() {
    rm -f "$1"
    mformat -C -i "$1" -f "$2" -v DUTEST ::
}

# bytes N VALUE: VALUE as N little-endian bytes, as printf escapes
bytes() {
    n=$1 v=$2 r=
    while [ "$n" -gt 0 ]; do
        r="$r\\$(printf '%03o' $((v % 256)))"
        v=$((v / 256))
        n=$((n - 1))
    done
    printf '%s' "$r"
}

# mkhdd IMAGE CYLINDERS SECTORS-PER-CLUSTER: a hard disk image with one
# active FAT16 primary partition filling it, as for a CF card
mkhdd() {
    rm -f "$1"
    total=$(($2 * HD_HEADS * HD_SECTORS))
    size=$((total - HD_SECTORS))
    last=$(($2 - 1))
    truncate -s $((total * 512)) "$1"
    # the partition entry: active, CHS 0/1/1 to the last cylinder's last
    # sector, type 6 (FAT16 over 32 MB), LBA start and length
    entry="\\200\\001\\001\\000\\006$(bytes 1 $((HD_HEADS - 1)))"
    entry="$entry$(bytes 1 $((HD_SECTORS | (last / 256) * 64)))$(bytes 1 $((last % 256)))"
    entry="$entry$(bytes 4 $HD_SECTORS)$(bytes 4 $size)"
    printf "$entry" | dd of="$1" bs=1 seek=446 conv=notrunc 2>/dev/null
    printf '\125\252' | dd of="$1" bs=1 seek=510 conv=notrunc 2>/dev/null
    # one reserved sector and no alignment, as DOS FORMAT lays it out:
    # MS-DOS 4.00 hangs reading the root directory of mkfs.fat's
    # default, aligned layout (4 reserved sectors)
    mkfs.fat -F 16 -s "$3" -a -R 1 -g $HD_HEADS/$HD_SECTORS -h $HD_SECTORS \
        -n DUTEST --offset $HD_SECTORS "$1" $((size / 2)) >/dev/null
    # MS-DOS 4.00's IO.SYS (SETHARD) believes a hard disk's boot sector
    # only if its OEM name starts with MSDOS, IBM or OS2; otherwise it
    # assumes a layout from the partition's size (2 KiB clusters here)
    printf 'MSDOS5.0' | dd of="$1" bs=1 seek=$((HD_OFFSET + 3)) conv=notrunc 2>/dev/null
}

# hd_geometry IMAGE: its cylinders/heads/sectors
hd_geometry() {
    echo "$(( $(stat -c %s "$1") / 512 / HD_HEADS / HD_SECTORS ))/$HD_HEADS/$HD_SECTORS"
}

# spec IMAGE: the image as mtools names it (with the partition offset)
spec() {
    case $1 in
    *hd*) echo "$1@@$HD_OFFSET" ;;
    *) echo "$1" ;;
    esac
}

# cluster IMAGE: bytes per cluster, from the boot sector
cluster() {
    minfo -i "$(spec "$1")" :: | awk '
        /^sector size:/  { bps = $3 }
        /^cluster size:/ { spc = $3 }
        END { print bps * spc }'
}

# Adding to an image: IMG, DRIVE and LST are set by begin.
begin() {
    IMG=$1
    DRIVE=$2
    LST=$1.lst
    : > "$LST"
    printf 'D %s:\\\n' "$DRIVE" >> "$LST"
}

# dos PATH: /A/B as A:\A\B for the manifest
dos() {
    printf '%s:%s\n' "$DRIVE" "$1" | tr / '\\'
}

# dir PATH [ATTRIBUTES]: a directory, such as /TREE, with mattrib flags
dir() {
    mmd -i "$(spec "$IMG")" "::$1"
    [ -z "${2:-}" ] || mattrib -i "$(spec "$IMG")" $2 "::$1"
    printf 'D %s\n' "$(dos "$1")" >> "$LST"
}

# file PATH SIZE [ATTRIBUTES]: a file of SIZE bytes of varied content
file() {
    head -c "$2" /dev/urandom > blob
    mcopy -i "$(spec "$IMG")" blob "::$1"
    [ -z "${3:-}" ] || mattrib -i "$(spec "$IMG")" $3 "::$1"
    # a long name is stored with a short one, which is what DOS sees
    short=$(mshortname -i "$(spec "$IMG")" "::$1" |
            sed 's/.*<\(.*\)>.*/\1/; s/^.*\///')
    printf 'F %s %s %s\n' "$(dos "${1%/*}/$short")" "$2" "$1" >> "$LST"
    rm -f blob
}

# fill_tree: the standard fixture, relative to cluster size $CS: empty,
# one-byte, exact and over-boundary files, hidden, system and read-only
# files and directories, empty and nested directories, a long name.
fill_tree() {
    file /ZERO.DAT 0
    file /ONE.DAT 1
    file /EXACT.DAT "$CS"
    file /OVER.DAT $((CS + 1))
    file /TWICE.DAT $((CS * 2))
    file /UNDER.DAT $((CS - 1))
    file /HIDDEN.DAT 100 +h
    file /SYSTEM.DAT 5000 +s
    file /HSR.DAT $((CS * 3 - 1)) "+h +s +r"
    dir /EMPTY
    dir /TREE
    file /TREE/A.TXT 3000
    dir /TREE/L1
    file /TREE/L1/B.TXT 7777
    dir /TREE/L1/L2
    file /TREE/L1/L2/C.TXT 1
    dir /TREE/L1/L2/L3
    file /TREE/L1/L2/L3/D.TXT 12345
    dir /TREE/L1/L2/L3/L4
    dir /TREE/MIXED
    for n in 10 100 1000 10000 33333; do
        file /TREE/MIXED/M$n.BIN $n
    done
    dir /HIDDIR +h
    file /HIDDIR/X.TXT 2000
    dir /SYSDIR +s
    file /SYSDIR/Y.TXT 0
    file /SYSDIR/Z.TXT 4097 +h
    dir /LFN
    file "/LFN/Long File Name.txt" 4000
}

# fill_deep: below /DEEP, a chain of directories deeper than DOS allows (its
# paths are limited to 64 characters) and than du's 128-byte path: a
# file at level 2 is reachable, the ones further down are not.
fill_deep() {
    dir /DEEP
    p=/DEEP
    for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14; do
        p=$p/DDDDDDD$((i % 10))
        dir "$p"
        case $i in
        2) file "$p/NEAR.TXT" 1500 ;;
        14) file "$p/FAR.TXT" 1 ;;
        esac
    done
}

# expected IMAGE KIND [ROOT]: the lines du should print for ROOT (by
# default the drive's root) and everything in it, as "SIZE PATH" in path
# order: KIND dirs for -k (allocated KiB) of directories only, all for -a
# (files too), bytes for -b (apparent bytes, directories only), alloc
# for allocated bytes (directories only).
expected() {
    ROOT=${3:-} awk -v cs="$(cluster "$1")" -v kind="$2" '
        BEGIN { root = ENVIRON["ROOT"] }
        function up(n, d) { return int(n / d) + (n % d != 0) }
        function under(p, r) {
            if (r == "") return 1
            if (r ~ /\\$/) return index(toupper(p), toupper(r)) == 1
            return toupper(p) == toupper(r) ||
                   index(toupper(p), toupper(r) "\\") == 1
        }
        $1 == "D" && under($2, root) { dirs[$2] = 1; alloc[$2] += 0 }
        $1 == "F" && under($2, root) {
            a = up($3, cs) * cs
            files[$2] = 1; alloc[$2] = a; bytes[$2] = $3
            # every directory above it, down to ROOT, gets it too
            p = $2
            while (match(p, /\\[^\\]*$/)) {
                p = substr(p, 1, RSTART - 1)
                if (p ~ /^[A-Z]:$/) q = p "\\"; else q = p
                if (!under(q, root)) break
                alloc[q] += a; bytes[q] += $3
                if (q ~ /^[A-Z]:\\$/) break
            }
        }
        END {
            for (p in alloc) {
                if (kind != "all" && !(p in dirs)) continue
                if (kind == "bytes") v = bytes[p] + 0
                else if (kind == "alloc") v = alloc[p]
                else v = up(alloc[p], 1024)
                print v, p
            }
        }' "$1.lst" | sort -k 2
}

# check_mdu IMAGE: every file's allocation, worked out from its size,
# is what its FAT chain holds (mdu, in clusters).  Prints mismatches.
check_mdu() {
    cs=$(cluster "$1")
    mdu -a -i "$(spec "$1")" :: | awk -v cs="$cs" -v lst="$1.lst" '
        function up(n, d) { return int(n / d) + (n % d != 0) }
        BEGIN {
            while ((getline l < lst) > 0) {
                n = split(l, f, " ")
                if (f[1] != "F") continue
                # the name given to mtools: the rest of the line
                p = l
                for (i = 1; i <= 3; i++) sub(/^[^ ]+ /, "", p)
                want[toupper(p)] = up(f[3], cs)
            }
        }
        {
            # "::/PATH CLUSTERS"; PATH may hold spaces
            c = $NF
            p = $0
            sub(/ [0-9]+$/, "", p)
            p = toupper(substr(p, 3))
            if (p in want) {
                if (c != want[p])
                    print "mdu: " p ": " c " clusters, expected " want[p]
                delete want[p]
            }
        }
        END { for (p in want) print "mdu: " p ": not found" }'
}
