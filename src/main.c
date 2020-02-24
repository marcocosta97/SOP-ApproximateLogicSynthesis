/**
 * @file main.c
 * @author Marco Costa
 * @brief file principale contenente euristica, decomposizione e gestione in/out
 * @date 2019-11-21
 * 
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/queue.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <libgen.h>

#include <cudd.h>

#include "PLAparser.h"

#include "queue.h"
#include "libpla.h"
#include "config.h"
#include "utils.h"

#define print_usage(name)                                              \
    fprintf(stderr, "%s [options] [-m error] input-file.pla\n", name); \
    fprintf(stderr, "%s [options] [-g error] input-file.pla\n", name);

/**
 * @brief definisce il tipo di errore ammesso
 * ct: complementable terms
 */
enum
{
    GLOBAL_OUTPUT_ERROR,
    MULTIPLE_OUTPUT_ERROR
} error_mode = MULTIPLE_OUTPUT_ERROR;
unsigned long long ct = DEFAULT_CT;
double r;
int ct_percent = 0;

enum
{
    VERBOSE_LOG,
    TEST_LOG,
    DECOMPOSITION_LOG
} output_mode = VERBOSE_LOG;

int NUM_OUT;
int NUM_IN;

struct test_stats original_pla_stats;
struct test_stats heuristic_pla_stats;
struct test_stats espresso_pla_stats;

/**
 * @brief Costruisce una PLA data una struttura ParsedPLA
 * 
 * @param pla la struttura
 * @param filename il file di out
 */
void mergeToPLA(ParsedPLA *pla, char *filename)
{
    int max_matrix_row_size = 0;
    for (int i = 0; i < NUM_OUT; i++)
        max_matrix_row_size += N_CUBES[i];

    int **M = safe_malloc(max_matrix_row_size * sizeof(int *));
    for (int i = 0; i < max_matrix_row_size; i++)
        M[i] = safe_calloc((NUM_IN + NUM_OUT), sizeof(int));

    struct queue *q = CUBE_LIST;
    int matrix_row_len = 0;
    for (int o = 0; o < NUM_OUT; o++)
    {
        CubeListEntry *curr;
        TAILQ_FOREACH(curr, &(q[o]), entries)
        {
            int add_tail = 1;
            if (curr == NULL)
                continue;
            for (int i = 0; i < matrix_row_len; i++)
            {
                if (memcmp(M[i], curr->cube, NUM_IN * sizeof(int)) == 0)
                {
                    M[i][NUM_IN + o] = 1;
                    add_tail = 0;
                    break;
                }
            }

            if (add_tail)
            {
                memcpy(M[matrix_row_len], curr->cube, NUM_IN * sizeof(int));
                M[matrix_row_len][NUM_IN + o] = 1;
                matrix_row_len++;
            }
        }
    }

    printMatrixtoFile(filename, M, matrix_row_len, NUM_IN, NUM_OUT);

    /* PULIZIA */
    for (int i = 0; i < max_matrix_row_size; i++)
        free(M[i]);
    free(M);
}

/**
 * @brief Costruisce una BDD contenente un singolo prodotto
 * 
 * @param cube il prodotto come vettore di interi
 * @param n_var il numero di variabili
 * @return DdNode* la BDD
 */
DdNode *construct_product(int *cube, int n_var)
{
    DdNode *temp_node;
    DdNode *new_node = Cudd_ReadOne(manager);
    Cudd_Ref(new_node);

    for (int i = 0; i < n_var; i++)
    {
        DdNode *var = Cudd_bddIthVar(manager, i);
        if (cube[i] == 0) /* effettua il complemento */
            temp_node = Cudd_bddAnd(manager, new_node, Cudd_Not(var));
        else if (cube[i] == 1)
            temp_node = Cudd_bddAnd(manager, new_node, var);
        else
            continue;

        Cudd_Ref(temp_node);
        Cudd_RecursiveDeref(manager, new_node);
        new_node = temp_node;
    }

    return new_node;
}

/**
 * @brief Operatore di copertura mediante confronto tra due vettori di interi
 * 
 * @param a 
 * @param b 
 * @return int 1 sse a copre b, 2 se a == b,  0 altrimenti
 */
int covers(int *a, int *b, int n_in)
{
    int equals = 2;
    for (int i = 0; i < n_in; i++)
    {
        if (a[i] == 2 && b[i] != 2)
            equals = 1; /* non sono uguali */
        else if (a[i] != 2 && a[i] != b[i])
            return 0;
    }

    return equals;
}

/**
 * @brief Rimozione dei prodotti ridondanti in coda
 * 
 * @param prod il prodotto inserito in coda
 * @param q la coda
 * @param n_in il numero di var. di input
 */
void invalidateRedundantInQueue(product_t *prod, prior_queue *q, int n_in)
{
    node_t *nodes = q->nodes;

    for (int i = 1; i <= q->len; i++)
    {
        product_t *curr_prod = nodes[i].data;

        /* c'è in coda un prodotto proveniente dalla stessa origine o c'è un prodotto uguale sullo stesso output */
        if ((curr_prod != NULL) && (prod->output_f == curr_prod->output_f) &&
            ((prod->product_number == curr_prod->product_number) || (memcmp(prod->cube, curr_prod->cube, n_in * sizeof(int)) == 0)))
            curr_prod->valid = 0;
    }
}

/**
 * @brief Rimozione dei prodotti nella lista coperti da prod. 
 * 
 * @param prod il nuovo prodotto
 * @param pla la lista di prodotti
 */
void removeCoveredProducts(product_t *prod, ParsedPLA *pla)
{
    CubeListEntry *curr;

    TAILQ_FOREACH(curr, &(CUBE_LIST[prod->output_f]), entries)
    {
        if ((prod->cube != curr->cube) && (covers(prod->cube, curr->cube, NUM_IN)))
        {
            TAILQ_REMOVE(&(CUBE_LIST[prod->output_f]), curr, entries);
            (N_CUBES[prod->output_f])--;
        }
    }
}

/**
 * @brief Routine di pulizia.
 * 
 * @param pla la pla principale
 * @param offset la BDD Off-set
 * @param dontPla la BDD DC-set
 */
void cleanRoutine(ParsedPLA *pla, DdNode **offset, ParsedPLA *dontPla)
{
    CubeListEntry *curr;

    for (int o = 0; o < NUM_OUT; o++)
    {
        Cudd_RecursiveDeref(manager, pla->vectorbdd_F[o]);
        Cudd_RecursiveDeref(manager, dontPla->vectorbdd_F[o]);
        Cudd_RecursiveDeref(manager, offset[o]);

        while ((curr = TAILQ_FIRST(&(CUBE_LIST[o]))) != NULL)
        {
            TAILQ_REMOVE(&(CUBE_LIST[o]), curr, entries);
            free(curr->cube);
            free(curr);
        }
    }

    free(N_CUBES);
    free(pla->vectorbdd_F);
    free(dontPla->vectorbdd_F);
    free(offset);
}

/**
 * @brief Costruisce un file PLA a partire dall'On-set e DC-set di una funzione.
 * 
 * @param filename il file di out
 * @param on_set BDD rappresentante l'On-set
 * @param dc_set BDD rappresentante il DC-set
 */
void mergeBDDtoFile(char *filename, DdNode **on_set, DdNode **dc_set)
{
    int max_matrix_row_size = 0;

    DdGen *gen1, *gen2;
    int *cube;
    CUDD_VALUE_TYPE value;
    for (int i = 0; i < NUM_OUT; i++)
    {
        Cudd_ForeachCube(manager, on_set[i], gen1, cube, value)
        {
            max_matrix_row_size++;
        }
        Cudd_ForeachCube(manager, dc_set[i], gen2, cube, value)
        {
            max_matrix_row_size++;
        }
    }

    int **M = safe_malloc(max_matrix_row_size * sizeof(int *));
    for (int i = 0; i < max_matrix_row_size; i++)
        M[i] = safe_calloc((NUM_IN + NUM_OUT), sizeof(int));

    int matrix_row_len = 0;
    for (int o = 0; o < NUM_OUT; o++)
    {
        DdGen *gen1, *gen2;
        Cudd_ForeachCube(manager, on_set[o], gen1, cube, value)
        {
            int add_tail = 1;

            for (int i = 0; i < matrix_row_len; i++)
            {
                if (memcmp(M[i], cube, NUM_IN * sizeof(int)) == 0)
                {
                    M[i][NUM_IN + o] = 1;
                    add_tail = 0;
                    break;
                }
            }

            if (add_tail)
            {
                memcpy(M[matrix_row_len], cube, NUM_IN * sizeof(int));
                M[matrix_row_len][NUM_IN + o] = 1;
                matrix_row_len++;
            }
        }
        Cudd_ForeachCube(manager, dc_set[o], gen2, cube, value)
        {
            int add_tail = 1;

            for (int i = 0; i < matrix_row_len; i++)
            {
                if (memcmp(M[i], cube, NUM_IN * sizeof(int)) == 0)
                {
                    M[i][NUM_IN + o] = 2;
                    add_tail = 0;
                    break;
                }
            }

            if (add_tail)
            {
                memcpy(M[matrix_row_len], cube, NUM_IN * sizeof(int));
                M[matrix_row_len][NUM_IN + o] = 2;
                matrix_row_len++;
            }
        }
    }

    printMatrixtoFile(filename, M, matrix_row_len, NUM_IN, NUM_OUT);

    for (int i = 0; i < max_matrix_row_size; i++)
        free(M[i]);
    free(M);
}

/**
 * @brief Procedura per la decomposizione euristica di una funzione f data la sua approssimazione g
 *  mediante operatore logico AND
 * 
 * @param f_dc il DC-set della funzione f
 * @param g_file il file PLA della funzione g
 * @param f_file il file PLA della funzione f
 */
void andDecomposition(ParsedPLA *f_dc, char *g_file, char *f_file)
{
    char *command;
    asprintf(&command, "espresso -Decho -of %s | sed -e '/\\.[p-type]/d' >" G_FILE, g_file);
    system(command);
    free(command);
    asprintf(&command, "espresso -Decho -of %s | sed -e '/\\.[p-type]/d' >" F_FILE, f_file);
    system(command);
    free(command);

    ParsedPLA f_on, g_on;
    DdNode **g_off = safe_malloc(NUM_OUT * sizeof(DdNode *));

    DdNode **h_off = safe_malloc(NUM_OUT * sizeof(DdNode *));
    DdNode **h_dc = safe_malloc(NUM_OUT * sizeof(DdNode *));

    parse(F_FILE, 0, &f_on, 0);
    parse(G_FILE, 0, &g_on, 0);

    /* f_off[i] = !(f_on[i] U f_dc[i]) */
    for (int i = 0; i < NUM_OUT; i++)
    {
        g_off[i] = Cudd_Not(g_on.vectorbdd_F[i]);
        Cudd_Ref(g_off[i]);

        h_dc[i] = Cudd_bddOr(manager, g_off[i], f_dc->vectorbdd_F[i]);
        Cudd_Ref(h_dc[i]);
    }

    char *curr_onset = safe_calloc((NUM_OUT + 1), sizeof(char));
    fflush(stdin);
    mergeBDDtoFile(TEMP_H_DECOMP, f_on.vectorbdd_F, h_dc);

    char *sys_command;
    asprintf(&sys_command, "espresso " TEMP_H_DECOMP " | sed -e '/\\.[p-type]/d' > " OUT_H_DECOMP);
    system(sys_command);

    ParsedPLA h_minim;
    parse(OUT_H_DECOMP, 0, &h_minim, 0);

    DdNode **and_out = safe_malloc(NUM_OUT * sizeof(DdNode *));

    for (int i = 0; i < NUM_OUT; i++)
        and_out[i] = Cudd_bddAnd(manager, g_on.vectorbdd_F[i], h_minim.vectorbdd_F[i]);

    FILE *eq = fopen(G_TIMES_H_FILE, "w+");
    fprintf(eq, ".i %d\n.o %d\n", NUM_IN, NUM_OUT);
    for (int o = 0; o < NUM_OUT; o++)
    {
        for (int i = 0; i < NUM_OUT; i++)
            curr_onset[i] = (i == o) ? '1' : '0';

        DdGen *gen_onset;
        int *cube;
        CUDD_VALUE_TYPE value;
        Cudd_ForeachCube(manager, and_out[o], gen_onset, cube, value)
        {
            for (int i = 0; i < NUM_IN; i++)
                (cube[i] == 2) ? fprintf(eq, "-") : fprintf(eq, "%d", cube[i]);
            fprintf(eq, " %s\n", curr_onset);
        }
    }

    /* pulizia */
    fclose(eq);
    free(curr_onset);
    free(and_out);
    free(sys_command);

    for (int i = 0; i < NUM_OUT; i++)
    {
        Cudd_RecursiveDeref(manager, g_off[i]);
        Cudd_RecursiveDeref(manager, h_dc[i]);
        Cudd_RecursiveDeref(manager, h_off[i]);
        Cudd_RecursiveDeref(manager, and_out[i]);
    }

    free(g_off);
    free(h_dc);
    free(h_off);
    free(and_out);

    /* verifica di correttezza */
    if (output_mode == VERBOSE_LOG)
    {
        printf("\n**************************\n");
        asprintf(&command, "espresso -Dverify %s " G_TIMES_H_FILE, f_file);
    }
    else
        asprintf(&command, "espresso -Dverify %s " G_TIMES_H_FILE " >> /dev/null 2>> /dev/null", f_file);

    if (system(command) != 0)
    {
        fprintf(stderr, "[!!] decomposition failed\n");
        exit(EXIT_FAILURE);
    }
    free(command);
}

/**
 * @brief Euristica di sintesi logica approssimata mediante espansione assistita.
 * 
 * @param pla la funzione originale
 * @param offset l'Off-set della funzione
 * @param dontPla il DC-set della funzione
 * @param s dati di test
 * @return double il tempo di calcolo
 */
double heuristic(ParsedPLA *pla, DdNode **offset, ParsedPLA *dontPla, struct test_stats *s)
{
    clock_t beginClock = clock(), endClock;

    prior_queue *queue = safe_calloc(1, sizeof(prior_queue));
    int *cube_iterator = safe_malloc(NUM_IN * sizeof(int));

    for (int o = 0; o < NUM_OUT; o++)
    {
        int product_i = 0;
        CubeListEntry *curr_entry;

        TAILQ_FOREACH(curr_entry, &(CUBE_LIST[o]), entries)
        {
            memcpy(cube_iterator, curr_entry->cube, NUM_IN * sizeof(int));

            for (int i = 0; i < NUM_IN; i++)
            {
                DdNode *curr_entry_node;

                if ((cube_iterator[i] == 1) || (cube_iterator[i] == 0))
                {
                    int dump = cube_iterator[i];
                    cube_iterator[i] = 2;
                    curr_entry_node = construct_product(cube_iterator, NUM_IN);

                    DdNode *intersect = Cudd_bddAnd(manager, curr_entry_node, offset[o]);
                    Cudd_Ref(intersect);
                    double complemented_minterms = Cudd_CountMinterm(manager, intersect, NUM_IN);

                    if ((complemented_minterms <= ct) && (complemented_minterms > 0)) /* può entrare nella coda */
                    {
                        int covered_prod = 0;
                        CubeListEntry *comparison_entry;
                        TAILQ_FOREACH(comparison_entry, &(CUBE_LIST[o]), entries)
                        {
                            int ret = covers(cube_iterator, comparison_entry->cube, NUM_IN);

                            if (ret == 1) /* a copre b ma a != b */
                                covered_prod++;
                            else if (ret == 2) /* il prodotto è già presente nella PLA */
                            {
                                covered_prod = -1;
                                break;
                            }
                        }

                        if (covered_prod >= 0)
                        {
                            product_t *cube_queue = safe_malloc(sizeof(product_t));
                            double priority = (double)covered_prod / complemented_minterms;
                            cube_queue->output_f = o;
                            cube_queue->compl_min = complemented_minterms;
                            cube_queue->covered_prod = covered_prod;
                            cube_queue->product_number = product_i;
                            cube_queue->valid = 1;
                            cube_queue->cube = safe_malloc(NUM_IN * sizeof(int));
                            cube_queue->offset_inters = intersect;
                            memcpy(cube_queue->cube, cube_iterator, NUM_IN * sizeof(int));

                            push(queue, priority, cube_queue);
                            (N_CUBES[o])++;
                        }
                        else
                            Cudd_RecursiveDeref(manager, intersect);
                    }

                    cube_iterator[i] = dump; /* ripristina cubo originale */
                    Cudd_RecursiveDeref(manager, curr_entry_node);
                }
            }

            product_i++;
        }
    }

    free(cube_iterator);

    if (output_mode == VERBOSE_LOG)
    {
        printf("**********************************\n");
        if (error_mode == GLOBAL_OUTPUT_ERROR)
            printf("Errore ammesso: max %lli mintermini sommati su tutti i %d output\n", ct, NUM_OUT);
        else
            printf("Errore ammesso: max %lli mintermini per ognuno dei %d output\n", ct, NUM_OUT);
        printf("r = %g, NUM_IN = %d\n", r, NUM_IN);

        printf("****************************\n");
        printf("Lunghezza coda prodotti eleggibili: %d\n", queue->len);
    }

    /* inizio estrazione coda */
    unsigned long long *current_errors = safe_calloc(NUM_OUT, sizeof(unsigned long long));
    unsigned long long total_error = 0;
    int added_product = 0, dcset_error = 0;
    product_t *curr_prod;

    while (queue->len > 0)
    {
        if ((error_mode == GLOBAL_OUTPUT_ERROR) && (total_error >= ct))
            break;

        curr_prod = pop(queue);

        if ((curr_prod == NULL) || (curr_prod->valid == 0))
            continue;

        DdNode *dcset_intersect = Cudd_bddAnd(manager, curr_prod->offset_inters, dontPla->vectorbdd_F[curr_prod->output_f]);
        Cudd_Ref(dcset_intersect);
        double dcset_minterms = Cudd_CountMinterm(manager, dcset_intersect, NUM_IN);
        double effective_minterms = curr_prod->compl_min - dcset_minterms;

        if (effective_minterms < 0)
        {
            fprintf(stderr, "Errore inatteso. Chiusura.\n");
            printf("COMPL: %g, DCSET: %g\n", curr_prod->compl_min, dcset_minterms);
            exit(EXIT_FAILURE);
        }

        Cudd_RecursiveDeref(manager, dcset_intersect);

        if ((error_mode == MULTIPLE_OUTPUT_ERROR && (effective_minterms + current_errors[curr_prod->output_f] > ct)) ||
            (error_mode == GLOBAL_OUTPUT_ERROR && (total_error + effective_minterms > ct)))
            continue; /* selezione greedy, toglilo dalla coda e continua */

        dcset_error += dcset_minterms;
        current_errors[curr_prod->output_f] += effective_minterms;
        total_error += effective_minterms;

        added_product++;

        invalidateRedundantInQueue(curr_prod, queue, NUM_IN);
        removeCoveredProducts(curr_prod, pla);

        CubeListEntry *expanded_product = safe_malloc(sizeof(CubeListEntry));
        expanded_product->cube = curr_prod->cube;
        TAILQ_INSERT_TAIL(&(CUBE_LIST[curr_prod->output_f]), expanded_product, entries);
        (N_CUBES[curr_prod->output_f])++;

        free(curr_prod);

#ifdef DEBUG
        printf("Scelto prodotto con m_compl = %f, covered = %d *** Ct_%d = %d\n", effective_minterms,
               curr_prod->covered_prod, curr_prod->output_f, current_errors[curr_prod->output_f]);
        print_cube(cube->cube, NUM_IN);
        printf("New queue len: %d\n", queue->len);
        for (int i = 1; i <= queue->len; i++)
        {
            if (queue->nodes[i].data != NULL)
            {
                printf("\t");
                print_cube(queue->nodes[i].data->cube, NUM_IN);
                printf(" - compl: %g, covered: %d, out: %d, priority: %g, valid: %d\n", queue->nodes[i].data->compl_min,
                       queue->nodes[i].data->covered_prod, queue->nodes[i].data->output_f, queue->nodes[i].priority,
                       queue->nodes[i].data->valid);
            }
        }
#endif
    }

    if (output_mode == VERBOSE_LOG)
    {
        printf("Prodotti aggiunti: %d\n", added_product);
        printf("********************************\n");
        printf("Errore totale computato: %lli\n", total_error);
        printf("Errore DC-set: %d\n", dcset_error);
        printf("Errore per output: ");
        for (int i = 0; i < (NUM_OUT - 1); i++)
            printf("%lli, ", current_errors[i]);
        printf("%lli", current_errors[(NUM_OUT - 1)]);
        printf("\n");

        struct test_stats temp;
        printf("\nDopo euristica -> ");
        mergeToPLA(pla, OUTPUT_PLA);
        getPLAFileData(OUTPUT_PLA, NUM_OUT, &temp);
        print_verbose_stats(temp);
    }

    free(current_errors);

    /**
     * @brief rimozione dei prodotti coperti dall'OR di tutti i prodotti della funzione
     *        eccetto lo stesso
     */
    for (int o = 0; o < NUM_OUT; o++)
    {
        CubeListEntry *outer, *inner;
        TAILQ_FOREACH(outer, &(CUBE_LIST[o]), entries)
        {
            if (outer == NULL)
                continue;

            DdNode *single_prod = construct_product(outer->cube, NUM_IN);
            DdNode *foo_or = Cudd_ReadLogicZero(manager);
            Cudd_Ref(foo_or);

            TAILQ_FOREACH(inner, &(CUBE_LIST[o]), entries)
            {
                if ((inner != NULL) && (outer != inner))
                {
                    DdNode *curr_node = construct_product(inner->cube, NUM_IN);
                    DdNode *tmp = Cudd_bddOr(manager, foo_or, curr_node);
                    Cudd_Ref(tmp);
                    Cudd_RecursiveDeref(manager, foo_or);
                    Cudd_RecursiveDeref(manager, curr_node);
                    foo_or = tmp;
                }
            }

            /* il prodotto singolo è coperto dall'or, possiamo toglierlo */
            if (Cudd_bddLeq(manager, single_prod, foo_or))
            {
                TAILQ_REMOVE(&(CUBE_LIST[o]), outer, entries);
                (N_CUBES[o])--;
            }

            Cudd_RecursiveDeref(manager, single_prod);
            Cudd_RecursiveDeref(manager, foo_or);
        }
    }

    mergeToPLA(pla, MINIMIZED_OUTPUT_PLA);
    getPLAFileData(MINIMIZED_OUTPUT_PLA, NUM_OUT, s);
    if (output_mode == VERBOSE_LOG)
    {
        printf("Dopo euristica e rimozione ridondanze -> ");
        print_verbose_stats(*s);
    }

    endClock = clock();
    double time_spent = (double)(endClock - beginClock) / CLOCKS_PER_SEC;

    return time_spent;
}

/**
 * @brief funzione main, si veda la funzione "usage" per l'utilizzo da riga di comando
 * 
 * @param argc 
 * @param argv 
 * @return int 
 */
int main(int argc, char *argv[])
{
    struct stat st = {0};
    int ret;

    if (stat(TEMP_DIR, &st) == -1)
    {
        ret = mkdir(TEMP_DIR, 0700);
        if (ret == -1)
        {
            fprintf(stderr, "Impossibile creare la cartella " TEMP_DIR " : ");
            perror("");
            exit(EXIT_FAILURE);
        }
    }
    if (stat(OUTPUT_DIR, &st) == -1)
    {
        ret = mkdir(OUTPUT_DIR, 0700);
        if (ret == -1)
        {
            fprintf(stderr, "Impossibile creare la cartella " OUTPUT_DIR " : ");
            perror("");
            exit(EXIT_FAILURE);
        }
    }

    if (argc < 2)
    {
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    int opt;
    char *endptr;

    while ((opt = getopt(argc, argv, "dgmt")) != -1)
    {
        if ((opt == 'g') || (opt == 'm'))
        {
            if (argv[optind][strlen(argv[optind]) - 1] == '%')
                ct_percent = 1;

            ct = strtol(argv[optind], &endptr, 10);
            check_strtol(ct, argv[optind], endptr);
            if (ct <= 0)
            {
                fprintf(stderr, "[!!] L'errore deve essere >= 0\n");
                exit(EXIT_FAILURE);
            }
            if ((ct_percent) && (ct > 100))
            {
                fprintf(stderr, "[!!] L'errore percentuale non può superare il 100%%\n");
                exit(EXIT_FAILURE);
            }
        }

        if (opt == 'g')
            error_mode = GLOBAL_OUTPUT_ERROR;
        else if (opt == 'm')
            error_mode = MULTIPLE_OUTPUT_ERROR;
        else if (opt == 't')
            output_mode = TEST_LOG;
        else if (opt == 'd')
            output_mode = DECOMPOSITION_LOG;
        else
        {
            print_usage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (access(argv[argc - 1], F_OK) == -1)
    {
        perror("[!!] impossibile accedere al file PLA");
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    /* minimizzazione della funzione */
    char *sys_command;
    asprintf(&sys_command, "espresso -Decho -od %s | sed -e '/\\.[p-type]/d' > " DONT_CARE_PLA, argv[argc - 1]);
    system(sys_command);
    free(sys_command);
#ifndef EXACT_MINIMIZATION
    asprintf(&sys_command, "espresso %s | sed -e '/\\.[p-type]/d' > " MINIM_PLA, argv[argc - 1]);
#endif
#ifdef EXACT_MINIMIZATION
    asprintf(&sys_command, "espresso -Dexact %s | sed -e '/\\.[p-type]/d' > " MINIM_PLA, argv[argc - 1]);
#endif
    system(sys_command);
    free(sys_command);

    /* ************************************************ */

    ParsedPLA dcSetFunc, minimizedFunc;
    DdNode **offsetBDD;
    struct test_stats s;
    double cpu_time;

    /* conversione file e parsing */
    convertDCSetPLA(DONT_CARE_PLA);
    parse(DONT_CARE_PLA, 1, &dcSetFunc, 0);
    parse(MINIM_PLA, 0, &minimizedFunc, 1);

    NUM_IN = minimizedFunc.num_in;
    NUM_OUT = minimizedFunc.num_out;

    if ((ct_percent) && (CHAR_BIT * sizeof(ct) < NUM_IN))
    {
        fprintf(stderr, "[!!] impossibile utilizzare l'errore percentuale con %d input, "
                        "specificare il numero di mintermini\n",
                NUM_IN);
        exit(EXIT_FAILURE);
    }

    offsetBDD = safe_malloc(NUM_OUT * sizeof(DdNode *));
    for (int i = 0; i < NUM_OUT; i++)
    {
        offsetBDD[i] = Cudd_Not(minimizedFunc.vectorbdd_F[i]);
        Cudd_Ref(offsetBDD[i]);
    }

    getPLAFileData(MINIM_PLA, NUM_OUT, &original_pla_stats);

    if (output_mode == VERBOSE_LOG)
    {
        printf("NUM IN: %d, NUM OUT: %d\n", NUM_IN, NUM_OUT);
        printf("*********************************\nFunzione originale: ");
        getPLAFileData(argv[argc - 1], NUM_OUT, &s);
        print_verbose_stats(s);
        printf("Funzione minimizzata: ");
        print_verbose_stats(original_pla_stats);
    }

    unsigned long long two_pow = (NUM_IN < 31) ? (1L << NUM_IN) : powl(2L, NUM_IN);

    if ((two_pow == 0LL) && (ct_percent))
    {
        fprintf(stderr, "[!!] errore inatteso, chiusura.");
        exit(EXIT_FAILURE);
    }

    /* la probabilità ct è richiesta in percentuale su 2^NUM_IN */
    if (ct_percent)
    {
        r = (double)ct / 100;
        ct = floorl(r * two_pow);

        if (ct < 0)
        {
            fprintf(stderr, "[!!] errore di range\n");
            exit(EXIT_FAILURE);
        }
    }
    else
        r = (two_pow == 0LL) ? 0.0f : (double)ct / two_pow;

    cpu_time = heuristic(&minimizedFunc, offsetBDD, &dcSetFunc, &heuristic_pla_stats);

    system("espresso " MINIMIZED_OUTPUT_PLA " > " ESPRESSO_OUTPUT_PLA);
    getPLAFileData(ESPRESSO_OUTPUT_PLA, NUM_OUT, &espresso_pla_stats);

    /**
     * @brief scegliamo tra la PLA dopo euristica e la PLA dopo euristica + espresso
     *        quale delle due fornisce la riduzione maggiore di letterali e la scegliamo per il 
     *        confronto con la PLA originale minimizzata.
     * NOTA: la precedenza viene data alla PLA col 
     *          1. minor numero di porte OR
     *          2. minor numero di letterali AND
     *          3. PLA euristica + espresso
     */
    struct test_stats *chosen_pla;

    if (heuristic_pla_stats.or_port == espresso_pla_stats.or_port)
        chosen_pla = (espresso_pla_stats.and_lit <= heuristic_pla_stats.and_lit) ? &espresso_pla_stats : &heuristic_pla_stats;
    else
        chosen_pla = (espresso_pla_stats.or_port < heuristic_pla_stats.or_port) ? &espresso_pla_stats : &heuristic_pla_stats;

    if (output_mode == VERBOSE_LOG)
    {
        printf("*********************************\nConfronto con PLA Espresso:\n");
        system("espresso -Dverify " MINIMIZED_OUTPUT_PLA " " MINIM_PLA);
        printf("\n*********************************\nConfronto con PLA euristica con ridondanze:\n");
        system("espresso -Dverify " MINIMIZED_OUTPUT_PLA " " OUTPUT_PLA);
        printf("\n*********************************\nEsecuzione di Espresso sulla PLA euristica senza ridondanze:\n");
        print_verbose_stats(espresso_pla_stats);
    }

    /**
     * @brief Calcolo del guadagno rispetto alla PLA originale
     */
    int or_diff, and_diff;
    double or_perc, and_perc, tot_perc;

    or_diff = original_pla_stats.or_port - chosen_pla->or_port;
    or_perc = (original_pla_stats.or_port > 0) ? ((double)or_diff / original_pla_stats.or_port) * 100 : 0;
    and_diff = original_pla_stats.and_lit - chosen_pla->and_lit;
    and_perc = ((double)and_diff / original_pla_stats.and_lit) * 100;

    tot_perc = ((double)or_diff + and_diff) / (original_pla_stats.or_port + original_pla_stats.and_lit) * 100;

    if (output_mode == VERBOSE_LOG)
    {
        printf("*********************************\nGuadagno: ");
        printf("OR: %d, AND: %d, TOT: %d\n", or_diff, and_diff, (or_diff + and_diff));
        printf("Guadagno percentuale: OR: %.2f%%, AND: %.2f%%, TOT: %.2f%%\n", or_perc, and_perc, tot_perc);
        printf("*********************************\nCPU time: %gs\n", cpu_time);
    }
    /**
     * @brief stampa il risultato dei test in formato CSV
     * 
     * FORMATTAZIONE: nome_file, ct, r, orig_and, orig_or, new_and, new_or, and_%, or_%, CPU_time[s]
     */
    else if (output_mode == TEST_LOG)
    {
        char *pla_name = basename(argv[argc - 1]);
        int len = strlen(pla_name);
        for (int i = 1; i <= 4; i++)
            pla_name[len - i] = '\0';

        if (cpu_time == 0.00f)
            cpu_time = 0.01f;

        printf("%s (%d/%d); %lli; %g; %d; %d; %d; %d; %.2f\n",
               pla_name, NUM_IN, NUM_OUT, ct, (r * (double)100),
               original_pla_stats.and_lit, original_pla_stats.or_port,
               chosen_pla->and_lit, chosen_pla->or_port,
               cpu_time);

        cleanRoutine(&minimizedFunc, offsetBDD, &dcSetFunc);
        Cudd_Quit(manager);
        return 0;
    }

    struct test_stats h_func_stats;
    andDecomposition(&dcSetFunc, MINIMIZED_OUTPUT_PLA, argv[argc - 1]);
    getPLAFileData(OUT_H_DECOMP, NUM_OUT, &h_func_stats);

    struct test_stats g_times_h = {.and_lit = chosen_pla->and_lit + h_func_stats.and_lit,
                                   .or_port = chosen_pla->or_port + h_func_stats.or_port};

    if (output_mode == VERBOSE_LOG)
    {
        printf("\n*************************\n");
        printf("OLD SOP LENGTH - AND: %d, OR: %d, TOT: %d\n", original_pla_stats.and_lit,
               original_pla_stats.or_port, (original_pla_stats.and_lit + original_pla_stats.or_port));
        printf("NEW SOP LENGTH - AND: %d, OR: %d, TOT: %d\n", g_times_h.and_lit, g_times_h.or_port,
               (g_times_h.and_lit + g_times_h.or_port));
    }
    /**
     * @brief stampa il risultato della decomposizione in formato CSV
     * 
     * FORMATTAZIONE: name, ct, r, f_and, g*h_and
     */
    else if (output_mode == DECOMPOSITION_LOG)
    {
        double and_area_factor, or_area_factor, tot_area_factor;
        char *pla_name = basename(argv[argc - 1]);
        int len = strlen(pla_name);
        for (int i = 1; i <= 4; i++)
            pla_name[len - i] = '\0';

        printf("%s (%d/%d); %d; %d;\n",
               pla_name, NUM_IN, NUM_OUT,
               original_pla_stats.and_lit, g_times_h.and_lit);
    }

    cleanRoutine(&minimizedFunc, offsetBDD, &dcSetFunc);

    Cudd_Quit(manager);

    return 0;
}