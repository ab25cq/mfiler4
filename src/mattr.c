#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <utime.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#define BOOL int
#define TRUE 1
#define FALSE 0

#if defined(__CYGWIN__)
#include <ncurses/term.h>
#include <sys/time.h>
#include <ncurses/ncurses.h>
#else
#include <curses.h>
#endif

void sig_exit(int signal)
{
    exit(1);
}

int main(int argc, char* argv[])
{
    /// is terminal ///
    if(!isatty(0) || !isatty(1)) {
        fprintf(stderr, "standard input is not a tty\n");
        return 1;
    }

    /// signal ///
    signal(SIGINT, sig_exit);
    signal(SIGQUIT, sig_exit);
    signal(SIGABRT, sig_exit);
    signal(SIGKILL, sig_exit);
    signal(SIGPIPE, sig_exit);
    signal(SIGALRM, sig_exit);
    signal(SIGTERM, sig_exit);
    signal(SIGHUP, sig_exit);

    /// check ///
    if(argc < 2) {
        fprintf(stderr, "no argument\n");
        exit(1);
    }
    int i;
    for(i=1; i<argc; i++) {
       if(access(argv[i], F_OK) != 0) {
           fprintf(stderr, "invalid file name\n");
           exit(1);
       }
    }
    
    /// get file stat///
    struct stat stat_;
    memset(&stat_, 0, sizeof(struct stat));
    if(stat(argv[1], &stat_) < 0) {
        fprintf(stderr, "stat err\n");
        exit(1);
    }

    /// editing mode ///
    enum eEditing { kModeYesNo, kMode, kDateYesNo, kDate, kOK };
    enum eEditing editing = kModeYesNo;
    BOOL cancel = FALSE;

    /// mode ///
    mode_t mode;
    if(argc == 2) {
        mode = stat_.st_mode;
    }
    else {
        mode = 0;
    }
    int mPCursor = 0;
    BOOL flg_change_permission = TRUE;

    /// date ///
    int mDCursor = 0;
    struct tm* mtimetm;
    if(argc == 2) {
        mtimetm = (struct tm*)localtime(&stat_.st_mtime);
    }
    else {
        time_t tmp = time(NULL);
        mtimetm = (struct tm*)localtime(&tmp);
    }
    BOOL flg_change_date = TRUE;
    
    /// start curses ///
    initscr();
    noecho();
    keypad(stdscr, 1);
    raw();

    clear();

    /// start main loop ///
    
    int key = -1;
    while(1) {
        /// draw ///
        attron(A_REVERSE);
        mvprintw(0,0, "+++ mattr (permission and date editor) +++");
        attroff(A_REVERSE);

        mvprintw(2, 0, "OK -> Enter Key, Cansel -> CTRL+C");

        char permission[10];

        permission[0] = mode & S_IRUSR ? 'r': '-';
        permission[1] = mode & S_IWUSR ? 'w': '-';
        permission[2] = mode & S_IXUSR ? 'x': '-';
        permission[3] = mode & S_IRGRP ? 'r': '-';
        permission[4] = mode & S_IWGRP ? 'w': '-';
        permission[5] = mode & S_IXGRP ? 'x': '-';
        permission[6] = mode & S_IROTH ? 'r': '-';
        permission[7] = mode & S_IWOTH ? 'w': '-';
        permission[8] = mode & S_IXOTH ? 'x': '-';
        permission[9] = 0;

        mvprintw(4,0, "permission: ");
        if(flg_change_permission) {
            if(kMode == kModeYesNo) attron(A_REVERSE);
            printw("CHANGE   ");
            if(kMode == kModeYesNo) attroff(A_REVERSE);
        }
        else {
            if(kMode == kModeYesNo) attron(A_REVERSE);
            printw("NO CHANGE");
            if(kMode == kModeYesNo) attroff(A_REVERSE);
        }
        mvprintw(4, 23, "%s",permission);
        
        mvprintw(5,0, "date      : ");
        if(flg_change_date) {
            if(kMode == kDateYesNo) attron(A_REVERSE);
            printw("CHANGE   ");
            if(kMode == kDateYesNo) attroff(A_REVERSE);
        }
        else {
            if(kMode == kDateYesNo) attron(A_REVERSE);
            printw("NO CHANGE");
            if(kMode == kDateYesNo) attroff(A_REVERSE);
        }
        
        mvprintw(5,23, "%3d %02d-%02d %02d:%02d"
                       , mtimetm->tm_year + 1900, mtimetm->tm_mon+1
                       , mtimetm->tm_mday, mtimetm->tm_hour, mtimetm->tm_min);

        mvprintw(7, 0, "name      : ");
        int i;
        for(i=1; i<argc; i++) {
            printw("%s ", argv[i]);
        }
        
        if(editing == kOK)
            mvprintw(7, 0, "Write OK?(Y/n)");        

        /// cursor ///
        switch(editing) {
        case kModeYesNo:
            move(4, 12);
            break;
        case kMode:
            move(4, 23+mPCursor);
            break;

        case kDateYesNo:
            move(5, 12);
            break;

        case kDate: {
            const int cursor[12] = {
                23, 28, 31, 34, 37
            };
            
            move(5, cursor[mDCursor]);
            }
            break;
            
        }
        
        refresh();
        
        /// input ///
        key = getch();

        if(editing == kModeYesNo) {
            if(key == ' ' || key == 16 || key == KEY_UP    // CTRL+P 
                || key == 14 || key == KEY_DOWN)           // CTRL+N
            {
                flg_change_permission = !flg_change_permission;
            }
            else if(key == 10 || key == 13) {    // CTRL+M CTRL+J
                if(flg_change_permission) {
                    editing = kMode;
                    mPCursor = 0;
                }
                else {
                    editing = kDateYesNo;
                }
            }
            else if(key == 3 || key == 7) {
                cancel = TRUE;
                break;
            }
        }
        else if(editing == kMode) {
            if(key == ' ' || key == 16 || key == KEY_UP    // CTRL+P 
                || key == 14 || key == KEY_DOWN)           // CTRL+N
            {
                const int mode_flg[9] = {
                    S_IRUSR, S_IWUSR, S_IXUSR,
                    S_IRGRP, S_IWGRP, S_IXGRP,
                    S_IROTH, S_IWOTH, S_IXOTH
                };
                
                mode = mode ^ mode_flg[mPCursor];
            }
            else if(key == 6 || key == KEY_RIGHT) {    // CTRL+F
                mPCursor++;
                if(mPCursor > 8) mPCursor = 8;
            }
            else if(key == 2 || key == KEY_LEFT) {    // CTRL+B
                mPCursor--;
                if(mPCursor < 0) mPCursor = 0;
            }
            else if(key == 10 || key == 13) {    // CTRL+M CTRL+J
                editing = kDateYesNo;
            }
            else if(key == 3 || key == 7) {
                editing = kModeYesNo;
            }
        }
        
        else if(editing == kDateYesNo) {
            if(key == ' ' || key == 16 || key == KEY_UP    // CTRL+P 
                || key == 14 || key == KEY_DOWN)           // CTRL+N
            {
                flg_change_date = !flg_change_date;
            }
            else if(key == 10 || key == 13) {    // CTRL+M CTRL+J
                if(flg_change_date) {
                    editing = kDate;
                    mDCursor = 0;
                }
                else {
                    editing = kOK;
                }
            }
            else if(key == 3 || key == 7) {
                editing = kModeYesNo;
            }
        }
        else if(editing == kDate) {
            if(key == 10 || key == 13) {    // CTRL+M CTRL+J
                editing = kOK;
            }
            else if(key == 6 || key == KEY_RIGHT) {    // CTRL+F
                mDCursor++;
                if(mDCursor > 4) mDCursor = 4;
            }
            else if(key == 2 || key == KEY_LEFT) {    // CTRL+B
                mDCursor--;
                if(mDCursor < 0) mDCursor = 0;
            }
            else if(key == 16 || key == KEY_UP) { // CTRL-P
                switch(mDCursor) {
                case 0:
                    mtimetm->tm_year++;
                    break;
                case 1:
                    mtimetm->tm_mon++;
                    break;
                case 2:
                    mtimetm->tm_mday++;
                    break;
                case 3:
                    mtimetm->tm_hour++;
                    break;
                case 4:
                    mtimetm->tm_min++;
                    break;
                }
            }
            else if(key == 14 || key == KEY_DOWN) { // CTRL-N
                switch(mDCursor) {
                case 0:
                    mtimetm->tm_year--;
                    break;
                case 1:
                    mtimetm->tm_mon--;
                    break;
                case 2:
                    mtimetm->tm_mday--;
                    break;
                case 3:
                    mtimetm->tm_hour--;
                    break;
                case 4:
                    mtimetm->tm_min--;
                    break;
                }
            }
            else if(key == 3 || key == 7) {
                editing = kDateYesNo;
            }
        }
else if(editing == kOK) {
            if(key == 'y' || key == 'Y' || key == 10 || key == 13) {
                break;
            }
            else if(key == 'n' || key == 'N') {
                cancel = TRUE;
                break;
            }
            else if(key == 3 || key == 7) {
                editing = kDateYesNo;
            }
        }
        
        /// rage check ///
        if(mtimetm->tm_year <= 0)
            mtimetm->tm_year = 0;
        else if(mtimetm->tm_year > 8099)
            mtimetm->tm_year = 8099;
        else if(mtimetm->tm_mon < 0)
            mtimetm->tm_mon = 0;
        else if(mtimetm->tm_mon > 11)
            mtimetm->tm_mon = 11;
        else if(mtimetm->tm_mday < 1)
            mtimetm->tm_mday = 1;
        else if(mtimetm->tm_mday > 31)
            mtimetm->tm_mday = 31;
        else if(mtimetm->tm_hour < 0)
            mtimetm->tm_hour = 0;
        else if(mtimetm->tm_hour > 23)
            mtimetm->tm_hour = 23;
        else if(mtimetm->tm_min < 0)
            mtimetm->tm_min = 0;
        else if(mtimetm->tm_min > 59)
            mtimetm->tm_min = 59;
    }
    
    endwin();

    if(!cancel) {
        struct utimbuf tb;
        tb.actime = stat_.st_atime;
        tb.modtime = mktime(mtimetm);
        
        int i;
        for(i=1; i<argc; i++) {
            if(flg_change_permission) {
                if(chmod(argv[i], mode) == -1) {
                    switch(errno) {
                        case ENAMETOOLONG :
                            fprintf(stderr, "chmod: path(%s) is too long\n", argv[i]);
                            break;

                        case ENOENT :
                            fprintf(stderr, "chmod: %s doesn't exist\n", argv[i]);
                            break;

                        case EPERM :
                            fprintf(stderr, "chmod: permission denied. (%s)\n", argv[i]);
                            break;

                        case EROFS :
                            fprintf(stderr, "chmod: file system is read only.(%s)\n", argv[i]);
                            break;

                        default:
                            fprintf(stderr, "chmod(%s) is error\n", argv[i]);
                            break;
                    }
                }
            }
            if(flg_change_date) {
                if(utime(argv[i], &tb) == -1) {
                    switch(errno) {
                        case EACCES:
                            fprintf(stderr, "utime: access err(%s)\n", argv[i]);
                            break;

                        case ENOENT:
                            fprintf(stderr, "utime: path(%s) doesn't exist\n", argv[i]);
                            break;

                        case EPERM:
                            fprintf(stderr, "utime: permission denied (%s)\n", argv[i]);
                            break;

                        case EROFS:
                            fprintf(stderr, "utime: file system is read only (%s)\n", argv[i]);
                            break;

                        default:
                            fprintf(stderr, "utime(%s) is error\n", argv[i]);
                            break;
                    }
                }
            }
        }
    }

    return 0;
}

