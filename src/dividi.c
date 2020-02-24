#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <string.h>

int main(int argc, char *argv[])
{

    FILE *ft = fopen(argv[1], "r+");
    int ch;
    char skip_buf[1024];
    char buf[10];

    int in, out;

    while ((ch = fgetc(ft)) != EOF)
    {
        if (ch == 'i')
        {
            fgetc(ft);
            fgets(buf, 10, ft);
            in = atoi(buf);
        }
        if (ch == 'o')
        {
            fgetc(ft);
            fgets(buf, 10, ft);
            out = atoi(buf);
            break;
        }
    }

    char *pla_name = basename(argv[1]);
    int len = strlen(pla_name);
    for (int i = 1; i <= 4; i++)
        pla_name[len - i] = '\0';

    char *curr = calloc((in + 1), sizeof(char));

    for (int i = 1; i <= out; i++)
    {
        char *filename;
        asprintf(&filename, "./multipla/%s_%d.pla", pla_name, i);
        FILE *out = fopen(filename, "w+");
        fprintf(out, ".i %d\n.o 1\n", in);

        int index = 0;

        while ((ch = fgetc(ft)) != EOF)
        {
            if (ch == '.')
                fgets(skip_buf, 1024, ft);
            else if (ch == '\n')
                index = 0;
            else if (ch == ' ')
            {
                index = 1;
            }
            else if ((ch == '1') && (index == i))
            {
                fprintf(out, "%s 1\n", curr);
                fgets(skip_buf, 1024, ft);
                index = 0;
            }
            else if (index == 0)
            {
                curr[0] = ch;
                fgets(&curr[1], in, ft);
            }
            else
                index++;
        }

        rewind(ft);
        free(filename);
        fclose(out);
    }

    fclose(ft);

    return 0;
}