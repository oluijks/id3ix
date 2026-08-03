#include "scan.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Defined by the Makefile via -DID3IX_VERSION; the fallback only applies when
 * building src/main.c directly without it. */
#ifndef ID3IX_VERSION
#define ID3IX_VERSION "unknown"
#endif

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
        if (argc < 3)
        {
            fprintf(stderr, "id3ix: scan requires a directory\n");
            show_usage(stderr);

            return EXIT_FAILURE;
        }

        printf("Scanning...\n");

        /* stdout is block-buffered when it is not a terminal, while stderr is
         * not buffered at all. Without this flush, any error the walk writes to
         * stderr appears ahead of this line whenever output is piped. */
        fflush(stdout);

        if (scan_directory(argv[2]) != 0)
        {
            return EXIT_FAILURE;
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
    fprintf(stream, "  id3ix scan <directory>\n\n");
    fprintf(stream, "Options:\n");
    fprintf(stream, "  -h, --help     Show this help message\n");
    fprintf(stream, "  -V, --version  Show version information\n");
}

static void show_version(void)
{
    printf("id3ix %s\n", ID3IX_VERSION);
}
