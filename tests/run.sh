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

# Version is explicitly requested output: stdout, exit 0.
assert_status 0 '--version exits 0' "$BIN" --version
assert_status 0 '-v exits 0' "$BIN" -v
assert_stdout_contains 'id3ix' '--version writes the name to stdout' "$BIN" --version

# `make test` passes VERSION in from the Makefile, which is the single source of
# truth for it. Check the binary reports that exact version, so a build that
# missed -DID3IX_VERSION and fell back to "unknown" fails here.
if [ -n "${VERSION:-}" ]
then
    assert_stdout_contains "id3ix $VERSION" '--version matches the Makefile VERSION' "$BIN" --version
fi

# scan requires a directory argument.
assert_status 1 'scan without a path exits 1' "$BIN" scan
assert_stderr_contains 'requires a directory' 'scan without a path explains why' "$BIN" scan
assert_status 0 'scan with a path exits 0' "$BIN" scan "$script_dir"
assert_stdout_contains 'Scanning' 'scan with a path reports progress' "$BIN" scan "$script_dir"

# Unknown commands are an error, and the message names the offending command.
assert_status 1 'unknown command exits 1' "$BIN" bogus
assert_stderr_contains "unknown command 'bogus'" 'unknown command is named on stderr' "$BIN" bogus
assert_stdout_empty 'unknown command writes nothing to stdout' "$BIN" bogus

printf '\n%d checks, %d failures\n' "$checks" "$failures"

if [ "$failures" -ne 0 ]
then
    exit 1
fi
