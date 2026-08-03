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

/* Copies one text frame's data into 'destination', converting to UTF-8 and
 * terminating it. 'data' points at the frame's data including its leading
 * encoding byte, and is not null-terminated, so nothing here may use the
 * string functions that would look for one. */
static void store_text(char *destination, size_t destination_size,
                       const unsigned char *data, size_t length)
{
    unsigned char encoding;
    size_t read_index;
    size_t write_index = 0;

    if (length < 1)
    {
        return;
    }

    encoding = data[0];
    data++;
    length--;

    if (encoding == ID3V2_ENCODING_LATIN1)
    {
        /* Latin-1 maps each byte to the code point of the same value, so the
         * conversion to UTF-8 is one byte below 0x80 and two above it. Without
         * this an accented character would reach the terminal as a byte that
         * is not valid UTF-8 and show up as a replacement mark. */
        for (read_index = 0; read_index < length; read_index++)
        {
            unsigned char byte = data[read_index];

            if (byte == 0)
            {
                break;
            }

            if (byte < 0x80)
            {
                if (write_index + 1 >= destination_size)
                {
                    break;
                }

                destination[write_index++] = (char)byte;
            }
            else
            {
                if (write_index + 2 >= destination_size)
                {
                    break;
                }

                destination[write_index++] = (char)(0xC0 | (byte >> 6));
                destination[write_index++] = (char)(0x80 | (byte & 0x3F));
            }
        }
    }
    else if (encoding == ID3V2_ENCODING_UTF8)
    {
        for (read_index = 0; read_index < length; read_index++)
        {
            if (data[read_index] == 0)
            {
                break;
            }

            if (write_index + 1 >= destination_size)
            {
                break;
            }

            destination[write_index++] = (char)data[read_index];
        }
    }
    else
    {
        /* UTF-16, in either of its two spellings. Storing the bytes raw would
         * produce mojibake, so the field is left empty and the caller reports
         * it as missing, which is at least true. */
        return;
    }

    destination[write_index] = '\0';
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
    default:
        return "unknown error";
    }
}
