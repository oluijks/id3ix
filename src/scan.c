#include "scan.h"

#include "id3v2.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

/* Longest path this walker will build. Paths longer than this are reported and
 * skipped rather than silently truncated into the wrong file. */
#define SCAN_PATH_MAX 4096

static int has_mp3_extension(const char *name)
{
    size_t length = strlen(name);
    const char *extension;

    if (length < 4)
    {
        return 0;
    }

    extension = name + length - 4;

    return extension[0] == '.' &&
           (extension[1] == 'm' || extension[1] == 'M') &&
           (extension[2] == 'p' || extension[2] == 'P') && extension[3] == '3';
}

static void report_file(const char *path)
{
    struct id3v2_tag tag;
    int result = id3v2_read(path, &tag);

    if (result != ID3V2_OK)
    {
        printf("%s: %s\n", path, id3v2_strerror(result));

        return;
    }

    printf("%s\n", path);
    printf("  title:  %s\n", tag.title[0] != '\0' ? tag.title : "-");
    printf("  artist: %s\n", tag.artist[0] != '\0' ? tag.artist : "-");
    printf("  album:  %s\n", tag.album[0] != '\0' ? tag.album : "-");
}

int scan_directory(const char *path)
{
    const struct dirent *entry;
    DIR *dir = opendir(path);

    if (dir == NULL)
    {
        fprintf(stderr, "id3ix: cannot open directory '%s'\n", path);

        return -1;
    }

    while ((entry = readdir(dir)) != NULL)
    {
        char child[SCAN_PATH_MAX];
        struct stat info;
        int written;

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        written = snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
        if (written < 0 || (size_t)written >= sizeof(child))
        {
            fprintf(stderr, "id3ix: path too long, skipping '%s/%s'\n", path,
                    entry->d_name);

            continue;
        }

        /* lstat rather than stat, so a symlink is described as a symlink
         * instead of as whatever it points at. Following them would let a link
         * back up the tree loop this walk forever. */
        if (lstat(child, &info) != 0)
        {
            continue;
        }

        if (S_ISDIR(info.st_mode))
        {
            scan_directory(child);
        }
        else if (S_ISREG(info.st_mode) && has_mp3_extension(entry->d_name))
        {
            report_file(child);
        }
    }

    closedir(dir);

    return 0;
}
