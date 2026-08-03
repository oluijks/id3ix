#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void show_usage(void);

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        show_usage();

        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "scan") == 0)
    {
        if (argc < 3)
        {
            fprintf(stderr, "Usage: id3ix scan <path>\n");

            return EXIT_FAILURE;
        }

        printf("Scanning...\n");
    }
    else
    {
        fprintf(stderr, "Usage: id3ix scan <path>\n");

        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

void show_usage(void)
{
    printf("id3ix - metadata utility\n\n");
    printf("Usage:\n");
    printf("  id3ix scan <directory>\n");
}
