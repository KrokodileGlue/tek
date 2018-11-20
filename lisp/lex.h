#ifndef LEX_H
#define LEX_H

#include <stdio.h>
#include "location.h"

struct token {
	/*
	 * There's no point in giving punctuation characters their own
	 * token types or data field; we can just use the type field
	 * to store it instead, that way we can just do stuff like:
	 *
	 *   tok->type == '.'
	 *
	 * N.B. TOK_EOF must always be the final value in this enum.
	 */

	enum {
		TOK_INT,
		TOK_STR,
		TOK_IDENT,
		TOK_EOF
	} type;

	char *body;            /* Region of input this is from.     */
	struct location loc;   /* Location in source code.          */

	/*
	 * Identifiers don't have their own data field because their
	 * fields would be identical to their bodies.
	 */

	union {
		char *s;       /* String contents.                  */
		int i;         /* Integer.                          */
	};
};

struct lexer {
	const char *file;      /* Filename.                         */
	struct location loc;   /* Current input location.           */
	const char *s;         /* Input stream.                     */
	const char *e;         /* End of input stream.              */
};

enum mode {
	TALK, CODE
};

struct lexer *new_lexer(const char *file, const char *s);
struct token *tok(struct lexer *l, enum mode mode);
void print_token(FILE *f, struct token *t);

#endif
