#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "parse.h"
#include "lex.h"
#include "eval.h"

static struct value *
parse_expr(struct value *env, struct lexer *l)
{
	struct token *t = tok(l, CODE);
	if (!t) return NULL;

	/*
	 * The cast to int is here to suppress the `case value 'x' not
	 * in enumerated type` warning GCC emits.
	 */

	switch ((int)t->type) {
	case '(': return parse(env, l);
	case ')': return RParen;
	case '.': return Dot;
	case '\'': return quote(parse_expr(env, l));
	case '[': {
		struct value *v = new_value(&t->loc);
		v->type = VAL_ARRAY;
		struct value *arr[512];
		unsigned num = 0;

		while ((t = tok(l, CODE)) && t->type != ']') {
			l->loc = t->loc;
			arr[num++] = parse_expr(env, l);
		}

		v->arr = malloc(num * sizeof *v->arr);
		memcpy(v->arr, arr, num * sizeof *v->arr);
		v->num = num;
		return v;
	} break;

	case TOK_IDENT:
		return make_symbol(copy_location(t->loc), t->body);

	case TOK_STR: {
		struct value *v = new_value(&t->loc);
		v->type = VAL_STRING;
		v->s = kdgu_copy(t->s);
		return v;
	} break;

	case TOK_INT: {
		struct value *v = new_value(&t->loc);
		v->type = VAL_INT;
		v->i = t->i;
		return v;
	} break;

	default:
		return error(&t->loc, "unexpected `%s'", t->body);
	}

	return NULL;
}

struct value *
parse(struct value *env, struct lexer *l)
{
	struct value *head = Nil, *tail = head;

	for (;;) {
		struct location *loc = copy_location(l->loc);
		struct value *o = parse_expr(env, l);
		if (!o) return error(loc, "unmatched `('");
		free(loc);
		if (o == RParen) return head;

		if (o == Dot) {
			tail->cdr = parse_expr(env, l);
			if (parse_expr(env, l) != RParen)
				return error(copy_location(l->loc),
				             "expected `)'");
			return head;
		}

		if (head->type == VAL_NIL) {
			head = cons(o, Nil);
			tail = head;
			continue;
		}

		tail->cdr = cons(o, Nil);
		tail = tail->cdr;
	}
}
