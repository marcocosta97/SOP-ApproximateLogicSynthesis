/**
 * @file config.h
 * @author Marco Costa
 * @brief Configurazione per le directory temporanee e nomi dei file
 * @date 2019-11-25
 */

#ifndef _CONFIG_H
#define _CONFIG_H

#define TEMP_DIR "/tmp/pla/"
#define OUTPUT_DIR "./out/"

#define DONT_CARE_PLA TEMP_DIR "dontset.pla"
#define MINIM_PLA TEMP_DIR "minimized.pla"
#define OFFSET_PLA TEMP_DIR "offset.pla"

#define MINIMIZED_OUTPUT_PLA OUTPUT_DIR "out_minimized.pla"
#define ESPRESSO_OUTPUT_PLA OUTPUT_DIR "out_espresso.pla"
#define OUTPUT_PLA OUTPUT_DIR "out.pla"

#define BEST_OUTPUT_PLA OUTPUT_DIR "best.pla" // the best between espresso and not

#define DEFAULT_CT 1

/* decomposizione */
#define G_FILE TEMP_DIR "g_file.pla"
#define F_FILE TEMP_DIR "f_file.pla"

#define TEMP_H_DECOMP TEMP_DIR "temp_h_func.pla"
#define G_TIMES_H_FILE TEMP_DIR "decomp_check.pla"
#define OUT_H_DECOMP OUTPUT_DIR "h_func.pla"

#define ORIGINAL_ONSET_PLA TEMP_DIR "original_onset.pla"

#endif