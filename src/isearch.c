#include "common.h"

#include "config.h"

#if defined(HAVE_CURSES_H)
#include <curses.h>
#elif defined(HAVE_NCURSES_H)
#include <ncurses.h>
#elif defined(HAVE_NCURSES_NCURSES_H)
#include <ncurses/ncurses.h>
#endif

static sObject* gInputFileName;      // 入力された文字列

///////////////////////////////////////////////////
// 外部にさらす定義
///////////////////////////////////////////////////
BOOL gISearch = FALSE;                  // 現在インクリメンタルサーチ中かどうか
BOOL gISearchPartMatch = FALSE;         // 現在インクリメンタルサーチパートマッチ中かどうか
BOOL gNoPartMatchClear = FALSE;         // ISearchClarでgISearchPartMatchをクリアするかどうか

// その文字がエクスプローラー風インクリメンタルサーチで使うものかどうか
BOOL IsISearchExploreChar(int meta, int key)
{
    return meta == 0 && ((key >= 'A' && key <= 'Z') 
                || (key >= '0' && key <= '9')
                || (key >= 'a' && key <='z')
                || key == '_'
                || key == '-'
                || key == '.');
}

BOOL IsISearchNULL()
{
    return string_length(gInputFileName) == 0;
}

void ISearchClear()
{
    string_put(gInputFileName, "");
    if(!gNoPartMatchClear) {
        gISearchPartMatch = FALSE;
    }
    else {
        gNoPartMatchClear = FALSE;
    }
}

///////////////////////////////////////////////////
// 内部定義
///////////////////////////////////////////////////
/// 入力された文字列がインクリメンタルサーチ向きかどうか ///
static BOOL is_isearch_char(int key)
{
    return key == 9 || (key >= ' ' && key <= '~')
                && key != '\\' || key >= 128;
}

static char* mystrcasestr(const char *haystack, const char *needle)
{
    char* p;
    p = (char*)haystack;

    while(*p) {
        char* p2;
        char* p3;
        BOOL match;

        p2 = p;
        p3 = (char*)needle;
        
        match = TRUE;
        while(*p3) {
            if(*p2 == 0) {
                match = FALSE;
                break;
            }
            
            if(tolower(*p2) != tolower(*p3)) {
                match = FALSE;
                break;
            }
            
            p2++;
            p3++;
        }
        
        if(match) {
            return p;
        }
        
        p++;
    }
    
    return NULL;
}

/// 後ろにマッチするファイル名にカーソルを移動 ///
static BOOL match_back(int start)
{
    if(string_c_str(gInputFileName)[0] != 0) {
        char buf[128];
        snprintf(buf, 128, "%d", start);

        char* argv[] = {
            buf, string_c_str(gInputFileName), NULL
        };

        int rcode;
        if(xyzsh_eval(&rcode, "isearch_match_back $ARGV[0] $ARGV[1]", "isearch", NULL, gStdin, gStdout, 2, argv, gMFiler4)) {
            if(rcode == 0) {
                return TRUE;
            }
        }
        else {
            merr_msg(string_c_str(gErrMsg));
        }
/*
        int i;
        for(i=start; i>=0; i--) {
            sFile* file = (sFile*)vector_item(filer_dir(adir())->mFiles, i);
            char* fnamev = string_c_str(file->mNameView);

            char* result = mystrcasestr(fnamev, string_c_str(gInputFileName));
            if(!gISearchPartMatch &&  result == fnamev
                || gISearchPartMatch && result != NULL)
            {
                (void)filer_cursor_move(adir(), i);
                return TRUE;
            }
        }
*/
    }

    return FALSE;
}

/// 前にマッチするファイル名にカーソルを移動 ///
static BOOL match_next(int start)
{
    if(string_c_str(gInputFileName)[0] != 0) {
        char buf[128];
        snprintf(buf, 128, "%d", start);

        char* argv[] = {
            buf, string_c_str(gInputFileName), NULL
        };

        int rcode;
        if(xyzsh_eval(&rcode, "isearch_match_next $ARGV[0] $ARGV[1]", "isearch", NULL, gStdin, gStdout, 2, argv, gMFiler4)) {
            if(rcode == 0) {
                return TRUE;
            }
        }
        else {
            merr_msg(string_c_str(gErrMsg));
        }

/*
        int i;
        for(i=start; i<vector_count(filer_dir(adir())->mFiles); i++) {
            sFile* file = (sFile*)vector_item(filer_dir(adir())->mFiles, i);
            char* fnamev = string_c_str(file->mNameView);

            char* result = mystrcasestr(fnamev, string_c_str(gInputFileName));
            if(!gISearchPartMatch &&  result == fnamev
                || gISearchPartMatch && result != NULL)
            {
                (void)filer_cursor_move(adir(), i);
                return TRUE;
            }
        }
*/
    }

    return FALSE;
}

///////////////////////////////////////////////////
// インクリメンタルサーチ初期化
///////////////////////////////////////////////////
void isearch_init()
{
    gInputFileName = STRING_NEW_STACK("");
}

///////////////////////////////////////////////////
// インクリメンタルサーチ解放
///////////////////////////////////////////////////
void isearch_final()
{
}

///////////////////////////////////////////////////
// インクリメンタルサーチキー入力
///////////////////////////////////////////////////
void isearch_input(int meta, int key)
{
    if(key == 8 || key == KEY_BACKSPACE || key == KEY_DC) {
        string_erase(gInputFileName, string_length(gInputFileName)-1, 1);
    }
    else if(key == 14 || key == KEY_DOWN) {
        if(!gISearchPartMatch) {
            gISearchPartMatch = TRUE;
            match_next(filer_dir(adir())->mCursor+1);
            gISearchPartMatch = FALSE;
        }
        else {
            match_next(filer_dir(adir())->mCursor+1);
        }
    }
    else if(key == 16 || key == KEY_UP) {
        if(!gISearchPartMatch) {
            gISearchPartMatch = TRUE;
            match_back(filer_dir(adir())->mCursor-1);
            gISearchPartMatch = FALSE;
        }
        else {
            match_back(filer_dir(adir())->mCursor-1);
        }
    }
    else if(key == ' ') {
        (void)filer_toggle_mark(adir(), filer_dir(adir())->mCursor);

        if(!gISearchPartMatch) {
            gISearchPartMatch = TRUE;
            match_next(filer_dir(adir())->mCursor+1);
            gISearchPartMatch = FALSE;
        }
        else {
            match_next(filer_dir(adir())->mCursor+1);
        }
    }
    else if(!is_isearch_char(key)) {
        gISearch = FALSE;
        gISearchPartMatch = FALSE;
        string_put(gInputFileName, "");
    }
    else {
        if(key == 9)
            string_push_back2(gInputFileName, ' ');
        else
            string_push_back2(gInputFileName, (char)key);

        if(!gISearchPartMatch) {
            if(!match_next(0)) {
                gISearchPartMatch = TRUE;

                if(!match_next(0)) {
                    string_erase(gInputFileName, string_length(gInputFileName)-1, 1);

                }

                gISearchPartMatch = FALSE;
            }
        }
        else {
            if(!match_next(0)) {
                string_erase(gInputFileName, string_length(gInputFileName)-1, 1);

            }
        }
    }
}

///////////////////////////////////////////////////
// インクリメンタルサーチ描写
///////////////////////////////////////////////////
#if defined(__CYGWIN__)
void isearch_view()
{
    const int maxx = mgetmaxx();
    const int maxy = mgetmaxy();

    mbox(maxy/2, maxx/3, maxx/3, 3);
    mvprintw(maxy/2+1, maxx/3+1, "/%s", string_c_str(gInputFileName));
}
#else
void isearch_view()
{
    const int maxx = mgetmaxx();
    const int maxy = mgetmaxy();

    if(gISearchPartMatch) 
        mvprintw(maxy -2, 0, "//");
    else
        mvprintw(maxy -2, 0, "/");

    char buf[1024];
    char* str = string_c_str(gInputFileName);
    const int len = strlen(str);
    int i;
    for(i=0; i<maxx-35; i++) {
        if(i<len) {
            buf[i] = str[i];
        }
        else {
            buf[i] = ' ';
        }
    }
    buf[i] = 0;
    printw(buf);
}
#endif

