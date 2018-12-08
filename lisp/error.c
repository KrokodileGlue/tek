#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "error.h"
#include "lisp.h"

struct value *
error(struct location *loc, const char *fmt, ...)
{
	struct value *v = new_value(loc);
	v->type = VAL_ERROR;
	v->loc = loc;

	int l = strlen(fmt) + 1;
	v->errmsg = malloc(l);

	va_list args;
	va_start(args, fmt);
	int len = vsnprintf(v->errmsg, l, fmt, args) + 1;

	if (len > l) {
		v->errmsg = realloc(v->errmsg, len);
		va_start(args, fmt);
		vsnprintf(v->errmsg, len, fmt, args);
	}

	va_end(args);

	return v;
}

void
print_error(FILE *f, struct value *e)
{
	fprintf(f, "%s: %s:%u:%u: %s\n\t",
	        (char *[]){"error","note"}[e->type - VAL_ERROR],
	        e->loc->file,
	        e->loc->line + 1, e->loc->column, e->errmsg);

	unsigned i = e->loc->idx;
	const char *a = e->loc->text;

	while (i && a[i] != '\n') i--;
	if (a[i] == '\n') i++;
	unsigned j = i;
	while (a[i] && a[i] != '\n') fputc(a[i], f), i++;

	fprintf(f, "\n\t");

	for (unsigned i = 0; i < e->loc->column; i++)
		fputc(isspace(a[j + i]) ? a[j + i] : ' ', f);

	fputc('^', f);

	for (unsigned i = 1; i < e->loc->len; i++)
		fputc('~', f);

	fputc('\n', f);

	if (e->cdr) print_error(f, e->cdr);
}
