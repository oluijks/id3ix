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
        struct scan_options options = {1, {0}};
        int summary = 0;
        int end_of_options = 0;
        int unreadable = 0;
        int paths = 0;
        int index;

        /* Options are read in a pass of their own, so that a flag written
         * after a path still applies to it. Doing it in one pass would make
         * `scan dir --summary` behave differently from `scan --summary dir`,
         * which nobody would expect and everybody would eventually type. */
        for (index = 2; index < argc; index++)
        {
            if (end_of_options || argv[index][0] != '-')
            {
                paths++;

                continue;
            }

            if (strcmp(argv[index], "--") == 0)
            {
                /* Everything after this is a path, even if it looks like an
                 * option. The escape hatch for a file whose name begins with a
                 * dash. */
                end_of_options = 1;
            }
            else if (strcmp(argv[index], "--summary") == 0)
            {
                summary = 1;
                options.print_lines = 0;
            }
            else
            {
                fprintf(stderr, "id3ix: unknown option '%s'\n", argv[index]);
                show_usage(stderr);

                return EXIT_FAILURE;
            }
        }

        if (paths == 0)
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
        end_of_options = 0;

        for (index = 2; index < argc; index++)
        {
            if (!end_of_options && argv[index][0] == '-')
            {
                if (strcmp(argv[index], "--") == 0)
                {
                    end_of_options = 1;
                }

                continue;
            }

            if (scan_path(argv[index], &options) != 0)
            {
                unreadable = 1;
            }
        }

        if (summary)
        {
            scan_print_summary(&options.totals);
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
    fprintf(stream, "  id3ix scan [--summary] <path>...\n\n");
    fprintf(stream, "Options:\n");
    fprintf(stream, "  --summary      Print counts instead of one line per "
                    "file\n");
    fprintf(stream, "  -h, --help     Show this help message\n");
    fprintf(stream, "  -V, --version  Show version information\n");
}

static void show_version(void)
{
    printf("id3ix %s\n", ID3IX_VERSION);
}
