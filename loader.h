#ifndef LOADER_H_
#define LOADER_H_

#include "dsu.h"

//  If CONTENT is true, load both contents and symbols.
//  If CONTENT is false, load just symbols.
void load_elf (dsu *a_dsu, const char *filename, bool content);

void load_bin (dsu &a_dsu, word addr, const unsigned char *buf, word len);

string symbolize (word addr);

void disp_symbols (void);

#endif /* LOADER_H_ */
