#ifndef _PLA_PARSER_H
#define _PLA_PARSER_H

/**
 * @file PLAparser.h
 * @author Marco Costa
 * @brief Contiene il prototipo per il metodo di parsing dei file .pla
 * @date 2019-11-19
 */

#include <stdio.h>
#include <stdlib.h>
#include <cudd.h>

#include "libpla.h"

/**
 * @brief legge il file .pla e costruisce la bdd relativa
 * @param inputfile .pla file
 * @param init_manager se 1 il cudd manager deve essere inizializzato
 * @param bdd la bdd risultante
 * @param isMinimized se deve essere costruito il vettore di liste di prodotti
 * @return -1 in caso di errore, 1 altrimenti
 */
int parse(char *inputfile, int init_manager, ParsedPLA *bdd, int isMinimized);

#endif