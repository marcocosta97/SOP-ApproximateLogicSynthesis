/**
 * @file PLAparser.c
 * @author Marco Costa
 * @brief Contiene l'implementazione dei metodi per il parsing dei file .pla
 * @date 2019-11-19
 */

#include "PLAparser.h"
#include <string.h>

#include "utils.h"
#include "libpla.h"

#define MAX_LEN 512

int *cube;

/**
 * @brief costruisce un nodo rappresentante un singolo prodotto
 * 
 * @param input il vettore di caratteri rappresentante il prodotto
 * @param pla la struttura relativa al file
 * @return DdNode* il nodo rappresentante il prodotto
 */
DdNode *read_product(char *input, ParsedPLA *pla)
{
	DdNode *f;
	DdNode *tmpNode;
	DdNode *var;

	f = Cudd_ReadOne(manager);
	Cudd_Ref(f);

	for (int i = pla->num_in - 1; i >= 0; i--)
	{
		if (input[i] == '-' || input[i] == '4' || input[i] == '~')
		{
			cube[i] = 2;
			continue;
		}
		var = Cudd_bddIthVar(manager, i);
		if (input[i] == '0')
		{
			tmpNode = Cudd_bddAnd(manager, Cudd_Not(var), f);
			cube[i] = 0;
		}
		else
		{
			tmpNode = Cudd_bddAnd(manager, var, f);
			cube[i] = 1;
		}
		Cudd_Ref(tmpNode);
		Cudd_RecursiveDeref(manager, f);
		f = tmpNode;
	}

	return f;
}

/**
 * @brief inserisce il nodo f all'interno della bdd
 * 
 * @param f il nodo
 * @param output l'output legato ad f
 * @param pla la struttura del file
 */
void build_bdd(DdNode *f, char *output, ParsedPLA *pla, int isMinimized)
{
	DdNode *tmpNode;
	for (int i = 0; i < pla->num_out; i++)
	{
		if (output[i] == '0') // OFF set
			continue;
		if (output[i] == '1')
		{ // ON set
			tmpNode = Cudd_bddOr(manager, f, pla->vectorbdd_F[i]);
			Cudd_Ref(tmpNode);
			Cudd_RecursiveDeref(manager, pla->vectorbdd_F[i]);
			pla->vectorbdd_F[i] = tmpNode;

			if (isMinimized)
			{
				CubeListEntry *c = alloc_node(pla->num_in);
				memcpy(c->cube, cube, pla->num_in * sizeof(int));
				TAILQ_INSERT_TAIL(&(CUBE_LIST[i]), c, entries);
				(N_CUBES[i])++;
			}
		}
	}

	Cudd_RecursiveDeref(manager, f);
}

int parse(char *inputfile, int init_manager, ParsedPLA *pla, int isMinimized)
{
	char tmp[MAX_LEN];
	int done = 0;
	DdNode *f;
	FILE *PLAFile;
	PLAFile = fopen(inputfile, "r");
	if (PLAFile == NULL)
	{
		fprintf(stderr, "Error opening file %s\n ", inputfile);
		return -1;
	}
	while (!done && fscanf(PLAFile, "%s\n", tmp) != EOF)
	{
		if (tmp[0] == '.')
		{
			switch (tmp[1])
			{
			case 'i':
			{
				fscanf(PLAFile, "%d\n", &(pla->num_in));
				if (init_manager)
					manager = Cudd_Init(pla->num_in, 0, CUDD_UNIQUE_SLOTS, CUDD_CACHE_SLOTS, 0);
			}
			break;
			case 'o':
			{
				int i;

				fscanf(PLAFile, "%d\n", &(pla->num_out));
				pla->vectorbdd_F = (DdNode **)calloc(pla->num_out, sizeof(DdNode *));
				if (pla->vectorbdd_F == NULL)
				{
					fprintf(stderr, "INPUT vectorbdd_F: Error in calloc\n");
					Cudd_Quit(manager);
					fclose(PLAFile);
					return -1;
				}
				for (i = 0; i < pla->num_out; i++)
				{
					pla->vectorbdd_F[i] = Cudd_ReadLogicZero(manager);
					Cudd_Ref(pla->vectorbdd_F[i]);
				}
				done = 1;
			}
			break;
			}
		}
	}
	if (pla->num_in <= 0 || pla->num_out <= 0)
	{
		fclose(PLAFile);
		return -1;
	}

	int readInput = 1;
	int readOutput = 0;
	int inputreaded = 0;
	int outputreaded = 0;
	char delimit[] = "|";
	char *input = (char *)calloc(pla->num_in + 1, sizeof(char));
	char *output = (char *)calloc(pla->num_out + 1, sizeof(char));

	cube = safe_malloc(pla->num_in * sizeof(int));
	if (isMinimized)
		initParsedPLA(pla);

	while (fscanf(PLAFile, "%s", tmp) > 0)
	{

		if (strlen(tmp) > pla->num_in + pla->num_out)
		{
			char *tmpstr;
			char *p1 = strtok_r(tmp, delimit, &tmpstr);
			strcpy(input, p1);
			char *p2 = strtok_r(NULL, "\n", &tmpstr);
			strcpy(output, p2);
			f = read_product(input, pla);
			Cudd_Ref(f);
			build_bdd(f, output, pla, isMinimized);
			readInput = 0;
			readOutput = 0;
		}
		else if (readInput)
		{
			inputreaded += strlen(tmp);
			if (inputreaded < pla->num_in)
			{ // input on multiple lines
				input = strcat(input, tmp);
			}
			else
			{
				input = strcat(input, tmp);
				readInput = 0;
				readOutput = 1;
				inputreaded = 0;
				f = read_product(input, pla); // reads the product in PLA
				Cudd_Ref(f);
				input[0] = '\0';
			}
		}
		else if (readOutput)
		{
			outputreaded += strlen(tmp);
			if (outputreaded < pla->num_out)
			{ // output on multiple lines
				output = strcat(output, tmp);
			}
			else
			{
				output = strcat(output, tmp);
				readInput = 1;
				readOutput = 0;
				outputreaded = 0;
				build_bdd(f, output, pla, isMinimized); // inserts the product in BDD
				output[0] = '\0';
			}
		}
	}

	free(input);
	free(output);
	free(cube);
	fclose(PLAFile);

	return 1;
}
