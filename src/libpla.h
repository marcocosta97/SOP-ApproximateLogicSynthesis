/**
 * @file libpla.h
 * @author Marco Costa
 * @brief Contiene metodi e strutture per l'interazione tra CUDD e i file PLA
 * @date 2019-11-25
 */

#ifndef _LIBPLA_H
#define _LIBPLA_H

#include <stdio.h>
#include <cudd.h>
#include <sys/queue.h>

DdManager *manager; /**< CUDD manager */

typedef struct CubeListEntry
{
    int *cube;
    TAILQ_ENTRY(CubeListEntry)
    entries;
} CubeListEntry;

int *N_CUBES;
TAILQ_HEAD(queue, CubeListEntry)
*CUBE_LIST;

typedef struct ParsedPLA
{
    int num_in;           /**< number of input variables */
    int num_x;            /**< number of x variables */
    int num_out;          /**< number of output */
    DdNode **vectorbdd_F; /**< BDD array for output */
} ParsedPLA;

/**
 * @brief Struttura per l'inserimento di dati di una PLA
 */
struct test_stats
{
    int prod_in;
    int prod_out;
    int and_lit;
    int or_port;
};

static inline void print_verbose_stats(struct test_stats s)
{
    printf("IN: %d, OUT: %d, TOT: %d, AND LITERALS: %d, OR PORT: %d\n",
           s.prod_in, s.prod_out, (s.prod_in + s.prod_out), s.and_lit, s.or_port);
}

/**
 * @brief Converte un file PLA in un file PLA contenente unicamente il DC-set
 * 
 * @param filename il file
 */
void convertDCSetPLA(char *filename);

/**
 * @brief Ottiene il numero di letterali e prodotti da un file PLA
 * 
 * @param filename il file
 * @param function_out il numero di uscite della funzione
 * @param s la struttura
 */
void getPLAFileData(char *filename, int function_out, struct test_stats *s);

/**
 * @brief Inizializzazione di una struttura ParsedPLA
 * 
 * @param bdd la struttura
 */
void initParsedPLA(ParsedPLA *bdd);

/**
 * @brief Inizializzazione di una struttura CubeListEntry di dimensione size
 * 
 * @param size 
 * @return CubeListEntry* 
 */
CubeListEntry *alloc_node(int size);

/**
 * @brief Stampa di una matrice come file PLA.
 * 
 * @param filename il file di out
 * @param M la matrice
 * @param len numero di righe
 * @param in numero di ingressi
 * @param out numero di uscite
 */
void printMatrixtoFile(char *filename, int **M, int len, int in, int out);

#endif