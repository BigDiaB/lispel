
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "debug.h"
#include "lispel.h"

enum lispel_type {
    none,number,variable,table,operator,ob,cb,expression
};

struct lispel_variable
{
    char name[1024];
    char* value;
    char** table_values;
    int table_length;
    enum lispel_type type;
};

struct lispel_token {
    enum lispel_type type;
    char data[1024];
    int id;
};

struct lispel_expression {
    int num_tokens;
    struct lispel_token* tokens;
};

struct lispel_chunk {
    char code[1024];
};

struct lispel_block
{
    char name[1024];
    char code[1024];
};

struct lispel_operator;

struct lispel_op_list {
    int used;
    int max;
    struct lispel_operator* operators;
};

struct lispel_environment
{
    struct lispel_op_list op_list;
    int blocks_used,variables_used,chunks_used,current_chunk;
    struct lispel_chunk chunks[1024];
    struct lispel_variable variables[32];
    struct lispel_block blocks[10];
};

struct lispel_state
{
    struct lispel_token tokens[100];
    int tokens_used;
    struct lispel_expression expressions[100];
    int expressions_used;
};

struct lispel_operator {
    char* name;
    int num_args;
    enum lispel_type* arg_types;
    void (*func)(char*, char**, int*,struct lispel_environment*);
};


void gen_chunks(struct lispel_environment* env, char* line)
{
    char* copy = malloc(strlen(line) + 1);
    char* curr = copy;
    memset(copy,0,strlen(line) + 1);
    strcpy(copy,line);

    while(*curr)
    {
        if (*curr == '(')
        {
            struct lispel_chunk chunk = {0};
            curr++;
            int depth = 1, i = 0;
            while(depth != 0 && *curr)
            {
                chunk.code[i] = *curr;
                if (*curr == '(')
                    depth++;
                if (*curr == ')')
                    depth--;
                curr++;
                i++;
            }
            
            env->chunks[env->chunks_used] = chunk;
            env->chunks_used++;
        }
        curr++;
    }
    free(copy);
}

void gen_tokens(char* code, struct lispel_environment* env, struct lispel_state* st)
{
    int depth = 0;
    int id = 0;

    while(*code)
    {
        struct lispel_token tok;
        if (*code == '#')
        {
            code++;
            while(*(code) != '#' && *code)
                code++;
        }
        else if (*code == '(')
        {
            tok.type = ob;
            tok.data[0] = depth;

            if (depth > 126)
            {
                {
                    printf("%s:%d: Error!:\n\tMaximale Tiefe von 127 überschritten!",__FUNCTION__,__LINE__);
                    exit(EXIT_FAILURE);
                }
            }
            depth++;
            tok.id = id;
            id++;
            st->tokens[(st->tokens_used)] = tok;
            (st->tokens_used)++;
            {
            }
        }
        else if (*code == ')')
        {
            depth--;
            tok.data[0] = depth;
            tok.type = cb;

            if (depth < 0)
            {
                printf("%s",code);
                {
                    printf("%s:%d: Error!:\n\tMinimale Tiefe von 0 unterschritten!",__FUNCTION__,__LINE__);
                    exit(EXIT_FAILURE);
                }
            }


            tok.id = id;
            id++;
            st->tokens[(st->tokens_used)] = tok;
            (st->tokens_used)++;
            {
            }
        }
        else if (*code < 58 && *code > 44 && *code != 47 && !(*code == '-' && (*(code + 1) == ' ' || *(code + 1) == '(' || *(code + 1) == ')')))
        {
            char num_test[1024] = {0};
            int i = 0;
            while(*code != ' ' && *code != ')' && *code != '(')
            {
                num_test[i] = *code;
                i++;
                code++;
            }
            code--;

            bool is_number = false;
            char* data = code;
            while(*data)
            {
                for (i = 45; i < 58; i++)
                {
                    if (i == 47)
                        i++;
                    if (*data == i)
                        is_number = true;
                    if (*data == ' ')
                        break;
                }
                if (!is_number)
                    break;
                data++;
            }

            if (is_number)
            {
                tok.type = number;
                strcpy(tok.data,num_test); 

                tok.id = id;
                id++;
                st->tokens[(st->tokens_used)] = tok;
                (st->tokens_used)++;
            }
        }
        else if (*code != ' ' && *code != '\n')
        {
            char op[1024] = {0};
            int i = 0;

            while(*code != ' ' && *code != ')' && *code != '(')
            {
                op[i] = *code;
                i++;
                code++;
            }
            code--;

            strcpy(tok.data,op); 
            bool found = false;

            for (i = 0; i < env->op_list.used; i++)
            {
                if ((0 == strcmp(tok.data,env->op_list.operators[i].name)))
                    found = true;
            }


            if (found)
            {
                tok.type = operator;
                tok.id = id;
                id++;
                st->tokens[(st->tokens_used)] = tok;
                (st->tokens_used)++;
            }
            else
            {
                bool var_found = false;
                int j;
                for (j = 0; j < env->variables_used; j++)
                {
                    if ((0 == strcmp(op,env->variables[j].name)))
                    {
                        var_found = true;
                        tok.type = variable;
                        tok.id = id;
                        id++;
                        st->tokens[(st->tokens_used)] = tok;
                        (st->tokens_used)++;
                        break;
                    }
                }
                if (!var_found)
                {
                    tok.type = variable;
                    tok.id = id;
                    id++;
                    st->tokens[(st->tokens_used)] = tok;
                    (st->tokens_used)++;
                }
            }
        }
        else if (*code == '#')
        {
            while(*code != '\n')
                code++;
        }
        code++;
        
    }
}

void gen_expressions(struct lispel_state* st)
{
    int curr_token = 0;
    while (curr_token < st->tokens_used)
    {
        int depth = 0,fti = curr_token;
        while(depth != -1 && curr_token < st->tokens_used)
        {
            curr_token++;
            if (st->tokens[curr_token].type == ob)
                depth++;
            else if (st->tokens[curr_token].type == cb)
                depth--;
        }
        curr_token++;

        struct lispel_expression expr;
        expr.num_tokens = curr_token - fti;
        expr.tokens = malloc(sizeof(struct lispel_token) * expr.num_tokens);
        int i;
        for (i = 0; i < expr.num_tokens; i++)
            expr.tokens[i] = st->tokens[i + fti];

        st->expressions[st->expressions_used] = expr;
        (st->expressions_used)++;
    }
    curr_token++;
}

void do_expression(struct lispel_expression* expr, struct lispel_environment* env, char* end_result)
{
    int num_tokens = expr->num_tokens;
    struct lispel_token tokens[num_tokens];
    int i,j,ob_num = 0;
    for (i = 0; i < num_tokens; i++)
        tokens[i] = expr->tokens[i];

    for (i = 0; i < num_tokens; i++)
    {

        if (tokens[i].type == ob)
        {
            ob_num++;
            static bool first = true;
            int highest_index = -1,highest = -1;
            for (j = 0; j < num_tokens; j++)
            {
                if (tokens[j].type == ob  && tokens[j].data[0] > highest)
                {
                    highest = tokens[j].data[0];
                    highest_index = j;
                    if (highest_index > 0 && first)
                        first = false;
                }
            }
            if (highest == -1)
                break;
            tokens[highest_index].data[0] = -1;
            int op_index = -1;
            for (j = 0; j < env->op_list.used; j++)
                if ((0 == strcmp(tokens[highest_index + 1].data,env->op_list.operators[j].name)))
                    op_index = j;

            if (op_index == -1)
                {
                    printf("%s:%d: Error!:\n\tOperator nicht gefunden: %s",__FUNCTION__,__LINE__,tokens[highest_index + 1].data);
                    exit(EXIT_FAILURE);
                }

            char** args = malloc(sizeof(char*) * (env->op_list.operators[op_index].num_args + 1));

            for (j = 0; j < env->op_list.operators[op_index].num_args; j++)
            {
                args[j] = malloc(env->op_list.operators[op_index].num_args * 1024);
            }

            int* types = malloc(sizeof(int) * env->op_list.operators[op_index].num_args);

            struct lispel_token result = {0};

            for (j = 0; j < env->op_list.operators[op_index].num_args; j++)
            {
                if (tokens[highest_index + j + 2].type == cb)
                    {
                        printf("%s:%d: Error!:\n\tZu wenige Argumente für %s-Operator!",__FUNCTION__,__LINE__,env->op_list.operators[op_index].name);
                        exit(EXIT_FAILURE);
                    }

                if (tokens[highest_index + j + 2].type != variable && tokens[highest_index + j + 2].type != env->op_list.operators[op_index].arg_types[j])
                    {
                        printf("%s:%d: Error!:\n\tFalscher Datentyp für %s-Operator Argument Nummer %d: %d != %d",__FUNCTION__,__LINE__,env->op_list.operators[op_index].name,j,tokens[highest_index + j + 2].type,env->op_list.operators[op_index].arg_types[j]);
                        exit(EXIT_FAILURE);
                    }

                strcpy(args[j],tokens[highest_index + j + 2].data);
                types[j] = tokens[highest_index + j + 2].type;
            }

            if (tokens[highest_index + j + 2].type != cb)
                {
                    printf("%s:%d: Error!:\n\tZu viele Argumente für %s-Operator!",__FUNCTION__,__LINE__,env->op_list.operators[op_index].name);
                    exit(EXIT_FAILURE);
                }

            args[env->op_list.operators[op_index].num_args] = (void*)expr;

            env->op_list.operators[op_index].func(result.data,args,types,env);

            int k = 0;
            for (k = 0; k < env->op_list.operators[op_index].num_args; k++)
            {
                free(args[k]);
            }
            free(args);
            free(types);


            if (highest == 0)
                strcpy(end_result,result.data);


            bool is_number = false;
            char* data = result.data;
            while(*data)
            {
                for (i = 45; i < 58; i++)
                {
                    if (i == 47)
                        i++;
                    if (*data == i)
                        is_number = true;
                    if (*data == ' ')
                        break;
                }
                if (!is_number)
                    break;
                data++;
            }

            if (is_number)
                result.type = number;
            else
                result.type = variable;

            tokens[highest_index] = result;
            int g,h;
            for (h = 0; h < 4; h++)
            {
                for (g = highest_index + 1; g < num_tokens - 1; g++)
                    tokens[g] = tokens[g + 1];
                num_tokens--;
            }


            i = -1;
        }
    }
}

void gen_blocks(char* data,char* rest, struct lispel_environment* env)
{
    char* code = malloc(strlen(data) + 1);
    strcpy(code,data);
    char* first_rm, *last_rm, *initial_code = code;
    char buffer[strlen(initial_code)];
    memset(buffer,0,strlen(initial_code));
    while (*code)
    {
        struct lispel_block block;
        memset(block.name,0,1024);
        memset(block.code,0,1024);

        if (*code == '#')
        {
            code++;
            while(*(code) != '#' && *code)
                code++;
        }
        else if (*code == '{')
        {
            first_rm = code;

            while(*code != ':')
                code++;
            code++;
            int i = 0,j;
            while(*code != ':')
            {
                block.name[i] = *code;
                code++;
                i++;
            }
            while(*code != '(')
                code++;
            while(*(code + i) != '}')
                i++;
            for (j = 0; j < i; j++)
            {
                block.code[j] = *code;
                code++;
            }
            last_rm = code;

            strcpy(env->blocks[env->blocks_used].name,block.name);
            strcpy(env->blocks[env->blocks_used].code,block.code);
            env->blocks_used++;

            first_rm[0] = 0;
            last_rm[0] = 0;

            strcpy(buffer,initial_code);
            strcat(buffer,last_rm + 1);
            strcpy(rest,buffer);
        }
        code++;
    }
    free(initial_code);
}


void lispel_do_block(char* result, char** args, int* __attribute__((unused))types, struct lispel_environment* env)
{
    int i,num = -1;
    for (i = 0; i < env->blocks_used; i++)
        if ((0 == strcmp(args[0],env->blocks[i].name)))
            num = i;
    if (num == -1)
        {
            printf("%s:%d: Error!:\n\tBlock nicht gefunden: %s",__FUNCTION__,__LINE__,args[0]);
            exit(EXIT_FAILURE);
        }

    strcpy(result,"0");

    struct lispel_state st = {0};

    char script[strlen(env->blocks[num].code) + 1];
    strcpy(script,env->blocks[num].code);

    gen_blocks(script,script,env);
    gen_chunks(env,script);

    gen_tokens(script,env,&st);

    gen_expressions(&st);

    for (i = 0; i < st.expressions_used; i++)
    {
        do_expression(&st.expressions[i],env,result);
        free(st.expressions[i].tokens);
    }
}

void lispel_if_do_block(char* result, char** args, int* types, struct lispel_environment* env)
{
    int i,num = -1;
    for (i = 0; i < env->blocks_used; i++)
        if ((0 == strcmp(args[1],env->blocks[i].name)))
            num = i;
    if (num == -1)
        {
            printf("%s:%d: Error!:\n\tBlock nicht gefunden: %s",__FUNCTION__,__LINE__,args[1]);
            exit(EXIT_FAILURE);
        }

    if (types[0] == variable)
    {
        int num = -1,g;
        for (g = env->variables_used; g >= 0 ; g--)
            if ((0 == strcmp(args[0],env->variables[g].name)))
                num = g;
        if (num == -1)
            {
                printf("%s:%d: Error!:\n\tVariable nicht gefunden: %s",__FUNCTION__,__LINE__,args[0]);
                exit(EXIT_FAILURE);
            }
        strcpy(args[0],env->variables[num].value);
    }

    strcpy(result,args[0]);

    if (!(0 == strcmp(args[0],"0")))
    {
        struct lispel_state st = {0};
        int i;

        char script[strlen(env->blocks[num].code) + 1];
        strcpy(script,env->blocks[num].code);

        gen_blocks(script,script,env);
        gen_chunks(env,script);

        gen_tokens(script,env,&st);

        gen_expressions(&st);

        for (i = 0; i < st.expressions_used; i++)
        {
            do_expression(&st.expressions[i],env,result);
            free(st.expressions[i].tokens);
        }
    }
}

void lispel_negate(char* result, char** args, int* types,struct lispel_environment* env)
{
    int g;
    if (types[0] == variable)
    {
        int num = -1;
        for (g = env->variables_used; g >= 0 ; g--)
            if ((0 == strcmp(args[0],env->variables[g].name)))
                num = g;
        if (num == -1)
            {
                printf("%s:%d: Error!:\n\tVariable nicht gefunden: %s",__FUNCTION__,__LINE__,args[0]);
                exit(EXIT_FAILURE);
            }
        strcpy(args[0],env->variables[num].value);
    }

    strcpy(result,(0 == strcmp("0",args[0]) ? "1" : "0"));
}

void lispel_bigger(char* result, char** args, int* types,struct lispel_environment* env)
{
    int g;
    if (types[0] == variable)
    {
        int num = -1;
        for (g = env->variables_used; g >= 0 ; g--)
            if ((0 == strcmp(args[0],env->variables[g].name)))
                num = g;
        if (num == -1)
            {
                printf("%s:%d: Error!:\n\tVariable nicht gefunden: %s",__FUNCTION__,__LINE__,args[0]);
                exit(EXIT_FAILURE);
            }
        strcpy(args[0],env->variables[num].value);
    }
    if (types[1] == variable)
    {
        int num = -1;
        for (g = env->variables_used; g >= 0 ; g--)
            if ((0 == strcmp(args[1],env->variables[g].name)))
                num = g;
        if (num == -1)
            {
                printf("%s:%d: Error!:\n\tVariable nicht gefunden: %s",__FUNCTION__,__LINE__,args[1]);
                exit(EXIT_FAILURE);
            }
        strcpy(args[1],env->variables[num].value);
    }

    void* num_1 = NULL;
    void* num_2 = NULL;

    char temp_result[2048] = {0};

    bool is_float[2] = {0};

    if (strstr(args[0],"."))
    {
        is_float[0] = true;
        num_1 = malloc(sizeof(float));
        sscanf(args[0],"%f",(float*)num_1);
    }
    else
    {
        num_1 = malloc(sizeof(int));
        sscanf(args[0],"%d",(int*)num_1);
    }

    if (strstr(args[1],"."))
    {
        is_float[1] = true;
        num_2 = malloc(sizeof(float));
        sscanf(args[1],"%f",(float*)num_2);
    }
    else
    {
        num_2 = malloc(sizeof(int));
        sscanf(args[1],"%d",(int*)num_2);
    }


    if (is_float[0] && is_float[1])
    {
        bool res = *((float*)num_1) > *((float*)num_2);
        sprintf(temp_result,"%d",res);
    }
    else if (!is_float[0] && is_float[1])
    {
        bool res = *((int*)num_1) > *((float*)num_2);
        sprintf(temp_result,"%d",res);
    }
    else if (is_float[0] && !is_float[1])
    {
        bool res = *((float*)num_1) > *((int*)num_2);
        sprintf(temp_result,"%d",res);
    }
    else if (!is_float[0] && !is_float[1])
    {
        bool res = *((int*)num_1) > *((int*)num_2);
        sprintf(temp_result,"%d",res);
    }

    free(num_1);
    free(num_2);

    strcpy(result,temp_result);
}

void lispel_smaller(char* result, char** args, int* types,struct lispel_environment* env)
{
    int g;
    if (types[0] == variable)
    {
        int num = -1;
        for (g = env->variables_used; g >= 0 ; g--)
            if ((0 == strcmp(args[0],env->variables[g].name)))
                num = g;
        if (num == -1)
            {
                printf("%s:%d: Error!:\n\tVariable nicht gefunden: %s",__FUNCTION__,__LINE__,args[0]);
                exit(EXIT_FAILURE);
            }
        strcpy(args[0],env->variables[num].value);
    }
    if (types[1] == variable)
    {
        int num = -1;
        for (g = env->variables_used; g >= 0 ; g--)
            if ((0 == strcmp(args[1],env->variables[g].name)))
                num = g;
        if (num == -1)
            {
                printf("%s:%d: Error!:\n\tVariable nicht gefunden: %s",__FUNCTION__,__LINE__,args[1]);
                exit(EXIT_FAILURE);
            }
        strcpy(args[1],env->variables[num].value);
    }

    void* num_1 = NULL;
    void* num_2 = NULL;

    char temp_result[2048] = {0};

    bool is_float[2] = {0};

    if (strstr(args[0],"."))
    {
        is_float[0] = true;
        num_1 = malloc(sizeof(float));
        sscanf(args[0],"%f",(float*)num_1);
    }
    else
    {
        num_1 = malloc(sizeof(int));
        sscanf(args[0],"%d",(int*)num_1);
    }

    if (strstr(args[1],"."))
    {
        is_float[1] = true;
        num_2 = malloc(sizeof(float));
        sscanf(args[1],"%f",(float*)num_2);
    }
    else
    {
        num_2 = malloc(sizeof(int));
        sscanf(args[1],"%d",(int*)num_2);
    }


    if (is_float[0] && is_float[1])
    {
        bool res = *((float*)num_1) < *((float*)num_2);
        sprintf(temp_result,"%d",res);
    }
    else if (!is_float[0] && is_float[1])
    {
        bool res = *((int*)num_1) < *((float*)num_2);
        sprintf(temp_result,"%d",res);
    }
    else if (is_float[0] && !is_float[1])
    {
        bool res = *((float*)num_1) < *((int*)num_2);
        sprintf(temp_result,"%d",res);
    }
    else if (!is_float[0] && !is_float[1])
    {
        bool res = *((int*)num_1) < *((int*)num_2);
        sprintf(temp_result,"%d",res);
    }

    free(num_1);
    free(num_2);

    strcpy(result,temp_result);
}

void lispel_equality(char* result, char** args, int* types,struct lispel_environment* env)
{
    int g;
    if (types[0] == variable)
    {
        int num = -1;
        for (g = env->variables_used; g >= 0 ; g--)
            if ((0 == strcmp(args[0],env->variables[g].name)))
                num = g;
        if (num == -1)
            {
                printf("%s:%d: Error!:\n\tVariable nicht gefunden: %s",__FUNCTION__,__LINE__,args[0]);
                exit(EXIT_FAILURE);
            }
        strcpy(args[0],env->variables[num].value);
    }
    if (types[1] == variable)
    {
        int num = -1;
        for (g = env->variables_used; g >= 0 ; g--)
            if ((0 == strcmp(args[1],env->variables[g].name)))
                num = g;
        if (num == -1)
            {
                printf("%s:%d: Error!:\n\tVariable nicht gefunden: %s",__FUNCTION__,__LINE__,args[1]);
                exit(EXIT_FAILURE);
            }
        strcpy(args[1],env->variables[num].value);
    }

    strcpy(result,(0 == strcmp(args[0],args[1]) ? "1" : "0"));
}

void lispel_character(char* result, char** args, int* types,struct lispel_environment* env)
{
    int g;
    if (types[0] == variable)
    {
        int num = -1;
        for (g = env->variables_used; g >= 0 ; g--)
            if ((0 == strcmp(args[0],env->variables[g].name)))
                num = g;
        if (num == -1)
            {
                printf("%s:%d: Error!:\n\tVariable nicht gefunden: %s",__FUNCTION__,__LINE__,args[0]);
                exit(EXIT_FAILURE);
            }
        strcpy(args[0],env->variables[num].value);
    }
    sscanf(args[0],"%d",&g);
    char chara[2] = {g,0};

    strcpy(result,args[0]);
    printf("%s",chara);
}

void lispel_print(char* result, char** args, int* types,struct lispel_environment* env)
{
    strcpy(result,args[0]);
    if (types[0] == variable)
    {
        int g;
        int num = -1;
        for (g = env->variables_used; g >= 0 ; g--)
            if ((0 == strcmp(args[0],env->variables[g].name)))
                num = g;
        if (num == -1)
            {
                printf("%s:%d: Error!:\n\tVariable nicht gefunden: %s",__FUNCTION__,__LINE__,args[0]);
                exit(EXIT_FAILURE);
            }
        strcpy(args[0],env->variables[num].value);
    }
    printf("%s",args[0]);
}

void lispel_var_destroy(char* result, char** args, int* types,struct lispel_environment* env)
{

    int i;
    for (i = env->variables_used; i >= 0 ; i--)
        if ((0 == strcmp(args[0],env->variables[i].name)))
        {
            types[0] = env->variables[i].type;
            break;
        }

    if (types[0] == variable)
    {
        int i, num = -1;
        for (i = env->variables_used; i >= 0 ; i--)
            if ((0 == strcmp(args[0],env->variables[i].name)))
                num = i;
        if (num == -1)
        {
            printf("%s:%d: Error!:\n\tVariable nicht gefunden: %s",__FUNCTION__,__LINE__,args[0]);
            exit(EXIT_FAILURE);
        }

        free(env->variables[num].value);

        for (i = num; i < env->variables_used - 1; i++)
            env->variables[i] = env->variables[i + 1];
        env->variables_used--;
    }
    
    if (types[0] == table)
    {
        int num = -1,g;
        for (g = env->variables_used; g >= 0 ; g--)
            if ((0 == strcmp(args[0],env->variables[g].name)))
                num = g;
        if (num == -1)
        {
            printf("%s:%d: Error!:\n\tTable nicht gefunden: %s",__FUNCTION__,__LINE__,args[0]);
            exit(EXIT_FAILURE);
        }


        for (g = 0; g < env->variables[num].table_length; g++)
        {
            free(env->variables[num].table_values[g]);
        }

        free(env->variables[num].table_values);

        for (g = num; g < env->variables_used - 1; g++)
                env->variables[g] = env->variables[g + 1];
        env->variables_used--;
    }

    strcpy(result,"0");
}

void lispel_var_assign(char* result, char** args, int* types,struct lispel_environment* env)
{
    struct lispel_variable* var = NULL;
    strcpy(result,args[0]);
    int g;
    if (types[1] == variable)
    {
        int num = -1;
        for (g = env->variables_used; g >= 0 ; g--)
            if ((0 == strcmp(args[1],env->variables[g].name)))
                num = g;
        if (num == -1)
            {
                printf("%s:%d: Error!:\n\tVariable nicht gefunden: %s",__FUNCTION__,__LINE__,args[1]);
                exit(EXIT_FAILURE);
            }
        strcpy(args[1],env->variables[num].value);
    }

    for (g = env->variables_used; g >= 0 ; g--)
    {
        if ((0 == strcmp(args[0],env->variables[g].name)))
            var = &env->variables[g];
    }

    if (var == NULL)
            {
                printf("%s:%d: Error!:\n\tVariable nicht gefunden: %s",__FUNCTION__,__LINE__,args[0]);
                exit(EXIT_FAILURE);
            }

    strcpy(var->value,args[1]);
    strcpy(result,args[0]);
}

void lispel_var_number(char* result, char** args, int* types,struct lispel_environment* env)
{

    if (types[0] != variable)
        {
            printf("%s:%d: Error!:\n\tKein gültiger Variablen-Name: %s",__FUNCTION__,__LINE__,args[0]);
            exit(EXIT_FAILURE);
        }

    struct lispel_variable var;
    var.type = variable;

    strcpy(var.name,args[0]);
    var.value = malloc(1024);
    strcpy(var.value,args[1]);
    env->variables[env->variables_used] = var;
    env->variables_used++;
    strcpy(result,args[0]);
}

void lispel_table_new(char* result, char** args, int* types,struct lispel_environment* env)
{

    if (types[0] != variable)
        {
            printf("%s:%d: Error!:\n\tKein gültiger Table-Name: %s",__FUNCTION__,__LINE__,args[0]);
            exit(EXIT_FAILURE);
        }

    env->variables[env->variables_used].type = table;

    int g;
    if (types[1] == variable)
    {
        int num = -1;
        for (g = env->variables_used; g >= 0 ; g--)
            if ((0 == strcmp(args[1],env->variables[g].name)))
                num = g;
        if (num == -1)
        {
            printf("%s:%d: Error!:\n\tVariable nicht gefunden: %s",__FUNCTION__,__LINE__,args[1]);
            exit(EXIT_FAILURE);
        }
        strcpy(args[1],env->variables[num].value);
    }

    if (strstr(args[1],"."))
    {
        int i;

        for (i = strlen(args[1] + 1); i > 0 && args[1][i + 1] != '.'; i--)
        {
            if (args[1][i] == '0' || args[1][i] == '.')
                args[1][i] = 0;
        } 
        if (strstr(args[1],"."))
        {
            printf("%s:%d: Error!:\n\tTables können nicht mit Dezimalzahlen initialisiert werden!: %s\n",__FUNCTION__,__LINE__,args[1]);
            exit(EXIT_FAILURE);
        }
    }

    strcpy(env->variables[env->variables_used].name,args[0]);

    int size;
    if (EOF == sscanf(args[1],"%d",&size))
    {
        printf("%s:%d: Error!:\n\tKein gültiger Initilisierungswert!: %s\n",__FUNCTION__,__LINE__,args[1]);
        exit(EXIT_FAILURE);
    }

    env->variables[env->variables_used].table_length = size;
    env->variables[env->variables_used].table_values = malloc(size * sizeof(char*));

    for (g = 0; g < size; g++)
    {
        env->variables[env->variables_used].table_values[g] = malloc(1024);
    }
    
    env->variables_used++;
    strcpy(result,args[0]);
}

void lispel_table_at(char* result, char** args, int* types,struct lispel_environment* env)
{
    int g;
    if (types[1] == variable)
    {
        int num = -1;
        for (g = 0; g < env->variables_used; g++)
            if ((0 == strcmp(args[1],env->variables[g].name)))
                num = g;
        if (num == -1)
            {
                printf("%s:%d: Error!:\n\tVariable nicht gefunden: %s",__FUNCTION__,__LINE__,args[1]);
                exit(EXIT_FAILURE);
            }
        strcpy(args[1],env->variables[num].value);
    }

    if (strstr(args[1],"."))
    {
        int i;

        for (i = strlen(args[1] + 1); i > 0 && args[1][i + 1] != '.'; i--)
        {
            if (args[1][i] == '0' || args[1][i] == '.')
                args[1][i] = 0;
        } 
        if (strstr(args[1],"."))
        {
            printf("%s:%d: Error!:\n\tTables können nicht mit Dezimalzahlen geindext werden!",__FUNCTION__,__LINE__);
            exit(EXIT_FAILURE);
        }        
    }

    int index;
    if (EOF == sscanf(args[1],"%d",&index))
    {
        printf("%s:%d: Error!:\n\tKein gültiger Index!: %s\n",__FUNCTION__,__LINE__,args[1]);
        exit(EXIT_FAILURE);
    }

    int num = -1;
    for (g = 0; g < env->variables_used; g++)
    {
        if ((0 == strcmp(args[0],env->variables[g].name)) && env->variables[g].type == table)
            num = g;
    }
    if (num == -1)
    {
        printf("%s:%d: Error!:\n\tTable nicht gefunden: %s",__FUNCTION__,__LINE__,args[0]);
        exit(EXIT_FAILURE);
    }

    if (index >= env->variables[num].table_length)
    {
        printf("%s:%d: Error!:\n\tTable hat nicht so viele Plätze: %d Höchster Index: %d",__FUNCTION__,__LINE__,index,env->variables[num].table_length - 1);
        exit(EXIT_FAILURE);
    }

    strcpy(env->variables[0].name,args[0]);
    strcat(env->variables[0].name,"_table_temp");

    env->variables[0].value = env->variables[num].table_values[index];

    strcpy(result,env->variables[0].name);
}

void lispel_add(char* result, char** args, int* types,struct lispel_environment* env)
{
    char* first = args[0];
    char* second = args[1];



    int l,g;
    for (l = 0; l < 2; l++)
    {
        if (types[l] == variable)
        {
            int num = -1;
            for (g = env->variables_used; g >= 0 ; g--)
                if ((0 == strcmp(args[l],env->variables[g].name)))
                    num = g;
            if (num == -1)
                {
                    printf("%s:%d: Error!:\n\tVariable nicht gefunden: %s",__FUNCTION__,__LINE__,args[l]);
                    exit(EXIT_FAILURE);
                }
            strcpy(args[l],env->variables[num].value);
        }
    }

    void* num_1 = NULL;
    void* num_2 = NULL;

    char temp_result[2048] = {0};

    bool is_float[2] = {0};

    if (strstr(first,"."))
    {
        is_float[0] = true;
        num_1 = malloc(sizeof(float));
        sscanf(first,"%f",(float*)num_1);
    }
    else
    {
        num_1 = malloc(sizeof(int));
        sscanf(first,"%d",(int*)num_1);
    }

    if (strstr(second,"."))
    {
        is_float[1] = true;
        num_2 = malloc(sizeof(float));
        sscanf(second,"%f",(float*)num_2);
    }
    else
    {
        num_2 = malloc(sizeof(int));
        sscanf(second,"%d",(int*)num_2);
    }


    if (is_float[0] && is_float[1])
    {
        float res = *((float*)num_1) + *((float*)num_2);
        sprintf(temp_result,"%f",res);
    }
    else if (!is_float[0] && is_float[1])
    {
        float res = *((int*)num_1) + *((float*)num_2);
        sprintf(temp_result,"%f",res);
    }
    else if (is_float[0] && !is_float[1])
    {
        float res = *((float*)num_1) + *((int*)num_2);
        sprintf(temp_result,"%f",res);
    }
    else if (!is_float[0] && !is_float[1])
    {
        int res = *((int*)num_1) + *((int*)num_2);
        sprintf(temp_result,"%d",res);
    }

    free(num_1);
    free(num_2);

    strcpy(result,temp_result);
}

void lispel_sub(char* result, char** args, int* types,struct lispel_environment* env)
{
    char* first = args[0];
    char* second = args[1];

    int l,g;
    for (l = 0; l < 2; l++)
    {
        if (types[l] == variable)
        {
            int num = -1;
            for (g = env->variables_used; g >= 0 ; g--)
                if ((0 == strcmp(args[l],env->variables[g].name)))
                    num = g;
            if (num == -1)
                {
                    printf("%s:%d: Error!:\n\tVariable nicht gefunden: %s",__FUNCTION__,__LINE__,args[l]);
                    exit(EXIT_FAILURE);
                }
            strcpy(args[l],env->variables[num].value);
        }
    }

    void* num_1 = NULL;
    void* num_2 = NULL;

    char temp_result[2048] = {0};

    bool is_float[2] = {0};

    if (strstr(first,"."))
    {
        is_float[0] = true;
        num_1 = malloc(sizeof(float));
        sscanf(first,"%f",(float*)num_1);
    }
    else
    {
        num_1 = malloc(sizeof(int));
        sscanf(first,"%d",(int*)num_1);
    }

    if (strstr(second,"."))
    {
        is_float[1] = true;
        num_2 = malloc(sizeof(float));
        sscanf(second,"%f",(float*)num_2);
    }
    else
    {
        num_2 = malloc(sizeof(int));
        sscanf(second,"%d",(int*)num_2);
    }


    if (is_float[0] && is_float[1])
    {
        float res = *((float*)num_1) - *((float*)num_2);
        sprintf(temp_result,"%f",res);
    }
    else if (!is_float[0] && is_float[1])
    {
        float res = *((int*)num_1) - *((float*)num_2);
        sprintf(temp_result,"%f",res);
    }
    else if (is_float[0] && !is_float[1])
    {
        float res = *((float*)num_1) - *((int*)num_2);
        sprintf(temp_result,"%f",res);
    }
    else if (!is_float[0] && !is_float[1])
    {
        int res = *((int*)num_1) - *((int*)num_2);
        sprintf(temp_result,"%d",res);
    }

    free(num_1);
    free(num_2);

    strcpy(result,temp_result);
}

void lispel_mul(char* result, char** args, int* types,struct lispel_environment* env)
{
    char* first = args[0];
    char* second = args[1];

    int l,g;
    for (l = 0; l < 2; l++)
    {
        if (types[l] == variable)
        {
            int num = -1;
            for (g = env->variables_used; g >= 0 ; g--)
                if ((0 == strcmp(args[l],env->variables[g].name)))
                    num = g;
            if (num == -1)
                {
                    printf("%s:%d: Error!:\n\tVariable nicht gefunden: %s",__FUNCTION__,__LINE__,args[l]);
                    exit(EXIT_FAILURE);
                }
            strcpy(args[l],env->variables[num].value);
        }
    }

    void* num_1 = NULL;
    void* num_2 = NULL;

    char temp_result[2048] = {0};

    bool is_float[2] = {0};

    if (strstr(first,"."))
    {
        is_float[0] = true;
        num_1 = malloc(sizeof(float));
        sscanf(first,"%f",(float*)num_1);
    }
    else
    {
        num_1 = malloc(sizeof(int));
        sscanf(first,"%d",(int*)num_1);
    }

    if (strstr(second,"."))
    {
        is_float[1] = true;
        num_2 = malloc(sizeof(float));
        sscanf(second,"%f",(float*)num_2);
    }
    else
    {
        num_2 = malloc(sizeof(int));
        sscanf(second,"%d",(int*)num_2);
    }


    if (is_float[0] && is_float[1])
    {
        float res = *((float*)num_1) * *((float*)num_2);
        sprintf(temp_result,"%f",res);
    }
    else if (!is_float[0] && is_float[1])
    {
        float res = *((int*)num_1) * *((float*)num_2);
        sprintf(temp_result,"%f",res);
    }
    else if (is_float[0] && !is_float[1])
    {
        float res = *((float*)num_1) * *((int*)num_2);
        sprintf(temp_result,"%f",res);
    }
    else if (!is_float[0] && !is_float[1])
    {
        int res = *((int*)num_1) * *((int*)num_2);
        sprintf(temp_result,"%d",res);
    }

    free(num_1);
    free(num_2);

    strcpy(result,temp_result);
}

void lispel_dib(char* result, char** args, int* types,struct lispel_environment* env)
{
    char* first = args[0];
    char* second = args[1];

    int l,g;
    for (l = 0; l < 2; l++)
    {
        if (types[l] == variable)
        {
            int num = -1;
            for (g = env->variables_used; g >= 0 ; g--)
                if ((0 == strcmp(args[l],env->variables[g].name)))
                    num = g;
            if (num == -1)
                {
                    printf("%s:%d: Error!:\n\tVariable nicht gefunden: %s",__FUNCTION__,__LINE__,args[l]);
                    exit(EXIT_FAILURE);
                }
            strcpy(args[l],env->variables[num].value);
        }
    }

    void* num_1 = NULL;
    void* num_2 = NULL;

    char temp_result[2048] = {0};

    bool is_float[2] = {0};

    if (strstr(first,"."))
    {
        is_float[0] = true;
        num_1 = malloc(sizeof(float));
        sscanf(first,"%f",(float*)num_1);
    }
    else
    {
        num_1 = malloc(sizeof(int));
        sscanf(first,"%d",(int*)num_1);
    }

    if (strstr(second,"."))
    {
        is_float[1] = true;
        num_2 = malloc(sizeof(float));
        sscanf(second,"%f",(float*)num_2);
    }
    else
    {
        num_2 = malloc(sizeof(int));
        sscanf(second,"%d",(int*)num_2);
    }


    if (is_float[0] && is_float[1])
    {
        float res = *((float*)num_1) / *((float*)num_2);
        sprintf(temp_result,"%f",res);
    }
    else if (!is_float[0] && is_float[1])
    {
        float res = *((int*)num_1) / *((float*)num_2);
        sprintf(temp_result,"%f",res);
    }
    else if (is_float[0] && !is_float[1])
    {
        float res = *((float*)num_1) / *((int*)num_2);
        sprintf(temp_result,"%f",res);
    }
    else if (!is_float[0] && !is_float[1])
    {
        float res = *((float*)num_1) / *((float*)num_2);
        sprintf(temp_result,"%f",res);
    }

    free(num_1);
    free(num_2);

    strcpy(result,temp_result);
}

void init_std_ops(struct lispel_op_list* op_list)
{
	
		
	(*op_list).operators[(*op_list).used].name = "+";
    (*op_list).operators[(*op_list).used].num_args = 2;
    (*op_list).operators[(*op_list).used].arg_types = malloc(sizeof(enum lispel_type) * (*op_list).operators[(*op_list).used].num_args);
    (*op_list).operators[(*op_list).used].arg_types[0] = number;
    (*op_list).operators[(*op_list).used].arg_types[1] = number;
    (*op_list).operators[(*op_list).used].func = lispel_add;
    (*op_list).used++;




    (*op_list).operators[(*op_list).used].name = "-";
    (*op_list).operators[(*op_list).used].num_args = 2;
    (*op_list).operators[(*op_list).used].arg_types = malloc(sizeof(enum lispel_type) * (*op_list).operators[(*op_list).used].num_args);
    (*op_list).operators[(*op_list).used].arg_types[0] = number;
    (*op_list).operators[(*op_list).used].arg_types[1] = number;
    (*op_list).operators[(*op_list).used].func = lispel_sub;
    (*op_list).used++;




    (*op_list).operators[(*op_list).used].name = "*";
    (*op_list).operators[(*op_list).used].num_args = 2;
    (*op_list).operators[(*op_list).used].arg_types = malloc(sizeof(enum lispel_type) * (*op_list).operators[(*op_list).used].num_args);
    (*op_list).operators[(*op_list).used].arg_types[0] = number;
    (*op_list).operators[(*op_list).used].arg_types[1] = number;
    (*op_list).operators[(*op_list).used].func = lispel_mul;
    (*op_list).used++;



    (*op_list).operators[(*op_list).used].name = "/";
    (*op_list).operators[(*op_list).used].num_args = 2;
    (*op_list).operators[(*op_list).used].arg_types = malloc(sizeof(enum lispel_type) * (*op_list).operators[(*op_list).used].num_args);
    (*op_list).operators[(*op_list).used].arg_types[0] = number;
    (*op_list).operators[(*op_list).used].arg_types[1] = number;
    (*op_list).operators[(*op_list).used].func = lispel_dib;
    (*op_list).used++;



    (*op_list).operators[(*op_list).used].name = "=";
    (*op_list).operators[(*op_list).used].num_args = 2;
    (*op_list).operators[(*op_list).used].arg_types = malloc(sizeof(enum lispel_type) * (*op_list).operators[(*op_list).used].num_args);
    (*op_list).operators[(*op_list).used].arg_types[0] = variable;
    (*op_list).operators[(*op_list).used].arg_types[1] = number;
    (*op_list).operators[(*op_list).used].func = lispel_var_assign;
    (*op_list).used++;



    (*op_list).operators[(*op_list).used].name = "p";
    (*op_list).operators[(*op_list).used].num_args = 1;
    (*op_list).operators[(*op_list).used].arg_types = malloc(sizeof(enum lispel_type) * (*op_list).operators[(*op_list).used].num_args);
    (*op_list).operators[(*op_list).used].arg_types[0] = number;
    (*op_list).operators[(*op_list).used].func = lispel_print;
    (*op_list).used++;



    (*op_list).operators[(*op_list).used].name = "c";
    (*op_list).operators[(*op_list).used].num_args = 1;
    (*op_list).operators[(*op_list).used].arg_types = malloc(sizeof(enum lispel_type) * (*op_list).operators[(*op_list).used].num_args);
    (*op_list).operators[(*op_list).used].arg_types[0] = number;
    (*op_list).operators[(*op_list).used].func = lispel_character;
    (*op_list).used++;



    (*op_list).operators[(*op_list).used].name = "==";
    (*op_list).operators[(*op_list).used].num_args = 2;
    (*op_list).operators[(*op_list).used].arg_types = malloc(sizeof(enum lispel_type) * (*op_list).operators[(*op_list).used].num_args);
    (*op_list).operators[(*op_list).used].arg_types[0] = number;
    (*op_list).operators[(*op_list).used].arg_types[1] = number;
    (*op_list).operators[(*op_list).used].func = lispel_equality;
    (*op_list).used++;


    (*op_list).operators[(*op_list).used].name = ">";
    (*op_list).operators[(*op_list).used].num_args = 2;
    (*op_list).operators[(*op_list).used].arg_types = malloc(sizeof(enum lispel_type) * (*op_list).operators[(*op_list).used].num_args);
    (*op_list).operators[(*op_list).used].arg_types[0] = number;
    (*op_list).operators[(*op_list).used].arg_types[1] = number;
    (*op_list).operators[(*op_list).used].func = lispel_bigger;
    (*op_list).used++;


    (*op_list).operators[(*op_list).used].name = "<";
    (*op_list).operators[(*op_list).used].num_args = 2;
    (*op_list).operators[(*op_list).used].arg_types = malloc(sizeof(enum lispel_type) * (*op_list).operators[(*op_list).used].num_args);
    (*op_list).operators[(*op_list).used].arg_types[0] = number;
    (*op_list).operators[(*op_list).used].arg_types[1] = number;
    (*op_list).operators[(*op_list).used].func = lispel_smaller;
    (*op_list).used++;


    (*op_list).operators[(*op_list).used].name = "!";
    (*op_list).operators[(*op_list).used].num_args = 1;
    (*op_list).operators[(*op_list).used].arg_types = malloc(sizeof(enum lispel_type) * (*op_list).operators[(*op_list).used].num_args);
    (*op_list).operators[(*op_list).used].arg_types[0] = number;
    (*op_list).operators[(*op_list).used].func = lispel_negate;
    (*op_list).used++;



    (*op_list).operators[(*op_list).used].name = "v";
    (*op_list).operators[(*op_list).used].num_args = 2;
    (*op_list).operators[(*op_list).used].arg_types = malloc(sizeof(enum lispel_type) * (*op_list).operators[(*op_list).used].num_args);
    (*op_list).operators[(*op_list).used].arg_types[0] = variable;
    (*op_list).operators[(*op_list).used].arg_types[1] = number;
    (*op_list).operators[(*op_list).used].func = lispel_var_number;
    (*op_list).used++;



    (*op_list).operators[(*op_list).used].name = "t";
    (*op_list).operators[(*op_list).used].num_args = 2;
    (*op_list).operators[(*op_list).used].arg_types = malloc(sizeof(enum lispel_type) * (*op_list).operators[(*op_list).used].num_args);
    (*op_list).operators[(*op_list).used].arg_types[0] = variable;
    (*op_list).operators[(*op_list).used].arg_types[1] = number;
    (*op_list).operators[(*op_list).used].func = lispel_table_new;
    (*op_list).used++;


    (*op_list).operators[(*op_list).used].name = "d";
    (*op_list).operators[(*op_list).used].num_args = 1;
    (*op_list).operators[(*op_list).used].arg_types = malloc(sizeof(enum lispel_type) * (*op_list).operators[(*op_list).used].num_args);
    (*op_list).operators[(*op_list).used].arg_types[0] = variable;
    (*op_list).operators[(*op_list).used].func = lispel_var_destroy;
    (*op_list).used++;


    (*op_list).operators[(*op_list).used].name = "a";
    (*op_list).operators[(*op_list).used].num_args = 2;
    (*op_list).operators[(*op_list).used].arg_types = malloc(sizeof(enum lispel_type) * (*op_list).operators[(*op_list).used].num_args);
    (*op_list).operators[(*op_list).used].arg_types[0] = table;
    (*op_list).operators[(*op_list).used].arg_types[1] = number;
    (*op_list).operators[(*op_list).used].func = lispel_table_at;
    (*op_list).used++;


    (*op_list).operators[(*op_list).used].name = "i";
    (*op_list).operators[(*op_list).used].num_args = 2;
    (*op_list).operators[(*op_list).used].arg_types = malloc(sizeof(enum lispel_type) * (*op_list).operators[(*op_list).used].num_args);
    (*op_list).operators[(*op_list).used].arg_types[0] = number;
    (*op_list).operators[(*op_list).used].arg_types[1] = variable;
    (*op_list).operators[(*op_list).used].func = lispel_if_do_block;
    (*op_list).used++; 

    (*op_list).operators[(*op_list).used].name = "b";
    (*op_list).operators[(*op_list).used].num_args = 1;
    (*op_list).operators[(*op_list).used].arg_types = malloc(sizeof(enum lispel_type) * (*op_list).operators[(*op_list).used].num_args);
    (*op_list).operators[(*op_list).used].arg_types[0] = variable;
    (*op_list).operators[(*op_list).used].func = lispel_do_block;
    (*op_list).used++;

        
	if (op_list->used > op_list->max)
	{
        printf("%s:%d: Error!:\n\tNicht genug Platz für alle Operatoren!\n",__FUNCTION__,__LINE__);
        exit(EXIT_FAILURE);
    }
}

void add_to_op_list(struct lispel_op_list* op_list, struct lispel_op_list* add)
{
    int i;
    for (i = 0; i < add->used; i++)
    {
        op_list->operators[op_list->used + i] = add->operators[i];

        if (op_list->used++ >= op_list->max)
            {
                printf("%s:%d: Error!:\n\tNicht genug Platz für alle Operatoren!",__FUNCTION__,__LINE__);
                exit(EXIT_FAILURE);
            }
    }
}




float lispel_get_var(struct lispel_environment* env, char* var_name)
{
    int i;
    for (i = 0; i < env->variables_used; i++)
        if ((0 == strcmp(var_name,env->variables[i].name)) && env->variables[i].type == variable)
        {
            if (strstr(env->variables[i].value,"."))
            {
                int j;

                for (j = strlen(env->variables[i].value + 1); j > 0 && env->variables[i].value[j + 1] != '.'; j--)
                {
                    if (env->variables[i].value[j] == '0' || env->variables[i].value[j] == '.')
                        env->variables[i].value[j] = 0;
                }
                if (strstr(env->variables[i].value,"."))
                {
                    float f;
                    sscanf(env->variables[i].value,"%f",&f);
                    return f;
                }
                else
                {
                    int f;
                    sscanf(env->variables[i].value,"%d",&f);
                    return f;
                }
            }
            else
            {
                int f;
                sscanf(env->variables[i].value,"%d",&f);
                return f;
            }

        }
    {
        printf("%s:%d: Error!:\n\tVariable nicht gefunden: %s\n",__FUNCTION__,__LINE__,var_name);
        exit(EXIT_FAILURE);
    }
    return 0;
}

float lispel_get_array_at(struct lispel_environment* env, char* var_name, int index)
{
    int i;
    for (i = 0; i < env->variables_used; i++)
        if ((0 == strcmp(var_name,env->variables[i].name)) && env->variables[i].type == table)
        {
            if (strstr(env->variables[i].value,"."))
            {
                int j;

                for (j = strlen(env->variables[i].value + 1); j > 0 && env->variables[i].value[j + 1] != '.'; j--)
                {
                    if (env->variables[i].table_values[index][j] == '0' || env->variables[i].table_values[index][j] == '.')
                        env->variables[i].table_values[index][j] = 0;
                }
                if (strstr(env->variables[i].table_values[index],"."))
                {
                    float f;
                    sscanf(env->variables[i].table_values[index],"%f",&f);
                    return f;
                }
                else
                {
                    int f;
                    sscanf(env->variables[i].table_values[index],"%d",&f);
                    return f;
                }
            }
            else
            {
                int f;
                sscanf(env->variables[i].table_values[index],"%d",&f);
                return f;
            }

        }
    {
        printf("%s:%d: Error!:\n\tTable nicht gefunden: %s\n",__FUNCTION__,__LINE__,var_name);
        exit(EXIT_FAILURE);
    }
    return 0;
}

void lispel_do(char* script, struct lispel_environment* env)
{
    char result[1024] = {0};
    int i;



    struct lispel_state st;
    memset(&st,0,sizeof(struct lispel_state));
    env->chunks_used = 0;

    gen_blocks(script,script,env);
    gen_chunks(env,script);
    gen_tokens(script,env,&st);
    gen_expressions(&st);

    for (i = 0; i < st.expressions_used; i++)
    {
        do_expression(&st.expressions[i],env,result);
        free(st.expressions[i].tokens);
    }
}

struct lispel_environment* lispel_init()
{
    struct lispel_environment* env = malloc(sizeof(struct lispel_environment));
    memset(env,0,sizeof(struct lispel_environment));
    env->variables_used = 1;
    strcpy(env->variables[0].name,"F6gUow6M");
    env->op_list.used = 0;
    env->op_list.max = 17;
    env->op_list.operators = malloc(sizeof(struct lispel_operator) * env->op_list.max);

    init_std_ops(&env->op_list);
    return env;
}

void lispel_deinit(struct lispel_environment* env)
{
    int i;
    for (i = 0; i < env->op_list.used; i++)
    {
        free(env->op_list.operators[i].arg_types);
    }
    free(env->variables[1].value);
    free(env->variables[2].value);
    free(env->op_list.operators);
    free(env);
}

int main(__attribute__((unused))int argc, __attribute__((unused))char* argv[])
{
    struct lispel_environment* env = lispel_init();

    char* code =
    "(t table 2)"

    "(= (a table 0) (+ 12 12))"
    "(= (a table 1) (+ 400 12))"

    "(p (a table 1))"
    "(c 32)"
    "(p (a table 0))"
    "(c 10)"

    "(d table)";

    while(1)
    lispel_do(code,env);

    lispel_deinit(env);

    #ifdef DEBUG_MEM
    DEBUG_MEMeval();
    #endif

    exit(0);
}