/*
 * ============================================================
 *  MAZE GENERATOR AND SOLVER  -  C Mastery Level
 * ============================================================
 *  Watch a random maze being carved, then watch BFS or DFS
 *  search for the exit. The shortest path is drawn in green.
 *
 *  Concepts used:
 *    - 2D arrays
 *    - Stack (recursive backtracker) to GENERATE the maze
 *    - Queue (BFS)  -> always finds the SHORTEST path
 *    - Stack (DFS)  -> finds A path, often a longer one
 *    - ANSI colors for animation
 *
 *  Compile:   gcc maze.c -o maze
 *  Run:       ./maze             (Windows: maze.exe)
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

/* Both must be ODD numbers. Width 37 -> 74 screen columns. */
#define W 37
#define H 19

#define START_X 1
#define START_Y 1
#define GOAL_X  (W - 2)
#define GOAL_Y  (H - 2)

static char          maze[H][W];   /* '#' = wall, ' ' = open */
static unsigned char seen[H][W];   /* explored by the solver */
static unsigned char path[H][W];   /* part of the final path */

static void enable_ansi(void)
{
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(h, &mode)) SetConsoleMode(h, mode | 0x0004);
#endif
}

/* Every maze cell is drawn as two characters wide so it looks square. */
static void draw(int cx, int cy, const char *status)
{
    printf("\033[H");
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            const char *s;
            if (x == cx && y == cy)                 s = "\033[41m  ";          /* current */
            else if (x == START_X && y == START_Y)  s = "\033[1;30;102mS ";   /* start   */
            else if (x == GOAL_X  && y == GOAL_Y)   s = "\033[1;30;101mE ";   /* exit    */
            else if (path[y][x])                    s = "\033[42m  ";          /* path    */
            else if (seen[y][x])                    s = "\033[44m  ";          /* explored*/
            else if (maze[y][x] == '#')             s = "\033[100m  ";         /* wall    */
            else                                    s = "\033[0m  ";           /* floor   */
            fputs(s, stdout);
        }
        fputs("\033[0m\n", stdout);
    }
    printf("%s\033[K\n", status);
    fflush(stdout);
}

static void shuffle4(int a[4])
{
    for (int i = 3; i > 0; i--) {
        int j = rand() % (i + 1);
        int t = a[i]; a[i] = a[j]; a[j] = t;
    }
}

/* ---------- GENERATION: recursive backtracker using an explicit stack ---------- */
static void generate(void)
{
    static const int dx[4] = {0, 2, 0, -2};
    static const int dy[4] = {-2, 0, 2, 0};
    int stack[W * H][2];
    int top = 0;

    memset(maze, '#', sizeof maze);
    memset(seen, 0, sizeof seen);
    memset(path, 0, sizeof path);

    maze[START_Y][START_X] = ' ';
    stack[top][0] = START_X;
    stack[top][1] = START_Y;
    top++;

    while (top > 0) {
        int x = stack[top - 1][0];
        int y = stack[top - 1][1];
        int order[4] = {0, 1, 2, 3};
        int moved = 0;
        shuffle4(order);

        for (int k = 0; k < 4; k++) {
            int d = order[k];
            int nx = x + dx[d], ny = y + dy[d];
            if (nx > 0 && nx < W - 1 && ny > 0 && ny < H - 1 && maze[ny][nx] == '#') {
                maze[y + dy[d] / 2][x + dx[d] / 2] = ' ';   /* knock down the wall between */
                maze[ny][nx] = ' ';
                stack[top][0] = nx;
                stack[top][1] = ny;
                top++;
                moved = 1;
                draw(nx, ny, "Carving the maze...");
                sleep_ms(6);
                break;
            }
        }
        if (!moved) top--;                                  /* dead end: backtrack */
    }

    /* Knock down a few extra walls so there are loops.
       Now there can be several routes, and BFS and DFS give different answers. */
    int extra = (W * H) / 40, made = 0;
    for (int tries = 0; tries < extra * 20 && made < extra; tries++) {
        int x = 1 + rand() % (W - 2);
        int y = 1 + rand() % (H - 2);
        if (maze[y][x] != '#') continue;
        int horizontal = (x % 2 == 0 && y % 2 == 1 && maze[y][x - 1] == ' ' && maze[y][x + 1] == ' ');
        int vertical   = (x % 2 == 1 && y % 2 == 0 && maze[y - 1][x] == ' ' && maze[y + 1][x] == ' ');
        if (horizontal || vertical) { maze[y][x] = ' '; made++; }
    }
    draw(-1, -1, "Maze ready!");
}

/* ---------- SOLVING ---------- */
static void solve(int use_bfs)
{
    static const int dx[4] = {0, 1, 0, -1};
    static const int dy[4] = {-1, 0, 1, 0};

    int frontier[W * H];
    int parent[W * H];
    unsigned char found_flag[W * H];
    int head = 0, tail = 0, explored = 0, found = 0;
    int start = START_Y * W + START_X;
    int goal  = GOAL_Y * W + GOAL_X;

    memset(seen, 0, sizeof seen);
    memset(path, 0, sizeof path);
    memset(found_flag, 0, sizeof found_flag);

    frontier[tail++] = start;
    found_flag[start] = 1;
    parent[start] = -1;

    while (head < tail) {
        /* BFS takes from the FRONT (queue), DFS takes from the BACK (stack) */
        int cur = use_bfs ? frontier[head++] : frontier[--tail];
        int cx = cur % W, cy = cur / W;
        seen[cy][cx] = 1;
        explored++;

        if (explored % 2 == 0) {
            draw(cx, cy, use_bfs ? "Solving with BFS (queue)..." : "Solving with DFS (stack)...");
            sleep_ms(8);
        }
        if (cur == goal) { found = 1; break; }

        for (int d = 0; d < 4; d++) {
            int nx = cx + dx[d], ny = cy + dy[d];
            if (nx < 0 || nx >= W || ny < 0 || ny >= H) continue;
            if (maze[ny][nx] == '#') continue;
            int idx = ny * W + nx;
            if (found_flag[idx]) continue;
            found_flag[idx] = 1;
            parent[idx] = cur;
            frontier[tail++] = idx;
        }
    }

    if (!found) { draw(-1, -1, "No path found (this should never happen)."); return; }

    /* Walk backwards from the exit to the start to rebuild the path */
    int route[W * H], len = 0;
    for (int c = goal; c != -1; c = parent[c]) route[len++] = c;

    for (int i = len - 1; i >= 0; i--) {
        path[route[i] / W][route[i] % W] = 1;
        draw(-1, -1, "Drawing the path...");
        sleep_ms(15);
    }

    char msg[160];
    snprintf(msg, sizeof msg, "%s: explored %d cells, path is %d steps long.",
             use_bfs ? "BFS" : "DFS", explored, len - 1);
    draw(-1, -1, msg);
}

int main(void)
{
    char line[32];
    srand((unsigned)time(NULL));
    enable_ansi();
    printf("\033[2J\033[?25l");

    for (;;) {
        generate();

        printf("\n 1) Solve with BFS (shortest path)\n 2) Solve with DFS\n 3) New maze\n 4) Quit\n Choice: ");
        fflush(stdout);
        if (!fgets(line, sizeof line, stdin)) break;
        int choice = atoi(line);
        if (choice == 4) break;
        if (choice == 1 || choice == 2) {
            solve(choice == 1);
            printf("\n Press Enter for a new maze (or Q then Enter to quit): ");
            fflush(stdout);
            if (!fgets(line, sizeof line, stdin) || tolower((unsigned char)line[0]) == 'q') break;
        }
        printf("\033[2J");
    }

    printf("\033[?25h\033[0m\nGoodbye!\n");
    return 0;
}
