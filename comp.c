#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#define MAX_TOKEN_LENGTH 100

typedef enum {
    TOKEN_KEYWORD,
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_OPERATOR,
    TOKEN_DELIMITER,
    TOKEN_EOF,
    TOKEN_ERROR
} TokenType;

typedef struct {
    TokenType type;
    char value[MAX_TOKEN_LENGTH];
    int line;
    int column;
} Token;

// Protótipos léxicos
int is_keyword(const char *str);
int is_operator(char c);
int is_delimiter(char c);
Token get_next_token(FILE *file);

// Listas de símbolos (palavras-chave, operadores, etc.)
const char *keywords[] = {"if", "else", "while", "for", "return", "int"};
const char operators[]   = {'+', '-', '*', '/', '=', '<', '>', '!', '&', '|'};
const char delimiters[]  = {'{', '}', '(', ')', ';', ',', ':'};

// Contadores de linha/coluna
static int current_line = 1;
static int current_column = 0;


Token current_token;
FILE *input_file;



// ESTRUTURAS E FUNÇÕES PARA ANÁLISE SEMÂNTICA

typedef struct {
    char name[MAX_TOKEN_LENGTH];
    char type[20];
    int scope_level;
} Symbol;

#define MAX_SYMBOLS 100
Symbol symbol_table[MAX_SYMBOLS];
int symbol_count = 0;
int current_scope_level = 0;

// Adiciona um símbolo à tabela, verificando redeclaração no escopo atual.
void add_symbol(const char *name, const char *type) {
    for (int i = 0; i < symbol_count; i++) {
        if (strcmp(symbol_table[i].name, name) == 0 && symbol_table[i].scope_level == current_scope_level) {
            fprintf(stderr, "\n---> Erro semântico: Redeclaração da variável '%s' (linha %d, coluna %d)\n",
                    name, current_token.line, current_token.column);
            exit(EXIT_FAILURE);
        }
    }
    if (symbol_count >= MAX_SYMBOLS) {
        fprintf(stderr, "Erro: Tabela de símbolos cheia!\n");
        exit(EXIT_FAILURE);
    }
    strcpy(symbol_table[symbol_count].name, name);
    strcpy(symbol_table[symbol_count].type, type);
    symbol_table[symbol_count].scope_level = current_scope_level;
    printf("SEMANTICO: Declarada variável '%s' (tipo: %s, escopo: %d)\n", name, type, current_scope_level);
    symbol_count++;
}

// Procura por um símbolo na tabela, do escopo mais interno para o mais externo.
Symbol* find_symbol(const char *name) {
    for (int i = symbol_count - 1; i >= 0; i--) {
        if (strcmp(symbol_table[i].name, name) == 0) {
            if (symbol_table[i].scope_level <= current_scope_level) {
                return &symbol_table[i];
            }
        }
    }
    return NULL;
}

// Gera um erro se o símbolo não for encontrado na tabela.
void check_symbol_declared(const char *name) {
    if (find_symbol(name) == NULL) {
        fprintf(stderr, "\n---> Erro semântico: Variável '%s' não declarada (linha %d, coluna %d)\n",
                name, current_token.line, current_token.column);
        exit(EXIT_FAILURE);
    }
    printf("SEMANTICO: Uso da variável '%s' validado.\n", name);
}

// Entra em um novo escopo.
void enter_scope() {
    current_scope_level++;
    printf("SEMANTICO: Entrou no escopo %d\n", current_scope_level);
}

// Sai do escopo atual, removendo seus símbolos da tabela.
void exit_scope() {
    printf("SEMANTICO: Saindo do escopo %d, removendo símbolos.\n", current_scope_level);
    int i = symbol_count - 1;
    while (i >= 0 && symbol_table[i].scope_level == current_scope_level) {
        // Opcional: imprimir qual símbolo está sendo removido para depuração
        // printf("    -> Removido '%s'\n", symbol_table[i].name);
        symbol_count--;
        i--;
    }
    current_scope_level--;
}


int getch(FILE *file) {
    int c = fgetc(file);
    if (c == '\n') {
        current_line++;
        current_column = 0;
    } else if (c != EOF) {
        current_column++;
    }
    return c;
}

void ungetch_adjust(FILE *file, int c) {
    if (c == EOF) return;
    ungetc(c, file);
    if (c == '\n') {
        current_line = (current_line > 1 ? current_line - 1 : 1);
    } else {
        current_column = (current_column > 0 ? current_column - 1 : 0);
    }
}


// FUNÇÕES DO PARSER (UTILITÁRIAS)

void next_token() {
    current_token = get_next_token(input_file);
    const char *type_str;
    switch (current_token.type) {
        case TOKEN_KEYWORD:     type_str = "KEYWORD"; break;
        case TOKEN_IDENTIFIER:  type_str = "IDENTIFIER"; break;
        case TOKEN_NUMBER:      type_str = "NUMBER"; break;
        case TOKEN_OPERATOR:    type_str = "OPERATOR"; break;
        case TOKEN_DELIMITER:   type_str = "DELIMITER"; break;
        case TOKEN_EOF:         type_str = "EOF"; break;
        case TOKEN_ERROR:       type_str = "ERROR"; break;
        default:                type_str = "UNKNOWN"; break;
    }
    if (current_token.type != TOKEN_EOF) {
        printf("Token %-10s '%s'   (linha %d, coluna %d)\n",
               type_str, current_token.value, current_token.line, current_token.column);
    } else {
        printf("Token %-10s             (linha %d, coluna %d)\n",
               type_str, current_token.line, current_token.column);
    }
}

void syntax_error(const char *message) {
    if (current_token.type == TOKEN_EOF) {
        fprintf(stderr, "\n---> Erro sintático: %s perto de <EOF> (linha %d, coluna %d)\n",
                message, current_token.line, current_token.column);
    } else {
        fprintf(stderr, "\n---> Erro sintático: %s perto de '%s' (linha %d, coluna %d)\n",
                message, current_token.value, current_token.line, current_token.column);
    }
    exit(EXIT_FAILURE);
}

int match_delimiter(char expected) {
    return (current_token.type == TOKEN_DELIMITER && current_token.value[0] == expected);
}

int match_operator(const char *op) {
    return (current_token.type == TOKEN_OPERATOR && strcmp(current_token.value, op) == 0);
}

void consume_delimiter(char expected) {
    if (match_delimiter(expected)) {
        next_token();
    } else {
        char buf[50];
        sprintf(buf, "esperando delimitador '%c'", expected);
        syntax_error(buf);
    }
}

void consume_operator(const char *op) {
    if (match_operator(op)) {
        next_token();
    } else {
        char buf[50];
        sprintf(buf, "esperando operador \"%s\"", op);
        syntax_error(buf);
    }
}

void consume_keyword(const char *kw) {
    if (current_token.type == TOKEN_KEYWORD && strcmp(current_token.value, kw) == 0) {
        next_token();
    } else {
        char buf[50];
        sprintf(buf, "esperando palavra-chave \"%s\"", kw);
        syntax_error(buf);
    }
}

// REGRAS DE PARSING (ANALISADOR SINTÁTICO)

void parse_program();
void parse_statement_list();
void parse_statement();
void parse_expression_statement();
void parse_compound_statement();
void parse_if_statement();
void parse_while_statement();
void parse_for_statement();
void parse_return_statement();
void parse_declaration();
void parse_expression();
void parse_assignment();
void parse_logical_or();
void parse_logical_and();
void parse_equality();
void parse_relational();
void parse_additive();
void parse_multiplicative();
void parse_unary();
void parse_primary();

void parse_program() {
    parse_statement_list();
    if (current_token.type != TOKEN_EOF) {
        syntax_error("Esperado EOF");
    }
}

void parse_statement_list() {
    while (current_token.type != TOKEN_EOF && !match_delimiter('}')) {
        parse_statement();
    }
}

void parse_statement() {
    if (current_token.type == TOKEN_KEYWORD && strcmp(current_token.value, "int") == 0) {
        parse_declaration();
    } else if (match_delimiter('{')) {
        parse_compound_statement();
    } else if (current_token.type == TOKEN_KEYWORD && strcmp(current_token.value, "if") == 0) {
        parse_if_statement();
    } else if (current_token.type == TOKEN_KEYWORD && strcmp(current_token.value, "while") == 0) {
        parse_while_statement();
    } else if (current_token.type == TOKEN_KEYWORD && strcmp(current_token.value, "for") == 0) {
        parse_for_statement();
    } else if (current_token.type == TOKEN_KEYWORD && strcmp(current_token.value, "return") == 0) {
        parse_return_statement();
    } else {
        parse_expression_statement();
    }
}

void parse_declaration() {
    consume_keyword("int");
    if (current_token.type != TOKEN_IDENTIFIER) {
        syntax_error("esperando identificador após 'int'");
    }
    char var_name[MAX_TOKEN_LENGTH];
    strcpy(var_name, current_token.value);
    add_symbol(var_name, "int");
    next_token();
    if (match_operator("=")) {
        next_token();
        parse_expression();
    }
    consume_delimiter(';');
}

void parse_expression_statement() {
    if (match_delimiter(';')) {
        next_token();
        return;
    }
    parse_expression();
    consume_delimiter(';');
}

void parse_compound_statement() {
    consume_delimiter('{');
    enter_scope();
    parse_statement_list();
    exit_scope();
    consume_delimiter('}');
}

void parse_if_statement() {
    consume_keyword("if");
    consume_delimiter('(');
    parse_expression();
    consume_delimiter(')');
    parse_statement();
    if (current_token.type == TOKEN_KEYWORD && strcmp(current_token.value, "else") == 0) {
        consume_keyword("else");
        parse_statement();
    }
}

void parse_while_statement() {
    consume_keyword("while");
    consume_delimiter('(');
    parse_expression();
    consume_delimiter(')');
    parse_statement();
}

void parse_for_statement() {
    consume_keyword("for");
    consume_delimiter('(');
    if (!match_delimiter(';'))
        parse_expression();
    consume_delimiter(';');
    if (!match_delimiter(';'))
        parse_expression();
    consume_delimiter(';');
    if (!match_delimiter(')'))
        parse_expression();
    consume_delimiter(')');
    parse_statement();
}

void parse_return_statement() {
    consume_keyword("return");
    if (!match_delimiter(';'))
        parse_expression();
    consume_delimiter(';');
}

void parse_expression() {
    parse_assignment();
}

void parse_assignment() {
    parse_logical_or();
    if (match_operator("=")) {
        consume_operator("=");
        parse_assignment();
    }
}

void parse_logical_or() {
    parse_logical_and();
    while (match_operator("||")) {
        consume_operator("||");
        parse_logical_and();
    }
}

void parse_logical_and() {
    parse_equality();
    while (match_operator("&&")) {
        consume_operator("&&");
        parse_equality();
    }
}

void parse_equality() {
    parse_relational();
    while (match_operator("==") || match_operator("!=")) {
        if (match_operator("==")) {
            consume_operator("==");
        } else {
            consume_operator("!=");
        }
        parse_relational();
    }
}

void parse_relational() {
    parse_additive();
    while (match_operator("<") || match_operator("<=") || match_operator(">") || match_operator(">=")) {
        if (match_operator("<=")) consume_operator("<=");
        else if (match_operator(">=")) consume_operator(">=");
        else if (match_operator("<")) consume_operator("<");
        else consume_operator(">");
        parse_additive();
    }
}

void parse_additive() {
    parse_multiplicative();
    while (match_operator("+") || match_operator("-")) {
        if (match_operator("+")) {
            consume_operator("+");
        } else {
            consume_operator("-");
        }
        parse_multiplicative();
    }
}

void parse_multiplicative() {
    parse_unary();
    while (match_operator("*") || match_operator("/")) {
        if (match_operator("*")) {
            consume_operator("*");
        } else {
            consume_operator("/");
        }
        parse_unary();
    }
}

void parse_unary() {
    if (match_operator("+") || match_operator("-") || match_operator("!")) {
        if (match_operator("+")) consume_operator("+");
        else if (match_operator("-")) consume_operator("-");
        else consume_operator("!");
        parse_unary();
    } else {
        parse_primary();
    }
}

void parse_primary() {
    if (current_token.type == TOKEN_IDENTIFIER) {
        check_symbol_declared(current_token.value);
        next_token();
    } else if (current_token.type == TOKEN_NUMBER) {
        next_token();
    } else if (match_delimiter('(')) {
        consume_delimiter('(');
        parse_expression();
        consume_delimiter(')');
    } else {
        syntax_error("Esperado identificador, número ou '('");
    }
}


// ANALISADOR LÉXICO (LEXER)

int is_keyword(const char *str) {
    for (int i = 0; i < (int)(sizeof(keywords)/sizeof(keywords[0])); i++) {
        if (strcmp(str, keywords[i]) == 0) return 1;
    }
    return 0;
}

int is_operator(char c) {
    for (int i = 0; i < (int)(sizeof(operators)/sizeof(operators[0])); i++) {
        if (c == operators[i]) return 1;
    }
    return 0;
}

int is_delimiter(char c) {
    for (int i = 0; i < (int)(sizeof(delimiters)/sizeof(delimiters[0])); i++) {
        if (c == delimiters[i]) return 1;
    }
    return 0;
}

Token get_next_token(FILE *file) {
    Token token = {TOKEN_ERROR, "", current_line, current_column};
    int c = getch(file);

    while (isspace(c)) {
        c = getch(file);
    }

    if (c == EOF) {
        token.type = TOKEN_EOF;
        token.line = current_line;
        token.column = current_column;
        token.value[0] = '\0';
        return token;
    }

    token.line = current_line;
    token.column = current_column;

    if (isalpha(c)) {
        int i = 0;
        do {
            token.value[i++] = c;
            c = getch(file);
        } while (isalnum(c) && i < MAX_TOKEN_LENGTH - 1);
        ungetch_adjust(file, c);
        token.value[i] = '\0';
        token.type = is_keyword(token.value) ? TOKEN_KEYWORD : TOKEN_IDENTIFIER;
        return token;
    } else if (isdigit(c)) {
        int i = 0;
        do {
            token.value[i++] = c;
            c = getch(file);
        } while (isdigit(c) && i < MAX_TOKEN_LENGTH - 1);
        ungetch_adjust(file, c);
        token.value[i] = '\0';
        token.type = TOKEN_NUMBER;
        return token;
    } else if (is_operator(c)) {
        int next_c = getch(file);
        char op[3] = {c, 0, 0};
        if ((c == '=' && next_c == '=') || (c == '!' && next_c == '=') ||
            (c == '<' && next_c == '=') || (c == '>' && next_c == '=') ||
            (c == '&' && next_c == '&') || (c == '|' && next_c == '|')) {
            op[1] = next_c;
        } else {
            ungetch_adjust(file, next_c);
        }
        strcpy(token.value, op);
        token.type = TOKEN_OPERATOR;
        return token;
    } else if (is_delimiter(c)) {
        token.value[0] = c;
        token.value[1] = '\0';
        token.type = TOKEN_DELIMITER;
        return token;
    } else {
        token.value[0] = c;
        token.value[1] = '\0';
        token.type = TOKEN_ERROR;
        return token;
    }
}


// FUNÇÃO PRINCIPAL

int main() {
    input_file = fopen("entrada.txt", "r");
    if (!input_file) {
        perror("Erro ao abrir arquivo de entrada (verifique se 'entrada.txt' existe)");
        return 1;
    }

    printf("--- Iniciando Análise Léxica, Sintática e Semântica ---\n\n");

    next_token();
    parse_program();

    printf("\n--- Análise concluída com sucesso! ---\n");

    fclose(input_file);
    return 0;
}