#ifndef PARSE_H
#define PARSE_H

#include "lisp.h"
#include "lex.h"

struct value *parse(struct value *env, struct lexer *l);

#endif
