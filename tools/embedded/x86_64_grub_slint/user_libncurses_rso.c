#include <curses.h>
#include <stdarg.h>
#include <stddef.h>

struct _radix_window {
    int reserved;
};

static struct _radix_window g_screen;
WINDOW *stdscr = &g_screen;
WINDOW *curscr = &g_screen;
int LINES = 25;
int COLS = 80;
int COLORS = 8;

static int g_unget_stack[8];
static int g_unget_count;

static long sys_write(int fd, const void *data, unsigned long size) {
    long rax = 1;
    register long rdi asm("rdi") = fd;
    register long rsi asm("rsi") = (long)data;
    register long rdx asm("rdx") = (long)size;
    register long r10 asm("r10") = 0;
    register long r8 asm("r8") = 0;
    register long r9 asm("r9") = 0;
    asm volatile("int $0x80" : "+a"(rax) : "D"(rdi), "S"(rsi), "d"(rdx), "r"(r10), "r"(r8), "r"(r9) : "memory");
    return rax;
}

static unsigned long s_len(const char *s) {
    unsigned long n = 0;
    if (s) while (s[n]) ++n;
    return n;
}

static void put(const char *s) {
    if (s) (void)sys_write(1, s, s_len(s));
}

static void put_num(int value) {
    char tmp[16];
    int n = 0;
    if (value < 0) {
        put("-");
        value = -value;
    }
    do {
        tmp[n++] = (char)('0' + (value % 10));
        value /= 10;
    } while (value && n < (int)sizeof(tmp));
    while (n) {
        char ch = tmp[--n];
        (void)sys_write(1, &ch, 1);
    }
}

WINDOW *initscr(void) {
    put("\x1b[?25h\x1b[2J\x1b[H");
    return stdscr;
}

int endwin(void) {
    put("\x1b[0m");
    return OK;
}

int cbreak(void) { return OK; }
int nocbreak(void) { return OK; }
int noecho(void) { return OK; }
int echo(void) { return OK; }
int keypad(WINDOW *, int) { return OK; }
int nodelay(WINDOW *, int) { return OK; }
int raw(void) { return OK; }
int noraw(void) { return OK; }
int nonl(void) { return OK; }
int halfdelay(int) { return OK; }
int curs_set(int) { return OK; }
int start_color(void) { return OK; }
int has_colors(void) { return TRUE; }
int use_default_colors(void) { return OK; }
int init_pair(short, short, short) { return OK; }
int color_pair(short pair) { return (int)pair; }
int set_escdelay(int) { return OK; }
int key_defined(const char *) { return 0; }
int define_key(const char *, int) { return OK; }
int typeahead(int) { return OK; }
int scrollok(WINDOW *, int) { return OK; }
int wscrl(WINDOW *, int) { return OK; }
int beep(void) { return OK; }
int refresh(void) { return OK; }
int wrefresh(WINDOW *) { return OK; }
int wnoutrefresh(WINDOW *) { return OK; }
int doupdate(void) { return OK; }
int isendwin(void) { return FALSE; }

WINDOW *newwin(int, int, int, int) { return stdscr; }
WINDOW *subwin(WINDOW *, int, int, int, int) { return stdscr; }
int delwin(WINDOW *) { return OK; }

int clear(void) {
    put("\x1b[2J\x1b[H");
    return OK;
}

int erase(void) { return clear(); }
int werase(WINDOW *) { return clear(); }
int wclear(WINDOW *) { return clear(); }

int move(int y, int x) {
    put("\x1b[");
    put_num(y + 1);
    put(";");
    put_num(x + 1);
    put("H");
    return OK;
}

int wmove(WINDOW *, int y, int x) { return move(y, x); }

int addstr(const char *s) {
    put(s);
    return OK;
}

int waddstr(WINDOW *, const char *s) { return addstr(s); }
int waddnstr(WINDOW *, const char *s, int n) {
    if (!s || n <= 0) return OK;
    (void)sys_write(1, s, (unsigned long)n);
    return OK;
}

int mvaddstr(int y, int x, const char *s) {
    move(y, x);
    return addstr(s);
}

int mvwaddstr(WINDOW *, int y, int x, const char *s) { return mvaddstr(y, x, s); }
int mvwaddnstr(WINDOW *, int y, int x, const char *s, int n) {
    move(y, x);
    return waddnstr(stdscr, s, n);
}

int addch(const chtype ch) {
    char c = (char)(ch & 0xffu);
    (void)sys_write(1, &c, 1);
    return OK;
}

int waddch(WINDOW *, const chtype ch) { return addch(ch); }
int mvwaddch(WINDOW *, int y, int x, const chtype ch) {
    move(y, x);
    return addch(ch);
}

int printw(const char *fmt, ...) {
    (void)fmt;
    va_list ap;
    va_start(ap, fmt);
    va_end(ap);
    return OK;
}

int wprintw(WINDOW *, const char *fmt, ...) {
    (void)fmt;
    va_list ap;
    va_start(ap, fmt);
    va_end(ap);
    return OK;
}

int mvwprintw(WINDOW *, int y, int x, const char *fmt, ...) {
    move(y, x);
    (void)fmt;
    va_list ap;
    va_start(ap, fmt);
    va_end(ap);
    return OK;
}

int ungetch(int ch) {
    if (g_unget_count >= (int)(sizeof(g_unget_stack) / sizeof(g_unget_stack[0]))) return ERR;
    g_unget_stack[g_unget_count++] = ch;
    return OK;
}

int getch(void) {
    if (g_unget_count > 0) return g_unget_stack[--g_unget_count];
    return ERR;
}

int wgetch(WINDOW *) { return getch(); }
int get_wch(wint_t *wch) {
    if (!wch) return ERR;
    int ch = getch();
    if (ch == ERR) return ERR;
    *wch = (wint_t)ch;
    return OK;
}
int wget_wch(WINDOW *, wint_t *wch) { return get_wch(wch); }
int wclrtoeol(WINDOW *) { put("\x1b[K"); return OK; }
int clrtoeol(void) { return wclrtoeol(stdscr); }
int wattron(WINDOW *, int) { return OK; }
int wattroff(WINDOW *, int) { return OK; }
int wattrset(WINDOW *, int) { return OK; }
int attron(int attrs) { return wattron(stdscr, attrs); }
int attroff(int attrs) { return wattroff(stdscr, attrs); }
int attrset(int attrs) { return wattrset(stdscr, attrs); }
int wredrawln(WINDOW *, int, int) { return OK; }
int napms(int ms) {
    long rax = 9;
    register long rdi asm("rdi") = (long)ms * 1000000L;
    register long rsi asm("rsi") = 0;
    register long rdx asm("rdx") = 0;
    register long r10 asm("r10") = 0;
    register long r8 asm("r8") = 0;
    register long r9 asm("r9") = 0;
    asm volatile("int $0x80" : "+a"(rax) : "D"(rdi), "S"(rsi), "d"(rdx), "r"(r10), "r"(r8), "r"(r9) : "memory");
    return OK;
}
