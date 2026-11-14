/*
 * sotest - scripting language interpreter for testing shared libraries.
 * Target: GNU/Linux x86, POSIX.1-2001, C99.
 *
 * Language (one line = one command):
 *   use  <so_name>    load a shared library (*.so)
 *   call <func_name>  call void func(void) from the loaded libraries
 * Extras: comments ('#', ';'), line continuation ('\'), interactive mode
 *         (run without arguments).
 *
 * Syntax error     -> warning, execution continues.
 * Execution error  -> message, script terminated (exit status 1).
 */
#define _POSIX_C_SOURCE 200112L
#include <ctype.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_LIBS 64

static void *libs[MAX_LIBS];
static char *lib_names[MAX_LIBS];
static int nlibs;

enum { R_OK_ = 0, R_SYNTAX = 1, R_ERROR = 2 };

static char *dup_str(const char *s)
{
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (!p) { perror("malloc"); exit(1); }
    memcpy(p, s, n);
    return p;
}

static int is_blank(int c) { return c == ' ' || c == '\t' || c == '\r'; }

/* Take next whitespace-delimited token from *p; returns NULL if none. */
static char *next_token(char **p)
{
    char *s = *p, *start;
    while (is_blank((unsigned char)*s)) s++;
    if (!*s) { *p = s; return NULL; }
    start = s;
    while (*s && !is_blank((unsigned char)*s)) s++;
    if (*s) *s++ = '\0';
    *p = s;
    return start;
}

static int has_extra(char *p)
{
    char *t = next_token(&p);
    return t != NULL && t[0] != '#' && t[0] != ';';
}

static int cmd_use(const char *name)
{
    struct stat st;
    char path[4096];
    void *h;
    int i;

    if (stat(name, &st) != 0) {
        fprintf(stderr, "error: library '%s': file not found\n", name);
        return R_ERROR;
    }
    if (!S_ISREG(st.st_mode)) {
        fprintf(stderr, "error: '%s' is not a shared library (not a regular file)\n", name);
        return R_ERROR;
    }
    /* dlopen() searches library paths when there is no '/', so force a path. */
    if (strchr(name, '/'))
        snprintf(path, sizeof path, "%s", name);
    else
        snprintf(path, sizeof path, "./%s", name);

    for (i = 0; i < nlibs; i++)
        if (strcmp(lib_names[i], path) == 0)
            return R_OK_;               /* already loaded */
    if (nlibs >= MAX_LIBS) {
        fprintf(stderr, "error: too many libraries loaded (max %d)\n", MAX_LIBS);
        return R_ERROR;
    }
    h = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!h) {
        fprintf(stderr, "error: '%s' is not a shared library: %s\n", name, dlerror());
        return R_ERROR;
    }
    lib_names[nlibs] = dup_str(path);
    libs[nlibs++] = h;
    return R_OK_;
}

static int cmd_call(const char *func)
{
    int i;
    union { void *obj; void (*fn)(void); } u;   /* avoid obj->fn pointer cast warning */

    if (nlibs == 0) {
        fprintf(stderr, "error: cannot call '%s': no library is loaded\n", func);
        return R_ERROR;
    }
    for (i = nlibs - 1; i >= 0; i--) {          /* most recently loaded first */
        dlerror();
        u.obj = dlsym(libs[i], func);
        if (u.obj && !dlerror()) {
            u.fn();
            fflush(stdout);
            return R_OK_;
        }
    }
    fprintf(stderr, "error: function '%s' not found in loaded libraries\n", func);
    return R_ERROR;
}

static int exec_line(char *line, const char *src, long lineno)
{
    char *p = line, *cmd, *arg;
    int rc;

    cmd = next_token(&p);
    if (!cmd || cmd[0] == '#' || cmd[0] == ';')
        return R_OK_;                            /* empty line or comment */

    if (strcmp(cmd, "use") && strcmp(cmd, "call")) {
        fprintf(stderr, "warning: %s:%ld: unknown command '%s' (ignored)\n", src, lineno, cmd);
        return R_SYNTAX;
    }
    arg = next_token(&p);
    if (!arg || arg[0] == '#' || arg[0] == ';') {
        fprintf(stderr, "warning: %s:%ld: '%s' requires an argument (ignored)\n", src, lineno, cmd);
        return R_SYNTAX;
    }
    if (has_extra(p)) {
        fprintf(stderr, "warning: %s:%ld: extra arguments after '%s %s' (ignored)\n", src, lineno, cmd, arg);
        return R_SYNTAX;
    }
    rc = (cmd[0] == 'u') ? cmd_use(arg) : cmd_call(arg);
    if (rc == R_ERROR)
        fprintf(stderr, "       at %s:%ld\n", src, lineno);
    return rc;
}

/* Read one physical line (any length) with fgets; NULL on EOF. */
static char *read_physical(FILE *f)
{
    char tmp[256], *buf = NULL;
    size_t len = 0;
    while (fgets(tmp, sizeof tmp, f)) {
        size_t n = strlen(tmp);
        buf = realloc(buf, len + n + 1);
        if (!buf) { perror("realloc"); exit(1); }
        memcpy(buf + len, tmp, n + 1);
        len += n;
        if (buf[len - 1] == '\n') break;
    }
    return buf;
}

/* Read one logical line (joins lines ending in a backslash). NULL on EOF. */
static char *read_logical(FILE *f, long *lineno)
{
    char *buf = NULL, *chunk;
    size_t len = 0, n;

    while ((chunk = read_physical(f)) != NULL) {
        (*lineno)++;
        n = strlen(chunk);
        while (n > 0 && (chunk[n - 1] == '\n' || chunk[n - 1] == '\r')) chunk[--n] = '\0';
        int cont = (n > 0 && chunk[n - 1] == 0x5C);   /* 0x5C = backslash */
        if (cont) chunk[--n] = '\0';
        buf = realloc(buf, len + n + 2);
        if (!buf) { perror("realloc"); exit(1); }
        memcpy(buf + len, chunk, n);
        len += n;
        buf[len] = '\0';
        free(chunk);
        if (!cont) return buf;
        buf[len++] = ' ';
        buf[len] = '\0';
    }
    return buf;      /* trailing continuation at EOF, or NULL */
}

static int run(FILE *f, const char *src, int interactive)
{
    long lineno = 0;
    char *line;
    for (;;) {
        if (interactive) { fputs("sotest> ", stdout); fflush(stdout); }
        line = read_logical(f, &lineno);
        if (!line) break;
        if (exec_line(line, src, lineno) == R_ERROR && !interactive) {
            free(line);
            return 1;                            /* execution error: stop script */
        }
        free(line);
    }
    return 0;
}

int main(int argc, char **argv)
{
    FILE *f;
    int rc, i;

    if (argc > 2) {
        fprintf(stderr, "usage: %s [script]\n", argv[0]);
        return 2;
    }
    if (argc == 1) {
        rc = run(stdin, "<stdin>", isatty(STDIN_FILENO));
    } else {
        f = fopen(argv[1], "r");
        if (!f) { fprintf(stderr, "error: cannot open script '%s'\n", argv[1]); return 1; }
        rc = run(f, argv[1], 0);
        fclose(f);
    }
    for (i = 0; i < nlibs; i++) { dlclose(libs[i]); free(lib_names[i]); }
    return rc;
}
