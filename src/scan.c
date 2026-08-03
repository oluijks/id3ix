#include "scan.h"

#include "id3v2.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* Longest path this walker will build. Paths longer than this are reported and
 * skipped rather than silently truncated into the wrong file. */
#define SCAN_PATH_MAX 4096

/* A directory's entry names, collected so they can be sorted before any of
 * them is looked at. readdir hands them back in whatever order the filesystem
 * stores them, which is neither alphabetical nor stable between runs, and
 * output that reorders itself cannot be diffed or compared against last
 * week's. */
struct entry_names
{
    char **items;
    size_t count;
    size_t capacity;
};

static void entry_names_free(struct entry_names *names)
{
    size_t index;

    for (index = 0; index < names->count; index++)
    {
        free(names->items[index]);
    }

    free(names->items);

    names->items = NULL;
    names->count = 0;
    names->capacity = 0;
}

static int entry_names_add(struct entry_names *names, const char *name)
{
    char *copy;

    if (names->count == names->capacity)
    {
        size_t capacity = names->capacity == 0 ? 32 : names->capacity * 2;
        char **grown = realloc(names->items, capacity * sizeof(*grown));

        if (grown == NULL)
        {
            return -1;
        }

        names->items = grown;
        names->capacity = capacity;
    }

    copy = malloc(strlen(name) + 1);
    if (copy == NULL)
    {
        return -1;
    }

    memcpy(copy, name, strlen(name) + 1);
    names->items[names->count++] = copy;

    return 0;
}

static int compare_names(const void *left, const void *right)
{
    const char *const *left_name = left;
    const char *const *right_name = right;

    return strcmp(*left_name, *right_name);
}

/* Length of 'path' with any trailing slashes left off, so that joining a name
 * onto it can add exactly one. "samples/" and "samples///" both give 7, and "/"
 * gives 0, which joins to "/name" rather than "//name". */
static size_t length_without_trailing_slashes(const char *path)
{
    size_t length = strlen(path);

    while (length > 0 && path[length - 1] == '/')
    {
        length--;
    }

    return length;
}

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

/* The second column of every line: either the tag version, or why there are no
 * fields to show. Short words rather than sentences, because this is something
 * to match on: `awk -F'\t' '$2=="none"'`. */
static const char *status_word(int result, const struct id3v2_tag *tag)
{
    switch (result)
    {
    case ID3V2_OK:
        return tag->version == 4 ? "2.4" : "2.3";
    case ID3V2_ENOTAG:
        return "none";
    case ID3V2_ENOFILE:
        return "unreadable";
    case ID3V2_EVERSION:
        return "oldversion";
    case ID3V2_EMALFORMED:
        return "malformed";
    case ID3V2_EUNSUPPORTED:
        return "unsupported";
    default:
        return "unknown";
    }
}

/* Adds one file to the totals. Exactly one of the status counts is incremented
 * for every file seen, so they add up to the total and a summary cannot
 * silently lose anything. */
static void count_file(struct scan_totals *totals, int result,
                       const struct id3v2_tag *tag)
{
    totals->files++;

    switch (result)
    {
    case ID3V2_OK:
        if (tag->version == 4)
        {
            totals->v24++;
        }
        else
        {
            totals->v23++;
        }

        totals->tagged++;

        if (tag->title[0] == '\0')
        {
            totals->missing_title++;
        }

        if (tag->artist[0] == '\0')
        {
            totals->missing_artist++;
        }

        if (tag->album[0] == '\0')
        {
            totals->missing_album++;
        }

        if (tag->track[0] == '\0')
        {
            totals->missing_track++;
        }

        if (tag->year[0] == '\0')
        {
            totals->missing_year++;
        }

        break;
    case ID3V2_ENOTAG:
        totals->no_tag++;
        break;
    case ID3V2_EVERSION:
        totals->old_version++;
        break;
    case ID3V2_EMALFORMED:
        totals->malformed++;
        break;
    case ID3V2_EUNSUPPORTED:
        totals->unsupported++;
        break;
    default:
        totals->unreadable++;
        break;
    }
}

/* Reads one file, counts it, and prints one tab separated line unless the
 * caller only wanted the counts. The line format exists so that a collection
 * can be filtered and sorted with the tools that already exist rather than
 * with options invented here. A file that could not be read still gets a line:
 * the ones with problems are the point, and a report that quietly omitted them
 * would be worse than useless.
 *
 * Fields are printed raw because they cannot contain a tab or a newline --
 * store_text discards control characters as it decodes. */
static void report_file(const char *path, struct scan_options *options)
{
    struct id3v2_tag tag;
    int result = id3v2_read(path, &tag);

    count_file(&options->totals, result, &tag);

    if (options->print_lines)
    {
        printf("%s\t%s\t%s\t%s\t%s\t%s\t%s\n", path, status_word(result, &tag),
               tag.title, tag.artist, tag.album, tag.track, tag.year);
    }
}

void scan_print_summary(const struct scan_totals *totals)
{
    printf("%lu files\n\n", totals->files);

    printf("  %-12s %6lu\n", "ID3v2.3", totals->v23);
    printf("  %-12s %6lu\n", "ID3v2.4", totals->v24);
    printf("  %-12s %6lu\n", "no tag", totals->no_tag);
    printf("  %-12s %6lu\n", "ID3v2.2", totals->old_version);
    printf("  %-12s %6lu\n", "malformed", totals->malformed);
    printf("  %-12s %6lu\n", "unsupported", totals->unsupported);
    printf("  %-12s %6lu\n", "unreadable", totals->unreadable);

    /* Every row is printed even at zero. A report whose shape changes with its
     * contents cannot be compared against the one from last week, which is
     * most of what a summary is for. */
    printf("\n%lu with a tag, of which\n\n", totals->tagged);

    printf("  %-12s %6lu\n", "no title", totals->missing_title);
    printf("  %-12s %6lu\n", "no artist", totals->missing_artist);
    printf("  %-12s %6lu\n", "no album", totals->missing_album);
    printf("  %-12s %6lu\n", "no track", totals->missing_track);
    printf("  %-12s %6lu\n", "no year", totals->missing_year);
}

static int scan_directory(const char *path, struct scan_options *options)
{
    struct entry_names names = {NULL, 0, 0};
    const struct dirent *entry;
    DIR *dir = opendir(path);
    size_t stem = length_without_trailing_slashes(path);
    size_t index;
    int failed = 0;

    if (dir == NULL)
    {
        fprintf(stderr, "id3ix: cannot open directory '%s'\n", path);

        return -1;
    }

    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        if (entry_names_add(&names, entry->d_name) != 0)
        {
            fprintf(stderr, "id3ix: out of memory reading '%s'\n", path);
            failed = -1;

            break;
        }
    }

    /* Closed before recursing rather than after. Holding it open would cost a
     * file descriptor for every level of depth, and a deep enough tree would
     * run the process out of them. */
    closedir(dir);

    qsort(names.items, names.count, sizeof(*names.items), compare_names);

    for (index = 0; index < names.count; index++)
    {
        const char *name = names.items[index];
        char child[SCAN_PATH_MAX];
        struct stat info;
        int written;

        /* A path typed with a trailing slash would otherwise produce
         * "samples//track.mp3". Harmless to open, but it makes the same scan
         * print different strings depending on how the argument was typed,
         * which spoils diffing one run against another. */
        written =
            snprintf(child, sizeof(child), "%.*s/%s", (int)stem, path, name);
        if (written < 0 || (size_t)written >= sizeof(child))
        {
            fprintf(stderr, "id3ix: path too long, skipping '%.*s/%s'\n",
                    (int)stem, path, name);
            failed = -1;

            continue;
        }

        /* lstat rather than stat, so a symlink is described as a symlink
         * instead of as whatever it points at. Following them would let a link
         * back up the tree loop this walk forever. A path named on the command
         * line is treated differently: see scan_path. */
        if (lstat(child, &info) != 0)
        {
            fprintf(stderr, "id3ix: cannot read '%s'\n", child);
            failed = -1;

            continue;
        }

        if (S_ISDIR(info.st_mode))
        {
            if (scan_directory(child, options) != 0)
            {
                failed = -1;
            }
        }
        else if (S_ISREG(info.st_mode) && has_mp3_extension(name))
        {
            report_file(child, options);
        }
    }

    entry_names_free(&names);

    return failed;
}

int scan_path(const char *path, struct scan_options *options)
{
    struct stat info;

    /* stat, not lstat: a symlink named on the command line was named on
     * purpose, so it is followed. Only the walk, which has to guess what is
     * worth opening, refuses to follow one. */
    if (stat(path, &info) != 0)
    {
        fprintf(stderr, "id3ix: cannot read '%s'\n", path);

        return -1;
    }

    if (S_ISDIR(info.st_mode))
    {
        return scan_directory(path, options);
    }

    if (S_ISREG(info.st_mode))
    {
        /* No extension test. The caller named this file, so whatever it is
         * called, they meant it. */
        report_file(path, options);

        return 0;
    }

    fprintf(stderr, "id3ix: not a file or directory: '%s'\n", path);

    return -1;
}
