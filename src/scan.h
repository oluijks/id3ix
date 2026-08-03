#ifndef ID3IX_SCAN_H
#define ID3IX_SCAN_H

/* Walking a directory tree and reporting the tags of the files in it. This is
 * deliberately the only part of the program that touches the filesystem: the
 * tag parsers work on paths and bytes, which keeps them testable without
 * needing files on disk. */

/* Recursively walks 'path', reading and printing the tag of every .mp3 file
 * found. Returns 0 on success, or -1 if 'path' itself could not be opened.
 * Directories encountered further down that cannot be opened are reported and
 * skipped rather than failing the whole walk. */
int scan_directory(const char *path);

#endif /* ID3IX_SCAN_H */
