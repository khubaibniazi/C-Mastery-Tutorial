/*
 * ============================================================
 *  MATRIX RAIN  -  C Mastery Level
 * ============================================================
 *  Green characters fall down your terminal like in the movie
 *  "The Matrix". Press Q (or ESC, or Ctrl+C) to stop.
 *
 *  Concepts used:
 *    - ANSI escape codes (colors, cursor positioning)
 *    - Reading the terminal size
 *    - Non-blocking keyboard input
 *    - Animation with timing (a frame loop)
 *    - Structs and 2D arrays
 *
 *  Compile:   gcc matrix.c -o matrix
 *  Run:       ./matrix           (Windows: matrix.exe)
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>

#ifdef _WIN32
    #include <windows.h>
    #include <conio.h>
#else
    #include <unistd.h>
    #include <termios.h>
    #include <sys/select.h>
    #include <sys/ioctl.h>
#endif

#define MAX_COLS 300
#define MAX_ROWS 100
#define FRAME_MS 45

static int  cols = 80, rows = 24;
static char ch[MAX_ROWS][MAX_COLS];      /* the character in each cell      */
static int  age[MAX_ROWS][MAX_COLS];     /* -1 = empty, 0 = brand new head  */

typedef struct {
    int head;      /* row of the bright head (can be negative = not on screen yet) */
    int speed;     /* the drop moves once every 'speed' frames                     */
    int tick;      /* frame counter for the speed                                  */
    int len;       /* how long the glowing tail is                                 */
} Drop;

static Drop drops[MAX_COLS];
static volatile int g_quit = 0;

static const char CHARSET[] =
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ@#$%&*+=<>?/\\|{}[]";

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
static void get_size(void)
{
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info)) {
        cols = info.srWindow.Right  - info.srWindow.Left + 1;
        rows = info.srWindow.Bottom - info.srWindow.Top  + 1;
    }
}

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
static void get_size(void)
{
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0) {
        cols = ws.ws_col;
        rows = ws.ws_row;
    }
}

#endif

/* ======================= the rain ======================= */

static char random_char(void)
{
    return CHARSET[rand() % (int)(sizeof CHARSET - 1)];
}

static void reset_drop(int x, int first_time)
{
    drops[x].head  = first_time ? -(rand() % rows) : -(rand() % 10);
    drops[x].speed = 1 + rand() % 3;
    drops[x].tick  = 0;
    drops[x].len   = 5 + rand() % (rows > 8 ? rows - 5 : 4);
}

static void init_rain(void)
{
    if (cols > MAX_COLS) cols = MAX_COLS;
    if (rows > MAX_ROWS) rows = MAX_ROWS;
    for (int y = 0; y < MAX_ROWS; y++)
        for (int x = 0; x < MAX_COLS; x++) { ch[y][x] = ' '; age[y][x] = -1; }
    for (int x = 0; x < cols; x++) reset_drop(x, 1);
    printf("\033[2J");
}

static void update(void)
{
    for (int x = 0; x < cols; x++) {
        Drop *d = &drops[x];
        if (++d->tick < d->speed) continue;
        d->tick = 0;
        d->head++;

        /* everything already in this column gets one step older */
        for (int y = 0; y < rows; y++) {
            if (age[y][x] >= 0) {
                age[y][x]++;
                if (age[y][x] > d->len) age[y][x] = -1;   /* tail has faded away */
            }
        }
        /* the new bright head */
        if (d->head >= 0 && d->head < rows) {
            ch[d->head][x]  = random_char();
            age[d->head][x] = 0;
        }
        if (d->head - d->len > rows) reset_drop(x, 0);
    }

    /* occasionally change a character, so the rain "glitches" */
    for (int y = 0; y < rows; y++)
        for (int x = 0; x < cols; x++)
            if (age[y][x] > 0 && rand() % 100 < 2) ch[y][x] = random_char();
}

static void render(void)
{
    int last_color = -1;
    for (int y = 0; y < rows; y++) {
        printf("\033[%d;1H", y + 1);
        for (int x = 0; x < cols; x++) {
            int a = age[y][x];
            int color;                                   /* 0 = empty, 1..4 = fading green */
            if (a < 0)       color = 0;
            else if (a == 0) color = 1;
            else if (a <= 2) color = 2;
            else if (a <= 8) color = 3;
            else             color = 4;

            if (color != last_color) {
                switch (color) {
                    case 0: fputs("\033[0m", stdout);      break;
                    case 1: fputs("\033[1;97m", stdout);   break;   /* white  */
                    case 2: fputs("\033[1;92m", stdout);   break;   /* bright */
                    case 3: fputs("\033[32m", stdout);     break;   /* green  */
                    case 4: fputs("\033[2;32m", stdout);   break;   /* dim    */
                }
                last_color = color;
            }
            putchar(color == 0 ? ' ' : ch[y][x]);
        }
    }
    fputs("\033[0m", stdout);
    fflush(stdout);
}

int main(void)
{
    srand((unsigned)time(NULL));
    signal(SIGINT, on_sigint);
    terminal_init();
    setvbuf(stdout, NULL, _IOFBF, 1 << 16);

    get_size();
    printf("\033[?1049h\033[?25l");      /* use the alternate screen, hide the cursor */
    init_rain();

    int frame = 0;
    while (!g_quit) {
        while (key_pressed()) {
            int k = read_key();
            if (k < 0) break;                       /* no more input available */
            if (k == 'q' || k == 'Q' || k == 27) g_quit = 1;
        }

        if (++frame % 20 == 0) {                    /* handle window resizing */
            int oc = cols, orow = rows;
            get_size();
            if (cols != oc || rows != orow) init_rain();
        }

        update();
        render();
        sleep_ms(FRAME_MS);
    }

    printf("\033[0m\033[?25h\033[?1049l");
    fflush(stdout);
    terminal_restore();
    printf("Wake up, Neo...\n");
    return 0;
}
