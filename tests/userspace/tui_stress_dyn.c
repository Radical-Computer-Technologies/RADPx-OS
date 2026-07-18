#include <curses.h>
#include <stddef.h>

static long sc(long n, long a, long b, long c, long d, long e, long f) {
    register long rax asm("rax") = n;
    register long rdi asm("rdi") = a;
    register long rsi asm("rsi") = b;
    register long rdx asm("rdx") = c;
    register long r10 asm("r10") = d;
    register long r8 asm("r8") = e;
    register long r9 asm("r9") = f;
    asm volatile("int $0x80" : "+a"(rax) : "D"(rdi), "S"(rsi), "d"(rdx), "r"(r10), "r"(r8), "r"(r9) : "memory");
    return rax;
}

static size_t s_len(const char *text) {
    size_t n = 0;
    if (text) while (text[n]) ++n;
    return n;
}

static void event(const char *text) {
    sc(1, 1, (long)text, (long)s_len(text), 0, 0, 0);
    sc(1, 1, (long)"\n", 1, 0, 0, 0);
}

int main(void) {
    WINDOW *screen = initscr();
    if (!screen || !stdscr) {
        event("RADIX_LIBNCURSES_RSO_INIT_FAIL");
        return 1;
    }

    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    start_color();
    init_pair(1, COLOR_GREEN, COLOR_BLACK);
    clear();
    mvaddstr(1, 2, "RADPx dynamic ncurses");
    mvaddstr(3, 4, "libncurses.rso");
    move(3, 4);
    refresh();

    ungetch('\n');
    ungetch(KEY_DOWN);
    const int down = getch();
    const int enter = getch();
    if (down != KEY_DOWN || enter != '\n') {
        endwin();
        event("RADIX_LIBNCURSES_RSO_INPUT_FAIL");
        return 1;
    }

    clrtoeol();
    mvaddstr(5, 2, "ok");
    refresh();
    endwin();
    event("RADIX_LIBNCURSES_RSO_OK");
    event("RADIX_TUI_STRESS_DYN_OK");
    return 0;
}
