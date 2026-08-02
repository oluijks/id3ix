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
        printf("Scanning...\n");
    }
    else
    {
        printf("Unknown command\n");
    }

    return EXIT_SUCCESS;
}

void show_usage(void)
{
    printf("id3ix - metadata utility\n\n");
    printf("Usage:\n");
    printf("  id3ix scan <directory>\n");
}