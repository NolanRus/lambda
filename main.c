#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdbool.h>

#include "parser.h"

#define MAX_FILE_LEN 1024

struct Buffer {
    size_t size;
    size_t filled;
    char *data;
};

static size_t bytesLeft(struct Buffer *buffer) {
    return buffer->size - buffer->filled - 1;
}

static char *currentPosition(struct Buffer *buffer) {
    return buffer->data + buffer->filled;
}

static struct Buffer *makeBuffer(size_t size) {
    struct Buffer *buffer = malloc(sizeof(struct Buffer));
    *buffer = (struct Buffer) { 
        .size = size, 
        .filled = 0,
        .data = malloc(size),
    };
    return buffer;
}

static void freeBuffer(struct Buffer *buffer) {
    if (!buffer) return;
    free(buffer->data);
    free(buffer);
}

static void append(struct Buffer *buffer, const char *data, size_t size) {
    size_t left = bytesLeft(buffer);
    size_t count = size < left ? size : left;
    memcpy(currentPosition(buffer), data, count);
    buffer->filled += count;
}

static void appendStr(struct Buffer *buffer, const char *str) {
    append(buffer, str, strlen(str));
}

static void 
printVariable(struct Variable *variable, struct Buffer *buffer) {
    if (!variable || !variable->name) {
        appendStr(buffer, "NULL");
    } else {
        appendStr(buffer, variable->name);
    }
}

static _Bool
isAbstraction(struct Term *term) {
    return term && term->type == TERM_ABSTRACTION;
}

static void 
printTermHelper(struct Term *term, struct Buffer *buffer, _Bool withParen) {
    if (!term) goto itsnull;
    switch (term->type) {
    case TERM_VARIABLE:
        printVariable(term->variable, buffer);
        break;
    case TERM_APPLICATION:
        if (!term->application) goto itsnull;
        if (withParen) appendStr(buffer, "(");
        printTermHelper(term->application->function, buffer, 
                isAbstraction(term->application->function));
        appendStr(buffer, " ");
        printTermHelper(term->application->argument, buffer, true);
        if (withParen) appendStr(buffer, ")");
        break;
    case TERM_ABSTRACTION:
        if (!term->abstraction) goto itsnull;
        if (withParen) appendStr(buffer, "(");
        appendStr(buffer, "\\");
        while (TERM_ABSTRACTION == term->type && term->abstraction) {
            printVariable(term->abstraction->parameter, buffer);
            appendStr(buffer, " ");
            term = term->abstraction->body;
        }
        appendStr(buffer, ". ");
        if (TERM_ABSTRACTION == term->type && !term->abstraction) {
            appendStr(buffer, "NULL");
        } else {
            printTermHelper(term, buffer, false);
        }
        if (withParen) appendStr(buffer, ")");
    }
    return;
itsnull:
    appendStr(buffer, "NULL");
}

static void 
printTerm(struct Term *term, struct Buffer *buffer) {
    printTermHelper(term, buffer, false);
}

static const char *terms[] = {
    "x",
    "x x",
    "x x x",
    "x (x x)",
    "(\\x . x) x",
    "x (\\x . x)",
    "x x (x x) x",
    "\\x y . y (x x)",
    "\\x y z . x (y z)",
    "\\x . x x (x x)",
    "x x (\\x . x (\\x . x)) x",
    "(\\x . x x) (\\y z . z x y) (x x x)"
};

static void check(const char *input) {
    struct Term *term;
    struct ErrorInfo errorInfo;
    enum Error error = parse(input, strlen(input), &term, &errorInfo);
    if (error) {
        fprintf(stderr, "Error (%d)\n", error);
        fprintf(stderr, "%d: %s\n", 
                (int) errorInfo.lineNumber, 
                errorInfo.lineStart);
        exit(1);
    } 
    struct Buffer *buffer = makeBuffer(100);
    printTerm(term, buffer);
    buffer->data[buffer->filled] = '\0';
    if (strcmp(buffer->data, input)) {
        fprintf(stderr, "Error in test\nexpected: %s\n     got: %s\n", input, buffer->data);
        exit(1);
    }
    freeBuffer(buffer);
}

void printUsage(char *program) {
    fprintf(stderr, "Usage: %s test\n", program);
    exit(1);
}

int main(int argc, char *argv[]) {
    if (argc != 2) printUsage(argv[0]);
    if (!strcmp(argv[1], "test")) {
        size_t n = sizeof(terms) / sizeof(terms[0]);
        for (size_t i = 0; i < n; ++i) {
            check(terms[i]);
        }
        fprintf(stderr, "ALL TESTS PASSED\n");
    } else if (!strcmp(argv[1], "parse")) {
        struct Term *term;
        struct ErrorInfo errorInfo;
        char input[512];
        if (!fgets(input, sizeof(input), stdin)) exit(1);
        enum Error error = parse(input, strlen(input), &term, &errorInfo);
        if (error) {
            fprintf(stderr, "Error: %s\n", errorInfo.errorMessage);
            fprintf(stderr, "%s", errorInfo.lineStart);
            for (size_t i = 0; i < errorInfo.lineOffset - 3; ++i) {
                fputc(' ', stderr);
            }
            fputs("~~~^~~~\n", stderr);
            exit(1);
        } 
    } else printUsage(argv[0]);
}
