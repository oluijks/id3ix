# What id3ix is for

## The problem

I have a music collection and I cannot tell what is wrong with it.

The tools I tried did not help. They show one file at a time, or a grid of
rows to read with your eyes. That works when you already know which file is
broken. It does not work for a few thousand files where the problem is
somewhere in the pile and you do not know what you are looking for.

So the goal is narrower and more useful than "a tag editor":

> Point it at a directory and have it tell me what is kinda wrong.

Emphasis on *kinda*. Not just missing fields — the softer problems that other
tools count as fine.

## The shape of it

Three stages, deliberately kept apart:

1. **Observe** — read the files and report what is actually in them.
2. **Diagnose** — say what looks wrong, and what is probably correct instead.
3. **Apply** — change the files. Not yet, and possibly never.

Stages 1 and 2 cannot damage anything. That is the point of the split: all the
interesting work lives where mistakes are free, and the dangerous part is a
separate thing that does not exist yet.

## Stage 1: observe

Read the tags and print one line per file, tab separated, empty field for
anything missing:

```
path <TAB> version <TAB> title <TAB> artist <TAB> album
```

One line per file matters more than it looks. It means the output can be piped
into the tools that already exist, so questions nobody built a feature for can
still be answered:

```sh
id3ix scan ~/Music | awk -F'\t' '$5==""' | wc -l     # files with no album
id3ix scan ~/Music | cut -f2 | sort | uniq -c        # which tag versions
```

The tool turns opaque binary into lines of text. Established tools do the rest.

What `scan` accepts, and the conventions it borrows from other Unix tools, is
described in [cli.md](cli.md).

**Done when** it can be pointed at a real music folder and the output looks
right.

Needs: the ID3v2 frame loop, which is the TODO in `src/id3v2.c`.

## Stage 2: diagnose

Report problems per directory, since in a music collection a directory is
almost always an album, and everything in it should agree.

Something like:

```
~/Music/Beatles/Revolver — 13 files
  album inconsistent    "Revolver" x11, "revolver" x2
  track gap             missing 3; tags claim 1/14 but 13 files present
  junk value            2 files: artist = "Unknown Artist"
  whitespace            1 file: title has a trailing space
  mixed versions        v2.3 x12, v2.2 x1
```

Two kinds of problem are worth catching, and both are ones a per-file view
cannot show:

**Junk values.** Fields that are technically filled in but useless.
`Unknown Artist`, `Track 01`, `untitled`, a year of `0000`, a title that is
just the filename. These pass an "is it empty?" check and tell you nothing.

**Disagreements between files.** Inconsistent album or artist spelling,
trailing whitespace, track numbers with gaps or duplicates, a track count that
does not match the number of files, mixed tag versions, titles with the track
number embedded in them. None of these are visible in any single file. The
wrongness is in the relationship.

Where the tool can tell which value is probably right — 11 files against 2 —
it should say so. Suggesting costs nothing and requires no write access.

**Done when** it says something about the collection that was not already
obvious.

## Stage 3: apply

Deliberately undecided.

Writing tags can destroy a collection permanently, and quickly. If it ever
happens it needs, at minimum: a dry run by default so changing files is an
explicit choice, writes to a temporary file followed by a rename rather than
editing in place, and testing on copies.

It is also worth noticing that stage 2 might be enough. Knowing exactly what is
wrong and where may be all that was ever missing — the fixing could be done by
hand, or by an existing tool now that there is something to aim it at.

## Deliberately not doing

- **Writing tags**, for now. See above.
- **Looking anything up online.** No network. The answers are usually already
  in the collection, in the other files in the same folder.
- **Being a general tag editor.** Plenty exist. The gap is diagnosis.
- **ID3v2.2 and ID3v1**, for now. Both are real and both turn up; neither is
  worth handling before v2.3 and v2.4 work properly.

## Where it is now

The CLI, the build, the tests and the file layout exist. `scan` walks
directories, finds `.mp3` files, and reads the ID3v2 header — version, and the
size of the tag.

The frame loop is not written yet, so no actual titles or artists come out.
That is the next thing, and everything above waits on it.
