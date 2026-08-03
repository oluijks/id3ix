#!/bin/sh
#
# Smoke tests for the id3ix CLI.
#
# Runs the built binary through each supported invocation and checks the exit
# status and where the output goes. Usage errors belong on stderr with a
# non-zero exit; explicitly requested output (--help, --version) belongs on
# stdout with exit 0.
#
# Run with: make test

set -u
unset CDPATH

script_dir=$(cd -- "$(dirname -- "$0")" && pwd)
BIN="${BIN:-$script_dir/../id3ix}"

failures=0
checks=0

# assert_status <expected> <description> <command...>
assert_status()
{
    expected="$1"
    description="$2"
    shift 2

    "$@" >/dev/null 2>&1
    actual=$?
    checks=$((checks + 1))

    if [ "$actual" -eq "$expected" ]
    then
        printf 'ok   %s\n' "$description"
    else
        printf 'FAIL %s (expected exit %d, got %d)\n' "$description" "$expected" "$actual"
        failures=$((failures + 1))
    fi
}

# assert_stdout_contains <pattern> <description> <command...>
assert_stdout_contains()
{
    pattern="$1"
    description="$2"
    shift 2

    output=$("$@" 2>/dev/null)
    checks=$((checks + 1))

    case "$output" in
        *"$pattern"*)
            printf 'ok   %s\n' "$description"
            ;;
        *)
            printf 'FAIL %s (stdout did not contain "%s")\n' "$description" "$pattern"
            failures=$((failures + 1))
            ;;
    esac
}

# assert_stderr_contains <pattern> <description> <command...>
assert_stderr_contains()
{
    pattern="$1"
    description="$2"
    shift 2

    output=$("$@" 2>&1 >/dev/null)
    checks=$((checks + 1))

    case "$output" in
        *"$pattern"*)
            printf 'ok   %s\n' "$description"
            ;;
        *)
            printf 'FAIL %s (stderr did not contain "%s")\n' "$description" "$pattern"
            failures=$((failures + 1))
            ;;
    esac
}

# assert_stdout_empty <description> <command...>
assert_stdout_empty()
{
    description="$1"
    shift

    output=$("$@" 2>/dev/null)
    checks=$((checks + 1))

    if [ -z "$output" ]
    then
        printf 'ok   %s\n' "$description"
    else
        printf 'FAIL %s (expected empty stdout, got "%s")\n' "$description" "$output"
        failures=$((failures + 1))
    fi
}

if [ ! -x "$BIN" ]
then
    printf 'error: %s not found or not executable; run make first\n' "$BIN" >&2
    exit 1
fi

# No arguments: usage error on stderr, nothing on stdout.
assert_status 1 'no args exits 1' "$BIN"
assert_stderr_contains 'Usage:' 'no args writes usage to stderr' "$BIN"
assert_stdout_empty 'no args writes nothing to stdout' "$BIN"

# Help is explicitly requested output: stdout, exit 0.
assert_status 0 '--help exits 0' "$BIN" --help
assert_status 0 '-h exits 0' "$BIN" -h
assert_stdout_contains 'Usage:' '--help writes usage to stdout' "$BIN" --help
assert_stdout_contains '--version' '--help lists the version flag' "$BIN" --help

# Version is explicitly requested output: stdout, exit 0. The short flag is -V,
# not -v, which stays free for a future --verbose on scan.
assert_status 0 '--version exits 0' "$BIN" --version
assert_status 0 '-V exits 0' "$BIN" -V
assert_stdout_contains 'id3ix' '--version writes the name to stdout' "$BIN" --version

# `make test` passes VERSION in from the Makefile, which is the single source of
# truth for it. Check the binary reports that exact version, so a build that
# missed -DID3IX_VERSION and fell back to "unknown" fails here.
if [ -n "${VERSION:-}" ]
then
    assert_stdout_contains "id3ix $VERSION" '--version matches the Makefile VERSION' "$BIN" --version
fi

# scan requires at least one path.
assert_status 1 'scan without a path exits 1' "$BIN" scan
assert_stderr_contains 'requires a path' 'scan without a path explains why' "$BIN" scan
assert_status 0 'scan with a path exits 0' "$BIN" scan "$script_dir"

# stdout carries one line per file and nothing else. This directory holds the
# test script and no music, so a successful scan of it says nothing at all --
# any progress message or banner would show up here.
assert_stdout_empty 'scan of a directory with no music says nothing' "$BIN" scan "$script_dir"

# A tree to walk. The files need no valid tag: every one still gets a line, and
# these tests are about which lines appear and in what order.
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT INT TERM

mkdir -p "$work/tree/inner" "$work/other"
for name in zulu alpha mike bravo
do
    : > "$work/tree/$name.mp3"
done
: > "$work/tree/inner/nested.mp3"
: > "$work/tree/notes.txt"
: > "$work/other/single.mp3"
: > "$work/plain-file"

# Entries within a directory come out sorted, whatever order readdir used. The
# names were created in the order zulu, alpha, mike, bravo.
#
# nested.mp3 appears in the middle because the subdirectory holding it is named
# "inner", which sorts between bravo and mike, and the walk descends where it
# finds a directory rather than saving them for the end. That is the intended
# order: an album's files stay next to the directory they came from.
assert_stdout_contains 'alpha.mp3 bravo.mp3 nested.mp3 mike.mp3 zulu.mp3' \
    'scan sorts directory entries, descending in place' \
    sh -c "\"$BIN\" scan \"$work/tree\" | cut -f1 | xargs -n1 basename | grep '\\.mp3$' | tr '\\n' ' '"

# Recursion reaches a subdirectory, and non-MP3 names found by the walk are
# skipped.
assert_stdout_contains 'nested.mp3' 'scan recurses into subdirectories' \
    sh -c "\"$BIN\" scan \"$work/tree\" | cut -f1"
assert_stdout_empty 'walk skips names that are not MP3s' \
    sh -c "\"$BIN\" scan \"$work/tree\" | cut -f1 | grep notes.txt"

# A file named directly is read whatever it is called, unlike one merely found.
assert_stdout_contains 'plain-file' 'a file named directly is read regardless of name' \
    "$BIN" scan "$work/plain-file"

# Several paths at once, in the order given rather than sorted.
assert_status 0 'scan accepts several paths' "$BIN" scan "$work/other" "$work/tree"
assert_stdout_contains 'single.mp3' 'several paths are all reported' \
    sh -c "\"$BIN\" scan \"$work/other\" \"$work/tree\" | cut -f1"

# An unreadable path is its own exit code, distinct from a usage error, and it
# does not stop the paths that follow it.
assert_status 2 'an unreadable path exits 2' "$BIN" scan "$work/no-such-thing"
assert_stdout_contains 'single.mp3' 'a bad path does not abandon the rest' \
    sh -c "\"$BIN\" scan \"$work/no-such-thing\" \"$work/other\" | cut -f1"
assert_status 2 'one bad path among good ones still exits 2' \
    "$BIN" scan "$work/other" "$work/no-such-thing"

# Every line has the same number of fields, so the output can be cut and awked.
assert_stdout_contains '7' 'every line has seven tab separated fields' \
    sh -c "\"$BIN\" scan \"$work/tree\" | awk -F'\t' '{print NF}' | sort -u"

# Unknown commands are an error, and the message names the offending command.
assert_status 1 'unknown command exits 1' "$BIN" bogus
assert_stderr_contains "unknown command 'bogus'" 'unknown command is named on stderr' "$BIN" bogus
assert_stdout_empty 'unknown command writes nothing to stdout' "$BIN" bogus

printf '\n%d checks, %d failures\n' "$checks" "$failures"

if [ "$failures" -ne 0 ]
then
    exit 1
fi
