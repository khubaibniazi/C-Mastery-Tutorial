/*
 * ============================================================
 *  STUDENT RECORD DATABASE  -  C Mastery Level
 * ============================================================
 *  Concepts used:
 *    - Singly linked list (malloc / free)
 *    - Pointer-to-pointer trick for deleting nodes
 *    - Function pointers (one sort function, many comparators)
 *    - Binary file I/O (students.dat) - data survives restarts
 *    - Input validation
 *
 *  Features: add, show, search by ID or name, update, delete,
 *  sort (ID / name / marks), statistics.
 *
 *  Compile:   gcc students.c -o students
 *  Run:       ./students         (Windows: students.exe)
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define DATA_FILE "students.dat"
#define PASS_MARK 40.0f

typedef struct {
    int   id;
    char  name[40];
    float marks;
} Student;

typedef struct Node {
    Student      data;
    struct Node *next;
} Node;

static Node *head = NULL;

/* ======================= input helpers ======================= */

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
        printf("  Enter a whole number from %d to %d.\n", min, max);
    }
}

static float read_marks(const char *prompt)
{
    char buf[64];
    for (;;) {
        read_line(prompt, buf, sizeof buf);
        char *end;
        double v = strtod(buf, &end);
        if (end != buf && *end == '\0' && v >= 0 && v <= 100) return (float)v;
        printf("  Marks must be between 0 and 100.\n");
    }
}

/* ======================= linked list basics ======================= */

static void append(const Student *s)
{
    Node *n = malloc(sizeof *n);
    if (!n) { printf("Out of memory!\n"); exit(1); }
    n->data = *s;
    n->next = NULL;

    if (!head) { head = n; return; }
    Node *t = head;
    while (t->next) t = t->next;
    t->next = n;
}

static Node *find_id(int id)
{
    for (Node *n = head; n; n = n->next)
        if (n->data.id == id) return n;
    return NULL;
}

static void free_all(void)
{
    while (head) {
        Node *dead = head;
        head = head->next;
        free(dead);
    }
}

/* ======================= file storage ======================= */

static void load(void)
{
    FILE *fp = fopen(DATA_FILE, "rb");
    if (!fp) return;
    Student s;
    while (fread(&s, sizeof s, 1, fp) == 1) append(&s);
    fclose(fp);
}

static void save(void)
{
    FILE *fp = fopen(DATA_FILE, "wb");
    if (!fp) { printf("  WARNING: could not save to %s\n", DATA_FILE); return; }
    for (Node *n = head; n; n = n->next) fwrite(&n->data, sizeof(Student), 1, fp);
    fclose(fp);
}

/* ======================= display ======================= */

static const char *grade_of(float m)
{
    if (m >= 90) return "A+";
    if (m >= 80) return "A";
    if (m >= 70) return "B";
    if (m >= 60) return "C";
    if (m >= PASS_MARK) return "D";
    return "F";
}

static void print_header(void)
{
    printf("\n  %-6s  %-28s  %7s  %-5s\n", "ID", "Name", "Marks", "Grade");
    printf("  ---------------------------------------------------\n");
}

static void print_student(const Student *s)
{
    printf("  %-6d  %-28s  %7.2f  %-5s\n", s->id, s->name, s->marks, grade_of(s->marks));
}

static int list_length(void)
{
    int c = 0;
    for (Node *n = head; n; n = n->next) c++;
    return c;
}

/* ======================= menu actions ======================= */

static void add_student(void)
{
    Student s;
    s.id = read_int("Student ID (1-99999): ", 1, 99999);
    if (find_id(s.id)) { printf("  That ID already exists.\n"); return; }

    read_line("Name: ", s.name, sizeof s.name);
    if (s.name[0] == '\0') { printf("  Name cannot be empty.\n"); return; }
    s.marks = read_marks("Marks (0-100): ");

    append(&s);
    save();
    printf("  Student added.\n");
}

static void show_all(void)
{
    if (!head) { printf("  The database is empty.\n"); return; }
    print_header();
    for (Node *n = head; n; n = n->next) print_student(&n->data);
    printf("\n  Total students: %d\n", list_length());
}

static void search_id(void)
{
    int id = read_int("ID to search: ", 1, 99999);
    Node *n = find_id(id);
    if (!n) { printf("  Not found.\n"); return; }
    print_header();
    print_student(&n->data);
}

static void to_lower_copy(const char *src, char *dst, size_t size)
{
    size_t i;
    for (i = 0; src[i] && i < size - 1; i++) dst[i] = (char)tolower((unsigned char)src[i]);
    dst[i] = '\0';
}

static void search_name(void)
{
    char key[40], low_key[40], low_name[40];
    read_line("Name (or part of it): ", key, sizeof key);
    if (key[0] == '\0') return;
    to_lower_copy(key, low_key, sizeof low_key);

    int found = 0;
    for (Node *n = head; n; n = n->next) {
        to_lower_copy(n->data.name, low_name, sizeof low_name);
        if (strstr(low_name, low_key)) {
            if (!found) print_header();
            print_student(&n->data);
            found++;
        }
    }
    if (!found) printf("  No matching students.\n");
}

static void update_student(void)
{
    int id = read_int("ID to update: ", 1, 99999);
    Node *n = find_id(id);
    if (!n) { printf("  Not found.\n"); return; }

    print_header();
    print_student(&n->data);
    printf("\n  1. Change name\n  2. Change marks\n");
    int c = read_int("Choice: ", 1, 2);
    if (c == 1) {
        char name[40];
        read_line("New name: ", name, sizeof name);
        if (name[0] == '\0') { printf("  Name cannot be empty.\n"); return; }
        strcpy(n->data.name, name);
    } else {
        n->data.marks = read_marks("New marks (0-100): ");
    }
    save();
    printf("  Updated.\n");
}

/* Deleting with a pointer-to-pointer: no special case for the head node! */
static void delete_student(void)
{
    int id = read_int("ID to delete: ", 1, 99999);
    Node **pp = &head;
    while (*pp && (*pp)->data.id != id) pp = &(*pp)->next;

    if (!*pp) { printf("  Not found.\n"); return; }

    Node *dead = *pp;
    printf("  Delete %s? (Y/N): ", dead->data.name);
    char ans[8];
    read_line("", ans, sizeof ans);
    if (tolower((unsigned char)ans[0]) != 'y') { printf("  Cancelled.\n"); return; }

    *pp = dead->next;
    free(dead);
    save();
    printf("  Deleted.\n");
}

/* ---- sorting with function pointers ---- */
typedef int (*Comparator)(const Student *, const Student *);

static int by_id(const Student *a, const Student *b)    { return a->id - b->id; }
static int by_name(const Student *a, const Student *b)  { return strcmp(a->name, b->name); }
static int by_marks(const Student *a, const Student *b) { return (a->marks > b->marks) - (a->marks < b->marks); }

static void sort_list(Comparator cmp, int descending)
{
    int swapped;
    do {
        swapped = 0;
        for (Node *n = head; n && n->next; n = n->next) {
            int r = cmp(&n->data, &n->next->data);
            if (descending) r = -r;
            if (r > 0) {
                Student tmp = n->data;
                n->data = n->next->data;
                n->next->data = tmp;
                swapped = 1;
            }
        }
    } while (swapped);
}

static void sort_menu(void)
{
    if (!head) { printf("  The database is empty.\n"); return; }
    printf("  Sort by:  1. ID   2. Name   3. Marks\n");
    int f = read_int("Choice: ", 1, 3);
    printf("  Order:    1. Ascending   2. Descending\n");
    int o = read_int("Choice: ", 1, 2);

    Comparator cmp = (f == 1) ? by_id : (f == 2) ? by_name : by_marks;
    sort_list(cmp, o == 2);
    save();
    show_all();
}

static void statistics(void)
{
    if (!head) { printf("  The database is empty.\n"); return; }
    int n = 0, passed = 0;
    float sum = 0;
    Node *best = head, *worst = head;

    for (Node *p = head; p; p = p->next) {
        n++;
        sum += p->data.marks;
        if (p->data.marks >= PASS_MARK) passed++;
        if (p->data.marks > best->data.marks)  best = p;
        if (p->data.marks < worst->data.marks) worst = p;
    }

    printf("\n  Students      : %d\n", n);
    printf("  Average marks : %.2f\n", sum / n);
    printf("  Highest       : %.2f  (%s)\n", best->data.marks, best->data.name);
    printf("  Lowest        : %.2f  (%s)\n", worst->data.marks, worst->data.name);
    printf("  Pass rate     : %.1f%%  (%d of %d)\n", 100.0 * passed / n, passed, n);
}

int main(void)
{
    load();
    printf("=====================================\n");
    printf("      STUDENT RECORD DATABASE\n");
    printf("=====================================\n");

    for (;;) {
        printf("\n 1. Add student        5. Update student\n");
        printf(" 2. Show all           6. Delete student\n");
        printf(" 3. Search by ID       7. Sort records\n");
        printf(" 4. Search by name     8. Statistics\n");
        printf(" 0. Exit\n");
        int c = read_int("Choice: ", 0, 8);
        if (c == 0) break;
        switch (c) {
            case 1: add_student();    break;
            case 2: show_all();       break;
            case 3: search_id();      break;
            case 4: search_name();    break;
            case 5: update_student(); break;
            case 6: delete_student(); break;
            case 7: sort_menu();      break;
            case 8: statistics();     break;
        }
    }

    free_all();
    printf("Goodbye! Your data is saved in %s\n", DATA_FILE);
    return 0;
}
