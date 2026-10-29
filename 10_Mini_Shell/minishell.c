/*
 * ============================================================
 *  MINI SHELL  -  C Mastery Level
 * ============================================================
 *  Your very own terminal inside the terminal!
 *
 *  Features:
 *    - Runs any program:         ls -l, gcc file.c, python3 ...
 *    - Built-ins:                cd, help, history, !!, !N, exit
 *    - Pipes:                    ls | sort | head -5
 *    - Redirection:              sort < in.txt > out.txt   (also >>)
 *    - Quotes:                   echo "hello   world"
 *    - Prompt turns red when the last command failed
 *
 *  Concepts used:
 *    - Processes: fork(), execvp(), waitpid()
 *    - Pipes and file descriptors: pipe(), dup2(), open()
 *    - Tokenizer / parser written by hand
 *    - Signals
 *
 *  On Linux and Mac the full shell is built (pipes, redirects).
 *  On Windows only the basics work (cd, history, exit; other
 *  commands are passed to the system), because fork() does not
 *  exist there. Use WSL for the complete version.
 *
 *  Compile:   gcc minishell.c -o minishell
 *  Run:       ./minishell
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
    #include <direct.h>
    #define getcwd _getcwd
    #define chdir  _chdir
#else
    #include <unistd.h>
    #include <sys/wait.h>
    #include <fcntl.h>
    #include <signal.h>
    #include <errno.h>
#endif

#define MAX_LINE    1024
#define MAX_TOKENS  128
#define MAX_ARGS    64
#define MAX_CMDS    16
#define MAX_HISTORY 100

static char *history[MAX_HISTORY];
static int   hist_count = 0;
static int   last_status = 0;

/* ======================= history ======================= */

static char *copy_string(const char *s)
{
    char *p = malloc(strlen(s) + 1);
    if (!p) { fprintf(stderr, "out of memory\n"); exit(1); }
    strcpy(p, s);
    return p;
}

static void add_history(const char *line)
{
    if (hist_count > 0 && strcmp(history[hist_count - 1], line) == 0) return;
    if (hist_count == MAX_HISTORY) {                      /* drop the oldest */
        free(history[0]);
        memmove(history, history + 1, (MAX_HISTORY - 1) * sizeof(char *));
        hist_count--;
    }
    history[hist_count++] = copy_string(line);
}

/* ======================= tokenizer ======================= */

typedef struct {
    char *text;
    int   op;       /* 0 = word, otherwise one of: '|' '<' '>' 'A' (A means >>) */
} Token;

/* Splits a line into words and operators, honouring "double" and 'single' quotes. */
static int tokenize(const char *line, Token *tok, int max, char *pool, size_t pool_size)
{
    int n = 0;
    char *p = pool;
    char *end = pool + pool_size;

    while (*line) {
        while (*line == ' ' || *line == '\t') line++;
        if (!*line) break;
        if (n >= max - 1 || p + 4 >= end) return -1;

        if (*line == '|' || *line == '<' || *line == '>') {
            tok[n].text = p;
            tok[n].op = *line;
            *p++ = *line++;
            if (tok[n].op == '>' && *line == '>') { tok[n].op = 'A'; *p++ = *line++; }
            *p++ = '\0';
            n++;
            continue;
        }

        tok[n].text = p;
        tok[n].op = 0;
        while (*line && *line != ' ' && *line != '\t' &&
               *line != '|' && *line != '<' && *line != '>') {
            if (*line == '"' || *line == '\'') {
                char q = *line++;
                while (*line && *line != q) {
                    if (p + 2 >= end) return -1;
                    *p++ = *line++;
                }
                if (*line != q) { fprintf(stderr, "minishell: unmatched quote\n"); return -2; }
                line++;
            } else {
                if (p + 2 >= end) return -1;
                *p++ = *line++;
            }
        }
        *p++ = '\0';
        n++;
    }
    return n;
}

/* ======================= parser ======================= */

typedef struct {
    char *argv[MAX_ARGS + 1];
    int   argc;
    char *in_file;
    char *out_file;
    int   append;
} Command;

static int parse(Token *tok, int n, Command *cmds)
{
    int nc = 0;
    memset(&cmds[0], 0, sizeof cmds[0]);

    for (int i = 0; i < n; i++) {
        Command *c = &cmds[nc];
        if (tok[i].op == 0) {
            if (c->argc >= MAX_ARGS) { fprintf(stderr, "minishell: too many arguments\n"); return -1; }
            c->argv[c->argc++] = tok[i].text;
            c->argv[c->argc] = NULL;
        } else if (tok[i].op == '|') {
            if (c->argc == 0) { fprintf(stderr, "minishell: syntax error near '|'\n"); return -1; }
            if (++nc >= MAX_CMDS) { fprintf(stderr, "minishell: too many pipes\n"); return -1; }
            memset(&cmds[nc], 0, sizeof cmds[nc]);
        } else {                                           /* < > >> need a file name */
            if (i + 1 >= n || tok[i + 1].op != 0) {
                fprintf(stderr, "minishell: syntax error: expected a file name\n");
                return -1;
            }
            if (tok[i].op == '<') c->in_file = tok[i + 1].text;
            else { c->out_file = tok[i + 1].text; c->append = (tok[i].op == 'A'); }
            i++;
        }
    }
    if (cmds[nc].argc == 0) {
        if (nc > 0) fprintf(stderr, "minishell: syntax error: missing command after '|'\n");
        return nc > 0 ? -1 : 0;
    }
    return nc + 1;
}

/* ======================= built-in commands ======================= */

static void print_help(void)
{
    printf("\nMINI SHELL - built-in commands:\n");
    printf("  cd [dir]     change directory (no argument = home)\n");
    printf("  history      show the commands you typed\n");
    printf("  !!           run the previous command again\n");
    printf("  !N           run command number N from history\n");
    printf("  help         show this message\n");
    printf("  exit [code]  leave the shell\n");
    printf("\nEverything else runs as a normal program. Pipes (|) and\n");
    printf("redirection (<  >  >>) work too, e.g.:  ls | sort > out.txt\n\n");
}

/* Returns 1 if the command was a built-in (and sets last_status) */
static int run_builtin(Command *c)
{
    const char *name = c->argv[0];

    if (strcmp(name, "exit") == 0) {
        int code = c->argc > 1 ? atoi(c->argv[1]) : 0;
        for (int i = 0; i < hist_count; i++) free(history[i]);
        exit(code);
    }
    if (strcmp(name, "cd") == 0) {
        const char *dir = c->argc > 1 ? c->argv[1] : getenv("HOME");
#ifdef _WIN32
        if (!dir) dir = getenv("USERPROFILE");
#endif
        if (!dir || chdir(dir) != 0) { perror("cd"); last_status = 1; }
        else last_status = 0;
        return 1;
    }
    if (strcmp(name, "help") == 0) { print_help(); last_status = 0; return 1; }
    if (strcmp(name, "history") == 0) {
        for (int i = 0; i < hist_count; i++) printf("%4d  %s\n", i + 1, history[i]);
        last_status = 0;
        return 1;
    }
    return 0;
}

/* ======================= running commands ======================= */
#ifndef _WIN32

static void run_pipeline(Command *cmds, int n)
{
    pid_t pids[MAX_CMDS];
    int prev_read = -1;

    for (int i = 0; i < n; i++) {
        int fd[2] = {-1, -1};
        if (i < n - 1 && pipe(fd) < 0) { perror("pipe"); break; }

        pid_t pid = fork();
        if (pid < 0) { perror("fork"); break; }

        if (pid == 0) {                                    /* ---- child process ---- */
            signal(SIGINT, SIG_DFL);                       /* Ctrl+C should kill the child */

            if (prev_read != -1) { dup2(prev_read, STDIN_FILENO);  close(prev_read); }
            if (fd[1] != -1)     { dup2(fd[1], STDOUT_FILENO);     close(fd[0]); close(fd[1]); }

            if (cmds[i].in_file) {
                int f = open(cmds[i].in_file, O_RDONLY);
                if (f < 0) { perror(cmds[i].in_file); _exit(1); }
                dup2(f, STDIN_FILENO);
                close(f);
            }
            if (cmds[i].out_file) {
                int flags = O_WRONLY | O_CREAT | (cmds[i].append ? O_APPEND : O_TRUNC);
                int f = open(cmds[i].out_file, flags, 0644);
                if (f < 0) { perror(cmds[i].out_file); _exit(1); }
                dup2(f, STDOUT_FILENO);
                close(f);
            }

            execvp(cmds[i].argv[0], cmds[i].argv);
            fprintf(stderr, "minishell: %s: %s\n", cmds[i].argv[0],
                    errno == ENOENT ? "command not found" : strerror(errno));
            _exit(127);
        }

        pids[i] = pid;                                     /* ---- parent process ---- */
        if (prev_read != -1) close(prev_read);
        if (fd[1] != -1)     close(fd[1]);
        prev_read = fd[0];
    }
    if (prev_read != -1) close(prev_read);

    for (int i = 0; i < n; i++) {
        int status = 0;
        if (pids[i] > 0 && waitpid(pids[i], &status, 0) > 0 && i == n - 1) {
            if (WIFEXITED(status))        last_status = WEXITSTATUS(status);
            else if (WIFSIGNALED(status)) last_status = 128 + WTERMSIG(status);
        }
    }
}

#endif

static void show_prompt(void)
{
    char cwd[512] = "?";
    if (!getcwd(cwd, sizeof cwd)) strcpy(cwd, "?");

    const char *home = getenv("HOME");
    char shown[560];
    if (home && home[0] && strncmp(cwd, home, strlen(home)) == 0)
        snprintf(shown, sizeof shown, "~%s", cwd + strlen(home));
    else
        snprintf(shown, sizeof shown, "%s", cwd);

    /* green name when the last command worked, red when it failed */
    printf("\033[1;%dmminishell\033[0m:\033[1;34m%s\033[0m$ ", last_status == 0 ? 32 : 31, shown);
    fflush(stdout);
}

int main(void)
{
    char line[MAX_LINE];

#ifdef _WIN32
    printf("(Windows mode: only basic features. Use WSL for pipes and redirection.)\n");
#else
    signal(SIGINT, SIG_IGN);            /* Ctrl+C must not kill the shell itself */
#endif
    printf("Welcome to MINI SHELL. Type 'help' for info, 'exit' to quit.\n");

    for (;;) {
        show_prompt();
        if (!fgets(line, sizeof line, stdin)) { printf("\nexit\n"); break; }
        line[strcspn(line, "\r\n")] = '\0';

        /* skip blank lines */
        char *s = line;
        while (isspace((unsigned char)*s)) s++;
        if (*s == '\0') continue;

        /* history expansion: !! and !N */
        char expanded[MAX_LINE];
        if (s[0] == '!' && (s[1] == '!' || isdigit((unsigned char)s[1]))) {
            int idx = (s[1] == '!') ? hist_count : atoi(s + 1);
            if (idx < 1 || idx > hist_count) { fprintf(stderr, "minishell: no such history entry\n"); continue; }
            snprintf(expanded, sizeof expanded, "%s", history[idx - 1]);
            printf("%s\n", expanded);
            s = expanded;
        }
        add_history(s);

#ifdef _WIN32
        {
            Token tok[MAX_TOKENS];
            char pool[MAX_LINE * 2];
            Command cmd;
            int nt = tokenize(s, tok, MAX_TOKENS, pool, sizeof pool);
            if (nt > 0 && parse(tok, nt, &cmd) == 1 && run_builtin(&cmd)) continue;
            last_status = system(s);
        }
#else
        Token tok[MAX_TOKENS];
        Command cmds[MAX_CMDS];
        char pool[MAX_LINE * 2];

        int nt = tokenize(s, tok, MAX_TOKENS, pool, sizeof pool);
        if (nt == -1) { fprintf(stderr, "minishell: line too long\n"); continue; }
        if (nt <= 0) continue;

        int nc = parse(tok, nt, cmds);
        if (nc <= 0) { if (nc < 0) last_status = 2; continue; }

        if (nc == 1 && run_builtin(&cmds[0])) continue;
        run_pipeline(cmds, nc);
#endif
    }

    for (int i = 0; i < hist_count; i++) free(history[i]);
    return 0;
}
