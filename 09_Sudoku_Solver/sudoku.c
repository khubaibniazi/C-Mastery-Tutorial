/*
 * ============================================================
 *  SUDOKU SOLVER  -  C Mastery Level
 * ============================================================
 *  Watch the computer solve a Sudoku using BACKTRACKING:
 *  try a number, move on, and if you get stuck, undo it and
 *  try the next one.  Every try and every undo is animated.
 *
 *  Concepts used:
 *    - Recursion and backtracking
 *    - 2D arrays
 *    - Validation (row / column / 3x3 box)
 *    - ANSI colors and animation
 *
 *  You can pick a built-in puzzle or type your own as 81
 *  characters (digits 1-9, and 0 or . for empty cells).
 *
 *  Compile:   gcc sudoku.c -o sudoku
 *  Run:       ./sudoku           (Windows: sudoku.exe)
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#ifdef _WIN32
    #include <windows.h>
    #define sleep_ms(ms) Sleep(ms)
#else
    #include <unistd.h>
    #define sleep_ms(ms) usleep((ms) * 1000)
#endif

static int  grid[9][9];
static int  given[9][9];           /* 1 = this number was part of the puzzle */
static long steps = 0;             /* total placements tried                 */
static int  draw_every = 0;        /* 0 = never draw, N = draw every N steps */
static int  delay = 0;             /* milliseconds to wait after drawing     */

static const char *PUZZLES[3] = {
    "530070000600195000098000060800060003400803001700020006060000280000419005000080079",  /* easy   */
    "003020600900305001001806400008102900700000008006708200002609500800203009005010300",  /* medium */
    "400000805030000000000700000020000060000080400000010000000603070500200000104000000"   /* hard   */
};

static void enable_ansi(void)
{
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(h, &mode)) SetConsoleMode(h, mode | 0x0004);
#endif
}

static void draw(int cr, int cc, const char *status)
{
    printf("\033[H");
    printf("  SUDOKU SOLVER   |   steps: %ld\033[K\n\n", steps);
    for (int r = 0; r < 9; r++) {
        if (r % 3 == 0) printf("  +-------+-------+-------+\n");
        printf("  ");
        for (int c = 0; c < 9; c++) {
            if (c % 3 == 0) printf("| ");
            int v = grid[r][c];
            if (v == 0)                      printf("\033[2m.\033[0m ");
            else if (r == cr && c == cc)     printf("\033[1;41m%d\033[0m ", v);   /* being tried */
            else if (given[r][c])            printf("\033[1;97m%d\033[0m ", v);  /* from puzzle */
            else                             printf("\033[32m%d\033[0m ", v);    /* solved by us */
        }
        printf("|\n");
    }
    printf("  +-------+-------+-------+\n\n  %s\033[K\n", status);
    fflush(stdout);
}

/* Can value v go at (r, c) without breaking a Sudoku rule? */
static int is_valid(int r, int c, int v)
{
    for (int i = 0; i < 9; i++) {
        if (grid[r][i] == v) return 0;                     /* same row    */
        if (grid[i][c] == v) return 0;                     /* same column */
    }
    int br = (r / 3) * 3, bc = (c / 3) * 3;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            if (grid[br + i][bc + j] == v) return 0;       /* same 3x3 box */
    return 1;
}

/* Fill cells in order 0..80. Returns 1 when the whole grid is solved. */
static int solve(int pos)
{
    while (pos < 81 && grid[pos / 9][pos % 9] != 0) pos++;  /* skip filled cells */
    if (pos == 81) return 1;

    int r = pos / 9, c = pos % 9;
    for (int v = 1; v <= 9; v++) {
        if (!is_valid(r, c, v)) continue;

        grid[r][c] = v;                                     /* try it */
        steps++;
        if (draw_every && steps % draw_every == 0) {
            draw(r, c, "Solving...");
            if (delay) sleep_ms(delay);
        }

        if (solve(pos + 1)) return 1;                       /* it worked! */

        grid[r][c] = 0;                                     /* BACKTRACK: undo */
        steps++;
    }
    return 0;
}

static int load_puzzle(const char *s)
{
    if (strlen(s) != 81) return 0;
    for (int i = 0; i < 81; i++) {
        char ch = s[i];
        int v;
        if (ch == '.' || ch == '0') v = 0;
        else if (ch >= '1' && ch <= '9') v = ch - '0';
        else return 0;
        grid[i / 9][i % 9] = v;
        given[i / 9][i % 9] = (v != 0);
    }
    /* make sure the starting puzzle does not already break the rules */
    for (int r = 0; r < 9; r++)
        for (int c = 0; c < 9; c++) {
            int v = grid[r][c];
            if (v == 0) continue;
            grid[r][c] = 0;
            int ok = is_valid(r, c, v);
            grid[r][c] = v;
            if (!ok) return 0;
        }
    return 1;
}

int main(void)
{
    char line[256];
    enable_ansi();

    printf("=====================================\n");
    printf("          SUDOKU SOLVER\n");
    printf("=====================================\n");
    printf(" 1) Easy puzzle\n 2) Medium puzzle\n 3) Hard puzzle (about 19 million steps - choose Instant speed!)\n 4) Type my own puzzle\n");
    printf("Choice: ");
    if (!fgets(line, sizeof line, stdin)) return 0;
    int choice = atoi(line);

    const char *puzzle;
    char own[256];
    if (choice >= 1 && choice <= 3) {
        puzzle = PUZZLES[choice - 1];
    } else {
        printf("Enter 81 characters (1-9, and 0 or . for empty cells):\n> ");
        if (!fgets(own, sizeof own, stdin)) return 0;
        own[strcspn(own, "\r\n")] = '\0';
        puzzle = own;
    }

    if (!load_puzzle(puzzle)) {
        printf("That puzzle is invalid (it needs exactly 81 valid characters with no repeats).\n");
        return 1;
    }

    printf("\nSpeed:  1) Slow (watch every step)   2) Fast   3) Instant\nChoice: ");
    if (!fgets(line, sizeof line, stdin)) return 0;
    int speed = atoi(line);
    if (speed == 1)      { draw_every = 1;  delay = 40; }
    else if (speed == 2) { draw_every = 500; delay = 0; }
    else                 { draw_every = 0;  delay = 0;  }

    printf("\033[2J");
    draw(-1, -1, "Starting...");

    clock_t t0 = clock();
    int ok = solve(0);
    double secs = (double)(clock() - t0) / CLOCKS_PER_SEC;

    char msg[128];
    if (ok) snprintf(msg, sizeof msg, "SOLVED in %ld steps (%.2f seconds).", steps, secs);
    else    snprintf(msg, sizeof msg, "This puzzle has no solution.");
    draw(-1, -1, msg);
    return 0;
}
