#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "builtin.h"
#include "error.h"
#include "lisp.h"
#include "eval.h"

const char **value_name = (const char *[]){
	"int",
	"cell",
	"string",
	"symbol",
	"builtin",
	"function",
	"macro",
	"env",
	"moved",
	"true",
	"nil",
	"rparen",
	"dot",
	"error",
};

struct value *Dot = &(struct value){VAL_DOT,{0},{0}};
struct value *RParen = &(struct value){VAL_RPAREN,{0},{0}};

struct value *
Nil(struct location loc)
{
	struct value *v = new_value(loc);
	v->type = VAL_NIL;
	return v;
}

struct value *
True(struct location loc)
{
	struct value *v = new_value(loc);
	v->type = VAL_TRUE;
	return v;
}

struct value *
new_value(struct location loc)
{
	struct value *v = malloc(sizeof *v);
	memset(v, 0, sizeof *v);
	v->loc = loc;
	return v;
}

struct value *
quote(struct value *v)
{
	return cons(make_symbol(v->loc,"quote"),cons(v,Nil(v->loc)));
}

struct value *
list_length(struct value *list)
{
	int len = 0;

	while (list->type != VAL_NIL) {
		if (list->type == VAL_CELL) {
			list = list->cdr;
			len++;
			continue;
		}

		return error(list->loc,
		             "a non-dotted list was expected here");
	}

	struct value *r = new_value(list->loc);

	r->type = VAL_INT;
	r->i = len;

	return r;
}

struct value *
print_value(FILE *f, struct value *v)
{
	switch (v->type) {
	case VAL_SYMBOL:
	case VAL_STRING: fprintf(f, "%s", v->s); break;
	case VAL_INT:    fprintf(f, "%d", v->i); break;
	case VAL_TRUE:   fprintf(f, "true");     break;
	case VAL_NIL:    fprintf(f, "nil");      break;
	case VAL_FUNCTION:
		fprintf(f, "<function:%p>", v);
		break;
	case VAL_CELL:
		fputc('(', f);

		while (v->type == VAL_CELL) {
			print_value(f, v->car);
			if (v->cdr->type == VAL_CELL) fputc(' ', f);
			v = v->cdr;
		}

		if (v->type != VAL_NIL)
			fputc(' ', f), print_value(f, v);

		fputc(')', f);
		break;
	default:
		return error(v->loc,
		             "bug: unimplemented printer for"
		             " expression of type `%s'",
		             TYPE_NAME(v->type));
	}

	return Nil(v->loc);
}

struct value *
cons(struct value *car, struct value *cdr)
{
	struct value *v = new_value(car->loc);
	v->type = VAL_CELL;
	v->car = car;
	v->cdr = cdr;
	return v;
}

struct value *
acons(struct value *x, struct value *y, struct value *a)
{
	return cons(cons(x, y), a);
}

struct value *
make_symbol(struct location loc, const char *s)
{
	if (!strcmp(s, "nil")) return Nil(loc);
	if (!strcmp(s, "t")) return True(loc);

	struct value *sym = new_value(loc);
	sym->type = VAL_SYMBOL;
	sym->s = malloc(strlen(s) + 1);
	strcpy(sym->s, s);
	return sym;
}

struct value *
expand(struct value *env, struct value *v)
{
	if (v->type != VAL_CELL || v->car->type != VAL_SYMBOL)
		return v;
	struct value *bind = find(env, v->car);
	if (!bind || bind->cdr->type != VAL_MACRO) return v;
	return progn(push_env(env, bind->cdr->param, v->cdr),
	             bind->cdr->body);
}

/*
 * Looks up `sym` in `env`, moving up into higher lexical scopes as
 * necessary. `sym` is assumed to be a `VAL_SYMBOL`. returns NULL if
 * the symbol could not be resolved.
 */

struct value *
find(struct value *env, struct value *sym)
{
	assert(sym->type == VAL_SYMBOL);

	/*
	 * We've walked up through every scope and haven't found the
	 * symbol. It must not exist.
	 */

	if (!env) return NULL;

	for (struct value *c = env->vars;
	     c->type != VAL_NIL;
	     c = c->cdr) {
		struct value *bind = c->car;
		if (!strcmp(sym->s, bind->car->s)) return bind;
	}

	return find(env->up, sym);
}

struct value *
add_variable(struct value *env, struct value *sym, struct value *body)
{
	env->vars = acons(sym, body, env->vars);
	return body;
}

void
add_builtin(struct value *env, const char *name, builtin *f)
{
	struct value *sym = make_symbol(BUILTIN, name);
	struct value *prim = malloc(sizeof *prim);
	prim->type = VAL_BUILTIN;
	prim->prim = f;
	add_variable(env, sym, prim);
}

struct value *
new_environment(void)
{
	struct value *env = new_value(NOWHERE);
	env->vars = Nil(NOWHERE);
	load_builtins(env);
	return env;
}

static struct value *
make_env(struct value *vars, struct value *up)
{
	struct value *r = malloc(sizeof *r);
	r->vars = vars;
	r->up = up;
	return r;
}

struct value *
push_env(struct value *env, struct value *vars, struct value *values)
{
	/* if (list_length(vars) != list_length(values)) */
	/* 	error("Cannot apply function: number of argument does not match"); */

	struct value *map = Nil(vars->loc);
	struct value *p = vars, *q = values;

	for (;
	     p->type == VAL_CELL;
	     p = p->cdr, q = q->cdr)
		map = acons(p->car, q->car, map);

	if (p->type != VAL_NIL)
		map = acons(p, q, map);

	return make_env(map, env);
}

#define p(...) printf(__VA_ARGS__)

void
print_tree(FILE *f, struct value *v)
{
	static int depth = 0;
	static int l = 0, arm[2048] = {0};
	depth++, (depth != 1 && p("\n")), arm[l] = 0;

	for (int i = 0; i < l - 1; i++)
		arm[i] ? p("│   ") : p("    ");
	if (l) arm[l - 1] ? p("├───") : p("╰───");

	if (!v) {
		p("(null)");
		return;
	}

	switch (v->type) {
	case VAL_BUILTIN:
		p("(builtin:%p)", v->prim);
		break;
	case VAL_SYMBOL:
		p("(symbol:%s)", v->s);
		break;
	case VAL_CELL:
		p("(cell)");
		l++;
		arm[l - 1] = 1;
		print_tree(f, v->car);
		arm[l - 1] = 0;
		print_tree(f, v->cdr);
		l--;
		break;
	case VAL_STRING:
		p("\"%s\"", v->s);
		break;
	case VAL_INT:
		p("(int:%d)", v->i);
		break;
	case VAL_NIL:
		p("(nil)");
		break;
	case VAL_TRUE:
		p("(true)");
		break;
	default: exit(1);
	}

	depth--;
}
