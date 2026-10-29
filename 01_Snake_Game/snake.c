/*
 * ============================================================
 *  SNAKE GAME  -  C Mastery Level
 * ============================================================
 *  Concepts used:
 *    - structs and enums
 *    - arrays (snake body as an array of points)
 *    - game loop (input -> update -> draw -> wait)
 *    - non-blocking keyboard input (Windows + Linux/Mac)
 *    - ANSI escape codes (colors, cursor movement)
 *    - file handling (high score saved in highscore.txt)
 *    - signal handling (clean exit on Ctrl+C)
 *
 *  Compile:   gcc snake.c -o snake
 *  Run:       ./snake        (Windows: snake.exe)
 *
 *  Controls:  W A S D or Arrow keys = move
 *             P = pause,  Q = quit
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <signal.h>

#ifdef _WIN32
    #include <conio.h>
    #include <windows.h>
    #ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
        #define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
    #endif
#else
    #include <termios.h>
    #include <unistd.h>
    #include <sys/select.h>
#endif

/* ---------------- Settings ---------------- */
#define WIDTH       40
#define HEIGHT      20
#define MAX_LEN     (WIDTH * HEIGHT)
#define HS_FILE     "highscore.txt"
#define POINTS      10      /* points per food */
#define START_DELAY 140     /* milliseconds per move at level 1 */
#define MIN_DELAY   50      /* fastest possible speed */

/* ---------------- Data types ---------------- */
typedef struct {
    int x, y;
} Point;

typedef enum { UP, DOWN, LEFT, RIGHT } Direction;

typedef struct {
    Point     body[MAX_LEN];   /* body[0] is the head */
    int       length;
    Direction dir;
} Snake;

static volatile int g_quit = 0;   /* set to 1 when the user wants to exit */

static void on_sigint(int sig)
{
    (void)sig;
    g_quit = 1;
}

/* ============================================================
 *  PLATFORM LAYER  (everything that differs between OSes)
 * ============================================================ */
#ifdef _WIN32

static void terminal_init(void)
{
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(h, &mode))
        SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}
static void terminal_restore(void) { }
static int  key_pressed(void)      { return _kbhit(); }
static int  read_key(void)         { return _getch(); }
static void sleep_ms(int ms)       { Sleep(ms); }

#else

static struct termios orig_termios;

static void terminal_init(void)
{
    struct termios raw;
    tcgetattr(STDIN_FILENO, &orig_termios);
    raw = orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO);   /* no line buffering, no echo */
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}
static void terminal_restore(void)
{
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
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
    if (read(STDIN_FILENO, &c, 1) == 1) return c;
    return -1;
}
static void sleep_ms(int ms) { usleep(ms * 1000); }

#endif

/* ============================================================
 *  INPUT
 *  Returns one of: 'w' 'a' 's' 'd' 'p' 'q', or 0 if no key.
 *  Arrow keys are converted into w/a/s/d.
 * ============================================================ */
static int get_command(void)
{
    int cmd = 0;

    while (key_pressed()) {
        int c = read_key();
        if (c < 0) break;                      /* no more input available */
#ifdef _WIN32
        if (c == 0 || c == 224) {              /* arrow keys send 2 codes */
            c = read_key();
            switch (c) {
                case 72: cmd = 'w'; break;
                case 80: cmd = 's'; break;
                case 75: cmd = 'a'; break;
                case 77: cmd = 'd'; break;
            }
        } else {
            cmd = tolower(c);
        }
#else
        if (c == 27) {                         /* ESC [ A/B/C/D = arrows */
            if (key_pressed() && read_key() == '[' && key_pressed()) {
                switch (read_key()) {
                    case 'A': cmd = 'w'; break;
                    case 'B': cmd = 's'; break;
                    case 'D': cmd = 'a'; break;
                    case 'C': cmd = 'd'; break;
                }
            }
        } else {
            cmd = tolower(c);
        }
#endif
    }
    return cmd;
}

/* ============================================================
 *  HIGH SCORE (file handling)
 * ============================================================ */
static int load_high(void)
{
    int h = 0;
    FILE *fp = fopen(HS_FILE, "r");
    if (fp) {
        if (fscanf(fp, "%d", &h) != 1) h = 0;
        fclose(fp);
    }
    return h;
}

static void save_high(int h)
{
    FILE *fp = fopen(HS_FILE, "w");
    if (fp) {
        fprintf(fp, "%d\n", h);
        fclose(fp);
    }
}

/* ============================================================
 *  GAME LOGIC
 * ============================================================ */
static void init_snake(Snake *s)
{
    s->length = 3;
    s->dir = RIGHT;
    for (int i = 0; i < s->length; i++) {
        s->body[i].x = WIDTH / 2 - i;
        s->body[i].y = HEIGHT / 2;
    }
}

static void place_food(const Snake *s, Point *food)
{
    int ok;
    do {
        ok = 1;
        food->x = rand() % WIDTH;
        food->y = rand() % HEIGHT;
        for (int i = 0; i < s->length; i++) {
            if (s->body[i].x == food->x && s->body[i].y == food->y) {
                ok = 0;       /* food landed on the snake, try again */
                break;
            }
        }
    } while (!ok);
}

static int level_of(int score) { return score / 50 + 1; }

static int delay_of(int score)
{
    int d = START_DELAY - (level_of(score) - 1) * 12;
    return d < MIN_DELAY ? MIN_DELAY : d;
}

/* ============================================================
 *  DRAWING
 *  Cursor jumps to the top-left each frame instead of clearing
 *  the whole screen, which avoids flicker.
 * ============================================================ */
static void draw(const Snake *s, const Point *food, int score, int high)
{
    char grid[HEIGHT][WIDTH];
    memset(grid, ' ', sizeof grid);

    grid[food->y][food->x] = '*';
    for (int i = s->length - 1; i >= 0; i--)
        grid[s->body[i].y][s->body[i].x] = (i == 0) ? 'O' : 'o';

    printf("\033[H");
    printf("\033[1;36m SNAKE \033[0m| Score: %d | High: %d | Level: %d\033[K\n",
           score, high, level_of(score));

    putchar('+');
    for (int x = 0; x < WIDTH; x++) putchar('-');
    printf("+\n");

    for (int y = 0; y < HEIGHT; y++) {
        putchar('|');
        for (int x = 0; x < WIDTH; x++) {
            switch (grid[y][x]) {
                case 'O': printf("\033[1;92mO\033[0m"); break;   /* head */
                case 'o': printf("\033[32mo\033[0m");   break;   /* body */
                case '*': printf("\033[1;91m*\033[0m"); break;   /* food */
                default:  putchar(' ');
            }
        }
        printf("|\n");
    }

    putchar('+');
    for (int x = 0; x < WIDTH; x++) putchar('-');
    printf("+\n");
    printf(" WASD/Arrows = move | P = pause | Q = quit\033[K\n");
    printf("\033[K\n");      /* status line (cleared every frame) */
    fflush(stdout);
}

static void status(const char *msg)
{
    printf("\033[%d;1H %s\033[K", HEIGHT + 5, msg);
    fflush(stdout);
}

/* ============================================================
 *  ONE FULL GAME
 * ============================================================ */
static void play(int *high)
{
    Snake snake;
    Point food;
    int score = 0, paused = 0, alive = 1, won = 0;

    init_snake(&snake);
    place_food(&snake, &food);
    printf("\033[2J");   /* clear the screen once at the start */

    while (alive && !g_quit) {
        /* ---- 1. INPUT ---- */
        int cmd = get_command();
        if (cmd == 'q') { g_quit = 1; break; }
        if (cmd == 'p') paused = !paused;

        if (paused) {
            status("PAUSED - press P to resume");
            sleep_ms(100);
            continue;
        }

        /* Turn, but never reverse straight into yourself */
        if (cmd == 'w' && snake.dir != DOWN)  snake.dir = UP;
        if (cmd == 's' && snake.dir != UP)    snake.dir = DOWN;
        if (cmd == 'a' && snake.dir != RIGHT) snake.dir = LEFT;
        if (cmd == 'd' && snake.dir != LEFT)  snake.dir = RIGHT;

        /* ---- 2. UPDATE ---- */
        Point head = snake.body[0];
        switch (snake.dir) {
            case UP:    head.y--; break;
            case DOWN:  head.y++; break;
            case LEFT:  head.x--; break;
            case RIGHT: head.x++; break;
        }

        /* Wall collision */
        if (head.x < 0 || head.x >= WIDTH || head.y < 0 || head.y >= HEIGHT) {
            alive = 0;
            break;
        }

        int ate = (head.x == food.x && head.y == food.y);

        /* Self collision (the tail moves away, so skip it unless we grow) */
        int check = ate ? snake.length : snake.length - 1;
        for (int i = 0; i < check; i++) {
            if (snake.body[i].x == head.x && snake.body[i].y == head.y) {
                alive = 0;
                break;
            }
        }
        if (!alive) break;

        /* Grow if we ate, then shift every segment forward by one */
        if (ate) {
            snake.body[snake.length] = snake.body[snake.length - 1];
            snake.length++;
            score += POINTS;
        }
        for (int i = snake.length - 1; i > 0; i--)
            snake.body[i] = snake.body[i - 1];
        snake.body[0] = head;

        if (ate) {
            if (snake.length >= MAX_LEN) { won = 1; break; }
            place_food(&snake, &food);
        }

        /* ---- 3. DRAW ---- */
        draw(&snake, &food, score, *high);

        /* ---- 4. WAIT (faster at higher levels) ---- */
        sleep_ms(delay_of(score));
    }

    if (g_quit) return;

    /* ---- Game over ---- */
    if (score > *high) {
        *high = score;
        save_high(score);
        draw(&snake, &food, score, *high);
        status(won ? "YOU WIN! NEW HIGH SCORE!" : "GAME OVER - NEW HIGH SCORE!");
    } else {
        draw(&snake, &food, score, *high);
        status(won ? "YOU WIN!" : "GAME OVER");
    }
}

static int ask_play_again(void)
{
    while (key_pressed()) read_key();   /* throw away leftover key presses */
    printf("\033[%d;1H Play again? (Y/N)\033[K", HEIGHT + 6);
    fflush(stdout);

    while (!g_quit) {
        if (key_pressed()) {
            int c = tolower(read_key());
            if (c == 'y') return 1;
            if (c == 'n' || c == 'q') return 0;
        }
        sleep_ms(50);
    }
    return 0;
}

/* ============================================================
 *  MAIN
 * ============================================================ */
int main(void)
{
    srand((unsigned)time(NULL));
    signal(SIGINT, on_sigint);

    int high = load_high();

    terminal_init();
    setvbuf(stdout, NULL, _IOFBF, 1 << 16);   /* buffer a whole frame */
    printf("\033[?25l");                      /* hide the cursor */

    do {
        play(&high);
    } while (!g_quit && ask_play_again());

    printf("\033[?25h\033[0m\033[%d;1H\nThanks for playing!\n", HEIGHT + 7);
    fflush(stdout);
    terminal_restore();
    return 0;
}
