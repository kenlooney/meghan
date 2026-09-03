#include <meglang/version.h>

#include <stdio.h>
#include <string.h>

static void usage(FILE *out, const char *program)
{
    fprintf(out, "usage: %s --help | --version\n", program);
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        usage(stdout, argv[0]);
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        printf("meg 0.%d\n", meg_version());
        return 0;
    }
    usage(stderr, argv[0]);
    return 2;
}
