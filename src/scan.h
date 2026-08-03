#ifndef ID3IX_SCAN_H
#define ID3IX_SCAN_H

/* Walking a directory tree and reporting the tags of the files in it. This is
 * deliberately the only part of the program that touches the filesystem: the
 * tag parsers work on paths and bytes, which keeps them testable without
 * needing files on disk. */

/* Reports the tags of 'path'.
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
int scan_path(const char *path);

#endif /* ID3IX_SCAN_H */
