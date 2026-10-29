/*
 * ============================================================
 *  HANGMAN WITH WORD FILE  -  C Mastery Level
 * ============================================================
 *  Concepts used:
 *    - File handling (words are loaded from words.txt)
 *    - Dynamic memory (malloc / realloc / free)
 *    - Array of strings, char arrays, string functions
 *    - rand() for random word selection
 *
 *  Add your own words to words.txt (one word per line).
 *  If words.txt is missing, a built-in list is used.
 *
 *  Compile:   gcc hangman.c -o hangman
 *  Run:       ./hangman          (Windows: hangman.exe)
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

#define MAX_WRONG 6
#define WORD_FILE "words.txt"

static char **words = NULL;
static int    word_count = 0;

static const char *STAGES[MAX_WRONG + 1] = {
    "  +---+\n  |   |\n      |\n      |\n      |\n      |\n=========",
    "  +---+\n  |   |\n  O   |\n      |\n      |\n      |\n=========",
    "  +---+\n  |   |\n  O   |\n  |   |\n      |\n      |\n=========",
    "  +---+\n  |   |\n  O   |\n /|   |\n      |\n      |\n=========",
    "  +---+\n  |   |\n  O   |\n /|\\  |\n      |\n      |\n=========",
    "  +---+\n  |   |\n  O   |\n /|\\  |\n /    |\n      |\n=========",
    "  +---+\n  |   |\n  O   |\n /|\\  |\n / \\  |\n      |\n========="
};

static const char *BUILTIN[] = {
    "POINTER", "FUNCTION", "VARIABLE", "COMPILER", "STRUCTURE", "RECURSION",
    "ARRAY", "MEMORY", "KEYBOARD", "PROGRAM", "ALGORITHM", "DEBUGGING"
};

static void enable_ansi(void)
{
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(h, &mode)) SetConsoleMode(h, mode | 0x0004);
#endif
}

static char *copy_string(const char *s)
{
    char *p = malloc(strlen(s) + 1);
    if (!p) { fprintf(stderr, "Out of memory\n"); exit(1); }
    strcpy(p, s);
    return p;
}

static void add_word(const char *w)
{
    char **tmp = realloc(words, (word_count + 1) * sizeof(char *));
    if (!tmp) { fprintf(stderr, "Out of memory\n"); exit(1); }
    words = tmp;
    words[word_count++] = copy_string(w);
}

static void load_words(void)
{
    FILE *fp = fopen(WORD_FILE, "r");
    if (fp) {
        char line[128];
        while (fgets(line, sizeof line, fp)) {
            line[strcspn(line, "\r\n")] = '\0';
            int ok = (line[0] != '\0');
            for (int i = 0; line[i]; i++) {
                if (!isalpha((unsigned char)line[i])) { ok = 0; break; }
                line[i] = (char)toupper((unsigned char)line[i]);
            }
            if (ok) add_word(line);
        }
        fclose(fp);
    }

    if (word_count == 0) {
        printf("(%s not found, using the built-in word list)\n", WORD_FILE);
        for (size_t i = 0; i < sizeof BUILTIN / sizeof BUILTIN[0]; i++)
            add_word(BUILTIN[i]);
    } else {
        printf("Loaded %d words from %s\n", word_count, WORD_FILE);
    }
}

static int is_complete(const char *word, const int guessed[26])
{
    for (int i = 0; word[i]; i++)
        if (!guessed[word[i] - 'A']) return 0;
    return 1;
}

static void draw(const char *word, const int guessed[26], int wrong, const char *msg)
{
    printf("\033[H\033[J");
    printf("===== H A N G M A N =====\n\n");
    printf("%s\n\n  Word:  ", STAGES[wrong]);
    for (int i = 0; word[i]; i++) {
        if (guessed[word[i] - 'A']) printf("%c ", word[i]);
        else                        printf("_ ");
    }
    printf("\n\n  Lives left: %d\n  Used letters: ", MAX_WRONG - wrong);
    for (int c = 0; c < 26; c++)
        if (guessed[c]) printf("%c ", 'A' + c);
    printf("\n\n  %s\n", msg);
}

/* Reveal one random hidden letter (costs one life) */
static void give_hint(const char *word, int guessed[26])
{
    int hidden[26], n = 0;
    for (int i = 0; word[i]; i++) {
        int c = word[i] - 'A';
        if (!guessed[c]) {
            int dup = 0;
            for (int k = 0; k < n; k++) if (hidden[k] == c) dup = 1;
            if (!dup) hidden[n++] = c;
        }
    }
    if (n > 0) guessed[hidden[rand() % n]] = 1;
}

/* Returns 1 if the player won */
static int play_round(const char *word)
{
    int guessed[26] = {0};
    int wrong = 0, hint_used = 0;
    char msg[128] = "Type a letter, a whole word, or ? for a hint (costs 1 life).";

    while (wrong < MAX_WRONG && !is_complete(word, guessed)) {
        draw(word, guessed, wrong, msg);
        printf("  Your guess: ");

        char line[64];
        if (!fgets(line, sizeof line, stdin)) exit(0);
        line[strcspn(line, "\r\n")] = '\0';

        if (line[0] == '\0') { strcpy(msg, "Please type something."); continue; }

        if (strcmp(line, "?") == 0) {
            if (hint_used)           strcpy(msg, "You already used your hint.");
            else if (wrong >= MAX_WRONG - 1) strcpy(msg, "Too risky - that would cost your last life!");
            else {
                give_hint(word, guessed);
                hint_used = 1;
                wrong++;
                strcpy(msg, "A letter was revealed. It cost you a life.");
            }
            continue;
        }

        int all_alpha = 1;
        for (int i = 0; line[i]; i++) {
            if (!isalpha((unsigned char)line[i])) all_alpha = 0;
            line[i] = (char)toupper((unsigned char)line[i]);
        }
        if (!all_alpha) { strcpy(msg, "Letters only, please."); continue; }

        if (line[1] == '\0') {                       /* single letter */
            int c = line[0] - 'A';
            if (guessed[c]) { strcpy(msg, "You already tried that letter."); continue; }
            guessed[c] = 1;
            if (strchr(word, line[0])) strcpy(msg, "Good guess!");
            else { wrong++; strcpy(msg, "Nope, that letter is not in the word."); }
        } else {                                     /* whole-word guess */
            if (strcmp(line, word) == 0) {
                for (int i = 0; word[i]; i++) guessed[word[i] - 'A'] = 1;
            } else {
                wrong++;
                strcpy(msg, "That is not the word.");
            }
        }
    }

    int won = is_complete(word, guessed);
    for (int i = 0; word[i]; i++) guessed[word[i] - 'A'] = 1;
    draw(word, guessed, wrong, won ? "YOU WON! The man is saved!" : "YOU LOST... the man hangs.");
    printf("  The word was: %s\n", word);
    return won;
}

int main(void)
{
    srand((unsigned)time(NULL));
    enable_ansi();
    load_words();

    int wins = 0, losses = 0;
    for (;;) {
        const char *word = words[rand() % word_count];
        if (play_round(word)) wins++; else losses++;

        printf("\n  Score: %d won, %d lost\n  Play again? (Y/N): ", wins, losses);
        char line[16];
        if (!fgets(line, sizeof line, stdin) || tolower((unsigned char)line[0]) != 'y')
            break;
    }

    for (int i = 0; i < word_count; i++) free(words[i]);
    free(words);
    printf("Thanks for playing!\n");
    return 0;
}
