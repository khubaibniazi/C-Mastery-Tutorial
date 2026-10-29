/*
 * ============================================================
 *  BANK MANAGEMENT SYSTEM  -  C Mastery Level
 * ============================================================
 *  Concepts used:
 *    - Structs and dynamic arrays (malloc / realloc / free)
 *    - Binary file storage   (accounts.dat)
 *    - Text file logging     (transactions.txt)
 *    - Input validation (never trust the user!)
 *    - PINs stored as a hash, never as plain text
 *    - Menu-driven programs
 *
 *  Features: create account, login with PIN, deposit, withdraw,
 *  transfer, statement, change PIN, close account, admin list.
 *  Admin password: admin123  (change ADMIN_PASS below)
 *
 *  NOTE: this is a learning project. A real bank needs far
 *  stronger security than a simple hash.
 *
 *  Compile:   gcc bank.c -o bank
 *  Run:       ./bank             (Windows: bank.exe)
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define ACC_FILE   "accounts.dat"
#define LOG_FILE   "transactions.txt"
#define FIRST_ACC  1001
#define ADMIN_PASS "admin123"
#define CUR        "$"          /* change to your own currency symbol */

typedef struct {
    int          accNo;
    char         name[48];
    unsigned int pinHash;
    double       balance;
} Account;

static Account *accounts = NULL;
static int      count = 0;

/* ======================= small helpers ======================= */

static unsigned int hash_pin(const char *pin)
{
    unsigned int h = 5381;                       /* djb2 hash with a fixed salt */
    for (const char *s = "bank:"; *s; s++) h = h * 33 + (unsigned char)*s;
    for (; *pin; pin++)                  h = h * 33 + (unsigned char)*pin;
    return h;
}

static void read_line(const char *prompt, char *buf, size_t size)
{
    printf("%s", prompt);
    fflush(stdout);
    if (!fgets(buf, (int)size, stdin)) { printf("\n"); exit(0); }
    buf[strcspn(buf, "\r\n")] = '\0';
}

static int read_int(const char *prompt, int min, int max)
{
    char buf[64];
    for (;;) {
        read_line(prompt, buf, sizeof buf);
        char *end;
        long v = strtol(buf, &end, 10);
        if (end != buf && *end == '\0' && v >= min && v <= max) return (int)v;
        printf("  Please enter a number between %d and %d.\n", min, max);
    }
}

static double read_money(const char *prompt)
{
    char buf[64];
    for (;;) {
        read_line(prompt, buf, sizeof buf);
        char *end;
        double v = strtod(buf, &end);
        if (end != buf && *end == '\0' && v > 0 && v <= 1e9) return v;
        printf("  Please enter a valid amount greater than 0.\n");
    }
}

static int valid_pin(const char *pin)
{
    if (strlen(pin) != 4) return 0;
    for (int i = 0; i < 4; i++) if (!isdigit((unsigned char)pin[i])) return 0;
    return 1;
}

static void read_new_pin(char *pin, size_t size)
{
    for (;;) {
        read_line("Choose a 4-digit PIN: ", pin, size);
        if (valid_pin(pin)) return;
        printf("  The PIN must be exactly 4 digits.\n");
    }
}

/* ======================= file storage ======================= */

static void load_accounts(void)
{
    FILE *fp = fopen(ACC_FILE, "rb");
    if (!fp) return;
    int n = 0;
    if (fread(&n, sizeof n, 1, fp) == 1 && n > 0) {
        accounts = malloc((size_t)n * sizeof(Account));
        if (accounts && fread(accounts, sizeof(Account), (size_t)n, fp) == (size_t)n)
            count = n;
        else { free(accounts); accounts = NULL; }
    }
    fclose(fp);
}

static void save_accounts(void)
{
    FILE *fp = fopen(ACC_FILE, "wb");
    if (!fp) { printf("  WARNING: could not save data!\n"); return; }
    fwrite(&count, sizeof count, 1, fp);
    if (count > 0) fwrite(accounts, sizeof(Account), (size_t)count, fp);
    fclose(fp);
}

static void log_tx(int accNo, const char *type, double amount, double balance)
{
    FILE *fp = fopen(LOG_FILE, "a");
    if (!fp) return;
    char stamp[32];
    time_t now = time(NULL);
    strftime(stamp, sizeof stamp, "%Y-%m-%d %H:%M:%S", localtime(&now));
    fprintf(fp, "%s|%d|%s|%.2f|%.2f\n", stamp, accNo, type, amount, balance);
    fclose(fp);
}

static int find_index(int accNo)
{
    for (int i = 0; i < count; i++)
        if (accounts[i].accNo == accNo) return i;
    return -1;
}

/* ======================= operations ======================= */

static void create_account(void)
{
    char name[48], pin[16];
    read_line("Full name: ", name, sizeof name);
    if (name[0] == '\0') { printf("  Name cannot be empty.\n"); return; }
    read_new_pin(pin, sizeof pin);

    double deposit = 0;
    char buf[32];
    read_line("Opening deposit (press Enter for 0): ", buf, sizeof buf);
    if (buf[0] != '\0') {
        char *end;
        deposit = strtod(buf, &end);
        if (*end != '\0' || deposit < 0 || deposit > 1e9) {
            printf("  Invalid amount. Account not created.\n");
            return;
        }
    }

    Account *tmp = realloc(accounts, (size_t)(count + 1) * sizeof(Account));
    if (!tmp) { printf("  Out of memory.\n"); return; }
    accounts = tmp;

    int next = FIRST_ACC;
    for (int i = 0; i < count; i++)
        if (accounts[i].accNo >= next) next = accounts[i].accNo + 1;

    Account *a = &accounts[count++];
    a->accNo = next;
    strncpy(a->name, name, sizeof a->name - 1);
    a->name[sizeof a->name - 1] = '\0';
    a->pinHash = hash_pin(pin);
    a->balance = deposit;

    save_accounts();
    log_tx(a->accNo, "OPEN", deposit, deposit);
    printf("\n  Account created!  Your account number is %d. Remember it!\n", a->accNo);
}

static int login(void)
{
    int no = read_int("Account number: ", 1, 999999);
    int i = find_index(no);
    if (i < 0) { printf("  No such account.\n"); return -1; }

    for (int t = 0; t < 3; t++) {
        char pin[16];
        read_line("PIN: ", pin, sizeof pin);
        if (hash_pin(pin) == accounts[i].pinHash) return i;
        printf("  Wrong PIN (%d attempt(s) left).\n", 2 - t);
    }
    printf("  Too many wrong attempts.\n");
    return -1;
}

static void deposit(int i)
{
    double amt = read_money("Amount to deposit: ");
    accounts[i].balance += amt;
    save_accounts();
    log_tx(accounts[i].accNo, "DEPOSIT", amt, accounts[i].balance);
    printf("  Done. New balance: %s%.2f\n", CUR, accounts[i].balance);
}

static void withdraw(int i)
{
    double amt = read_money("Amount to withdraw: ");
    if (amt > accounts[i].balance) {
        printf("  Insufficient funds. Your balance is %s%.2f\n", CUR, accounts[i].balance);
        return;
    }
    accounts[i].balance -= amt;
    save_accounts();
    log_tx(accounts[i].accNo, "WITHDRAW", amt, accounts[i].balance);
    printf("  Done. New balance: %s%.2f\n", CUR, accounts[i].balance);
}

static void transfer(int i)
{
    int to = read_int("Send to account number: ", 1, 999999);
    int j = find_index(to);
    if (j < 0)  { printf("  No such account.\n"); return; }
    if (j == i) { printf("  You cannot transfer to yourself.\n"); return; }

    printf("  Recipient: %s\n", accounts[j].name);
    double amt = read_money("Amount to transfer: ");
    if (amt > accounts[i].balance) {
        printf("  Insufficient funds. Your balance is %s%.2f\n", CUR, accounts[i].balance);
        return;
    }
    accounts[i].balance -= amt;
    accounts[j].balance += amt;
    save_accounts();
    log_tx(accounts[i].accNo, "TRANSFER_OUT", amt, accounts[i].balance);
    log_tx(accounts[j].accNo, "TRANSFER_IN",  amt, accounts[j].balance);
    printf("  Sent %s%.2f to %s. New balance: %s%.2f\n",
           CUR, amt, accounts[j].name, CUR, accounts[i].balance);
}

static void statement(int accNo)
{
    FILE *fp = fopen(LOG_FILE, "r");
    if (!fp) { printf("  No transactions yet.\n"); return; }

    enum { KEEP = 15 };
    char ring[KEEP][128];
    int total = 0;
    char line[128];

    while (fgets(line, sizeof line, fp)) {
        int no;
        if (sscanf(line, "%*[^|]|%d|", &no) == 1 && no == accNo) {
            strcpy(ring[total % KEEP], line);
            total++;
        }
    }
    fclose(fp);

    if (total == 0) { printf("  No transactions yet.\n"); return; }

    printf("\n  Last %d transaction(s):\n", total < KEEP ? total : KEEP);
    printf("  %-19s  %-13s  %10s  %10s\n", "Date", "Type", "Amount", "Balance");
    printf("  ------------------------------------------------------------\n");
    int start = total > KEEP ? total - KEEP : 0;
    for (int k = start; k < total; k++) {
        char date[32], type[32];
        int no;
        double amt, bal;
        if (sscanf(ring[k % KEEP], "%31[^|]|%d|%31[^|]|%lf|%lf", date, &no, type, &amt, &bal) == 5)
            printf("  %-19s  %-13s  %10.2f  %10.2f\n", date, type, amt, bal);
    }
}

static void change_pin(int i)
{
    char old[16], pin[16];
    read_line("Current PIN: ", old, sizeof old);
    if (hash_pin(old) != accounts[i].pinHash) { printf("  Wrong PIN.\n"); return; }
    read_new_pin(pin, sizeof pin);
    accounts[i].pinHash = hash_pin(pin);
    save_accounts();
    printf("  PIN changed.\n");
}

/* Returns 1 if the account was closed */
static int close_account(int i)
{
    char ans[16];
    printf("  Closing account %d. Remaining balance %s%.2f will be paid out.\n",
           accounts[i].accNo, CUR, accounts[i].balance);
    read_line("  Type YES to confirm: ", ans, sizeof ans);
    if (strcmp(ans, "YES") != 0) { printf("  Cancelled.\n"); return 0; }

    log_tx(accounts[i].accNo, "CLOSE", accounts[i].balance, 0);
    for (int k = i; k < count - 1; k++) accounts[k] = accounts[k + 1];   /* shift left */
    count--;
    save_accounts();
    printf("  Account closed.\n");
    return 1;
}

static void admin_list(void)
{
    char pass[32];
    read_line("Admin password: ", pass, sizeof pass);
    if (strcmp(pass, ADMIN_PASS) != 0) { printf("  Access denied.\n"); return; }

    double total = 0;
    printf("\n  %-8s  %-30s  %12s\n", "Acc No", "Name", "Balance");
    printf("  --------------------------------------------------------\n");
    for (int i = 0; i < count; i++) {
        printf("  %-8d  %-30s  %12.2f\n", accounts[i].accNo, accounts[i].name, accounts[i].balance);
        total += accounts[i].balance;
    }
    printf("  --------------------------------------------------------\n");
    printf("  %d account(s), total money held: %s%.2f\n", count, CUR, total);
}

static void account_menu(int i)
{
    for (;;) {
        printf("\n--- Welcome, %s (Acc %d) ---\n", accounts[i].name, accounts[i].accNo);
        printf(" 1. Check balance\n 2. Deposit\n 3. Withdraw\n 4. Transfer\n");
        printf(" 5. Statement\n 6. Change PIN\n 7. Close account\n 0. Log out\n");
        int c = read_int("Choice: ", 0, 7);
        switch (c) {
            case 1: printf("  Balance: %s%.2f\n", CUR, accounts[i].balance); break;
            case 2: deposit(i);  break;
            case 3: withdraw(i); break;
            case 4: transfer(i); break;
            case 5: statement(accounts[i].accNo); break;
            case 6: change_pin(i); break;
            case 7: if (close_account(i)) return; break;
            case 0: return;
        }
    }
}

int main(void)
{
    load_accounts();
    printf("=====================================\n");
    printf("      BANK MANAGEMENT SYSTEM\n");
    printf("=====================================\n");

    for (;;) {
        printf("\n 1. Create account\n 2. Login\n 3. Admin: list all accounts\n 0. Exit\n");
        int c = read_int("Choice: ", 0, 3);
        if (c == 0) break;
        if (c == 1) create_account();
        if (c == 2) { int i = login(); if (i >= 0) account_menu(i); }
        if (c == 3) admin_list();
    }

    free(accounts);
    printf("Goodbye!\n");
    return 0;
}
