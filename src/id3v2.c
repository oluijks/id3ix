#include "id3v2.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* The tag header is always 10 bytes:
 *
 *   0-2  "ID3"
 *   3    major version (3 for v2.3, 4 for v2.4)
 *   4    revision
 *   5    flags
 *   6-9  size of everything after this header, synchsafe
 */
#define ID3V2_HEADER_SIZE 10

/* A synchsafe integer stores 7 bits per byte with the top bit always clear, so
 * the encoded value can never contain a byte that a decoder might mistake for
 * an MP3 frame sync. That caps a tag at 2^28 bytes, 256 MiB. */
static uint32_t decode_synchsafe(const unsigned char *bytes)
{
    return ((uint32_t)(bytes[0] & 0x7F) << 21) |
           ((uint32_t)(bytes[1] & 0x7F) << 14) |
           ((uint32_t)(bytes[2] & 0x7F) << 7) | (uint32_t)(bytes[3] & 0x7F);
}

int id3v2_read(const char *path, struct id3v2_tag *out)
{
    unsigned char header[ID3V2_HEADER_SIZE];
    unsigned int major;
    uint32_t tag_size;
    FILE *file;

    /* Zero first, so 'out' is safe to read whatever happens after this. */
    memset(out, 0, sizeof(*out));

    file = fopen(path, "rb");
    if (file == NULL)
    {
        return ID3V2_ENOFILE;
    }

    if (fread(header, 1, sizeof(header), file) != sizeof(header))
    {
        /* Too short to hold a header, so it cannot hold a tag. */
        fclose(file);

        return ID3V2_ENOTAG;
    }

    if (memcmp(header, "ID3", 3) != 0)
    {
        fclose(file);

        return ID3V2_ENOTAG;
    }

    major = header[3];
    if (major != 3 && major != 4)
    {
        /* v2.2 exists but uses 3-byte frame IDs and 3-byte sizes, so it needs
         * its own frame loop rather than a tweak to the one below. */
        fclose(file);

        return ID3V2_EVERSION;
    }

    tag_size = decode_synchsafe(header + 6);

    /* TODO: walk the frames.
     *
     * Everything from here to tag_size bytes further on is a sequence of
     * frames, each one a 10-byte header followed by its data:
     *
     *   0-3  frame ID, four characters, e.g. "TIT2"
     *   4-7  size of the data after this header
     *   8-9  flags
     *
     * The frame size is encoded differently by version, and this is the
     * detail that quietly breaks parsers: in v2.3 it is a plain big-endian
     * uint32, in v2.4 it is synchsafe like the tag size above. Branch on
     * 'major'. Getting it wrong does not fail cleanly; it lands mid-frame and
     * reads bytes that still look structurally plausible.
     *
     * Frames whose ID starts with 'T' are text. Their first data byte is the
     * encoding: 0x00 Latin-1, 0x01 UTF-16 with BOM, 0x02 UTF-16BE, 0x03 UTF-8.
     * Handling 0x00 and 0x03 covers most files; skip the frame otherwise
     * rather than storing bytes in an encoding this code cannot interpret.
     *
     * The ones worth storing: TIT2 title, TPE1 artist, TALB album, TRCK track,
     * and for the year TYER on v2.3 or TDRC on v2.4.
     *
     * A frame ID beginning with a zero byte means padding has started; stop.
     *
     * Bound every read against the bytes remaining inside the tag, not against
     * the file size, and treat a frame that claims more than that as
     * ID3V2_EMALFORMED. The size fields are attacker-controlled: a file is
     * free to claim a 4 GB frame inside a 200-byte tag. Frame data is not
     * null-terminated either, so copy a bounded length into the struct's
     * fields and terminate it yourself rather than reaching for strcpy.
     *
     * Build with `make debug` while working on this. A read past the end of a
     * buffer here will usually not crash; it will return plausible-looking
     * adjacent memory, and the sanitized build is what turns that from
     * invisible into an error with a line number.
     */
    (void)tag_size;

    fclose(file);

    return ID3V2_OK;
}

const char *id3v2_strerror(int result)
{
    switch (result)
    {
    case ID3V2_OK:
        return "ok";
    case ID3V2_ENOFILE:
        return "could not be read";
    case ID3V2_ENOTAG:
        return "no ID3v2 tag";
    case ID3V2_EVERSION:
        return "unsupported ID3v2 version";
    case ID3V2_EMALFORMED:
        return "malformed ID3v2 tag";
    default:
        return "unknown error";
    }
}
