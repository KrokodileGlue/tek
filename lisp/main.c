#include <stdlib.h>
#include <string.h>

#include "lex.h"
#include "util.h"
#include "parse.h"
#include "eval.h"
#include "error.h"

int main(int argc, char **argv)
{
	(void)argc;

	struct lexer *lexer = new_lexer(argv[1], load_file(argv[1]));
	struct value *env = new_environment();
	struct token *t;
	unsigned num_error = 0;

	while ((t = tok(lexer, TALK))) {
		if (t->type != '(') continue;
		struct value *e = eval(env, parse(env, lexer));
		if (e->type != VAL_ERROR) continue;
		print_error(stdout, e);
		num_error++;
		if (num_error < 50) continue;
		print_error(stdout, error(t->loc,
		                    "encountered too many errors"));
		break;
	}

	return num_error;
}
