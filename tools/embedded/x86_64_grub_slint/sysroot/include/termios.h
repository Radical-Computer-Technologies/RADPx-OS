#ifndef RADIX_SYSROOT_TERMIOS_H
#define RADIX_SYSROOT_TERMIOS_H

#include <stdint.h>

typedef unsigned int tcflag_t;
typedef unsigned char cc_t;
typedef unsigned int speed_t;

#define NCCS 32
#define ICANON 0x00000001u
#define ECHO 0x00000002u
#define ICRNL 0x00000004u
#define ISIG 0x00000008u
#define IEXTEN 0x00000010u
#define IXON 0x00000020u
#define BRKINT 0x00000040u
#define PARMRK 0x00000080u
#define INLCR 0x00000100u
#define IGNCR 0x00000200u
#define ISTRIP 0x00000400u
#define OPOST 0x00000001u
#define ONLCR 0x00000002u
#define NOFLSH 0x00000020u
#define ECHOE 0x00000080u
#define ECHONL 0x00000040u
#define CSIZE 0x00000300u
#define CS8 0x00000300u
#define B0 0u
#define B50 50u
#define B75 75u
#define B110 110u
#define B134 134u
#define B150 150u
#define B200 200u
#define B300 300u
#define B600 600u
#define B1200 1200u
#define B1800 1800u
#define B2400 2400u
#define B4800 4800u
#define B9600 9600u
#define B19200 19200u
#define B38400 38400u
#define B57600 57600u
#define B115200 115200u
#define B230400 230400u
#define TCSANOW 0
#define TCSADRAIN 1
#define TCSAFLUSH 2
#define VINTR 0
#define VEOF 1
#define VERASE 2
#define VMIN 3
#define VTIME 4
#define VKILL 5
#define TCIFLUSH 0
#define TCOFLUSH 1
#define TCIOFLUSH 2

struct termios {
    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag;
    speed_t c_ispeed;
    speed_t c_ospeed;
    cc_t c_cc[NCCS];
};

int tcgetattr(int fd, struct termios *termios_p);
int tcsetattr(int fd, int optional_actions, const struct termios *termios_p);
int tcflush(int fd, int queue_selector);
void cfmakeraw(struct termios *termios_p);
speed_t cfgetispeed(const struct termios *termios_p);
speed_t cfgetospeed(const struct termios *termios_p);
int cfsetispeed(struct termios *termios_p, speed_t speed);
int cfsetospeed(struct termios *termios_p, speed_t speed);

#endif
