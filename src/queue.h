#ifndef _QUEUE_H
#define _QUEUE_H

/**
 * @file queue.h
 * @author Marco Costa
 * @brief Include i prototipi e le strutture per la gestione della coda di
 *  priorità
 * @version 0.1
 */

#include <stdio.h>
#include <stdlib.h>

#include <cudd.h>

#define INIT_SIZE 20

/**
 * @brief Struttura rappresentante un singolo prodotto espanso
 */
typedef struct
{
    int *cube;          /* prodotto rappresentato */
    int output_f;       /* funzione di output */
    int covered_prod;   /* numero di prodotti coperti */
    double compl_min;   /* numero di mintermini complementati */
    int product_number; /* espansione di provenienza */
    int valid;          /* validità prodotto in coda */
    DdNode *offset_inters;
} product_t;

typedef struct
{
    double priority;
    product_t *data;
} node_t;

typedef struct
{
    node_t *nodes;
    int len;
    int size;
} prior_queue;

/**
 * @brief Inserimento di un elemento in coda con priorità "priority"
 * 
 * @param h la coda di priorità
 * @param priority la priorità
 * @param data l'elemento da inserire in coda
 */
void push(prior_queue *h, double priority, product_t *data);

/**
 * @brief Estrazione del prodotto con priorità massima dalla coda jh
 * 
 * @param h la coda
 * @return product_t* il prodotto con priorità massima
 */
product_t *pop(prior_queue *h);

#endif