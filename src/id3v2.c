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

/* Each frame begins with a header of the same length:
 *
 *   0-3  frame ID, four characters, e.g. "TIT2"
 *   4-7  size of the data that follows
 *   8-9  flags
 */
#define ID3V2_FRAME_HEADER_SIZE 10

/* Header flag bits that change how the rest of the tag must be read. */
#define ID3V2_FLAG_UNSYNCHRONISED 0x80
#define ID3V2_FLAG_EXTENDED_HEADER 0x40

/* Most bytes read from one frame's data. Anything longer is truncated and the
 * remainder skipped: no title needs half a kilobyte, and refusing to read more
 * keeps a frame claiming megabytes from mattering. */
#define ID3V2_TEXT_READ_MAX 512

/* How far into a file to look for the start of MPEG audio before concluding
 * there is none. A file with no ID3v2 tag ought to begin with audio at once;
 * the window is generous because calling a real recording "not audio" is a
 * worse mistake than the reverse. */
#define ID3V2_SYNC_SEARCH_MAX 4096

/* Text frame encodings, given by the first byte of a text frame's data. */
#define ID3V2_ENCODING_LATIN1 0x00
#define ID3V2_ENCODING_UTF16_BOM 0x01
#define ID3V2_ENCODING_UTF16_BE 0x02
#define ID3V2_ENCODING_UTF8 0x03

/* A synchsafe integer stores 7 bits per byte with the top bit always clear, so
 * the encoded value can never contain a byte that a decoder might mistake for
 * an MP3 frame sync. That caps a tag at 2^28 bytes, 256 MiB. */
static uint32_t decode_synchsafe(const unsigned char *bytes)
{
    return ((uint32_t)(bytes[0] & 0x7F) << 21) |
           ((uint32_t)(bytes[1] & 0x7F) << 14) |
           ((uint32_t)(bytes[2] & 0x7F) << 7) | (uint32_t)(bytes[3] & 0x7F);
}

/* An ordinary big-endian 32-bit integer, all 8 bits of each byte used. */
static uint32_t decode_u32be(const unsigned char *bytes)
{
    return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
           ((uint32_t)bytes[2] << 8) | (uint32_t)bytes[3];
}

/* The frame size encoding is the one real difference between v2.3 and v2.4,
 * and the one most worth getting right: a wrong size does not fail cleanly, it
 * leaves the next read starting partway through a frame, where the bytes still
 * look like a plausible frame header. Note the tag header's own size is
 * synchsafe in both versions; only frame sizes differ. */
static uint32_t decode_frame_size(unsigned int major,
                                  const unsigned char *bytes)
{
    return major == 3 ? decode_u32be(bytes) : decode_synchsafe(bytes);
}

/* Frame IDs are upper case letters and digits. Anything else means the walk is
 * no longer aligned to a frame boundary, so the tag cannot be trusted. */
static int frame_id_is_plausible(const unsigned char *id)
{
    int i;

    for (i = 0; i < 4; i++)
    {
        if (!((id[i] >= 'A' && id[i] <= 'Z') || (id[i] >= '0' && id[i] <= '9')))
        {
            return 0;
        }
    }

    return 1;
}

/* Which struct field a frame belongs in, or NULL for the many frames that are
 * of no interest here. Returning the destination rather than a flag keeps the
 * caller from needing to know which frames exist. */
static char *field_for_frame(struct id3v2_tag *tag, const unsigned char *id,
                             unsigned int major)
{
    if (memcmp(id, "TIT2", 4) == 0)
    {
        return tag->title;
    }

    if (memcmp(id, "TPE1", 4) == 0)
    {
        return tag->artist;
    }

    if (memcmp(id, "TALB", 4) == 0)
    {
        return tag->album;
    }

    if (memcmp(id, "TRCK", 4) == 0)
    {
        return tag->track;
    }

    /* The year frame was renamed between versions, and its meaning widened:
     * TYER holds four digits, TDRC holds a timestamp that may be just a year
     * or may be a full date and time. */
    if (major == 3 && memcmp(id, "TYER", 4) == 0)
    {
        return tag->year;
    }

    if (major == 4 && memcmp(id, "TDRC", 4) == 0)
    {
        return tag->year;
    }

    return NULL;
}

/* Appends one code point to 'destination' as UTF-8. Returns 0 when there was
 * no room, so the caller stops rather than dropping characters silently in the
 * middle of a string.
 *
 * Control characters are discarded rather than encoded. A title has no use for
 * them, a tab or newline would break a tab separated line of output, and an
 * escape character would let a crafted tag write terminal escape sequences to
 * whoever runs this. */
static int append_utf8(char *destination, size_t destination_size,
                       size_t *position, uint32_t code_point)
{
    size_t needed;
    size_t at = *position;

    if (code_point < 0x20 || code_point == 0x7F)
    {
        return 1;
    }

    if (code_point < 0x80)
    {
        needed = 1;
    }
    else if (code_point < 0x800)
    {
        needed = 2;
    }
    else if (code_point < 0x10000)
    {
        needed = 3;
    }
    else
    {
        needed = 4;
    }

    /* One byte beyond 'needed' has to stay free for the terminator. */
    if (at + needed >= destination_size)
    {
        return 0;
    }

    switch (needed)
    {
    case 1:
        destination[at++] = (char)code_point;
        break;
    case 2:
        destination[at++] = (char)(0xC0 | (code_point >> 6));
        destination[at++] = (char)(0x80 | (code_point & 0x3F));
        break;
    case 3:
        destination[at++] = (char)(0xE0 | (code_point >> 12));
        destination[at++] = (char)(0x80 | ((code_point >> 6) & 0x3F));
        destination[at++] = (char)(0x80 | (code_point & 0x3F));
        break;
    default:
        destination[at++] = (char)(0xF0 | (code_point >> 18));
        destination[at++] = (char)(0x80 | ((code_point >> 12) & 0x3F));
        destination[at++] = (char)(0x80 | ((code_point >> 6) & 0x3F));
        destination[at++] = (char)(0x80 | (code_point & 0x3F));
        break;
    }

    *position = at;

    return 1;
}

/* Decodes UTF-16 into 'destination' as UTF-8. This is the encoding any tag
 * has to use for a script Latin-1 cannot spell -- Cyrillic, Greek, Japanese --
 * which in a v2.3 tag means UTF-16 or nothing, since v2.3 predates UTF-8. */
static void store_utf16(char *destination, size_t destination_size,
                        size_t *position, const unsigned char *data,
                        size_t length, int big_endian)
{
    size_t at;

    for (at = 0; at + 1 < length; at += 2)
    {
        uint32_t unit = big_endian ? ((uint32_t)data[at] << 8) | data[at + 1]
                                   : ((uint32_t)data[at + 1] << 8) | data[at];
        uint32_t code_point;

        /* The terminator is a null code unit, two bytes, not one. */
        if (unit == 0)
        {
            break;
        }

        if (unit >= 0xD800 && unit <= 0xDBFF)
        {
            /* A high surrogate: the code point is above the range a single
             * 16-bit unit can hold and is spelled with a second unit. */
            uint32_t low;

            if (at + 3 >= length)
            {
                break;
            }

            low = big_endian ? ((uint32_t)data[at + 2] << 8) | data[at + 3]
                             : ((uint32_t)data[at + 3] << 8) | data[at + 2];

            if (low < 0xDC00 || low > 0xDFFF)
            {
                break;
            }

            code_point = 0x10000 + ((unit - 0xD800) << 10) + (low - 0xDC00);
            at += 2;
        }
        else if (unit >= 0xDC00 && unit <= 0xDFFF)
        {
            /* A low surrogate with no high one before it. */
            break;
        }
        else
        {
            code_point = unit;
        }

        if (!append_utf8(destination, destination_size, position, code_point))
        {
            break;
        }
    }
}

/* Copies one text frame's data into 'destination', converting to UTF-8 and
 * terminating it. 'data' points at the frame's data including its leading
 * encoding byte, and is not null-terminated, so nothing here may use the
 * string functions that would look for one. */
static void store_text(char *destination, size_t destination_size,
                       const unsigned char *data, size_t length)
{
    unsigned char encoding;
    size_t read_index;
    size_t position = 0;

    if (destination_size < 1)
    {
        return;
    }

    destination[0] = '\0';

    if (length < 1)
    {
        return;
    }

    encoding = data[0];
    data++;
    length--;

    if (encoding == ID3V2_ENCODING_LATIN1)
    {
        /* Latin-1 maps each byte to the code point of the same value, so every
         * byte is already the code point it needs to be encoded from. */
        for (read_index = 0; read_index < length; read_index++)
        {
            if (data[read_index] == 0)
            {
                break;
            }

            if (!append_utf8(destination, destination_size, &position,
                             data[read_index]))
            {
                break;
            }
        }
    }
    else if (encoding == ID3V2_ENCODING_UTF8)
    {
        /* Already the target encoding, so the bytes are copied rather than
         * decoded. Bytes below 0x80 can be tested directly for control
         * characters because a UTF-8 continuation byte is never below 0x80. */
        for (read_index = 0; read_index < length; read_index++)
        {
            unsigned char byte = data[read_index];

            if (byte == 0)
            {
                break;
            }

            if (byte < 0x20 || byte == 0x7F)
            {
                continue;
            }

            if (position + 1 >= destination_size)
            {
                break;
            }

            destination[position++] = (char)byte;
        }
    }
    else if (encoding == ID3V2_ENCODING_UTF16_BOM)
    {
        /* A byte order mark should lead the text. Where one is missing the
         * spec's default applies, which is the same big-endian order that
         * encoding 0x02 states outright. */
        int big_endian = 1;

        if (length >= 2 && data[0] == 0xFF && data[1] == 0xFE)
        {
            big_endian = 0;
            data += 2;
            length -= 2;
        }
        else if (length >= 2 && data[0] == 0xFE && data[1] == 0xFF)
        {
            data += 2;
            length -= 2;
        }

        store_utf16(destination, destination_size, &position, data, length,
                    big_endian);
    }
    else if (encoding == ID3V2_ENCODING_UTF16_BE)
    {
        store_utf16(destination, destination_size, &position, data, length, 1);
    }
    else
    {
        /* An encoding byte outside the four the format defines. */
        return;
    }

    destination[position] = '\0';
}

/* Skips the optional extended header, which sits between the tag header and
 * the first frame. Its size is recorded differently by version: v2.3 gives the
 * size of what follows the size field, v2.4 gives a synchsafe size that
 * includes the field itself. Returns 0 on success. */
static int skip_extended_header(FILE *file, unsigned int major,
                                uint32_t *remaining)
{
    unsigned char size_bytes[4];
    uint32_t size;

    if (*remaining < sizeof(size_bytes))
    {
        return -1;
    }

    if (fread(size_bytes, 1, sizeof(size_bytes), file) != sizeof(size_bytes))
    {
        return -1;
    }

    *remaining -= (uint32_t)sizeof(size_bytes);

    size = major == 3 ? decode_u32be(size_bytes) : decode_synchsafe(size_bytes);

    if (major == 4)
    {
        if (size < sizeof(size_bytes))
        {
            return -1;
        }

        size -= (uint32_t)sizeof(size_bytes);
    }

    if (size > *remaining)
    {
        return -1;
    }

    if (size > 0 && fseek(file, (long)size, SEEK_CUR) != 0)
    {
        return -1;
    }

    *remaining -= size;

    return 0;
}

/* Whether these bytes begin an MPEG audio frame: eleven set bits, then fields
 * that must not hold the values the format reserves. The eleven bits alone
 * turn up too readily in arbitrary data to mean much on their own. */
static int is_frame_header(const unsigned char *bytes)
{
    if (bytes[0] != 0xFF || (bytes[1] & 0xE0) != 0xE0)
    {
        return 0;
    }

    if ((bytes[1] & 0x18) == 0x08)
    {
        return 0; /* reserved MPEG version */
    }

    if ((bytes[1] & 0x06) == 0x00)
    {
        return 0; /* reserved layer */
    }

    if ((bytes[2] & 0xF0) == 0xF0)
    {
        return 0; /* bitrate index the format calls bad */
    }

    if ((bytes[2] & 0x0C) == 0x0C)
    {
        return 0; /* reserved sampling rate */
    }

    return 1;
}

/* Whether the file holds MPEG audio anywhere near its start.
 *
 * Worth asking because a file with no ID3v2 tag and no audio either is not an
 * untagged recording, it is something else wearing an .mp3 name -- most often
 * a download that returned an error page. Reporting both as "no tag" is true
 * of each and useful about neither. */
static int looks_like_mpeg_audio(FILE *file)
{
    unsigned char window[ID3V2_SYNC_SEARCH_MAX];
    size_t filled;
    size_t at;

    if (fseek(file, 0, SEEK_SET) != 0)
    {
        return 0;
    }

    filled = fread(window, 1, sizeof(window), file);

    for (at = 0; at + 2 < filled; at++)
    {
        if (is_frame_header(window + at))
        {
            return 1;
        }
    }

    return 0;
}

/* Distinguishes a recording with no tag from a file that is not a recording,
 * for the paths where no ID3v2 tag was found. */
static int untagged_or_not_audio(FILE *file)
{
    return looks_like_mpeg_audio(file) ? ID3V2_ENOTAG : ID3V2_ENOTAUDIO;
}

int id3v2_read(const char *path, struct id3v2_tag *out)
{
    unsigned char header[ID3V2_HEADER_SIZE];
    unsigned int major;
    unsigned char flags;
    uint32_t remaining;
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
        int result = untagged_or_not_audio(file);

        fclose(file);

        return result;
    }

    if (memcmp(header, "ID3", 3) != 0)
    {
        int result = untagged_or_not_audio(file);

        fclose(file);

        return result;
    }

    major = header[3];
    if (major != 3 && major != 4)
    {
        /* v2.2 exists but uses 3-byte frame IDs and 3-byte sizes, so it needs
         * its own frame loop rather than a tweak to the one below. */
        fclose(file);

        return ID3V2_EVERSION;
    }

    out->version = (int)major;

    flags = header[5];

    if ((flags & ID3V2_FLAG_UNSYNCHRONISED) != 0)
    {
        /* The tag has had a zero byte inserted after every 0xFF that could be
         * mistaken for an MP3 frame sync. Undoing that has to happen before
         * any of the sizes below mean anything, so rather than misparse the
         * tag, say so. */
        fclose(file);

        return ID3V2_EUNSUPPORTED;
    }

    remaining = decode_synchsafe(header + 6);

    if ((flags & ID3V2_FLAG_EXTENDED_HEADER) != 0 &&
        skip_extended_header(file, major, &remaining) != 0)
    {
        fclose(file);

        return ID3V2_EMALFORMED;
    }

    /* Walk the frames. 'remaining' counts bytes left inside the tag, and every
     * read is bounded by it rather than by the size of the file: the sizes in
     * a tag are written by whatever last touched the file and are free to
     * claim a frame far larger than the tag that contains it. */
    while (remaining >= ID3V2_FRAME_HEADER_SIZE)
    {
        unsigned char frame_header[ID3V2_FRAME_HEADER_SIZE];
        uint32_t frame_size;
        char *field;

        if (fread(frame_header, 1, sizeof(frame_header), file) !=
            sizeof(frame_header))
        {
            /* The tag claims more than the file holds. What was read so far is
             * still worth returning. */
            break;
        }

        remaining -= ID3V2_FRAME_HEADER_SIZE;

        /* No frame ID begins with a zero byte, so this is the padding that
         * taggers leave to allow a later edit to grow the tag without
         * rewriting the whole file. */
        if (frame_header[0] == 0)
        {
            break;
        }

        if (!frame_id_is_plausible(frame_header))
        {
            fclose(file);

            return ID3V2_EMALFORMED;
        }

        frame_size = decode_frame_size(major, frame_header + 4);

        if (frame_size > remaining)
        {
            fclose(file);

            return ID3V2_EMALFORMED;
        }

        field = field_for_frame(out, frame_header, major);

        if (field != NULL && frame_size > 0)
        {
            unsigned char data[ID3V2_TEXT_READ_MAX];
            uint32_t wanted = frame_size;
            uint32_t leftover;

            if (wanted > sizeof(data))
            {
                wanted = (uint32_t)sizeof(data);
            }

            leftover = frame_size - wanted;

            if (fread(data, 1, wanted, file) != wanted)
            {
                break;
            }

            store_text(field, ID3V2_FIELD_MAX, data, wanted);

            /* A text frame longer than the buffer is truncated, but the rest
             * of it still has to be stepped over or the next read starts in
             * the middle of this frame. */
            if (leftover > 0 && fseek(file, (long)leftover, SEEK_CUR) != 0)
            {
                break;
            }
        }
        else if (frame_size > 0)
        {
            /* A frame of no interest, which may be album art running to
             * megabytes. Its size is exactly what lets it be stepped over
             * without understanding any of it. */
            if (fseek(file, (long)frame_size, SEEK_CUR) != 0)
            {
                break;
            }
        }

        remaining -= frame_size;
    }

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
    case ID3V2_EUNSUPPORTED:
        return "unsupported ID3v2 feature";
    case ID3V2_ENOTAUDIO:
        return "not an MP3";
    default:
        return "unknown error";
    }
}
