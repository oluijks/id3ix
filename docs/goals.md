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
path <TAB> status <TAB> title <TAB> artist <TAB> album <TAB> track <TAB> year
```

The status column is the tag version — `2.3` or `2.4` — or, where there are no
fields to show, the reason: `none`, `notaudio`, `malformed`, `oldversion`,
`unsupported`, `unreadable`.

`none` and `notaudio` are worth keeping apart. `none` is a recording nobody
tagged. `notaudio` is a file that is not a recording at all, whatever it is
named — most often a download that returned an error page. Both have no tag;
only one of them is a tagging problem.

Every file gets exactly one line including the broken ones, since those are the
point of the exercise and a report that quietly dropped them would be worse
than no report.

Short words rather than sentences, because this is something to match on.

One line per file matters more than it looks. It means the output can be piped
into the tools that already exist, so questions nobody built a feature for can
still be answered:

```sh
id3ix scan ~/Music | awk -F'\t' '$5==""' | wc -l     # files with no album
id3ix scan ~/Music | cut -f2 | sort | uniq -c        # what the collection is
id3ix scan ~/Music | awk -F'\t' '$2=="none"'         # files with no tag
```

The tool turns opaque binary into lines of text. Established tools do the rest.

Nothing else goes to stdout — no progress line, no banner — or every caller
would have to filter it back out. Diagnostics go to stderr.

Fields cannot contain a tab or a newline: control characters are discarded as
the text is decoded, which also stops a crafted tag from writing terminal
escape sequences to whoever runs this.

What `scan` accepts, and the conventions it borrows from other Unix tools, is
described in [cli.md](cli.md).

### The summary

`--summary` prints counts instead of a line per file: how many files, how they
break down by tag version or by the reason there is no tag, and among the ones
that do have a tag, how many are missing each field.

```
1247 files

  ID3v2.3        1004
  ID3v2.4         107
  no tag           83
  ID3v2.2          12
  malformed         1
  unsupported       0
  not an MP3        3
  unreadable        0

1123 with a tag, of which

  no title          9
  no artist        14
  no album        213
  no track        402
  no year         388
```

Missing fields are counted only among files whose tag could be read. A file
with no tag has no title either, and counting that as a missing title would
restate the same problem in a second place and leave both numbers meaning
nothing.

Every row is printed even at zero, so the shape of the report does not change
with its contents and this week's can be compared against last week's.

This is the one view meant to be read rather than piped. It answers "how bad is
it" before deciding what to do, which is the question the per-file lines are
bad at.

**Done when** it can be pointed at a real music folder and the output looks
right.

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
- **Unsynchronised tags**, which are reported as unsupported rather than
  misread.
- **Cyrillic written as Windows-1251 but labelled Latin-1.** Legacy Russian and
  Ukrainian taggers did this routinely, and the tag gives no way to tell it
  apart from real Latin-1 — both claim encoding 0x00. It needs a heuristic, and
  a heuristic is worth writing against a real collection rather than in the
  abstract. Properly encoded UTF-16 and UTF-8 Cyrillic both work today.

## Where it is now

Stage 1 is done. `scan` takes any number of files and directories, walks the
directories in sorted order, reads ID3v2.3 and ID3v2.4 tags, and prints a line
per file or a set of counts. Text arrives as UTF-8 whichever of the four
encodings a tag used, so Cyrillic and accented Latin both come out readable.

Stage 2 has not been started. That is the next thing.
