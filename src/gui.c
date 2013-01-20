#include "common.h"
#include <ctype.h>
#include <stdio.h>

#include "config.h"

#if defined(HAVE_CURSES_H)
#include <curses.h>
#elif defined(HAVE_NCURSES_H)
#include <ncurses.h>
#elif defined(HAVE_NCURSES_NCURSES_H)
#include <ncurses/ncurses.h>
#endif

void xinitscr()
{
    initscr();
    raw();
    noecho();
    keypad(stdscr, TRUE);
    //curs_set(0);

    int background = COLOR_BLACK;

    if(has_colors()) {
        if(start_color() == OK) {
            init_pair(1, COLOR_WHITE, background);
            init_pair(2, COLOR_BLUE, background);
            init_pair(3, COLOR_CYAN, background);
            init_pair(4, COLOR_GREEN, background);
            init_pair(5, COLOR_YELLOW, background);
            init_pair(6, COLOR_MAGENTA, background);
            init_pair(7, COLOR_RED, background);

            setenv("HAS_COLOR", "1", 1);
        }
        else {
            setenv("HAS_COLOR", "0", 1);
        }
    }
    else {
        setenv("HAS_COLOR", "0", 1);
    }
}

void xendwin()
{
    keypad(stdscr, FALSE);
    noraw();
    endwin();
}

int xgetch(int* meta)
{
    int key = getch();

    if(key == 27) {
        key = getch();

        *meta = 1;
        return key;
    }
    else {
        if(key >= kKeyMetaFirst
            && key <= kKeyMetaFirst + 127) 
        {
            *meta = 1;
            return key - kKeyMetaFirst;
        }
        else {
            *meta = 0;
            return key;
        }
    }
}

///////////////////////////////////////////////////
// エラーメッセージ表示
///////////////////////////////////////////////////
void merr_msg(char* msg, ...)
{
    if(gMainLoop == -1) {
        char msg2[1024];
        va_list args;
        va_start(args, msg);
        vsnprintf(msg2, 1024, msg, args);
        va_end(args);
        
        const int maxy = mgetmaxy();
        const int maxx = mgetmaxx();

        if(mis_raw_mode()) {
            clear();
            view();
            mclear_online(maxy-2);
            mclear_online(maxy-1);
            mvprintw(maxy-2, 0, "%s", msg2);
            refresh();

            (void)getch();

#if defined(__CYGWIN__)
            xclear_immediately();       // 画面の再描写
            view();
            refresh();
#endif
        }
        else {
            fprintf(stderr, "%s", msg2);
        }
    }
    else {
        char msg2[1024];
        va_list args;
        va_start(args, msg);
        vsnprintf(msg2, 1024, msg, args);
        va_end(args);

        fprintf(stderr, "%s", msg2);
    }
}

///////////////////////////////////////////////////
// エラーメッセージのノンストップ表示
///////////////////////////////////////////////////
void msg_nonstop(char* msg, ...)
{
    char msg2[1024];

    va_list args;
    va_start(args, msg);
    vsnprintf(msg2, 1024, msg, args);
    va_end(args);

    const int maxy = mgetmaxy();
    const int maxx = mgetmaxx();

    xclear();
    view();
    mclear_online(maxy-2);
    mclear_online(maxy-1);
    mvprintw(maxy-2, 0, "%s", msg2);
    refresh();
}

///////////////////////////////////////////////////
// 選択
///////////////////////////////////////////////////
char* choise(char* msg, char* str[], int len, int cancel)
{
    const int maxy = mgetmaxy();
    const int maxx = mgetmaxx();

    int cursor = 0;
    
    while(1) {
        /// view ///
        clear();
        view();

        mclear_online(maxy-2);
        mclear_online(maxy-1);

        move(maxy-2, 0);
        printw("%s", msg);
        
        printw(" ");
        int i;
        for(i=0; i< len; i++) {
            if(cursor == i) {
                attron(A_REVERSE);
                printw("%s", str[i]);
                attroff(A_REVERSE);
                printw(" ");
            }
            else {
                printw("%s ", str[i]);
            }
        }
        move(maxy-1, maxx-2);
        refresh();


        /// input ///
        int meta;
        int key = xgetch(&meta);
        if(key == 10 || key == 13) {
            break;
        }
        else if(key == 6 || key == KEY_RIGHT) {
            cursor++;

            if(cursor >= len) cursor = len-1;
        }
        else if(key == 2 || key == KEY_LEFT) {
            cursor--;

            if(cursor < 0) cursor= 0;
        }
        else if(key == 12) {            // CTRL-L
            xclear_immediately();
        }
        else if(key == 3 || key == 7 || key == 27) { // CTRL-C -G Escape
            return NULL;
        }
        else {
            int i;
            for(i=0; i< len; i++) {
                if(toupper(key) == toupper(str[i][0])) {
                    cursor = i;
                    goto finished;
                }
            }
        }
    }
finished:

#if defined(__CYGWIN__)
    xclear_immediately();       // 画面の再描写
    view();
    refresh();
#endif

    return str[cursor];
}

///////////////////////////////////////////////////////////////////
// インプットボックス
///////////////////////////////////////////////////////////////////
// result 0: ok 1: cancel
static void input_box_cursor_move(sObject* input, int* cursor, int v)
{
    char* str = string_c_str(input);
    int utfpos = str_pointer2kanjipos(gKanjiCode, str, str + *cursor);
    utfpos+=v;
    *cursor = str_kanjipos2pointer(gKanjiCode, str, utfpos) - str;
}

char* gInputBoxMsg;
sObject* gInputBoxInput = NULL;
int gInputBoxCursor;
    
void input_box_view()
{
    int maxx = mgetmaxx();
    int maxy = mgetmaxy();

    /// view ///
    mclear_online(maxy-2);
    mclear_online(maxy-1);
    
    mvprintw(maxy-2, 0, "%s", gInputBoxMsg);
    
    move(maxy-1, 0);
    
    const int len = string_length(gInputBoxInput);
    int i;
    for(i=0; i< len && i<maxx-1; i++) {
        printw("%c", string_c_str(gInputBoxInput)[i]);
    }

    //move_immediately(maxy -1, gInputBoxCursor);
    move(maxy -1, gInputBoxCursor);
}

int input_box(char* msg, char* result, int result_size, char* def_input, int def_cursor)
{
    gInputBoxMsg = msg;
    
    int result2 = 0;
    gInputBoxCursor = def_cursor;

    string_put(gInputBoxInput, def_input);
    
    gView = input_box_view;
    
    while(1) {
        input_box_view();
        refresh();

        /// input ///
        int meta;
        int key = xgetch(&meta);
        
        if(key == 10 || key == 13) {
            result2 = 0;
            break;
        }
        else if(key == 6 || key == KEY_RIGHT) {
            input_box_cursor_move(gInputBoxInput, &gInputBoxCursor, 1);
        }
        else if(key == 2 || key == KEY_LEFT) {
            input_box_cursor_move(gInputBoxInput, &gInputBoxCursor, -1);
        }
        else if(key == 8 || key == KEY_BACKSPACE) {    // CTRL-H
            if(gInputBoxCursor > 0) {
                char* str2 = string_c_str(gInputBoxInput);

                int utfpos = str_pointer2kanjipos(gKanjiCode, str2, str2 + gInputBoxCursor);
                char* before_point = str_kanjipos2pointer(gKanjiCode, str2, utfpos-1);
                int new_cursor = before_point-str2;

                string_erase(gInputBoxInput, before_point - str2, (str2 + gInputBoxCursor) - before_point);
                gInputBoxCursor = new_cursor;
            }
        }
        else if(key == 4 || key == KEY_DC) {    // CTRL-D DELETE
            char* str2 = string_c_str(gInputBoxInput);
            if(string_length(gInputBoxInput) > 0) {
                if(gInputBoxCursor < string_length(gInputBoxInput)) {
                    int utfpos = str_pointer2kanjipos(gKanjiCode, str2, str2 + gInputBoxCursor);
                    char* next_point = str_kanjipos2pointer(gKanjiCode, str2, utfpos+1);

                    string_erase(gInputBoxInput, gInputBoxCursor, next_point - (str2 + gInputBoxCursor));
                }
            }
        }
        else if(key == 1 || key == KEY_HOME) {    // CTRL-A
            input_box_cursor_move(gInputBoxInput, &gInputBoxCursor, -999);
        }
        else if(key == 5 || key == KEY_END) {    // CTRL-E
            input_box_cursor_move(gInputBoxInput, &gInputBoxCursor, 999);
        }
        else if(key == 11) {    // CTRL-K
            string_erase(gInputBoxInput, gInputBoxCursor, string_length(gInputBoxInput)-gInputBoxCursor);
        }
        
        else if(key == 21) {    // CTRL-U
            string_put(gInputBoxInput, "");

            gInputBoxCursor = 0;
        }
        else if(key == 23) {     // CTRL-W
            if(gInputBoxCursor > 0) {
                const char* s = string_c_str(gInputBoxInput);
                int pos = gInputBoxCursor-1;
                if(s[pos]==' ' || s[pos]=='/' || s[pos]=='\'' || s[pos]=='"') {
                    while(pos>=0 && (s[pos]==' ' || s[pos]=='/' || s[pos]=='\'' || s[pos]=='"'))
                    {
                        pos--;
                    }
                }
                while(pos>=0 && s[pos]!=' ' && s[pos]!='/' && s[pos]!='\'' && s[pos]!='"')
                {
                    pos--;
                }

                string_erase(gInputBoxInput, pos+1, gInputBoxCursor-pos-1);

                gInputBoxCursor = pos+1;
            }
        }
        else if(meta==1 && key == 'd') {     // Meta-d
            const char* s = string_c_str(gInputBoxInput);

            if(s[gInputBoxCursor] != 0) {
                int pos = gInputBoxCursor;
                pos++;
                while(s[pos]!=0 && (s[pos] == ' ' || s[pos] == '/' || s[pos] == '\'' || s[pos] == '"')) {
                    pos++;
                }
                while(s[pos]!=0 && s[pos] != ' ' && s[pos] != '/' && s[pos] != '\'' && s[pos] != '"') {
                    pos++;
                }

                string_erase(gInputBoxInput, gInputBoxCursor, pos-gInputBoxCursor);
            }
        }
        else if(meta==1 && key == 'b') {     // META-b
            if(gInputBoxCursor > 0) {
                const char* s = string_c_str(gInputBoxInput);
                int pos = gInputBoxCursor;
                pos--;
                while(pos>=0 && (s[pos] == ' ' || s[pos] == '/' || s[pos] == '\'' || s[pos] == '"')) {
                    pos--;
                }
                while(pos>=0 && s[pos] != ' ' && s[pos] != '/' && s[pos] != '\'' && s[pos] != '"') {
                    pos--;
                }

                gInputBoxCursor = pos+1;
            }
        }
        else if(meta==1 && key == 'f') {     // META-f
            const char* s = string_c_str(gInputBoxInput);

            if(s[gInputBoxCursor] != 0) {
                int pos = gInputBoxCursor;
                pos++;
                while(s[pos]!=0 && (s[pos] == ' ' || s[pos] == '/' || s[pos] == '\'' || s[pos] == '"')) {
                    pos++;
                }
                while(s[pos]!=0 && s[pos] != ' ' && s[pos] != '/' && s[pos] != '\'' && s[pos] != '"') {
                    pos++;
                }

                gInputBoxCursor = pos;
            }
        }
        else if(key == 3 || key == 7 || key == 27) { // CTRL-C -G Escape
            result2 = 1;
            break;
        }
        else if(key == 12) {            // CTRL-L
            xclear_immediately();
        }
        else {
            if(meta == 0 && !(key >= 0 && key <= 27)) {
                char tmp[128];

                snprintf(tmp, 128, "%c", key);
                string_insert(gInputBoxInput, gInputBoxCursor, tmp);
                gInputBoxCursor++;
            }
        }
    }
    
    gView = NULL;
    
    int maxx = mgetmaxx();
    int maxy = mgetmaxy();
    
    xstrncpy(result, string_c_str(gInputBoxInput), result_size);

    mmove_immediately(maxy -2, 0);

#if defined(__CYGWIN__)
    xclear_immediately();       // 画面の再描写
    view();
    refresh();
#endif

    return result2;
}


/////////////////////////////////////////////////////////////////////////
// 文字列選択コマンドライン版
/////////////////////////////////////////////////////////////////////////
char* gSelectStrMsg;
char** gSelectStrStr;
int gSelectStrCursor;
int gSelectStrLen;

void select_str_view()
{
    int maxx = mgetmaxx();
    int maxy = mgetmaxy();
    
    /// view ///
    mclear_online(maxy-2);
    mclear_online(maxy-1);

    move(maxy-2, 0);
    printw("%s", gSelectStrMsg);
    
//        move(maxy-1, 0);
    printw(" ");
    int i;
    for(i=0; i< gSelectStrLen; i++) {
        if(gSelectStrCursor == i) {
            attron(A_REVERSE);
            printw("%s", gSelectStrStr[i]);
            attroff(A_REVERSE);
            printw(" ");
        }
        else {
            printw("%s ", gSelectStrStr[i]);
        }
    }

    move(maxy-1, maxx-2);
}

int select_str(char* msg, char* str[], int len, int cancel)
{
    gSelectStrMsg = msg;
    gSelectStrStr = str;
    gSelectStrLen = len;
    
    gSelectStrCursor = 0;
    
    gView = select_str_view;
    
    while(1) {
        filer_view(0);
        filer_view(1);
        select_str_view();
        refresh();

        /// input ///
        int meta;
        int key = xgetch(&meta);
        if(key == 10 || key == 13) {
            break;
        }
        else if(key == 6 || key == KEY_RIGHT) {
            gSelectStrCursor++;

            if(gSelectStrCursor >= len) gSelectStrCursor = len-1;
        }
        else if(key == 2 || key == KEY_LEFT) {
            gSelectStrCursor--;

            if(gSelectStrCursor < 0) gSelectStrCursor= 0;
        }
        else if(key == 12) {            // CTRL-L
            xclear_immediately();
        }
        else if(key == 3 || key == 7 | key == 27) { // CTRL-C -G Escape
            gSelectStrCursor = cancel;
            break;
        }
        else {
            int i;
            for(i=0; i< len; i++) {
                if(toupper(key) == toupper(str[i][0])) {
                    gSelectStrCursor = i;
                    goto finished;
                }
            }
        }
    }
finished:
    
    gView = NULL;

#if defined(__CYGWIN__)
    xclear_immediately();       // 画面の再描写
    view();
    refresh();
#endif

    return gSelectStrCursor;
}

int select_str2(char* msg, char* str[], int len, int cancel)
{
    gSelectStrMsg = msg;
    gSelectStrStr = str;
    gSelectStrLen = len;
    
    gSelectStrCursor = 0;
    
    gView = select_str_view;
    
    while(1) {
        select_str_view();
        refresh();

        /// input ///
        int meta;
        int key = xgetch(&meta);
        if(key == 10 || key == 13) {
            break;
        }
        else if(key == 6 || key == KEY_RIGHT) {
            gSelectStrCursor++;

            if(gSelectStrCursor >= len) gSelectStrCursor = len-1;
        }
        else if(key == 2 || key == KEY_LEFT) {
            gSelectStrCursor--;

            if(gSelectStrCursor < 0) gSelectStrCursor= 0;
        }
        else if(key == 12) {            // CTRL-L
            xclear_immediately();
        }
        else if(key == 3 || key == 7 || key == 27) { // CTRL-C -G Escape
            gSelectStrCursor = cancel;
            break;
        }
        else {
            int i;
            for(i=0; i< len; i++) {
                if(toupper(key) == toupper(str[i][0])) {
                    gSelectStrCursor = i;
                    goto finished;
                }
            }
        }
    }
finished:
    
    gView = NULL;

#if defined(__CYGWIN__)
    xclear_immediately();       // 画面の再描写
    view();
    refresh();
#endif

    return gSelectStrCursor;
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

void mclear_lastline()
{
    char space[1024];
    int x;

    const int maxx = mgetmaxx();
    const int maxy = mgetmaxy();

    for(x=0; x<maxx-1; x++) {
        space[x] = ' ';
    }
    space[x] = 0;

    attron(0);
    mvprintw(maxy-1, 0, space);
}

void xclear()
{
//#ifndef __CYGWIN__
//#define __CYGWIN__
//#endif
#if defined(__CYGWIN__)
    int y;
    for(y=0; y<mgetmaxy()-1; y++) {
        mclear_online(y);
    }
    mclear_lastline();
#else
    clear();
#endif
}

void gui_init()
{
    gInputBoxInput = STRING_NEW_STACK("");
}

