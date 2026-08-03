# id3ix

A command-line metadata utility.

## Building

```sh
make
```

Requires a C17-compatible compiler (`clang` by default; override with `make CC=cc`).

## Usage

```sh
id3ix scan <directory>
```

Options:

- `-h`, `--help` — show help
- `-V`, `--version` — show version information

Help and version output go to stdout and exit `0`; usage errors go to stderr
and exit `1`.

## Layout

```
src/main.c     command line handling
src/scan.c     directory walking, the only code that touches the filesystem
src/id3v2.c    ID3v2 tag parsing, works from a path and bytes
tests/run.sh   CLI smoke tests
```

Keeping the walk and the parsing apart means the parser can be exercised
without files on disk, which matters because the cases worth testing are
malformed tags and those are easier to build as byte arrays than as real files.

## Development

The version number lives in one place: `VERSION` at the top of the `Makefile`,
passed to the compiler as `-DID3IX_VERSION`. Edit it there and rebuild; `make
test` checks the binary reports the version the Makefile declares.

- `make test` — run the CLI smoke tests in `tests/run.sh`
- `make debug` — build `id3ix-debug` with AddressSanitizer and
  UndefinedBehaviorSanitizer
- `make test-debug` — run the same test suite against the sanitized binary

The sanitized build catches memory errors and undefined behaviour that a normal
build compiles without complaint. Use it while working on anything that reads
bytes out of a file. gcc ships the sanitizer runtimes with the compiler; with
clang they live in a separate `libclang-rt-*-dev` package.
- `make format` — format source with clang-format
- `make format-check` — verify formatting without changing files (used in CI)
- `make lint` — run cppcheck and shellcheck
- `make clean` — remove build artifacts
- `make install` / `make uninstall` — install to `$(PREFIX)/bin` (default `/usr/local`)
