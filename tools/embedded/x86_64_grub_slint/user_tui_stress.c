#include <curses.h>
#include <stdio.h>
#include <stdlib.h>

static void event(const char *name) {
    printf("%s\n", name);
}

int main(void) {
    const char *term = getenv("TERM");
    if (!term || !*term) setenv("TERM", "radix", 1);

    WINDOW *screen = initscr();
    if (!screen) {
        event("tui-stress-fail:init");
        return 1;
    }
    cbreak();
    noecho();
    keypad(stdscr, TRUE);

    if (LINES <= 0 || COLS <= 0) {
        endwin();
        event("tui-stress-fail:size");
        return 1;
    }

    clear();
    mvaddstr(1, 2, "RADix TUI Stress");
    mvaddstr(3, 4, "> Terminal");
    mvaddstr(4, 4, "  Ncurses");
    move(3, 4);
    refresh();

    ungetch('\n');
    ungetch(KEY_DOWN);
    int key_down = getch();
    int key_enter = getch();
    if (key_down != KEY_DOWN || key_enter != '\n') {
        endwin();
        event("tui-stress-fail:input");
        return 1;
    }

    clear();
    mvaddstr(0, 0, "done");
    refresh();
    endwin();

    event("RADIX_NCURSES_INIT_OK");
    event("RADIX_NCURSES_CURSOR_OK");
    event("RADIX_NCURSES_INPUT_OK");
    event("RADIX_NCURSES_EXIT_OK");
    event("RADIX_NCURSES_PARITY_OK");
    event("tui-stress-ok");
    return 0;
}
