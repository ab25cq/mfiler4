#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <stdlib.h>
#include <string.h>
#include <wctype.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <wchar.h>
#include <errno.h>
#include <unistd.h>

#include "config.h"

#if defined(HAVE_CURSES_H)
#include <curses.h>
#elif defined(HAVE_NCURSES_H)
#include <ncurses.h>
#elif defined(HAVE_NCURSES_NCURSES_H)
#include <ncurses/ncurses.h>
#endif

#if defined(__CYGWIN__)
#include <ncurses/term.h>
#include <sys/time.h>
#endif

void termbuffer_init()
{
}

void termbuffer_final()
{
}

void xclear_immediately()
{
#if defined(__CYGWIN__)
    int y;
    for(y=0; y<mgetmaxy(); y++) {
        mclear_online(y);
    }
    refresh();
#else
    mclear_immediately();
#endif
}

void xclear()
{
#if defined(__CYGWIN__)
    int y;
    for(y=0; y<mgetmaxy()-1; y++) {
        mclear_online(y);
    }
    mclear_lastline();
#else
    if(getenv("MFILER4_CLEAR_WAY")) {
        int y;
        for(y=0; y<mgetmaxy()-1; y++) {
            mclear_online(y);
        }
        mclear_lastline();
        //erase();
    }
    else {
        clear();
    }
#endif
}

void xmove(int y, int x)
{
/*
    gX = x;
    gY = y;
*/
}

void xmove_immediately(int y, int x)
{
/*
    ttywrite(tparm(tigetstr("cup"), y, x));
*/
}

int xmvprintw_immediately(int y, int x, char* str, ...)
{
/*
    char buf[BUFSIZ];
    int i;

    va_list args;
    va_start(args, str);
    i = vsnprintf(buf, BUFSIZ, str, args);
    va_end(args);

    mmove_immediately(y, x);

    ttywrite(buf);

    return i;
*/
}

int xprintw_immediately(char* str, ...)
{
/*
    char buf[BUFSIZ];
    int i;

    va_list args;
    va_start(args, str);
    i = vsnprintf(buf, BUFSIZ, str, args);
    va_end(args);

    ttywrite(buf);

    return i;
*/
}

int xmvprintw(int y, int x, char* str, ...)
{
/*
    char buf[BUFSIZ];
    int i;
    char* p;
    wchar_t wbuf[BUFSIZ];
    wchar_t* wp;

    const int maxx = mgetmaxx();
    const int maxy = mgetmaxy();

    va_list args;
    va_start(args, str);
    i = vsnprintf(buf, BUFSIZ, str, args);
    va_end(args);

    mmove(y, x);

    if(gTerminalKanjiCode == kTKUtf8) {
        if(mbstowcs(wbuf, buf, BUFSIZ) == -1) {
            mbstowcs(wbuf, "?????", BUFSIZ);
        }

        wp = wbuf;
        while(*wp) {
            if(wis_2cols(*wp)) {
                gWBuf[gY][gX] = *wp;
                gWBuf[gY][gX+1] = 0;
                gBufAttr[gY][gX] = gBufAttrNow;
                gBufAttr[gY][gX+1] = 0;
                gX+=2;

                if(gX >= maxx) {
                    gX = 0;
                    gY++;
                }
                wp++;
            }
            else if(*wp == '\t') {
                gWBuf[gY][gX] = ' ';
                gBufAttr[gY][gX] = gBufAttrNow;
                gX++;

                if(gX >= maxx) {
                    gX = 0;
                    gY++;
                }
                wp++;
            }
            else if(*wp == '\n') {
                gY++;
                gX = 0;
                wp++;
                if(gY >= maxy) {
                    gY = maxy-1;;
                }
            }
            else {
                gWBuf[gY][gX] = *wp;
                gBufAttr[gY][gX] = gBufAttrNow;
                gX++;

                if(gX >= maxx) {
                    gX = 0;
                    gY++;
                }
                wp++;
            }
        }
    }
    else {
        p = buf;
        while(*p) {
            if(*p == '\n') {
                gX = 0;
                gY++;
                p++;
                if(gY >= maxy) {
                    gY = maxy-1;;
                }
            }
            else if(*p == '\t') {
                gBuf[gY][gX] = ' ';
                gBufAttr[gY][gX] = gBufAttrNow;
                gX++;
                if(gX >= maxx) {
                    gX = 0;
                    gY++;
                }
                p++;
            }
            else {
                gBuf[gY][gX] = *p;
                gBufAttr[gY][gX] = gBufAttrNow;
                gX++;
                if(gX >= maxx) {
                    gX = 0;
                    gY++;
                }
                p++;
            }
        }
    }

    return i;
*/
}

int xprintw(char* str, ...)
{
/*
    char buf[BUFSIZ];
    wchar_t wbuf[BUFSIZ];
    wchar_t* wp;
    int i;
    char* p;

    const int maxx = mgetmaxx();
    const int maxy = mgetmaxy();

    va_list args;
    va_start(args, str);
    i = vsnprintf(buf, BUFSIZ, str, args);
    va_end(args);

    if(gTerminalKanjiCode == kTKUtf8) {
        if(mbstowcs(wbuf, buf, BUFSIZ) == -1) {
            mbstowcs(wbuf, "?????", BUFSIZ);
        }

        wp = wbuf;
        while(*wp) {
            if(wis_2cols(*wp)) {
                gWBuf[gY][gX] = *wp;
                gWBuf[gY][gX+1] = 0;
                gBufAttr[gY][gX] = gBufAttrNow;
                gBufAttr[gY][gX+1] = 0;
                gX+=2;

                if(gX >= maxx) {
                    gX = 0;
                    gY++;
                }
                wp++;
            }
            else if(*wp == '\n') {
                gY++;
                gX = 0;
                wp++;
                if(gY >= maxy) {
                    gY = maxy-1;;
                }
            }
            else if(*wp == '\t') {
                gWBuf[gY][gX] = ' ';
                gBufAttr[gY][gX] = gBufAttrNow;
                gX++;

                if(gX >= maxx) {
                    gX = 0;
                    gY++;
                }
                wp++;
            }
            else {
                gWBuf[gY][gX] = *wp;
                gBufAttr[gY][gX] = gBufAttrNow;
                gX++;

                if(gX >= maxx) {
                    gX = 0;
                    gY++;
                }
                wp++;
            }
        }
    }
    else {
        p = buf;
        while(*p) {
            if(*p == '\n') {
                gY++;
                gX = 0;
                p++;
                if(gY >= maxy) {
                    gY = maxy-1;;
                }
            }
            else if(*p == '\t') {
                gBuf[gY][gX] = ' ';
                gBufAttr[gY][gX] = gBufAttrNow;
                gX++;
                if(gX >= maxx) {
                    gX = 0;
                    gY++;
                }
                p++;
            }
            else {
                gBuf[gY][gX] = *p;
                gBufAttr[gY][gX] = gBufAttrNow;
                gX++;
                if(gX >= maxx) {
                    gX = 0;
                    gY++;
                }
                p++;
            }
        }
    }

    return i;
*/
}

void xattron(int attrs)
{
/*
    gBufAttrNow = attrs;
*/
}

void xattroff()
{
/*
    gBufAttrNow = 0;
*/
}

