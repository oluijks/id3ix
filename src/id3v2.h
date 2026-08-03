#ifndef ID3IX_ID3V2_H
#define ID3IX_ID3V2_H

/* Reading ID3v2 tags, which live at the start of the file and hold their data
 * in variable-length frames. ID3v1, the fixed 128-byte block at the end of the
 * file, is a separate format and would belong in its own translation unit. */

/* Longest text kept for a single field, including the null terminator. Longer
 * values are truncated rather than rejected. */
#define ID3V2_FIELD_MAX 256

enum id3v2_result
{
    ID3V2_OK = 0,
    ID3V2_ENOFILE = -1,     /* could not be opened or read */
    ID3V2_ENOTAG = -2,      /* no ID3v2 tag present */
    ID3V2_EVERSION = -3,    /* a tag version this code does not handle */
    ID3V2_EMALFORMED = -4,  /* the tag contradicts itself */
    ID3V2_EUNSUPPORTED = -5 /* a tag feature this code does not handle */
};

/* Extracted fields. Any frame that was absent, unparseable, or in an encoding
 * that is not handled is left as an empty string, so callers can test
 * tag.title[0] rather than tracking which frames were seen. */
struct id3v2_tag
{
    char title[ID3V2_FIELD_MAX];  /* TIT2 */
    char artist[ID3V2_FIELD_MAX]; /* TPE1 */
    char album[ID3V2_FIELD_MAX];  /* TALB */
    char year[ID3V2_FIELD_MAX];   /* TYER in v2.3, TDRC in v2.4 */
    char track[ID3V2_FIELD_MAX];  /* TRCK */
};

/* Reads the ID3v2 tag at the start of the file at 'path'.
 *
 * 'out' is zeroed before anything else, so it is safe to read on any return
 * value. Returns ID3V2_OK on success or one of the negative enum id3v2_result
 * values on failure. A file with no tag is ID3V2_ENOTAG, not an error in the
 * caller's sense; scanning a directory is expected to hit plenty of those. */
int id3v2_read(const char *path, struct id3v2_tag *out);

/* A short description of a value returned by id3v2_read, for messages. The
 * returned string is statically allocated and must not be freed. */
const char *id3v2_strerror(int result);

#endif /* ID3IX_ID3V2_H */
