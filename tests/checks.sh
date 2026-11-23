# Checks of du's DOS test results, sourced by tests/run.sh and
# tests/grdos.sh after tests/fixtures.sh.  Step N of a DOS batch file
# leaves du's exit status in $RES/RN.TXT and its standard output in
# $RES/ON.TXT; pass and fail count the checks.

pass=0
fail=0

ok()     { pass=$((pass + 1)); }
not_ok() { echo "FAIL: $*"; fail=$((fail + 1)); }

# check NAME COMMAND...
check() {
    name=$1
    shift
    if "$@"; then ok; else not_ok "$name"; fi
}

# rc STEP EXPECTED-STATUS
rc() {
    got=$(tr -d ' \r' < "$RES/R$1.TXT" 2>/dev/null | sed 's/^rc//')
    if [ "$got" = "$2" ]; then
        ok
    else
        not_ok "step $1: exit status ${got:-missing}, expected $2"
    fi
}

# out STEP: ON.TXT without CRs
out() { tr -d '\r' < "$RES/O$1.TXT" 2>/dev/null; }

# compare STEP DESCRIPTION: got.txt against want.txt
compare() {
    if cmp -s got.txt want.txt; then
        ok
    else
        not_ok "step $1: $2"
        diff want.txt got.txt | sed 's/^/    /'
    fi
}

# expect STEP < expected-lines
expect() {
    out "$1" > got.txt
    cat > want.txt
    compare "$1" "output differs"
}

# totals STEP IMAGE KIND [ROOT]: ON.TXT reports exactly what
# fixtures.sh's expected says, in any order
totals() {
    out "$1" | sed 's/^ *\([0-9][0-9]*\)  /\1 /' | sort -k 2 > got.txt
    expected "$2" "$3" "${4:-}" > want.txt
    compare "$1" "$2 $3 ${4:-} totals differ"
}

# postorder STEP: no line is for a path inside one already reported, as
# Unix du reports a directory after everything in it
postorder() {
    bad=$(out "$1" | awk '
        { p[NR] = $2 }
        END {
            for (i = 1; i <= NR; i++) {
                pre = p[i]
                if (pre !~ /\\$/) pre = pre "\\"
                for (j = i + 1; j <= NR; j++)
                    if (index(p[j], pre) == 1) print p[j] " after " p[i]
            }
        }')
    check "step $1: post-order" [ -z "$bad" ]
}

# has STEP REGEX: some line of ON.TXT is all of the extended REGEX
has() {
    if out "$1" | grep -Eqx "$2"; then
        ok
    else
        not_ok "step $1: no line '$2'"
    fi
}

# value IMAGE KIND PATH: the expected number for one path
value() {
    expected "$1" "$2" | P=$3 awk '$2 == ENVIRON["P"] { print $1 }'
}

# rc_bat DRIVE: rc.bat, which records the errorlevel of step %1 in
# DRIVE:\R%1.TXT.  It uses goto rather than "if errorlevel N echo ...
# >file" because the DOSBox-X shell opens the redirection even when the
# test is false.
rc_bat() {
    cat <<EOS
@echo off
if errorlevel 3 goto rc3
if errorlevel 2 goto rc2
if errorlevel 1 goto rc1
echo rc0 >$1:\\r%1.txt
goto end
:rc1
echo rc1 >$1:\\r%1.txt
goto end
:rc2
echo rc2 >$1:\\r%1.txt
goto end
:rc3
echo rc3 >$1:\\r%1.txt
:end
EOS
}
