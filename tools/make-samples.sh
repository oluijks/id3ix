#!/bin/sh
#
# Builds a handful of real MP3s in samples/ to try the tool on.
#
# Generated rather than downloaded, because a download can fail and leave an
# error page wearing an .mp3 name, and because generated files can cover cases
# a random download will not: several text encodings, a v2.4 tag, a file with
# no tag at all.
#
# The audio is two seconds of silence encoded by lame, so these are genuine
# MPEG files rather than bytes that merely start with the right magic. The tags
# are written here rather than by lame, which expects Latin-1 input and quietly
# truncates anything else -- passing it a UTF-8 title produces a tag containing
# only the ASCII prefix.
#
# Needs lame and python3:
#   sudo apt-get install -y lame
#
# Usage: tools/make-samples.sh [directory]   (default: samples)

set -eu

out=${1:-samples}

if ! command -v lame >/dev/null 2>&1
then
    printf 'error: lame is not installed (try: sudo apt-get install -y lame)\n' >&2
    exit 1
fi

if ! command -v python3 >/dev/null 2>&1
then
    printf 'error: python3 is not installed\n' >&2
    exit 1
fi

mkdir -p "$out"

# A fifth of a second of silence at the lowest useful bitrate. The audio is
# not the point and small files are, so these come out around a kilobyte each
# rather than sixty.
head -c 17640 /dev/zero > "$out/.silence.raw"
lame -r -s 44.1 --bitwidth 16 -m m -b 32 --quiet \
    "$out/.silence.raw" "$out/.audio.mp3" 2>/dev/null
rm -f "$out/.silence.raw"

OUT="$out" python3 <<'PY'
import os

out = os.environ['OUT']
audio = open(os.path.join(out, '.audio.mp3'), 'rb').read()


def synchsafe(n):
    return bytes([(n >> 21) & 0x7F, (n >> 14) & 0x7F, (n >> 7) & 0x7F, n & 0x7F])


def frame(frame_id, text, encoding, major):
    """One text frame. Frame sizes are plain big-endian in v2.3 and synchsafe
    in v2.4, which is the difference that most often breaks a parser."""
    if encoding == 0:      # Latin-1
        payload = b'\x00' + text.encode('latin-1')
    elif encoding == 1:    # UTF-16 with a byte order mark
        payload = b'\x01' + b'\xff\xfe' + text.encode('utf-16-le')
    elif encoding == 3:    # UTF-8, v2.4 only
        payload = b'\x03' + text.encode('utf-8')
    else:
        raise ValueError(encoding)

    size = len(payload).to_bytes(4, 'big') if major == 3 else synchsafe(len(payload))

    return frame_id + size + b'\x00\x00' + payload


def frame_v22(frame_id, text):
    """A v2.2 frame: three characters of ID and a three byte size, where v2.3
    and v2.4 use four of each. That is why v2.2 needs its own frame loop rather
    than a branch inside the newer one."""
    payload = b'\x00' + text.encode('latin-1')

    return frame_id + len(payload).to_bytes(3, 'big') + payload


def write(name, frames, major=3, padding=64, flags=0, body_override=None):
    """Tag first, then the audio unchanged. Padding is what real taggers leave
    so a later edit can grow the tag without rewriting the whole file."""
    body = body_override
    if body is None:
        body = b''.join(frames) + b'\x00' * padding
    tag = b'ID3' + bytes([major, 0]) + bytes([flags]) + synchsafe(len(body)) + body
    with open(os.path.join(out, name), 'wb') as handle:
        handle.write(tag + audio)


# Invented titles throughout: these are fixtures, and a made up name says so
# at a glance while exercising the encodings just as well as a real one.
write('01-complete.mp3', [
    frame(b'TIT2', 'First Light', 0, 3),
    frame(b'TPE1', 'The Wind Ensemble', 0, 3),
    frame(b'TALB', 'Morning Records', 0, 3),
    frame(b'TRCK', '2/12', 0, 3),
    frame(b'TYER', '1997', 0, 3),
])

# Latin-1 with accents, which have to be converted to UTF-8 on the way out.
write('02-accents.mp3', [
    frame(b'TIT2', 'Étude à Trois', 0, 3),
    frame(b'TPE1', 'Renée Dupré', 0, 3),
])

# Cyrillic. A v2.3 tag has no choice but UTF-16 here: v2.3 predates UTF-8, and
# Latin-1 cannot spell it.
write('03-cyrillic.mp3', [
    frame(b'TIT2', 'Пісня Перша', 1, 3),
    frame(b'TPE1', 'Тестовий Гурт', 1, 3),
])

# v2.4: synchsafe frame sizes, UTF-8, and TDRC in place of TYER.
write('04-v24-utf8.mp3', [
    frame(b'TIT2', 'Blå Himmel', 3, 4),
    frame(b'TPE1', 'Håkan Ström', 3, 4),
    frame(b'TDRC', '1997-09-01', 3, 4),
], major=4)

# Tags with holes in them, which is what most of a real collection looks like.
write('05-partial.mp3', [frame(b'TIT2', 'Title But No Artist', 0, 3)])

write('07-no-album.mp3', [
    frame(b'TIT2', 'Second Light', 0, 3),
    frame(b'TPE1', 'The Wind Ensemble', 0, 3),
    frame(b'TRCK', '3/12', 0, 3),
])

write('08-title-only-v24.mp3', [frame(b'TIT2', 'Late Addition', 3, 4)], major=4)

# ID3v2.2, which this reader reports rather than parses. Written correctly all
# the same, so it is a usable fixture if v2.2 support ever arrives.
write('09-v22-old.mp3', [
    frame_v22(b'TT2', 'Ancient Recording'),
    frame_v22(b'TP1', 'Wax Cylinder Trio'),
], major=2)

# A frame claiming more bytes than the tag that contains it. The size fields
# are written by whatever last touched the file, and nothing stops them lying.
lying = b'TIT2' + (999999).to_bytes(4, 'big') + b'\x00\x00' + b'\x00' + b'x'
write('10-malformed.mp3', [], body_override=lying)

# The unsynchronisation flag, which means every 0xFF in the tag has had a zero
# byte inserted after it. Undoing that has to happen before any of the sizes
# mean anything, so the reader says so rather than misreading the tag.
write('11-unsynchronised.mp3', [
    frame(b'TIT2', 'Cannot Be Read Yet', 0, 3),
], flags=0x80)

os.rename(os.path.join(out, '.audio.mp3'), os.path.join(out, '06-untagged.mp3'))

# Not an MP3 at all: what a failed download leaves behind. No audio is appended
# to this one, which is the whole point of it.
with open(os.path.join(out, '12-not-an-mp3.mp3'), 'wb') as handle:
    handle.write(b'<!DOCTYPE html>\n<html><body>404 Not Found</body></html>\n')

# A file that exists but cannot be opened. This one cannot be a committed
# fixture: git records only the executable bit, so it would arrive readable
# from a clone. It also does nothing for root, who is exempt from the
# permission check. The test suite makes its own at run time.
locked = os.path.join(out, '13-unreadable.mp3')
with open(locked, 'wb') as handle:
    handle.write(audio)
os.chmod(locked, 0)

if os.access(locked, os.R_OK):
    print('note: 13-unreadable.mp3 is still readable, so it will report "none"',
          '\n      (running as root, which the permission check does not apply to)')
PY

count=$(find "$out" -maxdepth 1 -name '*.mp3' -type f | wc -l | tr -d ' ')
printf 'wrote %s files to %s/\n' "$count" "$out"
