#ifndef LISP_H
#define LISP_H

#include <stdio.h>
#include <kdg/kdgu.h>
#include "location.h"
#include "lex.h"

typedef struct value *builtin(struct value *, struct value *);

struct value {
	enum {
		VAL_INT,
		VAL_CELL,
		VAL_STRING,
		VAL_SYMBOL,
		VAL_BUILTIN,
		VAL_FUNCTION,
		VAL_MACRO,
		VAL_ENV,
		VAL_ARRAY,

		/* GC marker. */
		VAL_MOVED,

		/* Non-GC values. */
		VAL_TRUE,
		VAL_NIL,

		/* Dummies only used by the parser. */
		VAL_RPAREN,
		VAL_DOT,

		/* Hot potatoes. */
		VAL_ERROR,
		VAL_NOTE,
	} type;

	union {
		int i;
		kdgu *s;
		char *errmsg;

		/* Cell. */
		struct {
			struct value *car, *cdr;
		};

		/* Function. */
		struct {
			struct value *param;
			struct value *body;
			struct value *env;
		};

		/* Environment. */
		struct {
			struct value *vars;
			struct value *up;
		};

		/* Array. */
		struct {
			struct value **arr;
			unsigned num;
		};

		/* Builtin. */
		builtin *prim;
	};

	struct location *loc;
};

struct value *Dot, *RParen, *Nil, *True;
struct value *list_length(struct value *list);

struct value *quote(struct value *v);
struct value *add_variable(struct value *env,
                           struct value *sym,
                           struct value *body);
void add_builtin(struct value *env, const char *name, builtin *f);
struct value *find(struct value *env, struct value *sym);

struct value *new_environment(void);
struct value *push_env(struct value *env,
                       struct value *vars,
                       struct value *values);
struct value *new_value(struct location *loc);

struct value *cons(struct value *car, struct value *cdr);
struct value *acons(struct value *x, struct value *y, struct value *a);
struct value *make_symbol(struct location *loc, const char *s);
struct value *expand(struct value *env, struct value *v);

struct value *print_value(FILE *f, struct value *v);
void print_tree(FILE *f, struct value *v);

const char **value_name;

#define TYPE_NAME(X) (X > VAL_ERROR ? (char []){X, 0} : value_name[X])
#define IS_LIST(X) ((X)->type == VAL_NIL || (X)->type == VAL_CELL)

#endif
