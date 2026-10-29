/*
 * ============================================================
 *  TIC-TAC-TOE vs UNBEATABLE AI  -  C Mastery Level
 * ============================================================
 *  Concepts used:
 *    - Recursion
 *    - Minimax algorithm (the AI tries every possible future)
 *    - Alpha-beta pruning (skips branches that cannot matter)
 *    - Arrays, const tables, input validation
 *
 *  The AI plays 'O'. You play 'X'. You can draw, but you
 *  can never win. Try it!
 *
 *  Compile:   gcc tictactoe.c -o tictactoe
 *  Run:       ./tictactoe        (Windows: tictactoe.exe)
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#ifdef _WIN32
    #include <windows.h>
#endif

#define HUMAN 'X'
#define AI    'O'

static long nodes = 0;                 /* how many positions the AI looked at */
static int wins = 0, draws = 0, losses = 0;

/* The 8 winning lines: 3 rows, 3 columns, 2 diagonals */
static const int LINES[8][3] = {
    {0, 1, 2}, {3, 4, 5}, {6, 7, 8},
    {0, 3, 6}, {1, 4, 7}, {2, 5, 8},
    {0, 4, 8}, {2, 4, 6}
};

static void enable_ansi(void)
{
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(h, &mode)) SetConsoleMode(h, mode | 0x0004);
#endif
}

static char check_winner(const char b[9])
{
    for (int i = 0; i < 8; i++) {
        char a = b[LINES[i][0]];
        if (a != ' ' && a == b[LINES[i][1]] && a == b[LINES[i][2]])
            return a;
    }
    return 0;
}

static int board_full(const char b[9])
{
    for (int i = 0; i < 9; i++)
        if (b[i] == ' ') return 0;
    return 1;
}

/*
 * MINIMAX
 * Scores a position from the AI's point of view:
 *   positive = good for AI, negative = good for human, 0 = draw.
 * 'depth' makes the AI prefer faster wins and slower losses.
 * alpha/beta = alpha-beta pruning bounds.
 */
static int minimax(char b[9], int ai_turn, int depth, int alpha, int beta)
{
    nodes++;

    char w = check_winner(b);
    if (w == AI)    return 10 - depth;
    if (w == HUMAN) return depth - 10;
    if (board_full(b)) return 0;

    if (ai_turn) {                               /* AI wants the HIGHEST score */
        int best = -1000;
        for (int i = 0; i < 9; i++) {
            if (b[i] != ' ') continue;
            b[i] = AI;
            int score = minimax(b, 0, depth + 1, alpha, beta);
            b[i] = ' ';                          /* undo the move */
            if (score > best) best = score;
            if (best > alpha) alpha = best;
            if (beta <= alpha) break;            /* prune */
        }
        return best;
    } else {                                     /* human wants the LOWEST score */
        int best = 1000;
        for (int i = 0; i < 9; i++) {
            if (b[i] != ' ') continue;
            b[i] = HUMAN;
            int score = minimax(b, 1, depth + 1, alpha, beta);
            b[i] = ' ';
            if (score < best) best = score;
            if (best < beta) beta = best;
            if (beta <= alpha) break;
        }
        return best;
    }
}

static int best_move(char b[9])
{
    /* On an empty board every corner/center is equally good, so vary it */
    int empty = 1;
    for (int i = 0; i < 9; i++) if (b[i] != ' ') empty = 0;
    if (empty) {
        static const int openers[5] = {0, 2, 4, 6, 8};
        return openers[rand() % 5];
    }

    int best = -1000, move = -1;
    for (int i = 0; i < 9; i++) {
        if (b[i] != ' ') continue;
        b[i] = AI;
        int score = minimax(b, 0, 1, -1000, 1000);
        b[i] = ' ';
        if (score > best) { best = score; move = i; }
    }
    return move;
}

static void draw_board(const char b[9])
{
    printf("\n");
    for (int r = 0; r < 3; r++) {
        printf("        ");
        for (int c = 0; c < 3; c++) {
            int i = r * 3 + c;
            if (b[i] == HUMAN)      printf(" \033[1;31mX\033[0m ");
            else if (b[i] == AI)    printf(" \033[1;36mO\033[0m ");
            else                    printf(" \033[2m%d\033[0m ", i + 1);
            if (c < 2) printf("|");
        }
        printf("\n");
        if (r < 2) printf("        ---+---+---\n");
    }
    printf("\n");
}

static void goodbye(void)
{
    printf("\nFinal score  -  You: %d win(s) | Draws: %d | AI: %d win(s)\n",
           wins, draws, losses);
    printf("Thanks for playing!\n");
    exit(0);
}

static int ask_move(const char b[9])
{
    char line[64];
    for (;;) {
        printf("Your move (1-9, or Q to quit): ");
        if (!fgets(line, sizeof line, stdin)) goodbye();
        if (tolower((unsigned char)line[0]) == 'q') goodbye();
        int m = atoi(line);
        if (m >= 1 && m <= 9 && b[m - 1] == ' ') return m - 1;
        printf("  That square is invalid or already taken.\n");
    }
}

/* Returns 1 = human won, 0 = draw, -1 = AI won */
static int play(int human_first)
{
    char b[9];
    memset(b, ' ', sizeof b);
    int human_turn = human_first;

    for (;;) {
        draw_board(b);

        if (human_turn) {
            b[ask_move(b)] = HUMAN;
        } else {
            nodes = 0;
            int m = best_move(b);
            b[m] = AI;
            printf("AI plays %d  (it looked at %ld positions)\n", m + 1, nodes);
        }

        char w = check_winner(b);
        if (w)           { draw_board(b); return (w == HUMAN) ? 1 : -1; }
        if (board_full(b)) { draw_board(b); return 0; }
        human_turn = !human_turn;
    }
}

int main(void)
{
    char line[64];
    srand((unsigned)time(NULL));
    enable_ansi();

    printf("==============================\n");
    printf("   TIC-TAC-TOE vs UNBEATABLE AI\n");
    printf("==============================\n");
    printf("You are X. The AI is O. Good luck - you will need it!\n");

    for (;;) {
        printf("\nWho moves first?  1) You   2) AI   : ");
        if (!fgets(line, sizeof line, stdin)) goodbye();
        int first = (atoi(line) != 2);

        int result = play(first);
        if (result == 1)       { wins++;   printf("You won?! That should be impossible...\n"); }
        else if (result == 0)  { draws++;  printf("It's a draw. The best you can do against this AI!\n"); }
        else                   { losses++; printf("The AI wins this round.\n"); }

        printf("Score  -  You: %d | Draws: %d | AI: %d\n", wins, draws, losses);
        printf("Play again? (Y/N): ");
        if (!fgets(line, sizeof line, stdin) || tolower((unsigned char)line[0]) != 'y')
            goodbye();
    }
}
