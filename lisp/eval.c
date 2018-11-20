#include <string.h>
#include <assert.h>

#include "error.h"
#include "eval.h"
#include "lisp.h"
#include "util.h"

/*
 * Evaluates each element in `list` and returns the result of the last
 * evaluation. If evaluating an element results in an error,
 * evaluation is stopped and the error is returned.
 */

struct value *
progn(struct value *env, struct value *list)
{
	struct value *r = NULL;

	for (struct value *lp = list;
	     lp->type != VAL_NIL;
	     lp = lp->cdr) {
		r = eval(env, lp->car);
		if (r->type == VAL_ERROR) return r;
	}

	return r;
}

/*
 * Executes the body of the function or builtin `fn`. Returns an error
 * if `fn` is not a function or builtin.
 */

static struct value *
apply(struct value *env,
      struct location loc,
      struct value *fn,
      struct value *args)
{
	if (fn->type == VAL_BUILTIN)
		return fn->prim(env, args);

	/* If it's not a builtin it must be a function. */

	if (fn->type != VAL_FUNCTION) {
		struct value *e =
			error(loc, "function application requires " \
			      "a function value (this is %s %s)",
			      IS_VOWEL(*TYPE_NAME(fn->type))
			      ? "an" : "a",
			      TYPE_NAME(fn->type));
		e->cdr = error(fn->loc, "last defined here");
		e->cdr->type = VAL_NOTE;
		return e;
	}

	if (!IS_LIST(args))
		return error(args->loc,
		             "function application requires " \
		             "a list of arguments (this is %s %s)",
		             IS_VOWEL(*TYPE_NAME(args->car->type))
		             ? "an" : "a",
		             TYPE_NAME(args->car->type));

	return progn(push_env(fn->env,
	                      fn->param,
	                      eval_list(env, args)),
	             fn->body);
}

/*
 * Evaluates every element of `list` in order and returns a new list
 * containing the resulting values. If evaluating an element results
 * in an error, evaluation is stopped and the error is returned.
 */

struct value *
eval_list(struct value *env, struct value *list)
{
	struct value *head = NULL, *tail = NULL;

	for (struct value *l = list;
	     l->type != VAL_NIL;
	     l = l->cdr) {
		struct value *tmp = eval(env, l->car);
		if (tmp->type == VAL_ERROR) return tmp;
		if (!head) {
			head = tail = cons(tmp, Nil(list->loc));
			continue;
		}
		tail->cdr = cons(tmp, Nil(list->loc));
		tail = tail->cdr;
	}

	return head;
}

/*
 * Evaluates a node and returns the result.
 */

struct value *
eval(struct value *env, struct value *v)
{
	switch (v->type) {
	/*
	 * These are values that don't require any further
	 * interpretation.
	 */

	case VAL_INT:     case VAL_STRING:
	case VAL_BUILTIN: case VAL_FUNCTION:
	case VAL_ERROR:
	case VAL_TRUE:    case VAL_NIL:
		return v;

	/*
	 * Since this node is not being evaluated as a list, it must
	 * be a function or builtin call.
	 */

	case VAL_CELL: {
		struct value *expanded = expand(env, v);
		if (expanded != v) return eval(env, expanded);

		struct value *fn = eval(env, v->car);
		struct value *args = v->cdr;
		if (fn->type == VAL_ERROR) return fn;
		return apply(env, v->loc, fn, args);
	}

	/*
	 * Evaluating a bare symbol means we should look up what it
	 * means and return that.
	 */

	case VAL_SYMBOL: {
		struct value *bind = find(env, v);
		if (bind) return bind->cdr->loc = bind->car->loc, bind->cdr;
		return error(v->loc, "undeclared identifier");
	}

	/* This should never happen. */

	default:
		return error(v->loc, "bug: unimplemented evaluator");
	}

	return error(v->loc, "bug: unreachable");
}
