/*
 * ============================================================
 *  ESCAPE THE HAUNTED MANSION  -  C Mastery Level
 * ============================================================
 *  A text adventure game. Explore rooms, collect items, solve
 *  the puzzle, grab the golden idol and escape!
 *
 *  Concepts used:
 *    - Structs and arrays of structs (rooms and items)
 *    - Enums for readable code
 *    - Game state (flags and counters)
 *    - A simple command parser:  <verb> <noun>
 *    - Pointers and string functions
 *
 *  Type "help" in the game to see all commands.
 *
 *  Compile:   gcc adventure.c -o adventure
 *  Run:       ./adventure        (Windows: adventure.exe)
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_INV    4
#define LANTERN_OIL 10

enum { N, E, S, W };
enum { HALL, LIBRARY, STUDY, KITCHEN, GARDEN, CELLAR, VAULT, NUM_ROOMS };
enum { LANTERN, BREAD, KEY, SCROLL, IDOL, NUM_ITEMS };

#define INV    (-1)    /* item is in your inventory   */
#define HIDDEN (-3)    /* item exists but is not found yet */

typedef struct {
    const char *name;
    const char *desc;
    int         go[4];      /* room reached by N, E, S, W (-1 = no exit) */
} Room;

typedef struct {
    const char *name;
    const char *desc;
    int         where;      /* room number, INV or HIDDEN */
} Item;

static const Room rooms[NUM_ROOMS] = {
    [HALL] = { "Entrance Hall",
        "A huge hall with a cracked marble floor. Portraits stare at you.\n"
        "The front door slammed shut behind you and will not budge.",
        { LIBRARY, GARDEN, CELLAR, KITCHEN } },
    [LIBRARY] = { "Dusty Library",
        "Shelves of rotting books reach the ceiling. Cobwebs everywhere.",
        { -1, STUDY, HALL, -1 } },
    [STUDY] = { "Old Study",
        "A heavy oak desk sits in the middle of the room. One drawer looks\n"
        "oddly crooked. Maybe you should search it.",
        { -1, -1, -1, LIBRARY } },
    [KITCHEN] = { "Abandoned Kitchen",
        "Rusty pots hang from the ceiling. Something smells... old.",
        { -1, HALL, -1, -1 } },
    [GARDEN] = { "Overgrown Garden",
        "Dead roses and tall weeds. In the wall stands a tall iron gate -\n"
        "the way out! Its lock has a strange golden-shaped hole.",
        { -1, -1, -1, HALL } },
    [CELLAR] = { "Dark Cellar",
        "Damp stone walls. At the east end is a heavy iron door.",
        { HALL, VAULT, -1, -1 } },
    [VAULT] = { "Hidden Vault",
        "A small stone chamber. Gold coins glitter on the floor, and on a\n"
        "pedestal rests a magnificent idol.",
        { -1, -1, -1, CELLAR } },
};

static Item items[NUM_ITEMS] = {
    [LANTERN] = { "lantern", "A rusty oil lantern. It still has a little oil.", KITCHEN },
    [BREAD]   = { "bread",   "A loaf of bread, hard as a rock.",               KITCHEN },
    [KEY]     = { "key",     "A small brass key.",                             HIDDEN  },
    [SCROLL]  = { "scroll",  "An old scroll covered in tiny writing.",         LIBRARY },
    [IDOL]    = { "idol",    "A heavy golden idol. It hums softly.",           VAULT   },
};

/* ---------------- game state ---------------- */
static int room = HALL;
static int moves = 0;
static int lantern_lit = 0;
static int oil = LANTERN_OIL;
static int vault_open = 0;
static int game_over = 0;

/* ---------------- helpers ---------------- */

static int inventory_count(void)
{
    int c = 0;
    for (int i = 0; i < NUM_ITEMS; i++) if (items[i].where == INV) c++;
    return c;
}

static int has_light(void)
{
    return lantern_lit && items[LANTERN].where == INV && oil > 0;
}

static int is_dark(int r) { return r == CELLAR || r == VAULT; }

static int find_item(const char *name, int where)
{
    for (int i = 0; i < NUM_ITEMS; i++)
        if (strcmp(items[i].name, name) == 0 && items[i].where == where) return i;
    return -1;
}

static void show_room(void)
{
    printf("\n== %s ==\n", rooms[room].name);

    if (is_dark(room) && !has_light()) {
        printf("It is pitch black. You cannot see a thing. You need a light!\n");
    } else {
        printf("%s\n", rooms[room].desc);

        int any = 0;
        for (int i = 0; i < NUM_ITEMS; i++) {
            if (items[i].where == room) {
                printf("%s%s", any ? ", " : "You see: ", items[i].name);
                any = 1;
            }
        }
        if (any) printf("\n");
    }

    printf("Exits:");
    static const char *dir_names[4] = {"north", "east", "south", "west"};
    for (int d = 0; d < 4; d++)
        if (rooms[room].go[d] != -1) printf(" %s", dir_names[d]);
    printf("\n");
}

static void lose(const char *why)
{
    printf("\n%s\n\n*** GAME OVER ***\n", why);
    game_over = 1;
}

static void move_player(int dir)
{
    int dest = rooms[room].go[dir];
    if (dest == -1) { printf("You can't go that way.\n"); return; }

    if (dest == CELLAR && !has_light()) {
        printf("The cellar is pitch black. You hear something breathing down there.\n"
               "You step back. You need a light first.\n");
        return;
    }
    if (room == CELLAR && dest == VAULT && !vault_open) {
        printf("The iron door is locked tight. Maybe a key would help?\n");
        return;
    }

    room = dest;
    moves++;
    show_room();

    /* The lantern burns oil only in the dark rooms */
    if (is_dark(room) && has_light()) {
        oil--;
        if (oil == 3) printf("\nThe lantern flickers. It is running low on oil!\n");
        if (oil <= 0) lose("The lantern dies. Darkness swallows you whole...");
    }
}

static int parse_direction(const char *w)
{
    if (!strcmp(w, "n") || !strcmp(w, "north")) return N;
    if (!strcmp(w, "e") || !strcmp(w, "east"))  return E;
    if (!strcmp(w, "s") || !strcmp(w, "south")) return S;
    if (!strcmp(w, "w") || !strcmp(w, "west"))  return W;
    return -1;
}

static void show_help(void)
{
    printf("\nCommands:\n");
    printf("  n / e / s / w      move (or: go north)\n");
    printf("  look               look around again\n");
    printf("  take <item>        pick something up (max %d items)\n", MAX_INV);
    printf("  drop <item>        drop something\n");
    printf("  inventory (or i)   see what you carry\n");
    printf("  use <item>         use an item\n");
    printf("  read <item>        read something\n");
    printf("  eat <item>         eat something\n");
    printf("  search             search the room carefully\n");
    printf("  quit               give up\n");
}

static void do_take(const char *noun)
{
    if (noun[0] == '\0') { printf("Take what?\n"); return; }
    if (is_dark(room) && !has_light()) { printf("You can't find anything in the dark.\n"); return; }
    int i = find_item(noun, room);
    if (i < 0) { printf("There is no '%s' here.\n", noun); return; }
    if (inventory_count() >= MAX_INV) {
        printf("Your hands are full! (max %d items) Drop something first.\n", MAX_INV);
        return;
    }
    items[i].where = INV;
    printf("You take the %s.\n", items[i].name);
}

static void do_drop(const char *noun)
{
    int i = find_item(noun, INV);
    if (i < 0) { printf("You don't have '%s'.\n", noun); return; }
    items[i].where = room;
    printf("You drop the %s.\n", items[i].name);
}

static void do_inventory(void)
{
    printf("You carry (%d/%d):", inventory_count(), MAX_INV);
    int any = 0;
    for (int i = 0; i < NUM_ITEMS; i++) {
        if (items[i].where == INV) {
            printf(" %s%s", items[i].name, (i == LANTERN && lantern_lit) ? " (lit)" : "");
            any = 1;
        }
    }
    printf("%s\n", any ? "" : " nothing");
}

static void do_read(const char *noun)
{
    int i = find_item(noun, INV);
    if (i < 0) i = find_item(noun, room);
    if (i != SCROLL) { printf("There is nothing to read there.\n"); return; }
    printf("The scroll says:\n"
           "  \"The idol sleeps beneath the house, behind a door of iron.\n"
           "   A small brass key rests where words are written.\n"
           "   Carry light, but beware - the dark drinks oil.\n"
           "   Only the idol can open the gate to freedom.\"\n");
}

static void do_use(const char *noun)
{
    if (noun[0] == '\0') { printf("Use what?\n"); return; }
    int i = find_item(noun, INV);
    if (i < 0) { printf("You don't have '%s'.\n", noun); return; }

    switch (i) {
        case LANTERN:
            if (lantern_lit) { lantern_lit = 0; printf("You snuff out the lantern.\n"); }
            else             { lantern_lit = 1; printf("The lantern glows with a warm yellow light.\n"); }
            break;
        case KEY:
            if (room == CELLAR && !vault_open) {
                vault_open = 1;
                printf("CLICK! The iron door swings open to the east.\n");
            } else if (room == CELLAR) {
                printf("The door is already open.\n");
            } else {
                printf("There is nothing here to unlock.\n");
            }
            break;
        case IDOL:
            if (room == GARDEN) {
                printf("\nYou press the golden idol into the lock. The gate groans open...\n"
                       "Moonlight washes over you. You are FREE!\n\n"
                       "*** YOU WIN! ***\nYou escaped the mansion in %d moves.\n", moves);
                game_over = 1;
            } else {
                printf("The idol hums, but nothing happens here.\n");
            }
            break;
        default:
            printf("You can't use that right now.\n");
    }
}

static void do_search(void)
{
    if (room == STUDY && items[KEY].where == HIDDEN) {
        items[KEY].where = STUDY;
        printf("You pry open the crooked drawer and find a small brass key!\n");
    } else {
        printf("You search carefully but find nothing new.\n");
    }
}

static void do_eat(const char *noun)
{
    int i = find_item(noun, INV);
    if (i == BREAD) {
        items[BREAD].where = -2;      /* gone for good */
        printf("You chew the rock-hard bread. Your teeth hurt, but you feel brave.\n");
    } else if (i < 0) {
        printf("You don't have '%s'.\n", noun);
    } else {
        printf("You can't eat that!\n");
    }
}

int main(void)
{
    char line[128];

    printf("=====================================\n");
    printf("   ESCAPE THE HAUNTED MANSION\n");
    printf("=====================================\n");
    printf("You wake up inside a creepy old mansion.\n");
    printf("Find the golden idol and escape through the garden gate.\n");
    printf("Type 'help' for commands.\n");
    show_room();

    while (!game_over) {
        printf("\n> ");
        fflush(stdout);
        if (!fgets(line, sizeof line, stdin)) break;

        /* make everything lowercase */
        for (int i = 0; line[i]; i++) line[i] = (char)tolower((unsigned char)line[i]);

        char verb[32] = "", noun[32] = "";
        int n = sscanf(line, "%31s %31s", verb, noun);
        if (n < 1) continue;

        int dir = parse_direction(verb);
        if (dir < 0 && (!strcmp(verb, "go") || !strcmp(verb, "walk") || !strcmp(verb, "move")))
            dir = parse_direction(noun);

        if (dir >= 0)                                       move_player(dir);
        else if (!strcmp(verb, "go") || !strcmp(verb, "walk") || !strcmp(verb, "move"))
                                                            printf("Go where? (north, east, south, west)\n");
        else if (!strcmp(verb, "look") || !strcmp(verb, "l")) show_room();
        else if (!strcmp(verb, "take") || !strcmp(verb, "get") || !strcmp(verb, "grab")) do_take(noun);
        else if (!strcmp(verb, "drop"))                     do_drop(noun);
        else if (!strcmp(verb, "inventory") || !strcmp(verb, "inv") || !strcmp(verb, "i")) do_inventory();
        else if (!strcmp(verb, "use"))                      do_use(noun);
        else if (!strcmp(verb, "read"))                     do_read(noun);
        else if (!strcmp(verb, "eat"))                      do_eat(noun);
        else if (!strcmp(verb, "search"))                   do_search();
        else if (!strcmp(verb, "help") || !strcmp(verb, "?")) show_help();
        else if (!strcmp(verb, "quit") || !strcmp(verb, "exit")) {
            printf("You give up. The mansion keeps you forever...\n");
            break;
        }
        else printf("I don't understand '%s'. Type 'help' for commands.\n", verb);
    }
    return 0;
}
