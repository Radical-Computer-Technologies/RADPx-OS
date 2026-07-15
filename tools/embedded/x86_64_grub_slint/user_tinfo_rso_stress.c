#include <stddef.h>
#include <string.h>
#include <term.h>
#include <unistd.h>

static void event(const char *text) {
    write(1, text, strlen(text));
    write(1, "\n", 1);
}

static int put_ch(int ch) {
    char c = (char)ch;
    return (int)write(1, &c, 1);
}

int main(void) {
    event("RADIX_LIBTINFO_RSO_START");
    int err = 0;
    if (setupterm("radix", 1, &err) != OK) {
        event("RADIX_LIBTINFO_RSO_SETUPTERM_FAIL");
        return 1;
    }

    char *clear = tigetstr("clear");
    if (!clear || clear == (char*)-1) {
        event("RADIX_LIBTINFO_RSO_TIGETSTR_FAIL");
        return 1;
    }

    if (tputs(clear, 1, put_ch) == ERR) {
        event("RADIX_LIBTINFO_RSO_TPUTS_FAIL");
        return 1;
    }

    event("RADIX_LIBTINFO_RSO_OK");
    return 0;
}
