# Command line interface

What the commands accept, and why. Most of these are not original decisions —
they are what `grep`, `find`, `ls` and `du` already do, and matching them means
nobody has to read this file to guess right.

## Synopsis

```
id3ix scan <path>...
id3ix -h | --help
id3ix -V | --version
```

## What scan takes

**One or more paths.** Each may be a file or a directory, mixed freely:

```sh
id3ix scan album.mp3
id3ix scan track1.mp3 track2.mp3
id3ix scan ~/Music
id3ix scan ~/Music/Beatles ~/Music/Bowie one-off.mp3
```

Accepting a list is what makes shell expansion work, and that is most of the
value:

```sh
id3ix scan *.mp3
find ~/Music -name '*.mp3' -newer /tmp/marker | xargs id3ix scan
```

The shell expands `*.mp3` into a list of paths before the program starts, so
"throw a pile of files at it" costs nothing beyond accepting more than one
argument.

**A directory** is walked recursively.

**At least one path is required.** Several tools default to the current
directory when given nothing (`ls`, `du`). This one does not, on the grounds
that a tool which may eventually modify files should never act on a target the
user did not name. It costs one word to type and removes a category of
accident.

## Filter when discovering, not when told

A directory walk considers only files that look like MP3s. A file named
directly on the command line is read whatever it is called.

This is how `grep` behaves: `grep -r pattern dir/` skips things it decides are
uninteresting, while `grep pattern weirdfile` searches exactly what you named.
Discovery has to guess. An explicit instruction does not need guessing, and
silently ignoring a file the user typed out is the worst possible response —
no output, no error, and the tool looks broken.

So `id3ix scan recording.mpeg` reads it, and so does `id3ix scan TRACK.MP3`,
even though neither would have been picked up by a walk.

## Symlinks are not followed

The walk uses `lstat`, so a symlink is seen as a symlink and skipped rather
than being followed to whatever it points at.

The reason is loops: a link pointing back up its own tree would make the walk
recurse forever. `find` and `grep -r` default the same way, both offering an
opt-in flag to follow (`-L`, `-R`). That flag is worth adding here eventually;
it is not worth adding before the tool does anything useful.

## Entries are sorted within a directory

`readdir` returns entries in whatever order the filesystem stores them, which
is neither alphabetical nor stable. On a test directory it returned:

```
bravo.mp3  alpha.mp3  zulu.mp3  mike.mp3
```

Output that shifts between runs cannot be diffed, cannot be compared before and
after a change, and makes tests depend on the filesystem. `ls` sorts for the
same reason. Entries are therefore sorted before being processed.

Paths given on the command line are processed in the order given, not sorted —
if you asked for a specific order, that was the request.

## Exit codes

| code | meaning |
|---|---|
| 0 | ran to completion |
| 1 | usage error: no command, unknown command, missing argument |
| 2 | one or more paths could not be read |

A path that fails does not abandon the rest. The failure is reported on stderr,
the remaining paths are still processed, and the non-zero status shows up at
the end. Giving up on the first unreadable file in a collection of thousands
would be useless.

Code 1 for usage errors is what the tool already does and what its tests
assert. Code 2 keeps "you typed it wrong" distinguishable from "something in
the collection could not be read", which matters in a script.

## Output

Reports go to stdout, diagnostics to stderr. Explicitly requested output —
`--help`, `--version` — goes to stdout and exits 0; usage errors go to stderr
and exit non-zero.

That split is what makes `id3ix scan ~/Music > report.txt` leave the errors
visible on the terminal instead of burying them in the file.

The report format itself is described in [goals.md](goals.md): one line per
file, tab separated, empty field for anything missing.

## Not supported, deliberately

- **Reading paths from stdin.** `xargs` already does this well and composes
  with `find`, which handles the cases a built-in version would get wrong.
- **`-` meaning stdin.** An ID3 parser needs to seek; a pipe cannot.
- **Recursion depth limits or exclusion patterns.** `find` does this better.
  Pipe it in.

## Undecided

- **`--` to end option parsing**, for paths that begin with a dash. Standard,
  cheap, worth adding once there are enough options for it to matter.
- **AppleDouble files.** Music copied to exFAT or FAT drives on macOS grows
  companion files named `._track.mp3`, which end in `.mp3`, are not audio, and
  will show up in a walk as junk. Whether to skip them by default or report
  them as a finding is a real question, and worth deciding against a real
  collection rather than in the abstract.
