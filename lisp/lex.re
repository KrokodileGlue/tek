#include "lex.h"

#include <string.h>
#include <stdlib.h>

struct lexer *
new_lexer(const char *file, const char *s)
{
	struct lexer *l = malloc(sizeof *l);
	memset(l, 0, sizeof *l);

	l->s = s;
	l->e = s + strlen(s);
	l->file = file;
	l->loc.file = file;
	l->loc.text = s;

	return l;
}

#define YYCTYPE char
#define YYFILL(X) do {} while (0)
#define YYMARKER (*a)
#define YYCURSOR (*b)

static int
lex1(const char **a,
     const char **b,
     unsigned *line,
     unsigned *column,
     const char *YYLIMIT)
{
 loop:
	YYMARKER = YYCURSOR;
	/*!re2c
	  "\n" { (*line)++, *column = 0; goto loop; }
	  [ \t\v\r] { (*column)++; goto loop; }
	  "#" .* { goto loop; }
	  [!-/:-@[-`{-~] { return **a; }
	  [a-zA-Z0-9]+   { return TOK_IDENT; }
	  * { *b = NULL; return -1; }
	  "\000" { YYCURSOR--; return TOK_EOF; }
	  */
}

static int
lex2(const char **a,
     const char **b,
     unsigned *line,
     unsigned *column,
     const char *YYLIMIT)
{
 loop:
	YYMARKER = YYCURSOR;
	/*!re2c
	  dec = [1-9][0-9]*;
	  bin = '0b' [01]+;
	  hex = '0x' [0-9a-fA-F]+;
	  oct = '0' [0-7]*;
	  ident = [a-zA-Z+=!@#$%^&/*<>-];
	  "\n" { (*line)++, *column = 0; goto loop; }
	  [ \t\v\r] { (*column)++; goto loop; }
	  "#" .* { goto loop; }
	  ident [a-zA-Z0-9-]* { return TOK_IDENT; }
	  [!-/:-@[-`{-~] { return **a; }
	  '"' ("\\\""|[^"])* '"' { return TOK_STR; }
	  dec { return TOK_INT; }
	  bin | hex | oct { YYMARKER--; return TOK_INT; }
	  * { *b = NULL; return -1; }
	  "\000" { YYCURSOR--; return TOK_EOF; }
	*/
}

static char *
lex_escapes(struct token *t)
{
	char *r = malloc(t->loc.len + 1);
	unsigned j = 0;

	for (unsigned i = 1; i < t->loc.len - 1; i++) {
		if (t->body[i] != '\\') {
			r[j++] = t->body[i];
			continue;
		}
		i++;
		switch (t->body[i]) {
		case 'n': r[j++] = '\n'; break;
		case 't': r[j++] = '\t'; break;
		case '"': r[j++] = '"'; break;
		default:
			r[j++] = '\\';
			r[j++] = t->body[i];
		}
	}

	r[j] = 0;

	return r;
}

struct token *
tok(struct lexer *l, enum mode mode)
{
	struct token *t = malloc(sizeof *t);
	const char *a = l->s + l->loc.idx;
	const char *b = a;

	t->type = mode == CODE
		? lex2(&a, &b, &l->loc.line, &l->loc.column, l->e)
		: lex1(&a, &b, &l->loc.line, &l->loc.column, l->e);
	if (t->type == TOK_EOF) return free(t), NULL;

	/* Assign the basic fields that all tokens have. */

	l->loc.len = b - a;

	t->body = malloc(b - a + 1);
	memcpy(t->body, a, b - a);
	t->body[b - a] = 0;

	t->loc = l->loc;

	/*
	 * `lex()` can't count lines and columns inside token bodies,
	 * so we'll do it here.
	 */

	for (unsigned i = 0; i < l->loc.len; i++) {
		if (t->body[i] == '\n')
			l->loc.line++, l->loc.column = 0;
		else l->loc.column++;
	}

	/* Fill out the type-specific fields. */

	switch (t->type) {
	case TOK_STR:
		t->s = kdgu_news(lex_escapes(t));
		break;
	case TOK_INT:
		t->i = atoi(t->body);
		break;
	default:;
	}

	/*
	 * Update the lexer stream position by moving it to the end of
	 * the current token.
	 */

	l->loc.idx = b - l->s;

	return t;
}

void
print_token(FILE *f, struct token *t)
{
	static char *str[] = {
		"INTEGER",
		"STRING",
		"IDENTIFIER",
		"EOF"
	};

	if (t->type <= TOK_EOF) fprintf(f, "%10s", str[t->type]);
	else fprintf(f, "%10c", t->type);

	fprintf(f, " | %3u | %3u:%2u | %s\n", t->loc.idx,
	        t->loc.line + 1, t->loc.column, t->body);
}
