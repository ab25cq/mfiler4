#include "common.h"
#include "config.h"

#if defined(HAVE_CURSES_H)
#include <curses.h>
#elif defined(HAVE_NCURSES_H)
#include <ncurses.h>
#elif defined(HAVE_NCURSES_NCURSES_H)
#include <ncurses/ncurses.h>
#endif

#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <termios.h>
#include <stdarg.h>
#include <time.h>
#include <ctype.h>
#include <locale.h>
#include <pwd.h>
#include <dirent.h>
#include <stdlib.h>
#include <fcntl.h>
#include <termios.h>

#ifdef __LINUX__

#include <limits.h>
#include <signal.h>
#include <sys/wait.h>

#else

#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>

#endif

int gMainLoop = -1;
void (*gView)() = NULL;          // drawing function

char gTempDir[PATH_MAX];         // temorary directory path
char gHomeDir[PATH_MAX];         // home directory path

///////////////////////////////////////////////////
// input
///////////////////////////////////////////////////
static void input_utf8(int meta, int* key, int key_size)
{
    if(*key == 26) {         // CTRL-Z
        endwin();
        mreset_tty();
        kill(getpid(), SIGSTOP);
        xinitscr();
    }
    /// menu ///
    else if(gActiveMenu) {
        menu_input(meta, key[0]);
    }
    /// incremental search ///
    else if(gISearch) {
        isearch_input(meta, key, key_size);
    }
    /// filer ///
    else {
        filer_input(meta, key[0]);
    }
}

static void input(int meta, int key)
{
    if(key == 26) {         // CTRL-Z
        endwin();
        mreset_tty();
        kill(getpid(), SIGSTOP);
        xinitscr();
    }
    /// menu ///
    else if(gActiveMenu) {
        menu_input(meta, key);
    }
    /// incremental search ///
    else if(gISearch) {
        int keybuf[2];
        keybuf[0] = key;
        keybuf[1] = 0;
        int key_size = 1;
        isearch_input(meta, keybuf, key_size);
    }
    /// filer ///
    else {
        filer_input(meta, key);
    }
}

///////////////////////////////////////////////////
// drawing
///////////////////////////////////////////////////
void view()
{
    /// incremental search ///
    if(gISearch) {
        filer_view(0);
        filer_view(1);
        job_view();

        isearch_view();
#if defined(__CYGWIN__)
        cmdline_view_filer();
#endif
    }
    /// menu ///
    else if(gActiveMenu) {
        filer_view(0);
        filer_view(1);
        job_view();

        menu_view();
        cmdline_view_filer();
    }
    /// filer ///
    else {
        filer_view(0);
        filer_view(1);
        job_view();
        cmdline_view_filer();
    }

    if(gView) gView();
}

///////////////////////////////////////////////////
// runtime script
///////////////////////////////////////////////////
static BOOL read_rc_file(int argc, char** argv)
{
    char rc_fname[PATH_MAX];

    snprintf(rc_fname, PATH_MAX, "%s/mfiler4.xyzsh", SYSCONFDIR);
    if(access(rc_fname, R_OK) == 0) {
        if(!xyzsh_load_file(rc_fname, argv, argc, gMFiler4)) {
            return FALSE;
        }
    }
    else {
        fprintf(stderr, "can't find %s/mfiler4.xyzsh file\n", SYSCONFDIR);
        return FALSE;
    }

    snprintf(rc_fname, PATH_MAX, "%s/mfiler4.xyzsh", gHomeDir);

    if(access(rc_fname, R_OK) == 0) {
        if(!xyzsh_load_file(rc_fname, argv, argc, gMFiler4)) {
            return FALSE;
        }
    }

    return TRUE;
}

///////////////////////////////////////////////////
// on exiting
///////////////////////////////////////////////////
static void atexit_fun()
{
}

///////////////////////////////////////////////////
// signal
///////////////////////////////////////////////////
static void sig_winch(int s)
{
    if(gMainLoop == -1) {
        resizeterm(mgetmaxy(), mgetmaxx());
        xclear_immediately();       // refresh
        view();
        refresh();
    }
}

static void sig_cont(int s)
{
    if(gMainLoop == -1) {
        xclear_immediately();       // refresh
        view();
        refresh();
    }
}

void set_signal_mfiler()
{
    xyzsh_set_signal();

    struct sigaction sa4;

    memset(&sa4, 0, sizeof(sa4));
    sa4.sa_handler = SIG_IGN;
    if(sigaction(SIGCONT, &sa4, NULL) < 0) {
        perror("sigaction4");
        exit(1);
    }

    struct sigaction sa5;

    memset(&sa5, 0, sizeof(sa5));
    sa5.sa_handler = sig_winch;
    if(sigaction(SIGWINCH, &sa5, NULL) < 0) {
        perror("sigaction5");
        exit(1);
    }
}

///////////////////////////////////////////////////
// main function
///////////////////////////////////////////////////
static void usage()
{
    printf("usage mfiler4 [-c command] [--version ] [ filer initial directory or script file]\n\n");

    printf("-c : eval a command on mfiler4\n");
    printf("--version : display mfiler4 version\n");

    exit(0);
}

static void version()
{
    printf("mfiler4 version %s with shell scripting system xyzsh version %s.", getenv("VERSION"), getenv("XYZSH_VERSION"));
    puts("This program is a 2pain file manager with a embedded shell scripting system \"xyzsh\". mfiler4 is developped by ab25cq.");
    puts("compiled with");

    puts("+oniguruma");
    puts("+xyzsh");
    puts("+math");
    puts("+curses");
    
    exit(0);
}

static void set_mfenv()
{
    setenv("#", "-1", 1);
    setenv("VIEW_OPTION", "2pain", 1);
    setenv("SYSCONFDIR", SYSCONFDIR, 1);
    setenv("MF4DOCDIR", DOCDIR, 1);
    setenv("VIEW_FILE_SIZE", "Normal", 1);

    setenv("MF4HOME", gHomeDir, 1);
    setenv("MF4TEMP", gTempDir, 1);
    setenv("DOCDIR", DOCDIR, 1);
    setenv("DOTDIR_MASK", "0", 1);

    char buf[128];


    sObject* keycode = UOBJECT_NEW_GC(8, gMFiler4, "keycode", TRUE);
    uobject_init(keycode);
    uobject_put(gMFiler4, "keycode", keycode);

    snprintf(buf, 128, "%d", KEY_UP);
    uobject_put(keycode, "up", STRING_NEW_GC(buf, TRUE));
    snprintf(buf, 128, "%d", KEY_RIGHT);
    uobject_put(keycode, "right", STRING_NEW_GC(buf, TRUE));
    snprintf(buf, 128, "%d", KEY_DOWN);
    uobject_put(keycode, "down", STRING_NEW_GC(buf, TRUE));
    snprintf(buf, 128, "%d", KEY_LEFT);
    uobject_put(keycode, "left", STRING_NEW_GC(buf, TRUE));
    snprintf(buf, 128, "%d", KEY_IC);
    uobject_put(keycode, "insert", STRING_NEW_GC(buf, TRUE));
    snprintf(buf, 128, "%d", KEY_DC);
    uobject_put(keycode, "delete2", STRING_NEW_GC(buf, TRUE));
    snprintf(buf, 128, "%d", 127);
    uobject_put(keycode, "delete", STRING_NEW_GC(buf, TRUE));
    snprintf(buf, 128, "%d", KEY_HOME);
    uobject_put(keycode, "home", STRING_NEW_GC(buf, TRUE));
    snprintf(buf, 128, "%d", KEY_END);
    uobject_put(keycode, "end", STRING_NEW_GC(buf, TRUE));
    snprintf(buf, 128, "%d", KEY_PPAGE);
    uobject_put(keycode, "pageup", STRING_NEW_GC(buf, TRUE));
    snprintf(buf, 128, "%d", KEY_NPAGE);
    uobject_put(keycode, "pagedown", STRING_NEW_GC(buf, TRUE));

    snprintf(buf,128, "%d", 10);
    uobject_put(keycode, "enter", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", KEY_BACKSPACE);
    uobject_put(keycode, "backspace", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", KEY_F(1));
    uobject_put(keycode, "f1", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", KEY_F(2));
    uobject_put(keycode, "f2", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", KEY_F(3));
    uobject_put(keycode, "f3", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", KEY_F(4));
    uobject_put(keycode, "f4", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", KEY_F(5));
    uobject_put(keycode, "f5", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", KEY_F(6));
    uobject_put(keycode, "f6", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", KEY_F(7));
    uobject_put(keycode, "f7", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", KEY_F(8));
    uobject_put(keycode, "f8", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", KEY_F(9));
    uobject_put(keycode, "f9", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", KEY_F(10));
    uobject_put(keycode, "f10", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", KEY_F(11));
    uobject_put(keycode, "f11", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", KEY_F(12));
    uobject_put(keycode, "f12", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'a');
    uobject_put(keycode, "a", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'b');
    uobject_put(keycode, "b", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'c');
    uobject_put(keycode, "c", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'd');
    uobject_put(keycode, "d", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'e');
    uobject_put(keycode, "e", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'f');
    uobject_put(keycode, "f", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'g');
    uobject_put(keycode, "g", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'h');
    uobject_put(keycode, "h", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'i');
    uobject_put(keycode, "i", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'j');
    uobject_put(keycode, "j", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'k');
    uobject_put(keycode, "k", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'l');
    uobject_put(keycode, "l", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'm');
    uobject_put(keycode, "m", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'n');
    uobject_put(keycode, "n", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'o');
    uobject_put(keycode, "o", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'p');
    uobject_put(keycode, "p", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'q');
    uobject_put(keycode, "q", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'r');
    uobject_put(keycode, "r", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 's');
    uobject_put(keycode, "s", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 't');
    uobject_put(keycode, "t", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'u');
    uobject_put(keycode, "u", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'v');
    uobject_put(keycode, "v", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'w');
    uobject_put(keycode, "w", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'x');
    uobject_put(keycode, "x", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'y');
    uobject_put(keycode, "y", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'z');
    uobject_put(keycode, "z", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'A');
    uobject_put(keycode, "A", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'B');
    uobject_put(keycode, "B", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'C');
    uobject_put(keycode, "C", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'D');
    uobject_put(keycode, "D", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'E');
    uobject_put(keycode, "E", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'F');
    uobject_put(keycode, "F", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'G');
    uobject_put(keycode, "G", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'H');
    uobject_put(keycode, "H", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'I');
    uobject_put(keycode, "I", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'J');
    uobject_put(keycode, "J", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'K');
    uobject_put(keycode, "K", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'L');
    uobject_put(keycode, "L", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'M');
    uobject_put(keycode, "M", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'N');
    uobject_put(keycode, "N", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'O');
    uobject_put(keycode, "O", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'P');
    uobject_put(keycode, "P", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'Q');
    uobject_put(keycode, "Q", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'R');
    uobject_put(keycode, "R", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'S');
    uobject_put(keycode, "S", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'T');
    uobject_put(keycode, "T", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'U');
    uobject_put(keycode, "U", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'V');
    uobject_put(keycode, "V", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'W');
    uobject_put(keycode, "W", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'X');
    uobject_put(keycode, "X", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'Y');
    uobject_put(keycode, "Y", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 'Z');
    uobject_put(keycode, "Z", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 32);
    uobject_put(keycode, "space", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 1);
    uobject_put(keycode, "ctrl_a", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 2);
    uobject_put(keycode, "ctrl_b", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 3);
    uobject_put(keycode, "ctrl_c", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 4);
    uobject_put(keycode, "ctrl_d", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 5);
    uobject_put(keycode, "ctrl_e", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 6);
    uobject_put(keycode, "ctrl_f", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 7);
    uobject_put(keycode, "ctrl_g", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 8);
    uobject_put(keycode, "ctrl_h", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 9);
    uobject_put(keycode, "ctrl_i", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 10);
    uobject_put(keycode, "ctrl_j", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 11);
    uobject_put(keycode, "ctrl_k", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 12);
    uobject_put(keycode, "ctrl_l", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 13);
    uobject_put(keycode, "ctrl_m", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 14);
    uobject_put(keycode, "ctrl_n", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 15);
    uobject_put(keycode, "ctrl_o", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 16);
    uobject_put(keycode, "ctrl_p", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 17);
    uobject_put(keycode, "ctrl_q", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 18);
    uobject_put(keycode, "ctrl_r", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 19);
    uobject_put(keycode, "ctrl_s", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 20);
    uobject_put(keycode, "ctrl_t", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 21);
    uobject_put(keycode, "ctrl_u", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 22);
    uobject_put(keycode, "ctrl_v", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 23);
    uobject_put(keycode, "ctrl_w", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 24);
    uobject_put(keycode, "ctrl_x", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 25);
    uobject_put(keycode, "ctrl_y", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 26);
    uobject_put(keycode, "ctrl_z", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 27);
    uobject_put(keycode, "escape", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 9);
    uobject_put(keycode, "tab", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", '0');
    uobject_put(keycode, "0", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", '1');
    uobject_put(keycode, "1", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", '2');
    uobject_put(keycode, "2", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", '3');
    uobject_put(keycode, "3", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", '4');
    uobject_put(keycode, "4", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", '5');
    uobject_put(keycode, "5", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", '6');
    uobject_put(keycode, "6", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", '7');
    uobject_put(keycode, "7", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", '8');
    uobject_put(keycode, "8", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", '9');
    uobject_put(keycode, "9", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", '!');
    uobject_put(keycode, "exclam", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", '"');
    uobject_put(keycode, "dquote", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", '#');
    uobject_put(keycode, "sharp", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", '$');
    uobject_put(keycode, "dollar", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", '%');
    uobject_put(keycode, "percent", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", '&');
    uobject_put(keycode, "and", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", '\'');
    uobject_put(keycode, "squote", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", '(');
    uobject_put(keycode, "lparen", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", ')');
    uobject_put(keycode, "rparen", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", '~');
    uobject_put(keycode, "tilda", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", '=');
    uobject_put(keycode, "equal", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", '-');
    uobject_put(keycode, "minus", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", '^');
    uobject_put(keycode, "cup", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", '|');
    uobject_put(keycode, "vbar", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", '\\');
    uobject_put(keycode, "backslash", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", '@');
    uobject_put(keycode, "atmark", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", '`');
    uobject_put(keycode, "bapostrophe", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", '{');
    uobject_put(keycode, "lcurly", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", '[');
    uobject_put(keycode, "lbrack", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", '+');
    uobject_put(keycode, "plus", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", ';');
    uobject_put(keycode, "semicolon", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", '*');
    uobject_put(keycode, "star", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", ':');
    uobject_put(keycode, "colon", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", '}');
    uobject_put(keycode, "rcurly", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", ']');
    uobject_put(keycode, "rbrack", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", '<');
    uobject_put(keycode, "lss", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", ',');
    uobject_put(keycode, "comma", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", '>');
    uobject_put(keycode, "gtr", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", '.');
    uobject_put(keycode, "dot", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", '/');
    uobject_put(keycode, "slash", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", '?');
    uobject_put(keycode, "qmark", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", '_');
    uobject_put(keycode, "underbar", STRING_NEW_GC(buf, TRUE));

    snprintf(buf,128, "%d", 0);
    setenv("nometa", buf, 1);
    snprintf(buf,128, "%d", 1);
    setenv("meta", buf, 1);
#if !defined(__DARWIN__)
    snprintf(buf,128, "%ld", A_REVERSE);
    setenv("ma_reverse", buf, 1);
    snprintf(buf,128, "%ld", A_BOLD);
    setenv("ma_bold", buf, 1);
    snprintf(buf,128, "%ld", A_UNDERLINE);
    setenv("ma_underline", buf, 1);
#else
    snprintf(buf,128, "%d", A_REVERSE);
    setenv("ma_reverse", buf, 1);
    snprintf(buf,128, "%d", A_BOLD);
    setenv("ma_bold", buf, 1);
    snprintf(buf,128, "%d", A_UNDERLINE);
    setenv("ma_underline", buf, 1);
#endif
    snprintf(buf,128, "%d", COLOR_PAIR(1));
    setenv("ma_white", buf, 1);
    snprintf(buf,128, "%d", COLOR_PAIR(2));
    setenv("ma_blue", buf, 1);
    snprintf(buf,128, "%d", COLOR_PAIR(3));
    setenv("ma_cyan", buf, 1);
    snprintf(buf,128, "%d", COLOR_PAIR(4));
    setenv("ma_green", buf, 1);
    snprintf(buf,128, "%d", COLOR_PAIR(5));
    setenv("ma_yellow", buf, 1);
    snprintf(buf,128, "%d", COLOR_PAIR(6));
    setenv("ma_magenta", buf, 1);
    snprintf(buf,128, "%d", COLOR_PAIR(7));
    setenv("ma_red", buf, 1);

/*
    snprintf(buf,128, "%d", 0);
    uobject_put(gMFiler4, "nometa", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", 1);
    uobject_put(gMFiler4, "meta", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", A_REVERSE);
    uobject_put(gMFiler4, "ma_reverse", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", A_BOLD);
    uobject_put(gMFiler4, "ma_bold", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", A_UNDERLINE);
    uobject_put(gMFiler4, "ma_underline", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", COLOR_PAIR(1));
    uobject_put(gMFiler4, "ma_white", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", COLOR_PAIR(2));
    uobject_put(gMFiler4, "ma_blue", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", COLOR_PAIR(3));
    uobject_put(gMFiler4, "ma_cyan", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", COLOR_PAIR(4));
    uobject_put(gMFiler4, "ma_green", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", COLOR_PAIR(5));
    uobject_put(gMFiler4, "ma_yellow", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", COLOR_PAIR(6));
    uobject_put(gMFiler4, "ma_magenta", STRING_NEW_GC(buf, TRUE));
    snprintf(buf,128, "%d", COLOR_PAIR(7));
    uobject_put(gMFiler4, "ma_red", STRING_NEW_GC(buf, TRUE));
*/
}

void mygetcwd(char* result, int result_size)
{
    int l;
    
    l = 50;
    while(!getcwd(result, l)) {
         l += 50;
         if(l+1 >= result_size) {
            break;
         }
    }
}

void extname(char* result, int result_size, char* name)
{
    char* p;
    
    for(p = name + strlen(name); p != name; p--) {
        if(*p == '.' && p == name + strlen(name)) {
            xstrncpy(result, "", result_size);
            return;
        }
        else if(*p == '/') {
            xstrncpy(result, "", result_size);
            return;
        }
        else if(*p == '.') {
            xstrncpy(result, p+1, result_size);
            return;
        }
    }

    xstrncpy(result, "", result_size);
}

static int gMainResult;

static void temulator_fun(void* arg)
{
    /// start curses ///
    xinitscr();

    /// main loop ///
    fd_set mask;
    fd_set read_ok;

    FD_ZERO(&mask);
    FD_SET(0, &mask);

    while(gMainLoop == -1) {
        /// drawing ///
        xclear();
        view();
        refresh();

        const int maxy = mgetmaxy();
        mmove_immediately(maxy-2, 0);

        /// select for waiting to input ///
        read_ok = mask;
        if(xyzsh_job_num() > 0) {
            struct timeval tv;
            tv.tv_sec = 5;
            tv.tv_usec = 0;

            select(5, &read_ok, NULL, NULL, &tv);
        }
        else {
            select(5, &read_ok, NULL, NULL, NULL);
        }

        /// input ///
#ifdef HAVE_LIB_CURSESW
        if(FD_ISSET(0, &read_ok)) {
            int meta;
            int* key;
            int key_size;
            xgetch_utf8(&meta, ALLOC &key, &key_size);

            input_utf8(meta, key, key_size);

            FREE(key);
        }
#else
        if(FD_ISSET(0, &read_ok)) {
            int meta;
            int key = xgetch(&meta);

            input(meta, key);
        }
#endif

        /// works to background jobs ///
        xyzsh_wait_background_job();
    }

    /// end curses ///
    endwin();

    /// kill all jobs ///
    xyzsh_kill_all_jobs();

    /// finalization for modules ///
    isearch_final();
    menu_final();
    filer_final();
    xyzsh_final();
    commands_final();

    mreset_tty();
    //mrestore_ttysettings();

    /// checking memory leaks ///
    CHECKML_END();

    /// puts ///
    puts("");

    mreset_tty();

    gMainResult = gMainLoop;
}

int main(int argc, char* argv[])
{
    CHECKML_BEGIN();    // start to watch memory leak

    /// initialization for envronment variable ///
    setenv("VERSION", "1.2.7", 1);
    setenv("MFILER4_DOCDIR", DOCDIR, 1);
    setenv("MFILER4_DATAROOTDIR", DOCDIR, 1);

    /// save mfiler4 home directory ///
    char* home = getenv("HOME");
    if(home == NULL) {
        fprintf(stderr, "$HOME is NULL.exit");
        exit(1);
    }
    snprintf(gHomeDir, PATH_MAX, "%s/.mfiler4", home);

    if(access(gHomeDir, F_OK) != 0) {
        char buf[PATH_MAX];
        snprintf(buf, PATH_MAX, "mkdir -p %s", gHomeDir);
        if(system(buf) < 0) {
            fprintf(stderr, "can't make directory (%s)", gHomeDir);
            exit(1);
        }
    }

    if(chmod(gHomeDir, S_IRUSR|S_IWUSR|S_IXUSR) < 0) {
        fprintf(stderr, "can't chmod %s", gHomeDir);
        exit(1);
    }

    /// save mfiler4 temporary directory ///
    snprintf(gTempDir, PATH_MAX, "%s/temp", gHomeDir);
    if(access(gTempDir, F_OK) != 0) {
        if(mkdir(gTempDir, 0700) < 0) {
            fprintf(stderr, "can't mkdir %s", gTempDir);
            exit(1);
        }
    }

    if(chmod(gTempDir, S_IRUSR|S_IWUSR|S_IXUSR) < 0) {
        fprintf(stderr, "can't chmod %s", gTempDir);
        exit(1);
    }

    srandom(1000);

    msave_ttysettings();
    mreset_tty();

    /// optoin
    char send_cmd[BUFSIZ];
    char file_name[PATH_MAX];
    file_name[0] = 0;

    int i;
    int pid;
    for(i=1 ; i<argc; i++){
        // file name
        if(argv[i][0] != '-') {
            xstrncpy(file_name, argv[i], PATH_MAX);
        }
        // argument
        else {
            if(strcmp(argv[i], "--version") == 0) version();

            switch (argv[i][1]){
                case 'c':
                    if(i+1 < argc) {
                        snprintf(send_cmd, BUFSIZ, "c%s", argv[i+1]);
                        i++;
                    }
                    else {
                        usage();
                    }
                    break;

                case 'x':
                    setenv("MFILER4_CLEAR_WAY", "1", 1);
                    break;

                default:
                    usage();
            }
        }
    }

    if(!isatty(0) || !isatty(1)) {
        fprintf(stderr, "standard input is not a tty\n");
        return 1;
    }

    /// initialization for all modules ///
    xyzsh_init(kATConsoleApp, FALSE);
    commands_init();
    filer_init();
    menu_init();
    gui_init();
    isearch_init();

    /// set environment variables ///
    set_mfenv();

    /// locale ///
    //if(gTerminalKanjiCode == kTKUtf8) setlocale(LC_CTYPE, "ja_JP.UTF-8");

    /// atexit ///
    atexit(atexit_fun);

    /// make directories ///
    if(vector_count(gDirs) == 0) {
        if(strcmp(file_name, "") == 0) {
            char cwd[PATH_MAX];
            mygetcwd(cwd, PATH_MAX);
            xstrncat(cwd, "/", PATH_MAX);

            if(!filer_new_dir(cwd, FALSE, ".+")) {
                fprintf(stderr, "invalid current directory\n");
                exit(1);
            }
            if(!filer_new_dir(cwd, FALSE, ".+")) {
                fprintf(stderr, "invalid current directory\n");
                exit(1);
            }
        }
        else {
            sObject* file_name2 = STRING_NEW_MALLOC(file_name);
            string_quote(file_name2, kUtf8);

            if(!filer_new_dir(file_name, FALSE, ".+")) {
                fprintf(stderr, "invalid directory(%s)\n", file_name);
                exit(1);
            }
            if(!filer_new_dir(file_name, FALSE, ".+")) {
                fprintf(stderr, "invalid directory(%s)\n", file_name);
                exit(1);
            }
        }
    }

    (void)filer_activate(0);

    /// keybind ///
    if(!filer_add_keycommand2(0, 'q', "quit", "system", FALSE) || !filer_add_keycommand2(0, 3, "quit", "system", FALSE)) 
    {
        xyzsh_kill_all_jobs();

        isearch_final();
        menu_final();
        filer_final();
        xyzsh_final();
        commands_final();

        mreset_tty();
        //mrestore_ttysettings();

        CHECKML_END();

        puts("");

        mreset_tty();
        exit(1);
    }

    /// run time script ///
    gMainLoop = -1;

    if(!read_rc_file(argc, argv)) {
        xyzsh_kill_all_jobs();

        isearch_final();
        menu_final();
        filer_final();
        xyzsh_final();
        commands_final();

        mreset_tty();
        //mrestore_ttysettings();

        CHECKML_END();

        puts("");

        mreset_tty();
        exit(1);
    }

    /// signal ///
    set_signal_mfiler();

    /// start curses ///
//    xinitscr();

    //run_on_temulator(temulator_fun, NULL);

    /// start curses ///
    xinitscr();

    /// main loop ///
    fd_set mask;
    fd_set read_ok;

    FD_ZERO(&mask);
    FD_SET(0, &mask);

    while(gMainLoop == -1) {
        /// drawing ///
        xclear();
        view();
        refresh();

        const int maxy = mgetmaxy();
        mmove_immediately(maxy-2, 0);

        /// select for waiting to input ///
        read_ok = mask;
        if(xyzsh_job_num() > 0) {
            struct timeval tv;
            tv.tv_sec = 5;
            tv.tv_usec = 0;

            select(5, &read_ok, NULL, NULL, &tv);
        }
        else {
            select(5, &read_ok, NULL, NULL, NULL);
        }

        /// input ///
#ifdef HAVE_LIB_CURSESW
        if(FD_ISSET(0, &read_ok)) {
            int meta;
            int* key;
            int key_size;
            xgetch_utf8(&meta, ALLOC &key, &key_size);

            input_utf8(meta, key, key_size);

            FREE(key);
        }
#else
        if(FD_ISSET(0, &read_ok)) {
            int meta;
            int key = xgetch(&meta);

            input(meta, key);
        }
#endif

        /// works to background jobs ///
        xyzsh_wait_background_job();
    }

    /// end curses ///
    endwin();

    /// kill all jobs ///
    xyzsh_kill_all_jobs();

    /// finalization for modules ///
    isearch_final();
    menu_final();
    filer_final();
    xyzsh_final();
    commands_final();

    mreset_tty();
    //mrestore_ttysettings();

    /// checking memory leaks ///
    CHECKML_END();

    /// puts ///
    puts("");

    mreset_tty();

    return gMainLoop;
}

