#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdbool.h>

#include "parser.h"

#define MAX_NAME_LEN 63

struct Variable *
makeVariable(char *name) {
    struct Variable *variable = malloc(sizeof(struct Variable));
    variable->name = name;
    return variable;
}

static struct Term *
makeTermVariable(char *name) {
    struct Term *term = malloc(sizeof(struct Term));
    *term = (struct Term) {
        .type = TERM_VARIABLE,
        .variable = makeVariable(name),
    };
    return term;
}

static struct Term *
makeTermApplication(struct Term *function, struct Term *argument) {
    struct Term *term = malloc(sizeof(struct Term));
    *term = (struct Term) {
        .type = TERM_APPLICATION,
        .application = malloc(sizeof(struct Application)),
    };
    *term->application = (struct Application) {
        .function = function,
        .argument = argument,
    };
    return term;
}

static struct Term *
makeTermAbstraction(char *name, struct Term *body) {
    struct Term *term = malloc(sizeof(struct Term));
    *term = (struct Term) {
        .type = TERM_ABSTRACTION,
        .abstraction = malloc(sizeof(struct Abstraction)),
    };
    *term->abstraction = (struct Abstraction) {
        .parameter = makeVariable(name),
        .body = body,
    };
    return term;
}

extern void 
freeTerm(struct Term *term) {
    if (!term) return;
    switch (term->type) {
    case TERM_VARIABLE:
        if (term->variable) free(term->variable->name);
        free(term->variable);
        break;
    case TERM_APPLICATION:
        if (term->application) { 
            freeTerm(term->application->function);
            freeTerm(term->application->argument);
        }
        free(term->application);
        break;
    case TERM_ABSTRACTION:
        if (term->abstraction && term->abstraction->parameter) {
            free(term->abstraction->parameter->name);
        }
        free(term->abstraction->parameter);
        freeTerm(term->abstraction->body);
        break;
    }
    free(term);
}

enum Token {
    T_LEFT_PAREN,         // (
    T_RIGHT_PAREN,        // )
    T_VARIABLE,           // [a-zA-Z]+
    T_POINT,              // .
    T_BACK_SLASH,         // '\'
    T_EOF,                // end of file
};

struct ParserState {
    const char *input;
    size_t offset;
    size_t size;
    enum Token token;
    char *variable;
    size_t lineNumber;
    size_t lineOffset;
    const char *lineStart;
    const char *errorMessage;
    _Bool newLine;
};

static enum Error 
peekChar(struct ParserState *parser, char *c) {
    if (parser->offset >= parser->size) {
        return E_EOF;
    }
    *c = parser->input[parser->offset];
    return *c ? E_OK : E_EOF;
}

static void 
proceed(struct ParserState *parser) {
    enum Error e;
    char c;
    if (parser->offset >= parser->size) {
        return;
    }
    ++parser->offset;
    e = peekChar(parser, &c);
    if (e) return;
    if (parser->newLine) {
        parser->newLine = false;
        ++parser->lineNumber;
        parser->lineOffset = 0;
        parser->lineStart = &parser->input[parser->offset];
    } else {
        ++parser->lineOffset;
    }
    if (c == '\n') {
        parser->newLine = true;
    }
}

// Return value will never be E_EOF
static enum Error 
nextToken(struct ParserState *parser) {
    enum Error e;
    char c;
    e = peekChar(parser, &c);
    while (!e && isspace(c)) {
        proceed(parser);
        e = peekChar(parser, &c);
    }
    if (e) {
        if (e == E_EOF) {
            parser->token = T_EOF;
            return E_OK;
        }
        return e;
    }
    static struct {
        char c;
        enum Token t;
    } singleLetter[] = {
        { '(', T_LEFT_PAREN },
        { ')', T_RIGHT_PAREN },
        { '.', T_POINT },
        { '\\', T_BACK_SLASH },
    };
    size_t n = sizeof(singleLetter) / sizeof(*singleLetter);
    for (size_t i = 0; i < n; ++i) {
        if (c == singleLetter[i].c) {
            parser->token = singleLetter[i].t;
            proceed(parser);
            return E_OK;
        }
    }
    if (('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z')) {
        char *variable = malloc(MAX_NAME_LEN + 1);
        variable[0] = c;
        size_t i;
        for (i = 1; i < MAX_NAME_LEN; ++i) {
            proceed(parser);
            e = peekChar(parser, &c);
            if (e && e != E_EOF) {
                return e;
            }
            if (e == E_EOF || !(('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z'))) {
                break;
            }
            variable[i] = c;
        }
        variable[i] = '\0';
        parser->token = T_VARIABLE;
        parser->variable = variable;
        if (i == MAX_NAME_LEN) {
            return E_NAME_TOO_LONG;
        }
        return E_OK;
    }
    return E_UNKNOWN_TOKEN;
}

/*
 * Grammar:
 * T -> \ vars . T
 * T -> ( T ) T'
 * T -> var T'
 * T' -> T
 * T' -> <eps>
 */

static enum Error 
parseAbstraction(struct ParserState *parser, struct Term **result);

static enum Error 
parseRightOfApplication(struct ParserState *parser, struct Term *left, struct Term **result);

static enum Error 
parseTerm(struct ParserState *parser, struct Term **term);

static enum Error 
parseAbstraction(struct ParserState *parser, struct Term **result) {
    enum Error e;
    if (T_BACK_SLASH != parser->token) {
        parser->errorMessage = "Expected '\\'.";
        return E_UNEXPECTED_TOKEN;
    }
    e = nextToken(parser);
    if (e) return e;
    if (T_VARIABLE != parser->token) {
        parser->errorMessage = "Expected variable.";
        return E_UNEXPECTED_TOKEN;
    }
    struct Term *term = NULL;
    struct Term **termPtr = &term;
    while (T_VARIABLE == parser->token) {
        *termPtr = makeTermAbstraction(parser->variable, NULL);
        termPtr = &(*termPtr)->abstraction->body;
        e = nextToken(parser);
        if (e) break;
    }
    if (T_POINT != parser->token) {
        freeTerm(term);
        parser->errorMessage = "Expected '.' or variable.";
        return E_UNEXPECTED_TOKEN;
    }
    e = nextToken(parser);
    if (e) {
        freeTerm(term);
        return e;
    }
    e = parseTerm(parser, termPtr);
    if (e) {
        freeTerm(term);
        return e;
    }
    *result = term;
    return E_OK;
}

static enum Error 
parseRightOfApplication(struct ParserState *parser, struct Term *function, struct Term **result) {
    enum Error e;
    if (T_BACK_SLASH == parser->token) {
        struct Term *argument = NULL;
        e = parseAbstraction(parser, &argument);
        if (e) return e;
        *result = makeTermApplication(function, argument);
        return E_OK;
    } else if (T_LEFT_PAREN == parser->token) {
        e = nextToken(parser);
        if (e) return e;
        struct Term *argument;
        e = parseTerm(parser, &argument);
        if (e) return e;
        if (T_RIGHT_PAREN != parser->token) {
            parser->errorMessage = "Expected ).";
            return E_UNEXPECTED_TOKEN;
        }
        e = nextToken(parser);
        if (e) return e;
        struct Term *term = makeTermApplication(function, argument);
        e = parseRightOfApplication(parser, term, result);
        if (e) {
            freeTerm(term);
        }
        return e;
    } else if (T_VARIABLE == parser->token) {
        struct Term *argument = makeTermVariable(parser->variable);
        struct Term *term = makeTermApplication(function, argument);
        e = nextToken(parser);
        if (e) return e;
        e = parseRightOfApplication(parser, term, result);
        if (e) {
            freeTerm(term);
        }
        return e;
    } else {
        *result = function;
        return E_OK;
    }
}

static enum Error 
parseTerm(struct ParserState *parser, struct Term **term) {
    enum Error e;
    if (T_BACK_SLASH == parser->token) {
        return parseAbstraction(parser, term);
    } else if (T_LEFT_PAREN == parser->token) {
        e = nextToken(parser);
        if (e) return e;
        struct Term *left;
        e = parseTerm(parser, &left);
        if (e) return e;
        if (T_RIGHT_PAREN != parser->token) {
            parser->errorMessage = "Expected ')'.";
            return E_UNEXPECTED_TOKEN;
        }
        e = nextToken(parser);
        if (e) {
            freeTerm(left);
            return e;
        }
        e = parseRightOfApplication(parser, left, term);
        if (e) {
            freeTerm(left);
        }
        return e;
    } else if (T_VARIABLE == parser->token) {
        struct Term *left = makeTermVariable(parser->variable);
        e = nextToken(parser);
        if (e) {
            freeTerm(left);
            return e;
        }
        e = parseRightOfApplication(parser, left, term);
        if (e) {
            freeTerm(left);
        }
        return e;
    }
    parser->errorMessage = "Expected one of ['\\', '(', variable].";
    return E_UNEXPECTED_TOKEN;
}

extern enum Error 
parse(const char *input, size_t size, struct Term **term, struct ErrorInfo *errorInfo) {
    enum Error e;
    struct ParserState parser = (struct ParserState) {
        .input = input,
        .offset = 0,
        .size = size,
        .token = T_EOF,
        .variable = NULL,
        .lineNumber = 1,
        .lineOffset = 0,
        .lineStart = NULL,
        .errorMessage = "",
        .newLine = false,
    };
    e = nextToken(&parser);
    if (e) goto error;
    e = parseTerm(&parser, term);
    if (e) goto error;
    return E_OK;
error:
    if (errorInfo) {
        *errorInfo = (struct ErrorInfo) {
            .lineStart = parser.lineStart,
            .lineNumber = parser.lineNumber,
            .lineOffset = parser.lineOffset,
            .errorMessage = parser.errorMessage,
        };
    }
    return e;
}
