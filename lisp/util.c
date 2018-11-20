#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "util.h"

char *
load_file(const char *p)
{
	char *b = NULL;
	FILE *f = NULL;
	int len = -1;

	f = fopen(p, "rb");
	if (!f) return NULL;
	if (fseek(f, 0L, SEEK_END)) return fclose(f), NULL;
	len = ftell(f);
	if (len == -1) return fclose(f), NULL;
	b = malloc(len + 10);
	if (!b) return fclose(f), NULL;
	if (fseek(f, 0L, SEEK_SET)) return fclose(f), free(b), NULL;
	len = fread(b, 1, len, f);
	if (ferror(f)) return fclose(f), free(b), NULL;
	fclose(f);
	memset(b + len, 0, 10);

	return b;
}
