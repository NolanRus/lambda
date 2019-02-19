struct Term {
    enum TermType {
        TERM_VARIABLE,
        TERM_APPLICATION,
        TERM_ABSTRACTION
    } type;
    union {
        struct Variable {
            char *name;
        } *variable;
        struct Application {
            struct Term *function;
            struct Term *argument;
        } *application;
        struct Abstraction {
            struct Variable *parameter;
            struct Term *body;
        } *abstraction;
    };
};

extern void 
freeTerm(struct Term *term);

enum Error {
    E_OK,
    E_EOF,
    E_UNKNOWN_TOKEN,
    E_UNEXPECTED_TOKEN,
    E_NAME_TOO_LONG,
};

struct ErrorInfo {
    const char *errorMessage;
    const char *lineStart;
    size_t lineNumber;
    size_t lineOffset;
};

extern enum Error 
parse(const char *input, size_t size, struct Term **term, struct ErrorInfo *errorInfo);
