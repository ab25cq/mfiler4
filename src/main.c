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
        isearch_input(meta, key);
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

#if defined(HAVE_MIGEMO_H)
    puts("+migemo");
#else
    puts("-migemo");
#endif
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
    setenv("PROMPT", "can't read run time script '.mfiler4'. press q key to quit", 1);
    setenv("VIEW_FILE_SIZE", "Normal", 1);

    setenv("MF4HOME", gHomeDir, 1);
    setenv("MF4TEMP", gTempDir, 1);
    setenv("DATADIR", DATAROOTDIR, 1);
    setenv("DOTDIR_MASK", "0", 1);

    char buf[128];

    snprintf(buf, 128, "%d", KEY_UP);
    setenv("key_up", buf, 1);
    snprintf(buf, 128, "%d", KEY_RIGHT);
    setenv("key_right", buf, 1);
    snprintf(buf, 128, "%d", KEY_DOWN);
    setenv("key_down", buf, 1);
    snprintf(buf, 128, "%d", KEY_LEFT);
    setenv("key_left", buf, 1);
    snprintf(buf, 128, "%d", KEY_IC);
    setenv("key_insert", buf, 1);
    snprintf(buf, 128, "%d", KEY_DC);
    setenv("key_delete2", buf, 1);
    snprintf(buf, 128, "%d", 127);
    setenv("key_delete", buf, 1);
    snprintf(buf, 128, "%d", KEY_HOME);
    setenv("key_home", buf, 1);
    snprintf(buf, 128, "%d", KEY_END);
    setenv("key_end", buf, 1);
    snprintf(buf, 128, "%d", KEY_PPAGE);
    setenv("key_pageup", buf, 1);
    snprintf(buf, 128, "%d", KEY_NPAGE);
    setenv("key_pagedown", buf, 1);

    snprintf(buf,128, "%d", 10);
    setenv("key_enter", buf, 1);
    snprintf(buf,128, "%d", KEY_BACKSPACE);
    setenv("key_backspace", buf, 1);
    snprintf(buf,128, "%d", KEY_F(1));
    setenv("key_f1", buf, 1);
    snprintf(buf,128, "%d", KEY_F(2));
    setenv("key_f2", buf, 1);
    snprintf(buf,128, "%d", KEY_F(3));
    setenv("key_f3", buf, 1);
    snprintf(buf,128, "%d", KEY_F(4));
    setenv("key_f4", buf, 1);
    snprintf(buf,128, "%d", KEY_F(5));
    setenv("key_f5", buf, 1);
    snprintf(buf,128, "%d", KEY_F(6));
    setenv("key_f6", buf, 1);
    snprintf(buf,128, "%d", KEY_F(7));
    setenv("key_f7", buf, 1);
    snprintf(buf,128, "%d", KEY_F(8));
    setenv("key_f8", buf, 1);
    snprintf(buf,128, "%d", KEY_F(9));
    setenv("key_f9", buf, 1);
    snprintf(buf,128, "%d", KEY_F(10));
    setenv("key_f10", buf, 1);
    snprintf(buf,128, "%d", KEY_F(11));
    setenv("key_f11", buf, 1);
    snprintf(buf,128, "%d", KEY_F(12));
    setenv("key_f12", buf, 1);
    snprintf(buf,128, "%d", 'a');
    setenv("key_a", buf, 1);
    snprintf(buf,128, "%d", 'b');
    setenv("key_b", buf, 1);
    snprintf(buf,128, "%d", 'c');
    setenv("key_c", buf, 1);
    snprintf(buf,128, "%d", 'd');
    setenv("key_d", buf, 1);
    snprintf(buf,128, "%d", 'e');
    setenv("key_e", buf, 1);
    snprintf(buf,128, "%d", 'f');
    setenv("key_f", buf, 1);
    snprintf(buf,128, "%d", 'g');
    setenv("key_g", buf, 1);
    snprintf(buf,128, "%d", 'h');
    setenv("key_h", buf, 1);
    snprintf(buf,128, "%d", 'i');
    setenv("key_i", buf, 1);
    snprintf(buf,128, "%d", 'j');
    setenv("key_j", buf, 1);
    snprintf(buf,128, "%d", 'k');
    setenv("key_k", buf, 1);
    snprintf(buf,128, "%d", 'l');
    setenv("key_l", buf, 1);
    snprintf(buf,128, "%d", 'm');
    setenv("key_m", buf, 1);
    snprintf(buf,128, "%d", 'n');
    setenv("key_n", buf, 1);
    snprintf(buf,128, "%d", 'o');
    setenv("key_o", buf, 1);
    snprintf(buf,128, "%d", 'p');
    setenv("key_p", buf, 1);
    snprintf(buf,128, "%d", 'q');
    setenv("key_q", buf, 1);
    snprintf(buf,128, "%d", 'r');
    setenv("key_r", buf, 1);
    snprintf(buf,128, "%d", 's');
    setenv("key_s", buf, 1);
    snprintf(buf,128, "%d", 't');
    setenv("key_t", buf, 1);
    snprintf(buf,128, "%d", 'u');
    setenv("key_u", buf, 1);
    snprintf(buf,128, "%d", 'v');
    setenv("key_v", buf, 1);
    snprintf(buf,128, "%d", 'w');
    setenv("key_w", buf, 1);
    snprintf(buf,128, "%d", 'x');
    setenv("key_x", buf, 1);
    snprintf(buf,128, "%d", 'y');
    setenv("key_y", buf, 1);
    snprintf(buf,128, "%d", 'z');
    setenv("key_z", buf, 1);
    snprintf(buf,128, "%d", 'A');
    setenv("key_A", buf, 1);
    snprintf(buf,128, "%d", 'B');
    setenv("key_B", buf, 1);
    snprintf(buf,128, "%d", 'C');
    setenv("key_C", buf, 1);
    snprintf(buf,128, "%d", 'D');
    setenv("key_D", buf, 1);
    snprintf(buf,128, "%d", 'E');
    setenv("key_E", buf, 1);
    snprintf(buf,128, "%d", 'F');
    setenv("key_F", buf, 1);
    snprintf(buf,128, "%d", 'G');
    setenv("key_G", buf, 1);
    snprintf(buf,128, "%d", 'H');
    setenv("key_H", buf, 1);
    snprintf(buf,128, "%d", 'I');
    setenv("key_I", buf, 1);
    snprintf(buf,128, "%d", 'J');
    setenv("key_J", buf, 1);
    snprintf(buf,128, "%d", 'K');
    setenv("key_K", buf, 1);
    snprintf(buf,128, "%d", 'L');
    setenv("key_L", buf, 1);
    snprintf(buf,128, "%d", 'M');
    setenv("key_M", buf, 1);
    snprintf(buf,128, "%d", 'N');
    setenv("key_N", buf, 1);
    snprintf(buf,128, "%d", 'O');
    setenv("key_O", buf, 1);
    snprintf(buf,128, "%d", 'P');
    setenv("key_P", buf, 1);
    snprintf(buf,128, "%d", 'Q');
    setenv("key_Q", buf, 1);
    snprintf(buf,128, "%d", 'R');
    setenv("key_R", buf, 1);
    snprintf(buf,128, "%d", 'S');
    setenv("key_S", buf, 1);
    snprintf(buf,128, "%d", 'T');
    setenv("key_T", buf, 1);
    snprintf(buf,128, "%d", 'U');
    setenv("key_U", buf, 1);
    snprintf(buf,128, "%d", 'V');
    setenv("key_V", buf, 1);
    snprintf(buf,128, "%d", 'W');
    setenv("key_W", buf, 1);
    snprintf(buf,128, "%d", 'X');
    setenv("key_X", buf, 1);
    snprintf(buf,128, "%d", 'Y');
    setenv("key_Y", buf, 1);
    snprintf(buf,128, "%d", 'Z');
    setenv("key_Z", buf, 1);
    snprintf(buf,128, "%d", 32);
    setenv("key_space", buf, 1);
    snprintf(buf,128, "%d", 1);
    setenv("key_ctrl_a", buf, 1);
    snprintf(buf,128, "%d", 2);
    setenv("key_ctrl_b", buf, 1);
    snprintf(buf,128, "%d", 3);
    setenv("key_ctrl_c", buf, 1);
    snprintf(buf,128, "%d", 4);
    setenv("key_ctrl_d", buf, 1);
    snprintf(buf,128, "%d", 5);
    setenv("key_ctrl_e", buf, 1);
    snprintf(buf,128, "%d", 6);
    setenv("key_ctrl_f", buf, 1);
    snprintf(buf,128, "%d", 7);
    setenv("key_ctrl_g", buf, 1);
    snprintf(buf,128, "%d", 8);
    setenv("key_ctrl_h", buf, 1);
    snprintf(buf,128, "%d", 9);
    setenv("key_ctrl_i", buf, 1);
    snprintf(buf,128, "%d", 10);
    setenv("key_ctrl_j", buf, 1);
    snprintf(buf,128, "%d", 11);
    setenv("key_ctrl_k", buf, 1);
    snprintf(buf,128, "%d", 12);
    setenv("key_ctrl_l", buf, 1);
    snprintf(buf,128, "%d", 13);
    setenv("key_ctrl_m", buf, 1);
    snprintf(buf,128, "%d", 14);
    setenv("key_ctrl_n", buf, 1);
    snprintf(buf,128, "%d", 15);
    setenv("key_ctrl_o", buf, 1);
    snprintf(buf,128, "%d", 16);
    setenv("key_ctrl_p", buf, 1);
    snprintf(buf,128, "%d", 17);
    setenv("key_ctrl_q", buf, 1);
    snprintf(buf,128, "%d", 18);
    setenv("key_ctrl_r", buf, 1);
    snprintf(buf,128, "%d", 19);
    setenv("key_ctrl_s", buf, 1);
    snprintf(buf,128, "%d", 20);
    setenv("key_ctrl_t", buf, 1);
    snprintf(buf,128, "%d", 21);
    setenv("key_ctrl_u", buf, 1);
    snprintf(buf,128, "%d", 22);
    setenv("key_ctrl_v", buf, 1);
    snprintf(buf,128, "%d", 23);
    setenv("key_ctrl_w", buf, 1);
    snprintf(buf,128, "%d", 24);
    setenv("key_ctrl_x", buf, 1);
    snprintf(buf,128, "%d", 25);
    setenv("key_ctrl_y", buf, 1);
    snprintf(buf,128, "%d", 26);
    setenv("key_ctrl_z", buf, 1);
    snprintf(buf,128, "%d", 27);
    setenv("key_escape", buf, 1);
    snprintf(buf,128, "%d", 9);
    setenv("key_tab", buf, 1);
    snprintf(buf,128, "%d", '0');
    setenv("key_0", buf, 1);
    snprintf(buf,128, "%d", '1');
    setenv("key_1", buf, 1);
    snprintf(buf,128, "%d", '2');
    setenv("key_2", buf, 1);
    snprintf(buf,128, "%d", '3');
    setenv("key_3", buf, 1);
    snprintf(buf,128, "%d", '4');
    setenv("key_4", buf, 1);
    snprintf(buf,128, "%d", '5');
    setenv("key_5", buf, 1);
    snprintf(buf,128, "%d", '6');
    setenv("key_6", buf, 1);
    snprintf(buf,128, "%d", '7');
    setenv("key_7", buf, 1);
    snprintf(buf,128, "%d", '8');
    setenv("key_8", buf, 1);
    snprintf(buf,128, "%d", '9');
    setenv("key_9", buf, 1);
    snprintf(buf,128, "%d", '!');
    setenv("key_exclam", buf, 1);
    snprintf(buf,128, "%d", '"');
    setenv("key_dquote", buf, 1);
    snprintf(buf,128, "%d", '#');
    setenv("key_sharp", buf, 1);
    snprintf(buf,128, "%d", '$');
    setenv("key_dollar", buf, 1);
    snprintf(buf,128, "%d", '%');
    setenv("key_percent", buf, 1);
    snprintf(buf,128, "%d", '&');
    setenv("key_and", buf, 1);
    snprintf(buf,128, "%d", '\'');
    setenv("key_squote", buf, 1);
    snprintf(buf,128, "%d", '(');
    setenv("key_lparen", buf, 1);
    snprintf(buf,128, "%d", ')');
    setenv("key_rparen", buf, 1);
    snprintf(buf,128, "%d", '~');
    setenv("key_tilda", buf, 1);
    snprintf(buf,128, "%d", '=');
    setenv("key_equal", buf, 1);
    snprintf(buf,128, "%d", '-');
    setenv("key_minus", buf, 1);
    snprintf(buf,128, "%d", '^');
    setenv("key_cup", buf, 1);
    snprintf(buf,128, "%d", '|');
    setenv("key_vbar", buf, 1);
    snprintf(buf,128, "%d", '\\');
    setenv("key_backslash", buf, 1);
    snprintf(buf,128, "%d", '@');
    setenv("key_atmark", buf, 1);
    snprintf(buf,128, "%d", '`');
    setenv("key_bapostrophe", buf, 1);
    snprintf(buf,128, "%d", '{');
    setenv("key_lcurly", buf, 1);
    snprintf(buf,128, "%d", '[');
    setenv("key_lbrack", buf, 1);
    snprintf(buf,128, "%d", '+');
    setenv("key_plus", buf, 1);
    snprintf(buf,128, "%d", ';');
    setenv("key_semicolon", buf, 1);
    snprintf(buf,128, "%d", '*');
    setenv("key_star", buf, 1);
    snprintf(buf,128, "%d", ':');
    setenv("key_colon", buf, 1);
    snprintf(buf,128, "%d", '}');
    setenv("key_rcurly", buf, 1);
    snprintf(buf,128, "%d", ']');
    setenv("key_rbrack", buf, 1);
    snprintf(buf,128, "%d", '<');
    setenv("key_lss", buf, 1);
    snprintf(buf,128, "%d", ',');
    setenv("key_comma", buf, 1);
    snprintf(buf,128, "%d", '>');
    setenv("key_gtr", buf, 1);
    snprintf(buf,128, "%d", '.');
    setenv("key_dot", buf, 1);
    snprintf(buf,128, "%d", '/');
    setenv("key_slash", buf, 1);
    snprintf(buf,128, "%d", '?');
    setenv("key_qmark", buf, 1);
    snprintf(buf,128, "%d", '_');
    setenv("key_underbar", buf, 1);
    snprintf(buf,128, "%d", 0);
    setenv("nometa", buf, 1);
    snprintf(buf,128, "%d", 1);
    setenv("meta", buf, 1);
    snprintf(buf,128, "%d", A_REVERSE);
    setenv("ma_reverse", buf, 1);
    snprintf(buf,128, "%d", A_BOLD);
    setenv("ma_bold", buf, 1);
    snprintf(buf,128, "%d", A_UNDERLINE);
    setenv("ma_underline", buf, 1);
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

int main(int argc, char* argv[])
{
    CHECKML_BEGIN(FALSE);    // start to watch memory leak

    /// initialization for envronment variable ///
    setenv("VERSION", "1.0.9", 1);
    setenv("MFILER4_DATAROOTDIR", DATAROOTDIR, 1);

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

                default:
                    usage();
            }
        }
    }

    if(!isatty(0) || !isatty(1)) {
        fprintf(stderr, "standard input is not a tty\n");
        return 1;
    }

    /// set environment variables ///
    set_mfenv();

    /// initialization for all modules ///
    xyzsh_init(kATConsoleApp, FALSE);
    commands_init();
    filer_init();
    menu_init();
    gui_init();
    isearch_init();

    /// signal ///
    set_signal_mfiler();

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
        if(FD_ISSET(0, &read_ok)) {
            int meta;
            int key = xgetch(&meta);

            input(meta, key);
        }

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

