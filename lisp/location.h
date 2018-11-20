#ifndef LOCATION_H
#define LOCATION_H

#define NOWHERE (struct location){-1,0,0,0,"*none*","<none>"}
#define BUILTIN (struct location){0,0,0,0,"built-in-function","<internal implementation>"}

struct location {
	unsigned line, column; /* Line/column of first character.   */
	unsigned len;          /* Length of the region.             */
	unsigned idx;          /* Location in the token stream.     */
	const char *file;      /* Name of the file this is from.    */
	const char *text;      /* Complete body of the file.        */
};

#endif
