
#include <LSD/LSD.h>

#include "lispel.h"

bool is_number(char* data)
{
    int i;
    bool is_number = false;
    while(*data)
    {
        for (i = 45; i < 58; i++)
        {
            if (i == 47)
                i++;
            if (*data == i)
                is_number = true;
            if (*data == ' ')
                return false;
        }
        if (!is_number)
            return false;
        data++;
    }
    return true;
}


enum lispel_arg_type {
    none,number,string,variable,operator,ob,cb,expression
};

struct lispel_variable
{
    char name[1024];
    char value[1024];
    enum lispel_arg_type type;
};

struct lispel_token {
    enum lispel_arg_type type;
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
    enum lispel_arg_type* arg_types;
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
            
            #ifdef DEBUG
            LSD_Log(LSD_ltMESSAGE,"CHUNK ADDED!");
            #endif

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
                LSD_Log(LSD_ltERROR,"Maximale Tiefe von 127 überschritten!");
                }
            depth++;
            tok.id = id;
            id++;
            st->tokens[(st->tokens_used)] = tok;
            (st->tokens_used)++;
            {
            #ifdef DEBUG
            LSD_Log(LSD_ltMESSAGE,"Opening Brace hinzugefügt!: Depth: %d",tok.data[0]);
            #endif
            }
        }
        else if (*code == ')')
        {
            depth--;
            tok.data[0] = depth;
            tok.type = cb;

            if (depth < 0)
            {
                LSD_Log(LSD_ltMESSAGE,"%s",code);
                LSD_Log(LSD_ltERROR,"Minimale Tiefe von 0 unterschritten!");
            }


            tok.id = id;
            id++;
            st->tokens[(st->tokens_used)] = tok;
            (st->tokens_used)++;
            {
            #ifdef DEBUG
            LSD_Log(LSD_ltMESSAGE,"Closing Brace hinzugefügt!: Depth: %d",tok.data[0]);
            #endif
            }
        }
        else if (*code == '"')
        {
            char str[1024] = {0};
            int i = 0;
            code++;
            while(*code != '"')
            {
                str[i] = *code;
                i++;
                code++;
            }

            tok.type = string;
            strcpy(tok.data,str); 

            tok.id = id;
            id++;
            st->tokens[(st->tokens_used)] = tok;
            (st->tokens_used)++;
            {
            #ifdef DEBUG
            LSD_Log(LSD_ltMESSAGE,"String hinzugefügt!: Inhalt: %s",tok.data);
            #endif
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

            if (is_number(num_test))
            {
                tok.type = number;
                strcpy(tok.data,num_test); 

                tok.id = id;
                id++;
                st->tokens[(st->tokens_used)] = tok;
                (st->tokens_used)++;
                {
                #ifdef DEBUG
                LSD_Log(LSD_ltMESSAGE,"Zahl hinzugefügt!: Wert: %s",tok.data);
                #endif
                }
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
                if (LSD_Sys_strcmp(tok.data,env->op_list.operators[i].name))
                    found = true;
            }


            if (found)
            {
                tok.type = operator;
                tok.id = id;
                id++;
                st->tokens[(st->tokens_used)] = tok;
                (st->tokens_used)++;
                #ifdef DEBUG
                LSD_Log(LSD_ltMESSAGE,"Operator hinzugefügt!: Art: %s",tok.data);
                #endif
            }
            else
            {
                bool var_found = false;
                int j;
                for (j = 0; j < env->variables_used; j++)
                {
                    if (LSD_Sys_strcmp(op,env->variables[j].name))
                    {
                        var_found = true;
                        tok.type = variable;
                        tok.id = id;
                        id++;
                        st->tokens[(st->tokens_used)] = tok;
                        (st->tokens_used)++;
                        #ifdef DEBUG
                        LSD_Log(LSD_ltMESSAGE,"Variablen-Token hinzugefügt!: Name: %s Wert: %s",env->variables[j].name,env->variables[j].value);
                        #endif
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
                    #ifdef DEBUG
                    LSD_Log(LSD_ltMESSAGE,"Variablen-Token hinzugefügt(nofound)!: Name: %s Wert: %s",op,0);
                    #endif
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
            #ifdef DEBUG
            LSD_Log(LSD_ltMESSAGE,"HIGHEST IS %d IN %d!",highest,highest_index);
            int gsus;
            for (gsus = 0; gsus < num_tokens; gsus++)
                if (tokens[gsus].type != ob && tokens[gsus].type != cb)
                    LSD_Log(LSD_ltMESSAGE,"%d -> %s",tokens[gsus].type,tokens[gsus].data);
                else
                    LSD_Log(LSD_ltMESSAGE,"%d -> %d",tokens[gsus].type,tokens[gsus].data[0]);
            #endif
            int op_index = -1;
            for (j = 0; j < env->op_list.used; j++)
                if (LSD_Sys_strcmp(tokens[highest_index + 1].data,env->op_list.operators[j].name))
                    op_index = j;

            if (op_index == -1)
                LSD_Log(LSD_ltERROR,"Operator nicht gefunden: %s : %d",tokens[highest_index + 1].data);

            char** args = malloc(sizeof(char*) * (env->op_list.operators[op_index].num_args + 1));

            for (j = 0; j < env->op_list.operators[op_index].num_args; j++)
                args[j] = malloc(env->op_list.operators[op_index].num_args * 1024);

            int* types = malloc(sizeof(int) * env->op_list.operators[op_index].num_args);

            struct lispel_token result = {0};

            for (j = 0; j < env->op_list.operators[op_index].num_args; j++)
            {
                if (tokens[highest_index + j + 2].type == cb)
                    LSD_Log(LSD_ltERROR,"Zu wenige Argumente für %s-Operator!",env->op_list.operators[op_index].name);

                if (tokens[highest_index + j + 2].type != variable && tokens[highest_index + j + 2].type != env->op_list.operators[op_index].arg_types[j])
                    LSD_Log(LSD_ltERROR,"Falscher Datentyp für %s-Operator Argument Nummer %d: %d != %d",env->op_list.operators[op_index].name,j,tokens[highest_index + j + 2].type,env->op_list.operators[op_index].arg_types[j]);

                strcpy(args[j],tokens[highest_index + j + 2].data);
                types[j] = tokens[highest_index + j + 2].type;
            }

            if (tokens[highest_index + j + 2].type != cb)
                LSD_Log(LSD_ltERROR,"Zu viele Argumente für %s-Operator!",env->op_list.operators[op_index].name);

            args[env->op_list.operators[op_index].num_args] = (void*)expr;

            #ifdef DEBUG
            LSD_Log(LSD_ltMESSAGE,"Führe %s-Operator aus!",env->op_list.operators[op_index].name);
            #endif
            env->op_list.operators[op_index].func(result.data,args,types,env);

            int k = 0;
            for (k = 0; k < env->op_list.operators[op_index].num_args; k++)
                    free(args[k]);
            free(args);
            free(types);

            #ifdef DEBUG
            LSD_Log(LSD_ltMESSAGE,"RESULT: %s",result.data);
            #endif

            if (highest == 0)
                strcpy(end_result,result.data);


            if (is_number(result.data))
                result.type = number;
            else if (strstr(result.data,"\""))
                result.type = string;
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
}


void lispel_do_block(char* result, char** args, int* __attribute__((unused))types, struct lispel_environment* env)
{
    int i,num = -1;
    for (i = 0; i < env->blocks_used; i++)
        if (LSD_Sys_strcmp(args[0],env->blocks[i].name))
            num = i;
    if (num == -1)
        LSD_Log(LSD_ltERROR,"Block nicht gefunden: %s",args[0]);

    strcpy(result,"0");

    struct lispel_state st = {0};

    char script[strlen(env->blocks[num].code) + 1];
    strcpy(script,env->blocks[num].code);

    gen_blocks(script,script,env);
    gen_chunks(env,script);

    gen_tokens(script,env,&st);

    gen_expressions(&st);

    for (i = 0; i < st.expressions_used; i++)
        do_expression(&st.expressions[i],env,result);
}

void lispel_if_do_block(char* result, char** args, int* types, struct lispel_environment* env)
{
    int i,num = -1;
    for (i = 0; i < env->blocks_used; i++)
        if (LSD_Sys_strcmp(args[1],env->blocks[i].name))
            num = i;
    if (num == -1)
        LSD_Log(LSD_ltERROR,"Block nicht gefunden: %s",args[1]);

    if (types[0] == variable)
    {
        int num = -1,g;
        for (g = 0; g < env->variables_used; g++)
            if (LSD_Sys_strcmp(args[0],env->variables[g].name))
                num = g;
        if (num == -1)
            LSD_Log(LSD_ltERROR,"Variable nicht gefunden: %s",args[0]);
        strcpy(args[0],env->variables[num].value);
    }

    strcpy(result,args[0]);

    if (!LSD_Sys_strcmp(args[0],"0"))
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
            do_expression(&st.expressions[i],env,result);
    }
}

void lispel_negate(char* result, char** args, int* types,struct lispel_environment* env)
{
    int g;
    if (types[0] == variable)
    {
        int num = -1;
        for (g = 0; g < env->variables_used; g++)
            if (LSD_Sys_strcmp(args[0],env->variables[g].name))
                num = g;
        if (num == -1)
            LSD_Log(LSD_ltERROR,"Variable nicht gefunden: %s",args[0]);
        strcpy(args[0],env->variables[num].value);
    }

    strcpy(result,LSD_Sys_strcmp("0",args[0]) ? "1" : "0");
}

void lispel_bigger(char* result, char** args, int* types,struct lispel_environment* env)
{
    int g;
    if (types[0] == variable)
    {
        int num = -1;
        for (g = 0; g < env->variables_used; g++)
            if (LSD_Sys_strcmp(args[0],env->variables[g].name))
                num = g;
        if (num == -1)
            LSD_Log(LSD_ltERROR,"Variable nicht gefunden: %s",args[0]);
        strcpy(args[0],env->variables[num].value);
    }
    if (types[1] == variable)
    {
        int num = -1;
        for (g = 0; g < env->variables_used; g++)
            if (LSD_Sys_strcmp(args[1],env->variables[g].name))
                num = g;
        if (num == -1)
            LSD_Log(LSD_ltERROR,"Variable nicht gefunden: %s",args[1]);
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


    strcpy(result,temp_result);
}

void lispel_smaller(char* result, char** args, int* types,struct lispel_environment* env)
{
    int g;
    if (types[0] == variable)
    {
        int num = -1;
        for (g = 0; g < env->variables_used; g++)
            if (LSD_Sys_strcmp(args[0],env->variables[g].name))
                num = g;
        if (num == -1)
            LSD_Log(LSD_ltERROR,"Variable nicht gefunden: %s",args[0]);
        strcpy(args[0],env->variables[num].value);
    }
    if (types[1] == variable)
    {
        int num = -1;
        for (g = 0; g < env->variables_used; g++)
            if (LSD_Sys_strcmp(args[1],env->variables[g].name))
                num = g;
        if (num == -1)
            LSD_Log(LSD_ltERROR,"Variable nicht gefunden: %s",args[1]);
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


    strcpy(result,temp_result);
}

void lispel_equality(char* result, char** args, int* types,struct lispel_environment* env)
{
    int g;
    if (types[0] == variable)
    {
        int num = -1;
        for (g = 0; g < env->variables_used; g++)
            if (LSD_Sys_strcmp(args[0],env->variables[g].name))
                num = g;
        if (num == -1)
            LSD_Log(LSD_ltERROR,"Variable nicht gefunden: %s",args[0]);
        strcpy(args[0],env->variables[num].value);
    }
    if (types[1] == variable)
    {
        int num = -1;
        for (g = 0; g < env->variables_used; g++)
            if (LSD_Sys_strcmp(args[1],env->variables[g].name))
                num = g;
        if (num == -1)
            LSD_Log(LSD_ltERROR,"Variable nicht gefunden: %s",args[1]);
        strcpy(args[1],env->variables[num].value);
    }

    strcpy(result,LSD_Sys_strcmp(args[0],args[1]) ? "1" : "0");
}

void lispel_character(char* result, char** args, int* types,struct lispel_environment* env)
{
    int g;
    if (types[0] == variable)
    {
        int num = -1;
        for (g = 0; g < env->variables_used; g++)
            if (LSD_Sys_strcmp(args[0],env->variables[g].name))
                num = g;
        if (num == -1)
            LSD_Log(LSD_ltERROR,"Variable nicht gefunden: %s",args[0]);
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
        for (g = 0; g < env->variables_used; g++)
            if (LSD_Sys_strcmp(args[0],env->variables[g].name))
                num = g;
        if (num == -1)
            LSD_Log(LSD_ltERROR,"Variable nicht gefunden: %s",args[0]);
        strcpy(args[0],env->variables[num].value);
    }
    printf("%s",args[0]);
}

void lispel_var_destroy(char* result, char** args, int* types,struct lispel_environment* env)
{
    int i, num = -1;
    if (types[0] == variable)
    {
        for (i = 0; i < env->variables_used; i++)
            if (LSD_Sys_strcmp(args[0],env->variables[i].name))
                num = i;
        if (num == -1)
            LSD_Log(LSD_ltERROR,"Variable nicht gefunden: %s",args[0]);
        for (i = num; i < env->variables_used - 1; i++)
            env->variables[i] = env->variables[i + 1];
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
        for (g = 0; g < env->variables_used; g++)
            if (LSD_Sys_strcmp(args[1],env->variables[g].name))
                num = g;
        if (num == -1)
            LSD_Log(LSD_ltERROR,"Variable nicht gefunden: %s",args[1]);
        strcpy(args[1],env->variables[num].value);
    }

    for (g = 0; g < env->variables_used; g++)
    {
        if (LSD_Sys_strcmp(args[0],env->variables[g].name))
            var = &env->variables[g];
    }

    if (var == NULL)
            LSD_Log(LSD_ltERROR,"Variable nicht gefunden: %s",args[0]);

    strcpy(var->value,args[1]);
    strcpy(result,args[0]);
}

void lispel_var_number(char* result, char** args, int* types,struct lispel_environment* env)
{

    if (types[0] != variable)
        LSD_Log(LSD_ltERROR,"Kein gültiger Variablen-Name: %s",args[0]);

    struct lispel_variable var;
    var.type = number;

    strcpy(var.name,args[0]);
    strcpy(var.value,args[1]);
    env->variables[env->variables_used] = var;
    env->variables_used++;
    strcpy(result,args[0]);
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
            for (g = 0; g < env->variables_used; g++)
                if (LSD_Sys_strcmp(args[l],env->variables[g].name))
                    num = g;
            if (num == -1)
                LSD_Log(LSD_ltERROR,"Variable nicht gefunden: %s",args[l]);
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
            for (g = 0; g < env->variables_used; g++)
                if (LSD_Sys_strcmp(args[l],env->variables[g].name))
                    num = g;
            if (num == -1)
                LSD_Log(LSD_ltERROR,"Variable nicht gefunden: %s",args[l]);
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
            for (g = 0; g < env->variables_used; g++)
                if (LSD_Sys_strcmp(args[l],env->variables[g].name))
                    num = g;
            if (num == -1)
                LSD_Log(LSD_ltERROR,"Variable nicht gefunden: %s",args[l]);
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
            for (g = 0; g < env->variables_used; g++)
                if (LSD_Sys_strcmp(args[l],env->variables[g].name))
                    num = g;
            if (num == -1)
                LSD_Log(LSD_ltERROR,"Variable nicht gefunden: %s",args[l]);
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


    strcpy(result,temp_result);
}

void init_std_ops(struct lispel_op_list* op_list)
{
	
		
		(*op_list).operators[(*op_list).used].name = "+";
	    (*op_list).operators[(*op_list).used].num_args = 2;
	    (*op_list).operators[(*op_list).used].arg_types = malloc(sizeof(enum lispel_arg_type) * (*op_list).operators[(*op_list).used].num_args);
	    (*op_list).operators[(*op_list).used].arg_types[0] = number;
	    (*op_list).operators[(*op_list).used].arg_types[1] = number;
	    (*op_list).operators[(*op_list).used].func = lispel_add;
	    (*op_list).used++;


	
	
	    (*op_list).operators[(*op_list).used].name = "-";
	    (*op_list).operators[(*op_list).used].num_args = 2;
	    (*op_list).operators[(*op_list).used].arg_types = malloc(sizeof(enum lispel_arg_type) * (*op_list).operators[(*op_list).used].num_args);
	    (*op_list).operators[(*op_list).used].arg_types[0] = number;
	    (*op_list).operators[(*op_list).used].arg_types[1] = number;
	    (*op_list).operators[(*op_list).used].func = lispel_sub;
	    (*op_list).used++;


	
	
	    (*op_list).operators[(*op_list).used].name = "*";
	    (*op_list).operators[(*op_list).used].num_args = 2;
	    (*op_list).operators[(*op_list).used].arg_types = malloc(sizeof(enum lispel_arg_type) * (*op_list).operators[(*op_list).used].num_args);
	    (*op_list).operators[(*op_list).used].arg_types[0] = number;
	    (*op_list).operators[(*op_list).used].arg_types[1] = number;
	    (*op_list).operators[(*op_list).used].func = lispel_mul;
	    (*op_list).used++;

	
    
        (*op_list).operators[(*op_list).used].name = "/";
        (*op_list).operators[(*op_list).used].num_args = 2;
        (*op_list).operators[(*op_list).used].arg_types = malloc(sizeof(enum lispel_arg_type) * (*op_list).operators[(*op_list).used].num_args);
        (*op_list).operators[(*op_list).used].arg_types[0] = number;
        (*op_list).operators[(*op_list).used].arg_types[1] = number;
        (*op_list).operators[(*op_list).used].func = lispel_dib;
        (*op_list).used++;

    
    
        (*op_list).operators[(*op_list).used].name = "v";
        (*op_list).operators[(*op_list).used].num_args = 2;
        (*op_list).operators[(*op_list).used].arg_types = malloc(sizeof(enum lispel_arg_type) * (*op_list).operators[(*op_list).used].num_args);
        (*op_list).operators[(*op_list).used].arg_types[0] = variable;
        (*op_list).operators[(*op_list).used].arg_types[1] = number;
        (*op_list).operators[(*op_list).used].func = lispel_var_number;
        (*op_list).used++;

    
    
        (*op_list).operators[(*op_list).used].name = "=";
        (*op_list).operators[(*op_list).used].num_args = 2;
        (*op_list).operators[(*op_list).used].arg_types = malloc(sizeof(enum lispel_arg_type) * (*op_list).operators[(*op_list).used].num_args);
        (*op_list).operators[(*op_list).used].arg_types[0] = variable;
        (*op_list).operators[(*op_list).used].arg_types[1] = number;
        (*op_list).operators[(*op_list).used].func = lispel_var_assign;
        (*op_list).used++;

    
    
        (*op_list).operators[(*op_list).used].name = "p";
        (*op_list).operators[(*op_list).used].num_args = 1;
        (*op_list).operators[(*op_list).used].arg_types = malloc(sizeof(enum lispel_arg_type) * (*op_list).operators[(*op_list).used].num_args);
        (*op_list).operators[(*op_list).used].arg_types[0] = number;
        (*op_list).operators[(*op_list).used].func = lispel_print;
        (*op_list).used++;

    
    
        (*op_list).operators[(*op_list).used].name = "c";
        (*op_list).operators[(*op_list).used].num_args = 1;
        (*op_list).operators[(*op_list).used].arg_types = malloc(sizeof(enum lispel_arg_type) * (*op_list).operators[(*op_list).used].num_args);
        (*op_list).operators[(*op_list).used].arg_types[0] = number;
        (*op_list).operators[(*op_list).used].func = lispel_character;
        (*op_list).used++;

    
    
        (*op_list).operators[(*op_list).used].name = "==";
        (*op_list).operators[(*op_list).used].num_args = 2;
        (*op_list).operators[(*op_list).used].arg_types = malloc(sizeof(enum lispel_arg_type) * (*op_list).operators[(*op_list).used].num_args);
        (*op_list).operators[(*op_list).used].arg_types[0] = number;
        (*op_list).operators[(*op_list).used].arg_types[1] = number;
        (*op_list).operators[(*op_list).used].func = lispel_equality;
        (*op_list).used++;


        (*op_list).operators[(*op_list).used].name = ">";
        (*op_list).operators[(*op_list).used].num_args = 2;
        (*op_list).operators[(*op_list).used].arg_types = malloc(sizeof(enum lispel_arg_type) * (*op_list).operators[(*op_list).used].num_args);
        (*op_list).operators[(*op_list).used].arg_types[0] = number;
        (*op_list).operators[(*op_list).used].arg_types[1] = number;
        (*op_list).operators[(*op_list).used].func = lispel_bigger;
        (*op_list).used++;


        (*op_list).operators[(*op_list).used].name = "<";
        (*op_list).operators[(*op_list).used].num_args = 2;
        (*op_list).operators[(*op_list).used].arg_types = malloc(sizeof(enum lispel_arg_type) * (*op_list).operators[(*op_list).used].num_args);
        (*op_list).operators[(*op_list).used].arg_types[0] = number;
        (*op_list).operators[(*op_list).used].arg_types[1] = number;
        (*op_list).operators[(*op_list).used].func = lispel_smaller;
        (*op_list).used++;


        (*op_list).operators[(*op_list).used].name = "!";
        (*op_list).operators[(*op_list).used].num_args = 1;
        (*op_list).operators[(*op_list).used].arg_types = malloc(sizeof(enum lispel_arg_type) * (*op_list).operators[(*op_list).used].num_args);
        (*op_list).operators[(*op_list).used].arg_types[0] = number;
        (*op_list).operators[(*op_list).used].func = lispel_negate;
        (*op_list).used++;


        (*op_list).operators[(*op_list).used].name = "d";
        (*op_list).operators[(*op_list).used].num_args = 1;
        (*op_list).operators[(*op_list).used].arg_types = malloc(sizeof(enum lispel_arg_type) * (*op_list).operators[(*op_list).used].num_args);
        (*op_list).operators[(*op_list).used].arg_types[0] = variable;
        (*op_list).operators[(*op_list).used].func = lispel_var_destroy;
        (*op_list).used++;


        (*op_list).operators[(*op_list).used].name = "i";
        (*op_list).operators[(*op_list).used].num_args = 2;
        (*op_list).operators[(*op_list).used].arg_types = malloc(sizeof(enum lispel_arg_type) * (*op_list).operators[(*op_list).used].num_args);
        (*op_list).operators[(*op_list).used].arg_types[0] = number;
        (*op_list).operators[(*op_list).used].arg_types[1] = variable;
        (*op_list).operators[(*op_list).used].func = lispel_if_do_block;
        (*op_list).used++; 

        (*op_list).operators[(*op_list).used].name = "b";
        (*op_list).operators[(*op_list).used].num_args = 1;
        (*op_list).operators[(*op_list).used].arg_types = malloc(sizeof(enum lispel_arg_type) * (*op_list).operators[(*op_list).used].num_args);
        (*op_list).operators[(*op_list).used].arg_types[0] = variable;
        (*op_list).operators[(*op_list).used].func = lispel_do_block;
        (*op_list).used++;      

        

	if (op_list->used > op_list->max)
		LSD_Log(LSD_ltERROR,"Nicht genug Platz für alle Operatoren!");
}

void add_to_op_list(struct lispel_op_list* op_list, struct lispel_op_list* add)
{
    int i;
    for (i = 0; i < add->used; i++)
    {
        op_list->operators[op_list->used + i] = add->operators[i];

        if (op_list->used++ >= op_list->max)
            LSD_Log(LSD_ltERROR,"Nicht genug Platz für alle Operatoren!");
    }
}




float lispel_get_var(struct lispel_environment* env, char* var_name)
{
    int i;
    for (i = 0; i < env->variables_used; i++)
        if (LSD_Sys_strcmp(var_name,env->variables[i].name))
        {
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
    LSD_Log(LSD_ltERROR,"Variable nicht gefunden: %s",var_name);
    return 0;
}

void lispel_do(char* script, struct lispel_environment* env)
{
    char result[1024] = {0};
    int i;

    struct lispel_state st;
    memset(&st,0,sizeof(struct lispel_state));

    gen_blocks(script,script,env);
    gen_chunks(env,script);
    gen_tokens(script,env,&st);
    gen_expressions(&st);

    for (i = 0; i < st.expressions_used; i++)
        do_expression(&st.expressions[i],env,result);
}

struct lispel_environment* lispel_init()
{
    struct lispel_environment* env = malloc(sizeof(struct lispel_environment));
    memset(env,0,sizeof(struct lispel_environment));
    env->op_list.used = 0;
    env->op_list.max = 15;
    env->op_list.operators = malloc(sizeof(struct lispel_operator) * env->op_list.max);

    init_std_ops(&env->op_list);
    return env;
}

void lispel_deinit(struct lispel_environment* env)
{
    int i;
    for (i = 0; i < env->op_list.used; i++)
        free(env->op_list.operators[i].arg_types);
    free(env->op_list.operators);
    free(env);
}