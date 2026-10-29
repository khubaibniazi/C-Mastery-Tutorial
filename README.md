# 🚀 C Programming – Mastery Level Tutorial

Welcome to the **Mastery Level** of my C series! 🎯

You already know the basics and the medium-level concepts. Now it is time to **build real programs**: games, tools and algorithms that run in the terminal and are fun to show to other people.

Every program is a single `.c` file with **comments in every section**, so you can learn by reading, running and changing the code.

> 📘 Series: Beginner → **Medium** ([C-Trainee-Tutorial](https://github.com/khubaibniazi/C-Trainee-Tutorial)) → **Mastery** (this repo)

---

## 🛠 Tools Used
- **Language:** C
- **Compiler:** GCC
- **Editor:** Visual Studio Code

---

## 📂 Programs

| # | Program | What it does | Key concepts |
|---|---------|--------------|--------------|
| 01 | 🐍 **Snake Game** | Classic snake with levels, speed-up and saved high score | Structs, game loop, non-blocking input |
| 02 | ❌⭕ **Tic-Tac-Toe vs AI** | An AI you can never beat | Recursion, Minimax, alpha-beta pruning |
| 03 | 🧩 **Maze Solver** | Carves a random maze, then solves it with BFS or DFS | Stacks, queues, 2D arrays |
| 04 | 🔤 **Hangman** | Word game with ASCII art and a word file | File handling, malloc/realloc |
| 05 | 🏦 **Bank System** | Accounts, PINs, transfers, statements | Structs, binary files, validation |
| 06 | 🎓 **Student Database** | Add, search, sort, delete records forever | Linked lists, function pointers |
| 07 | 💚 **Matrix Rain** | Falling green code like the movie | ANSI codes, animation |
| 08 | 🏚 **Text Adventure** | Escape the haunted mansion | Structs, enums, command parser |
| 09 | 🔢 **Sudoku Solver** | Watch backtracking solve a puzzle | Recursion, backtracking |
| 10 | 💻 **Mini Shell** | Your own terminal with pipes and redirection | fork, exec, pipe, dup2 |
| 11 | 🔐 **File Encryption** | Lock any file with a password | Bitwise XOR, binary I/O |
| 12 | 🧬 **Game of Life** | Cells that live, die and form patterns | Double buffering, animation |
| 13 | ⌨ **Typing Speed Tester** | WPM, accuracy and colored mistakes | Timing, string comparison |
| 14 | 🧮 **Expression Calculator** | Solves `3 + 4 * (2 - 1)` correctly | Stacks, Shunting-Yard algorithm |

---

## ▶ How to Run Any Program

Open the VS Code terminal (`Ctrl + ``), go into the program's folder and compile:

```bash
cd 01_Snake_Game
gcc snake.c -o snake
./snake            # Windows: snake.exe
```

Special cases:

```bash
gcc calculator.c -o calculator -lm     # the calculator needs the math library (-lm)
```

- Use the **VS Code terminal** or **Windows Terminal** so colors and animation work.
- **Mini Shell** (`10`) is built for Linux/Mac. On Windows use **WSL** for the full version.
- The Hangman program reads `words.txt`, so run it from inside its folder.

---

## 💡 Challenges to Try
- Snake: add obstacles or a bonus food that disappears.
- Maze: let the player walk through it with the keyboard.
- Bank: add interest on savings.
- Calculator: add variables like `x = 5`.
- Adventure: add new rooms, items and a monster.

---

## 👤 Author

**Khubaib Niazi**
GitHub: https://github.com/khubaibniazi
LinkedIn: https://www.linkedin.com/in/khubaib-niazi/
