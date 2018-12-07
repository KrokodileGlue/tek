#ifndef ERROR_H
#define ERROR_H

#include <stdio.h>
#include "location.h"

struct value *error(struct location *loc, const char *fmt, ...);
void print_error(FILE *f, struct value *e);

#endif
