
#ifndef lispel_h
#define lispel_h

struct lispel_environment;

void lispel_deinit(struct lispel_environment* env);
struct lispel_environment* lispel_init();
void lispel_do(char* script, struct lispel_environment* env);
float lispel_get_var(struct lispel_environment* env, char* var_name);
float lispel_get_array_at(struct lispel_environment* env, char* var_name, int index);

#endif