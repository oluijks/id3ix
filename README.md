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

## Development

The version number lives in one place: `VERSION` at the top of the `Makefile`,
passed to the compiler as `-DID3IX_VERSION`. Edit it there and rebuild; `make
test` checks the binary reports the version the Makefile declares.

- `make test` — run the CLI smoke tests in `tests/run.sh`
- `make format` — format source with clang-format
- `make format-check` — verify formatting without changing files (used in CI)
- `make lint` — run cppcheck and shellcheck
- `make clean` — remove build artifacts
- `make install` / `make uninstall` — install to `$(PREFIX)/bin` (default `/usr/local`)
