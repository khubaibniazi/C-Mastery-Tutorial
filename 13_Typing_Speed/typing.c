/*
 * ============================================================
 *  TYPING SPEED TESTER  -  C Mastery Level
 * ============================================================
 *  A sentence appears. Type it as fast and as accurately as
 *  you can, then press Enter. You get your WPM (words per
 *  minute), your accuracy and a colored mistake report.
 *  Your best score is saved in typing_best.txt.
 *
 *  Concepts used:
 *    - High-resolution timing (clock_gettime / QueryPerformanceCounter)
 *    - String comparison character by character
 *    - File handling (saving the high score)
 *    - Arrays of strings, random selection without repeats
 *    - Simple scoring formulas
 *
 *  WPM formula:  (correct characters / 5) / minutes
 *  (one "word" = 5 characters, the standard used everywhere)
 *
 *  Compile:   gcc typing.c -o typing
 *  Run:       ./typing           (Windows: typing.exe)
 * ============================================================
 */

#ifndef _WIN32
    #define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#ifdef _WIN32
    #include <windows.h>
    #define sleep_ms(ms) Sleep(ms)
#else
    static void sleep_ms(int ms)
    {
        struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L };
        nanosleep(&ts, NULL);
    }
#endif

#define BEST_FILE "typing_best.txt"
#define MAX_INPUT 512

static const char *SENTENCES[] = {
    "The quick brown fox jumps over the lazy dog.",
    "A pointer is a variable that stores a memory address.",
    "Practice makes progress, and progress makes perfect.",
    "C is a powerful language that sits close to the hardware.",
    "Every great programmer was once a beginner who never gave up.",
    "Debugging is like being a detective in a crime movie where you are the culprit.",
    "Memory leaks happen when you forget to free what you allocated.",
    "Segmentation fault means you touched memory that was not yours.",
    "Good code is simple, readable and does exactly what it says.",
    "Arrays in C start at zero, so the last index is always one less than the size.",
    "Loops let the computer repeat boring work without ever complaining.",
    "Compile early, compile often, and read every warning carefully.",
    "A function should do one thing and do it well.",
    "Stack and heap are two different places where your data can live.",
    "Learning to code is a marathon and not a sprint.",
    "The best way to understand recursion is to understand recursion.",
    "Never trust user input, always check it before you use it.",
    "Small steps every day will take you further than a single giant leap."
};
#define NUM_SENTENCES ((int)(sizeof SENTENCES / sizeof SENTENCES[0]))

static void enable_ansi(void)
{
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(h, &mode)) SetConsoleMode(h, mode | 0x0004);
#endif
}

/* Seconds as a double, with sub-millisecond precision */
static double now_seconds(void)
{
#ifdef _WIN32
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)f.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
#endif
}

static double load_best(void)
{
    double best = 0;
    FILE *fp = fopen(BEST_FILE, "r");
    if (fp) {
        if (fscanf(fp, "%lf", &best) != 1) best = 0;
        fclose(fp);
    }
    return best;
}

static void save_best(double wpm)
{
    FILE *fp = fopen(BEST_FILE, "w");
    if (fp) { fprintf(fp, "%.1f\n", wpm); fclose(fp); }
}

static const char *rank_title(double wpm)
{
    if (wpm < 15)  return "Sleepy Turtle";
    if (wpm < 30)  return "Steady Walker";
    if (wpm < 45)  return "Quick Runner";
    if (wpm < 60)  return "Fast Racer";
    if (wpm < 80)  return "Cheetah";
    return "LIGHTNING";
}

static void countdown(void)
{
    for (int i = 3; i >= 1; i--) {
        printf("  Get ready... %d\r", i);
        fflush(stdout);
        sleep_ms(800);
    }
    printf("  GO!            \n");
}

/* Prints your attempt: green = right, red = wrong, underscore = missing */
static void show_diff(const char *target, const char *typed)
{
    size_t tl = strlen(target), yl = strlen(typed);
    printf("  Result : ");
    for (size_t i = 0; i < tl; i++) {
        if (i >= yl)                    printf("\033[41m_\033[0m");
        else if (typed[i] == target[i]) printf("\033[32m%c\033[0m", typed[i]);
        else                            printf("\033[1;31m%c\033[0m", typed[i]);
    }
    for (size_t i = tl; i < yl; i++) printf("\033[1;31m%c\033[0m", typed[i]);   /* extra characters */
    printf("\n");
}

typedef struct {
    double wpm;
    double accuracy;
} RoundResult;

static int play_round(int number, int total, const char *target, RoundResult *res)
{
    char typed[MAX_INPUT];

    printf("\n--------------------------------------------------\n");
    printf("  Round %d of %d\n\n", number, total);
    printf("  Type this:\n\n  \033[1;36m%s\033[0m\n\n", target);
    countdown();

    printf("  > ");
    fflush(stdout);
    double t0 = now_seconds();
    if (!fgets(typed, sizeof typed, stdin)) return 0;
    double elapsed = now_seconds() - t0;
    typed[strcspn(typed, "\r\n")] = '\0';

    size_t tl = strlen(target), yl = strlen(typed);
    size_t correct = 0;
    for (size_t i = 0; i < tl && i < yl; i++)
        if (typed[i] == target[i]) correct++;

    size_t longest = tl > yl ? tl : yl;
    res->accuracy = longest ? 100.0 * (double)correct / (double)longest : 0;

    if (elapsed < 0.2) elapsed = 0.2;                         /* avoid dividing by ~0 */
    res->wpm = ((double)correct / 5.0) / (elapsed / 60.0);

    printf("\n");
    show_diff(target, typed);
    printf("  Time   : %.2f seconds\n", elapsed);
    printf("  Speed  : %.1f WPM\n", res->wpm);
    printf("  Accuracy: %.1f%%\n", res->accuracy);
    return 1;
}

int main(void)
{
    char line[32];
    srand((unsigned)time(NULL));
    enable_ansi();

    printf("=====================================\n");
    printf("        TYPING SPEED TESTER\n");
    printf("=====================================\n");

    double best = load_best();
    if (best > 0) printf("  Your best speed so far: %.1f WPM\n", best);

    printf("\n  How many rounds? (1-10): ");
    if (!fgets(line, sizeof line, stdin)) return 0;
    int rounds = atoi(line);
    if (rounds < 1)  rounds = 3;
    if (rounds > 10) rounds = 10;
    if (rounds > NUM_SENTENCES) rounds = NUM_SENTENCES;

    /* Shuffle the sentence order so no sentence repeats */
    int order[NUM_SENTENCES];
    for (int i = 0; i < NUM_SENTENCES; i++) order[i] = i;
    for (int i = NUM_SENTENCES - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int t = order[i]; order[i] = order[j]; order[j] = t;
    }

    double sum_wpm = 0, sum_acc = 0, top_wpm = 0;
    int done = 0;

    for (int r = 0; r < rounds; r++) {
        RoundResult res;
        if (!play_round(r + 1, rounds, SENTENCES[order[r]], &res)) break;
        sum_wpm += res.wpm;
        sum_acc += res.accuracy;
        if (res.wpm > top_wpm) top_wpm = res.wpm;
        done++;
    }

    if (done == 0) return 0;

    double avg_wpm = sum_wpm / done;
    double avg_acc = sum_acc / done;

    printf("\n==================================================\n");
    printf("                  FINAL RESULTS\n");
    printf("==================================================\n");
    printf("  Rounds played    : %d\n", done);
    printf("  Average speed    : %.1f WPM\n", avg_wpm);
    printf("  Fastest round    : %.1f WPM\n", top_wpm);
    printf("  Average accuracy : %.1f%%\n", avg_acc);
    printf("  Your rank        : \033[1;33m%s\033[0m\n", rank_title(avg_wpm));

    if (avg_acc >= 90 && avg_wpm > best) {
        save_best(avg_wpm);
        printf("\n  *** NEW PERSONAL BEST! ***\n");
    } else if (avg_acc < 90) {
        printf("\n  (Scores are only saved with 90%% accuracy or better.)\n");
    }
    printf("\n");
    return 0;
}
