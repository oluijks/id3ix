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
- `-v`, `--version` — show version information

Help and version output go to stdout and exit `0`; usage errors go to stderr
and exit `1`.

## Development

- `make test` — run the CLI smoke tests in `tests/run.sh`
- `make format` — format source with clang-format
- `make format-check` — verify formatting without changing files (used in CI)
- `make lint` — run cppcheck and shellcheck
- `make clean` — remove build artifacts
- `make install` / `make uninstall` — install to `$(PREFIX)/bin` (default `/usr/local`)
