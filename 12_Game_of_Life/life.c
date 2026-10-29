/*
 * ============================================================
 *  CONWAY'S GAME OF LIFE  -  C Mastery Level
 * ============================================================
 *  A "zero-player game": you set up some living cells and
 *  watch them evolve by 4 simple rules:
 *
 *    1. A live cell with 2 or 3 neighbours survives.
 *    2. A live cell with fewer than 2 neighbours dies (lonely).
 *    3. A live cell with more than 3 neighbours dies (crowded).
 *    4. A dead cell with exactly 3 neighbours becomes alive.
 *
 *  From these rules come gliders, oscillators and even
 *  "guns" that fire gliders forever!
 *
 *  Colors show the AGE of a cell:
 *     bright green = newborn,  green = young,  blue = old/stable
 *
 *  Controls:  P = pause   + / - = speed   R = random   Q = menu
 *
 *  Concepts used: 2D arrays, double buffering, wrap-around
 *  neighbours with the % operator, ANSI colors, animation.
 *
 *  Compile:   gcc life.c -o life
 *  Run:       ./life             (Windows: life.exe)
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <signal.h>

#ifdef _WIN32
    #include <windows.h>
    #include <conio.h>
#else
    #include <unistd.h>
    #include <termios.h>
    #include <sys/select.h>
#endif

#define GW 38            /* grid width  (each cell is 2 characters wide) */
#define GH 24            /* grid height */

static unsigned char cur[GH][GW];     /* current generation */
static unsigned char nxt[GH][GW];     /* next generation (double buffering) */
static int           age[GH][GW];     /* how many generations each cell has lived */
static int           wrap = 1;        /* 1 = edges connect around like a donut */
static volatile int  g_quit = 0;

static void on_sigint(int sig) { (void)sig; g_quit = 1; }

/* ======================= platform layer ======================= */
#ifdef _WIN32

static void terminal_init(void)
{
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(h, &mode)) SetConsoleMode(h, mode | 0x0004);
}
static void terminal_restore(void) { }
static int  key_pressed(void) { return _kbhit(); }
static int  read_key(void)    { return _getch(); }
static void sleep_ms(int ms)  { Sleep(ms); }

#else

static struct termios orig_termios;
static int have_termios = 0;

static void terminal_init(void)
{
    if (tcgetattr(STDIN_FILENO, &orig_termios) == 0) {
        struct termios raw = orig_termios;
        raw.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
        have_termios = 1;
    }
}
static void terminal_restore(void)
{
    if (have_termios) tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
}
static int key_pressed(void)
{
    struct timeval tv = {0, 0};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}
static int read_key(void)
{
    unsigned char c;
    return (read(STDIN_FILENO, &c, 1) == 1) ? c : -1;
}
static void sleep_ms(int ms) { usleep(ms * 1000); }

#endif

/* ======================= patterns ======================= */

static void clear_grid(void)
{
    memset(cur, 0, sizeof cur);
    memset(age, 0, sizeof age);
}

static void randomize(void)
{
    clear_grid();
    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++)
            cur[y][x] = (rand() % 4 == 0);
}

/* Stamp a pattern made of '.' and 'O' characters onto the grid */
static void stamp(const char **rows, int nrows, int ox, int oy)
{
    for (int y = 0; y < nrows; y++)
        for (int x = 0; rows[y][x]; x++)
            if (rows[y][x] == 'O' && oy + y < GH && ox + x < GW)
                cur[oy + y][ox + x] = 1;
}

static void load_preset(int choice)
{
    static const char *gun[] = {
        "........................O...........",
        "......................O.O...........",
        "............OO......OO............OO",
        "...........O...O....OO............OO",
        "OO........O.....O...OO..............",
        "OO........O...O.OO....O.O...........",
        "..........O.....O.......O...........",
        "...........O...O....................",
        "............OO......................"
    };
    static const char *pulsar[] = {
        "..OOO...OOO..", ".............", "O....O.O....O", "O....O.O....O",
        "O....O.O....O", "..OOO...OOO..", ".............", "..OOO...OOO..",
        "O....O.O....O", "O....O.O....O", "O....O.O....O", ".............",
        "..OOO...OOO.."
    };
    static const char *rpent[]  = { ".OO", "OO.", ".O." };
    static const char *acorn[]  = { ".O.....", "...O...", "OO..OOO" };
    static const char *glider[] = { ".O.", "..O", "OOO" };

    clear_grid();
    wrap = 1;
    switch (choice) {
        case 1: randomize(); break;
        case 2: wrap = 0; stamp(gun, 9, 0, 2); break;
        case 3: stamp(pulsar, 13, 12, 5); break;
        case 4: stamp(rpent, 3, 18, 10); break;
        case 5: stamp(acorn, 3, 15, 11); break;
        case 6:
            stamp(glider, 3, 3, 3);
            stamp(glider, 3, 20, 5);
            stamp(glider, 3, 10, 14);
            stamp(glider, 3, 28, 16);
            break;
    }
}

/* ======================= simulation ======================= */

static int neighbours(int y, int x)
{
    int n = 0;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dy == 0 && dx == 0) continue;
            int yy = y + dy, xx = x + dx;
            if (wrap) {
                yy = (yy + GH) % GH;          /* wrap around the edges */
                xx = (xx + GW) % GW;
            } else if (yy < 0 || yy >= GH || xx < 0 || xx >= GW) {
                continue;
            }
            n += cur[yy][xx];
        }
    }
    return n;
}

static int step(void)
{
    int alive = 0;
    for (int y = 0; y < GH; y++) {
        for (int x = 0; x < GW; x++) {
            int n = neighbours(y, x);
            if (cur[y][x]) nxt[y][x] = (n == 2 || n == 3);
            else           nxt[y][x] = (n == 3);

            if (nxt[y][x]) { age[y][x] = cur[y][x] ? age[y][x] + 1 : 1; alive++; }
            else           age[y][x] = 0;
        }
    }
    memcpy(cur, nxt, sizeof cur);
    return alive;
}

static void render(int generation, int alive, int delay, int paused)
{
    printf("\033[H");
    printf(" \033[1mGAME OF LIFE\033[0m | Gen: %d | Alive: %d | Speed: %dms | Edges: %s%s\033[K\n",
           generation, alive, delay, wrap ? "wrap" : "solid", paused ? " | PAUSED" : "");

    int last = -1;
    for (int y = 0; y < GH; y++) {
        for (int x = 0; x < GW; x++) {
            int color;
            if (!cur[y][x])           color = 0;
            else if (age[y][x] <= 1)  color = 1;
            else if (age[y][x] <= 4)  color = 2;
            else                      color = 3;

            if (color != last) {
                switch (color) {
                    case 0: fputs("\033[0m",   stdout); break;
                    case 1: fputs("\033[102m", stdout); break;   /* bright green */
                    case 2: fputs("\033[42m",  stdout); break;   /* green        */
                    case 3: fputs("\033[44m",  stdout); break;   /* blue         */
                }
                last = color;
            }
            fputs("  ", stdout);
        }
        fputs("\033[0m\n", stdout);
        last = -1;
    }
    printf(" P pause | + faster | - slower | R random | Q menu\033[K\n");
    fflush(stdout);
}

static void run_simulation(void)
{
    int generation = 0, delay = 100, paused = 0;
    int alive = 0;
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) {
        if (cur[y][x]) { alive++; age[y][x] = 1; }
    }

    printf("\033[2J");
    while (!g_quit) {
        while (key_pressed()) {
            int k = read_key();
            if (k < 0) break;
            k = tolower(k);
            if (k == 'q') return;
            if (k == 'p') paused = !paused;
            if (k == '+' || k == '=') { delay -= 20; if (delay < 20) delay = 20; }
            if (k == '-' || k == '_') { delay += 20; if (delay > 600) delay = 600; }
            if (k == 'r') { randomize(); generation = 0; }
        }

        if (!paused) {
            alive = step();
            generation++;
        }
        render(generation, alive, delay, paused);

        if (alive == 0 && !paused) {
            printf(" Everything died out after %d generations. Press Q.\033[K\n", generation);
            fflush(stdout);
            paused = 1;
        }
        sleep_ms(delay);
    }
}

int main(void)
{
    srand((unsigned)time(NULL));
    signal(SIGINT, on_sigint);
    terminal_init();
    setvbuf(stdout, NULL, _IOFBF, 1 << 16);
    printf("\033[?25l");

    while (!g_quit) {
        printf("\033[2J\033[H");
        printf("=====================================\n");
        printf("      CONWAY'S GAME OF LIFE\n");
        printf("=====================================\n");
        printf(" 1) Random soup\n 2) Gosper glider gun (fires gliders forever)\n");
        printf(" 3) Pulsar (an oscillator)\n 4) R-pentomino (chaos from 5 cells)\n");
        printf(" 5) Acorn (grows for a long time)\n 6) Four gliders\n 0) Quit\n\nChoice: ");
        printf("\033[?25h");
        fflush(stdout);

        /* menu uses normal line input, so restore the terminal first */
        terminal_restore();
        char line[32];
        if (!fgets(line, sizeof line, stdin)) break;
        int c = atoi(line);
        if (c == 0) break;
        if (c < 1 || c > 6) continue;

        terminal_init();
        printf("\033[?25l");
        load_preset(c);
        run_simulation();
    }

    printf("\033[0m\033[?25h\033[2J\033[H");
    fflush(stdout);
    terminal_restore();
    printf("Thanks for watching life evolve!\n");
    return 0;
}
