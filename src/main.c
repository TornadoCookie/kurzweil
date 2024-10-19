#include <stdio.h>
#include "cfbf.h"

#define PROGRAM_VERSION "v1.0.0"

int main(int argc, char **argv)
{
    printf("Kurzweil .kes parser " PROGRAM_VERSION "\n");

    printf("Filename: %s\n", argv[1]);

    FILE *f = fopen(argv[1], "rb");
    if (!f)
    {
        perror("Unable to open file");
        return 1;
    }

    printf("Starting CFBF parser...\n");

    CFBF cfbf = parse_cfbf(f);

    fclose(f);

    return 0;
}
