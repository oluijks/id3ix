#include "scan.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Defined by the Makefile via -DID3IX_VERSION; the fallback only applies when
 * building src/main.c directly without it. */
#ifndef ID3IX_VERSION
#define ID3IX_VERSION "unknown"
#endif

/* EXIT_FAILURE, which is 1, means the command line was wrong. A separate code
 * means the command line was fine but something in the collection could not be
 * read, which is a different thing to react to in a script. */
#define ID3IX_EXIT_UNREADABLE 2

static void show_usage(FILE *stream);
static void show_version(void);

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        show_usage(stderr);

        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)
    {
        show_usage(stdout);

        return EXIT_SUCCESS;
    }

    if (strcmp(argv[1], "-V") == 0 || strcmp(argv[1], "--version") == 0)
    {
        show_version();

        return EXIT_SUCCESS;
    }

    if (strcmp(argv[1], "scan") == 0)
    {
        int unreadable = 0;
        int index;

        if (argc < 3)
        {
            fprintf(stderr, "id3ix: scan requires a path\n");
            show_usage(stderr);

            return EXIT_FAILURE;
        }

        /* Paths are taken in the order given rather than sorted. Sorting is
         * for entries found inside a directory, where the order was nobody's
         * decision; an argument list is already an expression of one.
         *
         * Nothing else is printed here on purpose. stdout carries one line per
         * file and nothing else, so that it can be piped straight into awk or
         * sort; progress chatter would be another line for every caller to
         * filter back out. */
        for (index = 2; index < argc; index++)
        {
            if (scan_path(argv[index]) != 0)
            {
                unreadable = 1;
            }
        }

        if (unreadable)
        {
            return ID3IX_EXIT_UNREADABLE;
        }
    }
    else
    {
        fprintf(stderr, "id3ix: unknown command '%s'\n", argv[1]);
        show_usage(stderr);

        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static void show_usage(FILE *stream)
{
    fprintf(stream, "id3ix - metadata utility\n\n");
    fprintf(stream, "Usage:\n");
    fprintf(stream, "  id3ix scan <path>...\n\n");
    fprintf(stream, "Options:\n");
    fprintf(stream, "  -h, --help     Show this help message\n");
    fprintf(stream, "  -V, --version  Show version information\n");
}

static void show_version(void)
{
    printf("id3ix %s\n", ID3IX_VERSION);
}
