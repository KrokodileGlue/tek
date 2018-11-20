#ifndef EVAL_H
#define EVAL_H

struct value *progn(struct value *env, struct value *list);
struct value *eval_list(struct value *env, struct value *list);
struct value *eval(struct value *env, struct value *v);

#endif
