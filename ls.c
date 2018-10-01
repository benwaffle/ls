#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>

typedef enum {
    NORMAL,
    ALL,
    ALL_EXCEPT_DOT
} list_filter;

int main(int argc, char *argv[])
{
    int ch;
    list_filter filter = getuid() == 0 ? ALL_EXCEPT_DOT : NORMAL;
    bool multi_col = isatty(STDOUT_FILENO);
    bool file_type_char = false;


    while ((ch = getopt(argc, argv, "AacCdFfhiklnqRrSstuwx1")) != -1) {
        switch (ch) {
            case 'A':
                filter = ALL_EXCEPT_DOT;
                break;
            case 'a':
                filter = ALL;
                break;
            case 'C':
                multi_col = true;
                break;
            case 'F':
                file_type_char = true;
                break;
        }
    }

    argc -= optind;
    argv += optind;

    char **files = argv;

    if (*argv == NULL)
        files = (char*[]){".", NULL};

    while (*files) {
        printf("%s\n", *files);
        files++;
    }
}
