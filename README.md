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

## Development

- `make format` — format source with clang-format
- `make format-check` — verify formatting without changing files (used in CI)
- `make lint` — run cppcheck static analysis
- `make clean` — remove build artifacts
- `make install` / `make uninstall` — install to `$(PREFIX)/bin` (default `/usr/local`)
