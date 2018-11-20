#ifndef UTIL_H
#define UTIL_H

#define IS_VOWEL(X) (X == 'A' || X == 'E' || X == 'I' || X == 'O'	\
                     || X == 'U' || X == 'Y' || X == 'a' || X == 'e'	\
                     || X == 'i' || X == 'o' || X == 'u' || X == 'y')

char *load_file(const char *p);

#endif
