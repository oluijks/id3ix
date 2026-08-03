#ifndef ID3IX_SCAN_H
#define ID3IX_SCAN_H

/* Walking a directory tree and reporting the tags of the files in it. This is
 * deliberately the only part of the program that touches the filesystem: the
 * tag parsers work on paths and bytes, which keeps them testable without
 * needing files on disk. */

/* What a walk found. Counted whether or not it is asked for, since counting
 * costs nothing next to reading the files. */
struct scan_totals
{
    unsigned long files;

    /* One of these is incremented for every file, so together they add up to
     * 'files'. */
    unsigned long v23;
    unsigned long v24;
    unsigned long no_tag;
    unsigned long old_version;
    unsigned long malformed;
    unsigned long unsupported;
    unsigned long unreadable;

    /* Counted only among files whose tag could be read. A file with no tag has
     * no title either, and reporting that as a missing title would restate the
     * same problem in a second place and make both numbers useless. */
    unsigned long tagged;
    unsigned long missing_title;
    unsigned long missing_artist;
    unsigned long missing_album;
    unsigned long missing_track;
    unsigned long missing_year;
};

struct scan_options
{
    /* Print one line per file. Cleared by --summary, which wants the counts
     * rather than a line for every file in the collection. */
    int print_lines;

    struct scan_totals totals;
};

/* Reports the tags of 'path', accumulating into 'options->totals'.
 *
 * A directory is walked recursively, considering only files whose name looks
 * like an MP3. A file named directly is read whatever it is called: the walk
 * has to guess what is worth opening, an explicit argument does not, and
 * silently ignoring a file someone typed out would look like a broken tool.
 *
 * Returns 0, or -1 if anything could not be read. A failure part way through
 * does not abandon the rest: it is reported on stderr, the walk carries on,
 * and the caller turns the return value into an exit status at the end.
 * Stopping at the first unreadable file in a collection of thousands would
 * make the tool useless for the job it exists to do. */
int scan_path(const char *path, struct scan_options *options);

/* Writes the counts to stdout in a form meant to be read rather than parsed.
 * The per-file output is what to pipe into other tools; this is the view for
 * finding out how big the problem is before deciding what to do about it. */
void scan_print_summary(const struct scan_totals *totals);

#endif /* ID3IX_SCAN_H */
