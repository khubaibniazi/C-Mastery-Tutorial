/*
 * ============================================================
 *  FILE ENCRYPTION TOOL  -  C Mastery Level
 * ============================================================
 *  Locks any file (text, image, PDF, anything) with a password
 *  and unlocks it again.
 *
 *  How it works:
 *    1. Your password + a random "salt" are mixed 100,000 times
 *       into a 64-bit key (key derivation).
 *    2. The key seeds a pseudo-random number generator that
 *       produces a stream of random bytes (the keystream).
 *    3. Every byte of the file is XORed with a keystream byte.
 *       XOR is its own undo:  (a ^ k) ^ k == a
 *    4. A small header stores the salt and a password check, so
 *       a wrong password is detected BEFORE any output is made.
 *
 *  Concepts used:
 *    - Bitwise operators (^  <<  >>)
 *    - Binary file I/O (fread / fwrite in chunks)
 *    - 64-bit integers, hashing, command-line arguments
 *    - Reading a password without showing it
 *
 *  !!! IMPORTANT !!!
 *  This is a LEARNING project. It is NOT secure enough to
 *  protect real secrets. Real software uses AES or ChaCha20.
 *
 *  Compile:   gcc crypt.c -o crypt
 *  Run:       ./crypt                          (menu)
 *             ./crypt e secret.txt             (encrypt -> secret.txt.enc)
 *             ./crypt d secret.txt.enc         (decrypt -> secret.txt)
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#ifdef _WIN32
    #include <conio.h>
#else
    #include <unistd.h>
    #include <termios.h>
#endif

typedef unsigned long long u64;
typedef unsigned char      u8;

#define MAGIC      "CENC1"
#define MAGIC_LEN  5
#define CHUNK      65536
#define EXT        ".enc"

/* ======================= crypto pieces ======================= */

static u64 fnv1a(const u8 *data, size_t n, u64 h)
{
    for (size_t i = 0; i < n; i++) {
        h ^= data[i];
        h *= 1099511628211ULL;
    }
    return h;
}

/* Turn a password + salt into a 64-bit key by repeated hashing */
static u64 derive_key(const char *password, u64 salt)
{
    u64 h = 14695981039346656037ULL ^ salt;
    size_t len = strlen(password);
    for (u64 round = 0; round < 100000; round++) {
        h = fnv1a((const u8 *)password, len, h);
        h ^= round;
        h ^= h >> 29;
        h *= 0xBF58476D1CE4E5B9ULL;
    }
    return h;
}

/* xorshift64*: a fast pseudo-random generator. Same seed -> same stream. */
static u64 next_random(u64 *state)
{
    u64 x = *state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    *state = x;
    return x * 2685821657736338717ULL;
}

typedef struct {
    u64 state;
    u64 word;
    int left;           /* unused bytes remaining in 'word' */
} KeyStream;

static void ks_init(KeyStream *ks, u64 key)
{
    ks->state = key ? key : 0x9E3779B97F4A7C15ULL;    /* state must never be 0 */
    ks->word = 0;
    ks->left = 0;
}

static u8 ks_next(KeyStream *ks)
{
    if (ks->left == 0) { ks->word = next_random(&ks->state); ks->left = 8; }
    u8 b = (u8)(ks->word & 0xFF);
    ks->word >>= 8;
    ks->left--;
    return b;
}

static u64 password_check(u64 key)
{
    return fnv1a((const u8 *)&key, sizeof key, 0xCBF29CE484222325ULL ^ 0x5A5A5A5A5A5A5A5AULL);
}

/* Write/read a 64-bit number byte by byte so the file format is the same on every computer */
static void put_u64(FILE *fp, u64 v)
{
    for (int i = 0; i < 8; i++) fputc((int)((v >> (8 * i)) & 0xFF), fp);
}

static int get_u64(FILE *fp, u64 *v)
{
    *v = 0;
    for (int i = 0; i < 8; i++) {
        int c = fgetc(fp);
        if (c == EOF) return 0;
        *v |= ((u64)c) << (8 * i);
    }
    return 1;
}

static u64 random_salt(void)
{
    u64 s = (u64)time(NULL) * 6364136223846793005ULL;
    s ^= (u64)clock() << 20;
    s ^= (u64)(size_t)&s;                       /* address varies between runs */
    u64 st = s ? s : 1;
    next_random(&st);
    return next_random(&st);
}

/* ======================= password input ======================= */

static void read_password(const char *prompt, char *buf, size_t size)
{
    printf("%s", prompt);
    fflush(stdout);
#ifdef _WIN32
    size_t i = 0;
    for (;;) {
        int c = _getch();
        if (c == '\r' || c == '\n') break;
        if (c == '\b') { if (i > 0) { i--; printf("\b \b"); } continue; }
        if (i + 1 < size) { buf[i++] = (char)c; printf("*"); }
    }
    buf[i] = '\0';
    printf("\n");
#else
    struct termios old, hidden;
    int is_tty = (tcgetattr(STDIN_FILENO, &old) == 0);
    if (is_tty) {
        hidden = old;
        hidden.c_lflag &= ~ECHO;                 /* do not show what is typed */
        tcsetattr(STDIN_FILENO, TCSANOW, &hidden);
    }
    if (!fgets(buf, (int)size, stdin)) buf[0] = '\0';
    if (is_tty) { tcsetattr(STDIN_FILENO, TCSANOW, &old); printf("\n"); }
    buf[strcspn(buf, "\r\n")] = '\0';
#endif
}

/* ======================= encrypt / decrypt ======================= */

static void show_progress(long done, long total)
{
    if (total <= 0) return;
    printf("\r  Working... %3d%%", (int)(done * 100 / total));
    fflush(stdout);
}

static long file_size(FILE *fp)
{
    long cur = ftell(fp);
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, cur, SEEK_SET);
    return size;
}

/* Copies in -> out, XORing every byte with the keystream. */
static int stream_xor(FILE *in, FILE *out, u64 key, long total)
{
    static u8 buf[CHUNK];
    KeyStream ks;
    ks_init(&ks, key);

    long done = 0;
    size_t got;
    while ((got = fread(buf, 1, sizeof buf, in)) > 0) {
        for (size_t i = 0; i < got; i++) buf[i] ^= ks_next(&ks);
        if (fwrite(buf, 1, got, out) != got) { printf("\n  Write error (disk full?).\n"); return 0; }
        done += (long)got;
        show_progress(done, total);
    }
    show_progress(total, total);
    printf("\n");
    return 1;
}

static int encrypt_file(const char *inname, const char *outname, const char *password)
{
    FILE *in = fopen(inname, "rb");
    if (!in) { printf("  Cannot open '%s'.\n", inname); return 0; }

    FILE *out = fopen(outname, "wb");
    if (!out) { printf("  Cannot create '%s'.\n", outname); fclose(in); return 0; }

    u64 salt = random_salt();
    u64 key  = derive_key(password, salt);

    fwrite(MAGIC, 1, MAGIC_LEN, out);
    put_u64(out, salt);
    put_u64(out, password_check(key));

    long total = file_size(in);
    int ok = stream_xor(in, out, key, total);

    fclose(in);
    fclose(out);
    if (ok) printf("  Encrypted %ld bytes -> %s\n", total, outname);
    else    remove(outname);
    return ok;
}

static int decrypt_file(const char *inname, const char *outname, const char *password)
{
    FILE *in = fopen(inname, "rb");
    if (!in) { printf("  Cannot open '%s'.\n", inname); return 0; }

    char magic[MAGIC_LEN];
    u64 salt, check;
    if (fread(magic, 1, MAGIC_LEN, in) != MAGIC_LEN || memcmp(magic, MAGIC, MAGIC_LEN) != 0 ||
        !get_u64(in, &salt) || !get_u64(in, &check)) {
        printf("  '%s' is not a file made by this tool.\n", inname);
        fclose(in);
        return 0;
    }

    u64 key = derive_key(password, salt);
    if (password_check(key) != check) {
        printf("  WRONG PASSWORD. Nothing was written.\n");
        fclose(in);
        return 0;
    }

    FILE *out = fopen(outname, "wb");
    if (!out) { printf("  Cannot create '%s'.\n", outname); fclose(in); return 0; }

    long total = file_size(in) - (MAGIC_LEN + 16);
    int ok = stream_xor(in, out, key, total);

    fclose(in);
    fclose(out);
    if (ok) printf("  Decrypted %ld bytes -> %s\n", total, outname);
    else    remove(outname);
    return ok;
}

/* ======================= user interface ======================= */

static void make_output_name(int encrypt, const char *in, char *out, size_t size)
{
    size_t len = strlen(in), ext = strlen(EXT);
    if (encrypt) {
        snprintf(out, size, "%s%s", in, EXT);
    } else if (len > ext && strcmp(in + len - ext, EXT) == 0) {
        snprintf(out, size, "%.*s", (int)(len - ext), in);      /* strip ".enc" */
    } else {
        snprintf(out, size, "%s.dec", in);
    }
}

static int run(int encrypt, const char *inname, const char *given_out)
{
    char outname[512], pw[128], pw2[128];

    if (given_out) snprintf(outname, sizeof outname, "%s", given_out);
    else           make_output_name(encrypt, inname, outname, sizeof outname);

    if (strcmp(inname, outname) == 0) { printf("  Input and output must be different files.\n"); return 0; }

    FILE *test = fopen(outname, "rb");
    if (test) {
        fclose(test);
        printf("  '%s' already exists. Overwrite? (y/N): ", outname);
        int c = getchar();
        while (getchar() != '\n' && !feof(stdin)) { }
        if (tolower(c) != 'y') { printf("  Cancelled.\n"); return 0; }
    }

    read_password("  Password: ", pw, sizeof pw);
    if (pw[0] == '\0') { printf("  The password cannot be empty.\n"); return 0; }
    if (encrypt) {
        read_password("  Repeat password: ", pw2, sizeof pw2);
        if (strcmp(pw, pw2) != 0) { printf("  The passwords do not match.\n"); return 0; }
    }

    return encrypt ? encrypt_file(inname, outname, pw) : decrypt_file(inname, outname, pw);
}

int main(int argc, char **argv)
{
    /* command-line mode: crypt e|d <file> [output] */
    if (argc >= 3) {
        int enc = (tolower((unsigned char)argv[1][0]) == 'e');
        int dec = (tolower((unsigned char)argv[1][0]) == 'd');
        if (!enc && !dec) { printf("Usage: %s e|d <file> [output]\n", argv[0]); return 1; }
        return run(enc, argv[2], argc >= 4 ? argv[3] : NULL) ? 0 : 1;
    }

    printf("=====================================\n");
    printf("        FILE ENCRYPTION TOOL\n");
    printf("=====================================\n");
    printf("(Learning project - not for real secrets!)\n");

    for (;;) {
        char choice[16], name[512];
        printf("\n 1. Encrypt a file\n 2. Decrypt a file\n 0. Exit\nChoice: ");
        if (!fgets(choice, sizeof choice, stdin)) break;
        int c = atoi(choice);
        if (c == 0) break;
        if (c != 1 && c != 2) continue;

        printf("File name: ");
        if (!fgets(name, sizeof name, stdin)) break;
        name[strcspn(name, "\r\n")] = '\0';
        if (name[0] == '\0') continue;

        run(c == 1, name, NULL);
    }
    printf("Goodbye!\n");
    return 0;
}
