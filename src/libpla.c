/**
 * @file libpla.c
 * @author Marco Costa
 * @brief Implementazione dei metodi contenuti in libpla.h;
 * @date 2019-11-25
 * 
 */

#include <stdio.h>
#include <stdlib.h>

#include "libpla.h"
#include "utils.h"

void initParsedPLA(ParsedPLA *bdd)
{
    CUBE_LIST = safe_malloc(bdd->num_out * sizeof(TAILQ_HEAD(queue, CubeListEntry)));
    for (int i = 0; i < bdd->num_out; i++)
        TAILQ_INIT(&(CUBE_LIST[i]));
    N_CUBES = safe_calloc(bdd->num_out, sizeof(int));
}

CubeListEntry *alloc_node(int size)
{
    CubeListEntry *c = safe_malloc(sizeof(CubeListEntry));
    c->cube = safe_malloc(size * sizeof(int));
    return c;
}

void convertDCSetPLA(char *filename)
{
    FILE *ft;
    int ch;
    char skip_buf[1024];

    ft = fopen(filename, "r+");
    if (ft == NULL)
    {
        fprintf(stderr, "cannot open target file %s\n", filename);
        exit(1);
    }

    while ((ch = fgetc(ft)) != EOF)
    {
        if (ch == '.')
            fgets(skip_buf, 1024, ft);
        else if (ch == '~')
        {
            fseek(ft, -1, SEEK_CUR);
            fputc('0', ft);
            fseek(ft, 0, SEEK_CUR);
        }
        else if (ch == '2')
        {
            fseek(ft, -1, SEEK_CUR);
            fputc('1', ft);
            fseek(ft, 0, SEEK_CUR);
        }
    }
    fclose(ft);
}

void printMatrixtoFile(char *filename, int **M, int len, int in, int out)
{
    FILE *f = fopen(filename, "w+");
    if (f == NULL)
    {
        fprintf(stderr, "Impossibile aprire il file %s:", filename);
        perror(NULL);
        exit(EXIT_FAILURE);
    }

    fprintf(f, ".i %d\n.o %d\n", in, out);
    for (int i = 0; i < len; i++)
    {
        for (int j = 0; j < (in + out); j++)
        {
            if (j == in)
            {
                // printf(" ");
                fprintf(f, " ");
            }
            if (M[i][j] == 2)
            {
                // printf("-");
                fprintf(f, "-");
            }
            else
            {
                // printf("%d", M[i][j]);
                fprintf(f, "%d", M[i][j]);
            }
        }
        // printf("\n");
        fprintf(f, "\n");
    }

    fclose(f);
}

void getPLAFileData(char *filename, int function_out, struct test_stats *s)
{
    FILE *ft;
    int ch;
    char skip_buf[1024];
    int output = 0, curr_line_literals = 0, curr_function_out = 0;
    int in_n = 0, out_n = 0, tot_product = 0;

    int *or_literals = safe_calloc(function_out, sizeof(int));

    ft = fopen(filename, "r+");
    if (ft == NULL)
    {
        fprintf(stderr, "cannot open target file %s\n", filename);
        exit(1);
    }

    while ((ch = fgetc(ft)) != EOF)
    {
        if (ch == ' ')
        {
            output = 1;
            curr_function_out = 0;
        }
        else if (ch == '\n')
        {
            curr_line_literals = output = 0;
        }
        else if (ch == '.')
            fgets(skip_buf, 1024, ft);
        else if (!output && ((ch == '0') || (ch == '1')))
        {
            in_n++;
            curr_line_literals++;
        }
        else if ((output) && (ch == '1'))
        {
            out_n++;
            tot_product += curr_line_literals;
            (or_literals[curr_function_out])++;
            curr_function_out++;
        }
        else if ((output) && (ch == '0'))
            curr_function_out++;
    }
    fclose(ft);

    int total_or_port = 0;
    for (int i = 0; i < function_out; i++)
        total_or_port += or_literals[i];

    s->and_lit = tot_product;
    s->or_port = total_or_port;
    s->prod_in = in_n;
    s->prod_out = out_n;

    free(or_literals);
}