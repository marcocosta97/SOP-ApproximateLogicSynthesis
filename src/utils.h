#ifndef _UTILS_H
#define _UTILS_H

/**
 * @file utils.h
 * @author Marco Costa
 * @brief Funzioni di utilità generica per gestione della memoria
 *  e controlli di errore
 */

#include <stdio.h>
#include <stdlib.h>

#define print_cube(cube, n)     \
    for (int k = 0; k < n; k++) \
        printf("%d", cube[k]);  \
    printf("\n");

#define min(a, b) \
    ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

#define check_strtol(res, str, endptr)                           \
    if (endptr == str)                                           \
    {                                                            \
        perror("[!!] impossibile parsare il valore");            \
        exit(EXIT_FAILURE);                                      \
    }                                                            \
    if ((res == LONG_MAX || res == LONG_MIN) && errno == ERANGE) \
    {                                                            \
        perror("[!!] il valore è out of range");                 \
        exit(EXIT_FAILURE);                                      \
    }

static inline void *safe_malloc(size_t n)
{
    void *p = malloc(n);
    if (!p && n > 0)
    {
        fprintf(stderr, "Impossibile allocare la memoria\n");
        exit(EXIT_FAILURE);
    }
    return p;
}

static inline void *safe_calloc(size_t nmemb, size_t nsize)
{
    void *p = calloc(nmemb, nsize);
    if (!p && nmemb > 0 && nsize > 0)
    {
        fprintf(stderr, "Impossibile allocare la memoria\n");
        exit(EXIT_FAILURE);
    }
    return p;
}

#endif