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

# Two seconds of silence: 44100 samples/s * 2 s * 2 bytes * 2 channels.
head -c 352800 /dev/zero > "$out/.silence.raw"
lame -r -s 44.1 --bitwidth 16 -m m -b 128 --quiet \
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


def write(name, frames, major=3, padding=64):
    """Tag first, then the audio unchanged. Padding is what real taggers leave
    so a later edit can grow the tag without rewriting the whole file."""
    body = b''.join(frames) + b'\x00' * padding
    tag = b'ID3' + bytes([major, 0]) + b'\x00' + synchsafe(len(body)) + body
    with open(os.path.join(out, name), 'wb') as handle:
        handle.write(tag + audio)


write('01-complete.mp3', [
    frame(b'TIT2', 'Paranoid Android', 0, 3),
    frame(b'TPE1', 'Radiohead', 0, 3),
    frame(b'TALB', 'OK Computer', 0, 3),
    frame(b'TRCK', '2/12', 0, 3),
    frame(b'TYER', '1997', 0, 3),
])

# Latin-1 with accents, which have to be converted to UTF-8 on the way out.
write('02-accents.mp3', [
    frame(b'TIT2', 'Café del Mar', 0, 3),
    frame(b'TPE1', 'Émilie Simon', 0, 3),
])

# Cyrillic. A v2.3 tag has no choice but UTF-16 here: v2.3 predates UTF-8, and
# Latin-1 cannot spell it.
write('03-cyrillic.mp3', [
    frame(b'TIT2', 'Ой у лузі червона калина', 1, 3),
    frame(b'TPE1', 'Пікардійська Терція', 1, 3),
])

# v2.4: synchsafe frame sizes, UTF-8, and TDRC in place of TYER.
write('04-v24-utf8.mp3', [
    frame(b'TIT2', 'Jóga', 3, 4),
    frame(b'TPE1', 'Björk', 3, 4),
    frame(b'TDRC', '1997-09-01', 3, 4),
], major=4)

# A tag with holes in it, which is what most of a real collection looks like.
write('05-partial.mp3', [frame(b'TIT2', 'Title But No Artist', 0, 3)])

os.rename(os.path.join(out, '.audio.mp3'), os.path.join(out, '06-untagged.mp3'))
PY

count=$(find "$out" -maxdepth 1 -name '*.mp3' -type f | wc -l | tr -d ' ')
printf 'wrote %s files to %s/\n' "$count" "$out"
