
#ifndef lispel_h
#define lispel_h

struct lispel_variable;
struct lispel_token;
struct lispel_expression;
struct lispel_chunk;
struct lispel_block;
struct lispel_operator;
struct lispel_op_list;
struct lispel_environment;
struct lispel_state;
struct lispel_operator;
enum lispel_arg_type;

void gen_chunks(struct lispel_environment* env, char* line);
void gen_tokens(char* code, struct lispel_environment* env, struct lispel_state* st);
void gen_expressions(struct lispel_state* st);
void do_expression(struct lispel_expression* expr, struct lispel_environment* env, char* end_result);
void gen_blocks(char* data,char* rest, struct lispel_environment* env);

void add_to_op_list(struct lispel_op_list* op_list, struct lispel_op_list* add);
void init_std_ops(struct lispel_op_list* op_list);

void lispel_deinit(struct lispel_environment* env);
struct lispel_environment* lispel_init();
void lispel_do(char* script, struct lispel_environment* env);
float lispel_get_var(struct lispel_environment* env, char* var_name);

#endif