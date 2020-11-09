# SOP-ApproximateLogicSynthesis

This software performs Approximate Logic Synthesis of Sum Of Products forms as described on [Shin, Gupta (2010)](https://www.researchgate.net/publication/221341253_Approximate_logic_synthesis_for_error_tolerant_applications). Furthermore, it uses a technique called bi-decomposition to rebuild the original function in a new 3-level form (AND-OR-AND) without approximation. The final number of literals is guaranteed to be **less or equal than** the original function. A reference on bi-decomposition can be found [here](https://upcommons.upc.edu/bitstream/handle/2117/126070/01201580.pdf).

You can find the results for this study on my bachelor thesis [here](https://github.com/mcdeldams/SOP-ApproximateLogicSynthesis/blob/master/pdf/tesi.pdf) (*in Italian!*).

## Requirements

This software requires the [cudd](https://web.mit.edu/sage/export/tmp/y/usr/share/doc/polybori/cudd/cuddExtDet.html) library (at least version 3.0) in order to perform efficiently basic operations on Boolean functions (you can download it [here](https://github.com/ivmai/cudd/releases)) and the [Espresso logic minimizer](https://ptolemy.berkeley.edu/projects/embedded/pubs/downloads/espresso/index.htm) installed on your system.

## Building

Edit the Makefile `CUDD_PATH = /YOUR/CUDD/PATH` variabile with the path of your cudd installation and then execute
```bash
$ make all
```

## Running

Once built execute the following command
```bash
$ ./main -g error input.pla
```
where *error* is the desidered error percentage for all the inputs as defined in Shin, Gupta (2010) and *input.pla* is the Boolean function in a PLA format.

