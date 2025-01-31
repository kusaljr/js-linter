#include <stdio.h>
#include <ctype.h>
#include <string.h>

#define MAX_TOKEN_LEN 100
#define MAX_VARS 100
#define MAX_WARNINGS 100

typedef enum
{
    TOKEN_KEYWORD,
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_OPERATOR,
    TOKEN_DELIMITER,
    TOKEN_STRING,
    TOKEN_COMMENT,
    TOKEN_UNKNOWN
} TokenType;

typedef struct
{
    char name[MAX_TOKEN_LEN];
    int used;
    int line_number;
    int column_number;
} Variable;

Variable variables[MAX_VARS];
int variables_count = 0;

typedef struct
{
    const char *op;
    int len;
} Operator;

typedef struct
{
    char message[MAX_TOKEN_LEN];
} Warning;

Warning warnings[MAX_WARNINGS];

const Operator operators[] = {
    {"===", 3}, {"!==", 3}, {"==", 2}, {"!=", 2}, {"<=", 2}, {">=", 2}, {"+=", 2}, {"-=", 2}, {"*=", 2}, {"/=", 2}, {"++", 2}, {"--", 2}, {"&&", 2}, {"||", 2}, {"=>", 2}};
#define OPERATORS_COUNT (sizeof(operators) / sizeof(operators[0]))

const char *keywords[] = {
    "if", "else", "for", "while", "return", "function",
    "var", "let", "const", "null", "true", "false", "console", "log"};
#define KEYWORDS_COUNT (sizeof(keywords) / sizeof(keywords[0]))

const char delimiters[] = "[]{}(),;";

int total_lines = 0;
int warnings_count = 0;

void add_warning(const char *message)
{
    if (warnings_count < MAX_WARNINGS)
    {
        strcpy(warnings[warnings_count].message, message);
        warnings_count++;
    }
}

void declare_variable(const char *name, int line, int column)
{
    if (variables_count < MAX_VARS)
    {
        strcpy(variables[variables_count].name, name);
        variables[variables_count].used = 0;
        variables[variables_count].line_number = line;
        variables[variables_count].column_number = column;
        variables_count++;
    }
}

void use_variable(const char *name)
{
    for (int i = 0; i < variables_count; i++)
    {
        if (strcmp(variables[i].name, name) == 0)
        {
            variables[i].used = 1;
            return;
        }
    }
    printf("[WARNING] Undeclared variable used: %s\n", name);
}

void check_missing_semicolon(FILE *file)
{
    char ch;
    while ((ch = fgetc(file)) != EOF && isspace(ch))
        ;
    if (ch != ';' && ch != '}' && ch != '\n')
    {
        printf("[WARNING] Missing semicolon\n");
    }
    ungetc(ch, file);
}

int is_keyword(const char *word)
{
    for (int i = 0; i < KEYWORDS_COUNT; i++)
    {
        if (strcmp(word, keywords[i]) == 0)
        {
            return 1;
        }
    }
    return 0;
}

int is_delimiter(char ch)
{
    return strchr(delimiters, ch) != NULL;
}

const Operator *check_operator(FILE *file, char first_char)
{
    char buffer[4] = {first_char, '\0', '\0', '\0'};
    long file_pos = ftell(file);
    int max_look_ahead = 2;
    int chars_read = 1;

    for (int i = 1; i <= max_look_ahead; i++)
    {
        int ch = fgetc(file);
        if (ch == EOF)
            break;
        buffer[i] = ch;
        chars_read++;
    }

    for (int len = chars_read; len > 0; len--)
    {
        buffer[len] = '\0';
        for (int j = 0; j < OPERATORS_COUNT; j++)
        {
            if (operators[j].len == len && strcmp(buffer, operators[j].op) == 0)
            {
                fseek(file, file_pos + len, SEEK_SET);
                return &operators[j];
            }
        }
    }

    fseek(file, file_pos, SEEK_SET);
    return NULL;
}

void handle_string(FILE *file, char quote)
{
    char token[MAX_TOKEN_LEN];
    int token_index = 0;
    token[token_index++] = quote;

    int ch;
    while ((ch = fgetc(file)) != EOF)
    {
        token[token_index++] = ch;

        if (ch == '\\')
        {
            ch = fgetc(file);
            if (ch != EOF)
            {
                token[token_index++] = ch;
            }
        }
        else if (ch == quote)
        {
            break;
        }
    }

    token[token_index] = '\0';
    printf("[STRING] %s\n", token);
}

void tokenize(FILE *file)
{
    int ch;
    char token[MAX_TOKEN_LEN];
    int token_index = 0;

    while ((ch = fgetc(file)) != EOF)
    {
        if (ch == '\n')
        {
            total_lines++;
        }
        if (isalpha(ch) || ch == '_')
        {
            token_index = 0;
            do
            {
                if (token_index < MAX_TOKEN_LEN - 1)
                {
                    token[token_index++] = ch;
                }
                ch = fgetc(file);
            } while (ch != EOF && (isalnum(ch) || ch == '_'));

            token[token_index] = '\0';

            if (is_keyword(token))
            {
                printf("[KEYWORD] %s\n", token);
                if (strcmp(token, "var") == 0 || strcmp(token, "let") == 0 || strcmp(token, "const") == 0)
                {
                    while ((ch = fgetc(file)) != EOF && isspace(ch))
                        ;
                    token_index = 0;
                    while (ch != EOF && (isalnum(ch) || ch == '_'))
                    {
                        token[token_index++] = ch;
                        ch = fgetc(file);
                    }
                    token[token_index] = '\0';
                    declare_variable(token, total_lines, ftell(file));
                    ungetc(ch, file);
                }
            }
            else
            {
                printf("[IDENTIFIER] %s\n", token);
                use_variable(token);
            }

            if (ch != EOF)
            {
                ungetc(ch, file);
            }
        }
        else if (isdigit(ch) || (ch == '.' && isdigit(fgetc(file))))
        {
            token_index = 0;
            if (ch == '.')
            {
                token[token_index++] = '0';
            }
            ungetc(ch, file);

            while ((ch = fgetc(file)) != EOF && (isdigit(ch) || ch == '.'))
            {
                token[token_index++] = ch;
            }
            token[token_index] = '\0';
            printf("[NUMBER] %s\n", token);
            ungetc(ch, file);
        }
        else if (ch == '"' || ch == '\'')
        {
            handle_string(file, ch);
        }
        else if (ch == '/')
        {
            ch = fgetc(file);
            if (ch == '/')
            {
                token_index = 0;
                while ((ch = fgetc(file)) != EOF && ch != '\n')
                {
                    token[token_index++] = ch;
                }
                token[token_index] = '\0';
                printf("[COMMENT] //%s\n", token);
            }
            else if (ch == '*')
            {
                token_index = 0;
                int prev_char = 0;
                while ((ch = fgetc(file)) != EOF)
                {
                    if (prev_char == '*' && ch == '/')
                    {
                        break;
                    }
                    if (token_index < MAX_TOKEN_LEN - 1)
                    {
                        token[token_index++] = prev_char;
                    }
                    prev_char = ch;
                }
                token[token_index] = '\0';
                printf("[COMMENT] /*%s*/\n", token);
            }
            else
            {
                ungetc(ch, file);
                printf("[OPERATOR] /\n");
            }
        }
        else if (is_delimiter(ch))
        {
            printf("[DELIMITER] %c\n", ch);
        }
        else if (isspace(ch))
        {
            continue;
        }
        else if (ch == '.')
        {
            printf("[OPERATOR] .\n");
            fseek(file, -1, SEEK_CUR);
        }
        else
        {
            const Operator *op = check_operator(file, ch);
            if (op)
            {
                printf("[OPERATOR] %s\n", op->op);
            }
            else if (strchr("+-*=<>&|!", ch))
            {
                printf("[OPERATOR] %c\n", ch);
            }
            else
            {
                printf("[UNKNOWN] %c\n", ch);
            }
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    FILE *file = fopen(argv[1], "r");
    if (!file)
    {
        perror("Error opening file");
        return 1;
    }

    tokenize(file);
    fclose(file);
    for (int i = 0; i < variables_count; i++)
    {
        if (!variables[i].used)
        {
            printf("[WARNING] Unused variable: %s\n", variables[i].name);
        }
    }
    // generate a json file with statistics
    FILE *json_file = fopen("stats.json", "w");
    if (!json_file)
    {
        perror("Error opening file");
        return 1;
    }

    fprintf(json_file, "{\n");
    fprintf(json_file, "  \"variables\": [\n");
    for (int i = 0; i < variables_count; i++)
    {
        fprintf(json_file, "    {\"name\": \"%s\", \"used\": %d}%s\n",
                variables[i].name,
                variables[i].used,
                (i < variables_count - 1) ? "," : "");
    }
    fprintf(json_file, "  ],\n"); // Close variables array

    fprintf(json_file, "  \"total_lines\": %d,\n", total_lines);
    fprintf(json_file, "  \"warnings\": [\n");

    int warnings_printed = 0;
    // Print regular warnings
    for (int i = 0; i < warnings_count; i++)
    {
        fprintf(json_file, "    {\"message\": \"%s\"}", warnings[i].message);
        warnings_printed++;

        // Count unused variables to determine if we need a comma
        int unused_vars = 0;
        for (int j = 0; j < variables_count; j++)
        {
            if (!variables[j].used)
                unused_vars++;
        }

        if (i < warnings_count - 1 || unused_vars > 0)
        {
            fprintf(json_file, ",");
        }
        fprintf(json_file, "\n");
    }

    // Print unused variable warnings
    for (int i = 0; i < variables_count; i++)
    {
        if (!variables[i].used)
        {
            fprintf(json_file, "    {\"message\": \"Unused variable: %s\", "
                               "\"location\": {\"line\": %d, \"column\": %d}}%s\n",
                    variables[i].name,
                    variables[i].line_number,
                    variables[i].column_number,
                    (i < variables_count - 1 && !variables[i + 1].used) ? "," : "");
        }
    }

    fprintf(json_file, "  ]\n"); // Close warnings array
    fprintf(json_file, "}\n");   // Close main object

    fclose(json_file);
    return 0;

    return 0;
}