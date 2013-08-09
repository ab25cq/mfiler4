#include "common.h"

#include "config.h"

#if defined(HAVE_CURSES_H)
#include <curses.h>
#elif defined(HAVE_NCURSES_H)
#include <ncurses.h>
#elif defined(HAVE_NCURSES_NCURSES_H)
#include <ncurses/ncurses.h>
#endif

#if defined(HAVE_MIGEMO_H)
#include <migemo.h>
static migemo* gMigemo;
static regex_t* gReg;
static sObject* gMigemoCache; // 一文字だけの正規表現のクェリーは重いのでキャッシュする
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
static void mfiler4_migemo_init();

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
#if !defined(HAVE_MIGEMO_H)
    if(string_c_str(gInputFileName)[0] != 0) {
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
    }

    return FALSE;

#else

    if(string_length(gInputFileName) <= 2) {
        if(string_c_str(gInputFileName)[0] != 0) {
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
        }

        return FALSE;
    }
    else if(string_c_str(gInputFileName)[0] != 0) {
        /// get reg ///
        if(gReg == NULL) {
            regex_t* reg = hash_item(gMigemoCache, (unsigned char*)string_c_str(gInputFileName));
            if(reg) {
                gReg = reg;
            }
            else {
                OnigUChar* p = migemo_query(gMigemo, (unsigned char*)string_c_str(gInputFileName));
                if(p == NULL) {
                    return FALSE;
                }

                int r;
                OnigErrorInfo err_info;

                OnigUChar* p2 = MALLOC(sizeof(char)*strlen(p)*2);
                char* p3 = p2;
                char* p4 = p;
                while(*p) {
                    if(*p == '+') {
                        *p3++ = '\\';
                        *p3++ = *p4++;
                    }
                    else {
                        *p3++ = *p4++;
                    }
                }
                *p3 = 0;

                if(gKanjiCode == kUtf8) {
                    r = onig_new(&gReg, p2, p2 + strlen((char*)p2), ONIG_OPTION_DEFAULT, ONIG_ENCODING_UTF8, ONIG_SYNTAX_DEFAULT,  &err_info);
                }
                else if(gKanjiCode == kEucjp) {
                    r = onig_new(&gReg, p2, p2 + strlen((char*)p2), ONIG_OPTION_DEFAULT, ONIG_ENCODING_EUC_JP, ONIG_SYNTAX_DEFAULT, &err_info);
                }
                else if(gKanjiCode == kSjis) {
                    r = onig_new(&gReg, p2, p2 + strlen((char*)p2), ONIG_OPTION_DEFAULT, ONIG_ENCODING_SJIS, ONIG_SYNTAX_DEFAULT, &err_info);
                }

                migemo_release(gMigemo, (unsigned char*) p);

                if(r != ONIG_NORMAL) {
                    return FALSE;
                }

                /// 一文字だけの正規表現は重いのでキャッシュする
                unsigned char* p5 = string_c_str(gInputFileName);
                if(strlen(p5) == 1 && *p5 >= 'A' && *p5 <= 'z') {
                    hash_put(gMigemoCache, (unsigned char*)string_c_str(gInputFileName), gReg);
                }
            }
        }

        /// start ///
        int i;
        for(i=start; i>=0; i--) {
            sFile* file = (sFile*)vector_item(filer_dir(adir())->mFiles, i);
            char* fnamev = string_c_str(file->mNameView);

            BOOL all_english = TRUE;
            const int len = strlen(fnamev);
            char* str = fnamev;
            int j;
            for(j=0; j<len; j++) {
                if(!(str[j]>=' ' && str[j]<='~')) {
                    all_english = FALSE;
                }
            }

            if(all_english) {
                char* result = mystrcasestr(fnamev, string_c_str(gInputFileName));
                if(!gISearchPartMatch && result == fnamev
                    || gISearchPartMatch && result != NULL)
                {
                    (void)filer_cursor_move(adir(), i);
                    
                    return TRUE;
                }
                
            }
            else {
                OnigRegion* region = onig_region_new();
                
                OnigUChar* str2 = (OnigUChar*)fnamev;

                int r2 = onig_search(gReg, str2, str2 + strlen((char*)str2), str2, str2 + strlen((char*)str2), region, ONIG_OPTION_NONE);

                onig_region_free(region, 1);

                if(!gISearchPartMatch && r2 == 0
                    || gISearchPartMatch && r2 >= 0)
                {
                    (void)filer_cursor_move(adir(), i);
                    return TRUE;
                }
            }
        }
    }

    return FALSE;
#endif
}

/// 前にマッチするファイル名にカーソルを移動 ///
static BOOL match_next(int start)
{
#if !defined(HAVE_MIGEMO_H)
    if(string_c_str(gInputFileName)[0] != 0) {
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
    }

    return FALSE;

#else

    if(string_length(gInputFileName) <= 2) {
        if(string_c_str(gInputFileName)[0] != 0) {
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
        }

        return FALSE;
    }
    else if(string_c_str(gInputFileName)[0] != 0) {
        /// get reg ///
        if(gReg == NULL) {
            regex_t* reg = hash_item(gMigemoCache, (unsigned char*)string_c_str(gInputFileName));
            if(reg) {
                gReg = reg;
            }
            else {
                OnigUChar* p = migemo_query(gMigemo, (unsigned char*)string_c_str(gInputFileName));
                if(p == NULL) {
                    return FALSE;
                }

                OnigUChar* p2 = MALLOC(strlen(p)*2);
                char* p3 = p2;
                char* p4 = p;
                while(*p4) {
                    if(*p4 == '+') {
                        *p3++ = '\\';
                        *p3++ = *p4++;
                    }
                    else {
                        *p3++ = *p4++;
                    }
                }
                *p3 = 0;

                int r;
                OnigErrorInfo err_info;

                if(gKanjiCode == kUtf8) {
                    r = onig_new(&gReg, p2, p2 + strlen((char*)p2), ONIG_OPTION_DEFAULT, ONIG_ENCODING_UTF8, ONIG_SYNTAX_DEFAULT,  &err_info);
                }
                else if(gKanjiCode == kEucjp) {
                    r = onig_new(&gReg, p2, p2 + strlen((char*)p2), ONIG_OPTION_DEFAULT, ONIG_ENCODING_EUC_JP, ONIG_SYNTAX_DEFAULT, &err_info);
                }
                else {
                    r = onig_new(&gReg, p2, p2 + strlen((char*)p2), ONIG_OPTION_DEFAULT, ONIG_ENCODING_SJIS, ONIG_SYNTAX_DEFAULT, &err_info);
                }
                migemo_release(gMigemo, (unsigned char*) p);

                FREE(p2);

                if(r != ONIG_NORMAL) {
                    return FALSE;
                }

                /// 一文字だけの正規表現は重いのでキャッシュする
                unsigned char* p5 = string_c_str(gInputFileName);
                if(strlen(p5) == 1 && *p5 >= 'A' && *p5 <= 'z') {
                    hash_put(gMigemoCache, (unsigned char*)string_c_str(gInputFileName), gReg);
                }
            }
        }

        /// start ///
        int i;
        sObject* v = filer_dir(adir())->mFiles;
        for(i=start; i<vector_count(v); i++) {
            sFile* file = (sFile*)vector_item(filer_dir(adir())->mFiles, i);
            char* fnamev = string_c_str(file->mNameView);

            BOOL all_english = TRUE;
            char* p = fnamev;
            while(*p) {
                if(!(*p>=' ' && *p<='~')) {
                    all_english = FALSE;
                    break;
                }
                p++;
            }

            if(all_english) {
                char* result = mystrcasestr(fnamev, string_c_str(gInputFileName));
                if(!gISearchPartMatch && result == fnamev
                    || gISearchPartMatch && result != NULL)
                {
                    (void)filer_cursor_move(adir(), i);
                    
                    return TRUE;
                }
                
            }
            else {
                OnigRegion* region = onig_region_new();
                
                OnigUChar* str2 = (OnigUChar*)fnamev;

                int r2 = onig_search(gReg, str2, str2 + strlen((char*)str2), str2, str2 + strlen((char*)str2), region, ONIG_OPTION_NONE);

                onig_region_free(region, 1);

                if(!gISearchPartMatch && r2 == 0
                    || gISearchPartMatch && r2 >= 0)
                {
                    (void)filer_cursor_move(adir(), i);
                    return TRUE;
                }
            }
        }
    }

    return FALSE;
#endif
}

///////////////////////////////////////////////////
// インクリメンタルサーチ初期化
///////////////////////////////////////////////////
static void mfiler4_migemo_init()
{
#if defined(HAVE_MIGEMO_H)
    char buf[PATH_MAX];
    char migemodir[PATH_MAX];
    gMigemo = migemo_open(NULL);

    snprintf(migemodir, PATH_MAX,"%s", SYSTEM_MIGEMODIR);

    if(gKanjiCode == kUtf8) {
        snprintf(buf, PATH_MAX, "%s/utf-8/migemo-dict", migemodir);
        if(migemo_load(gMigemo, MIGEMO_DICTID_MIGEMO, buf) == MIGEMO_DICTID_INVALID) {
            fprintf(stderr, "%s is not found\n", buf);
            exit(1);
        }
        snprintf(buf, PATH_MAX, "%s/utf-8/roma2hira.dat", migemodir);
        if(migemo_load(gMigemo, MIGEMO_DICTID_ROMA2HIRA, buf) == MIGEMO_DICTID_INVALID) {
            fprintf(stderr, "%s is not found\n", buf);
            exit(1);
        }
        snprintf(buf, PATH_MAX, "%s/utf-8/hira2kata.dat", migemodir);
        if(migemo_load(gMigemo, MIGEMO_DICTID_HIRA2KATA, buf) == MIGEMO_DICTID_INVALID) {
            fprintf(stderr, "%s is not found\n", buf);
            exit(1);
        }
        snprintf(buf, PATH_MAX, "%s/utf-8/han2zen.dat", migemodir);
        if(migemo_load(gMigemo, MIGEMO_DICTID_HAN2ZEN, buf) == MIGEMO_DICTID_INVALID) {
            fprintf(stderr, "%s is not found\n", buf);
            exit(1);
        }
    }
    else if(gKanjiCode == kEucjp) {
        snprintf(buf, PATH_MAX, "%s/euc-jp/migemo-dict", migemodir);
        if(migemo_load(gMigemo, MIGEMO_DICTID_MIGEMO, buf) == MIGEMO_DICTID_INVALID) {
            fprintf(stderr, "%s is not found\n", buf);
            exit(1);
        }
        snprintf(buf, PATH_MAX, "%s/euc-jp/roma2hira.dat", migemodir);
        if(migemo_load(gMigemo, MIGEMO_DICTID_ROMA2HIRA, buf) == MIGEMO_DICTID_INVALID) {
            fprintf(stderr, "%s is not found\n", buf);
            exit(1);
        }
        snprintf(buf, PATH_MAX, "%s/euc-jp/hira2kata.dat", migemodir);
        if(migemo_load(gMigemo, MIGEMO_DICTID_HIRA2KATA, buf) == MIGEMO_DICTID_INVALID) {
            fprintf(stderr, "%s is not found\n", buf);
            exit(1);
        }
        snprintf(buf, PATH_MAX, "%s/euc-jp/han2zen.dat", migemodir);
        if(migemo_load(gMigemo, MIGEMO_DICTID_HAN2ZEN, buf) == MIGEMO_DICTID_INVALID) {
            fprintf(stderr, "%s is not found\n", buf);
            exit(1);
        }
    }
    else {
        snprintf(buf, PATH_MAX, "%s/cp932/migemo-dict", migemodir);
        if(migemo_load(gMigemo, MIGEMO_DICTID_MIGEMO, buf) == MIGEMO_DICTID_INVALID) {
            fprintf(stderr, "%s is not found\n", buf);
            exit(1);
        }
        snprintf(buf, PATH_MAX, "%s/cp932/roma2hira.dat", migemodir);
        if(migemo_load(gMigemo, MIGEMO_DICTID_ROMA2HIRA, buf) == MIGEMO_DICTID_INVALID) {
            fprintf(stderr, "%s is not found\n", buf);
            exit(1);
        }
        snprintf(buf, PATH_MAX, "%s/cp932/hira2kata.dat", migemodir);
        if(migemo_load(gMigemo, MIGEMO_DICTID_HIRA2KATA, buf) == MIGEMO_DICTID_INVALID) {
            fprintf(stderr, "%s is not found\n", buf);
            exit(1);
        }
        snprintf(buf, PATH_MAX, "%s/cp932/han2zen.dat", migemodir);
        if(migemo_load(gMigemo, MIGEMO_DICTID_HAN2ZEN, buf) == MIGEMO_DICTID_INVALID) {
            fprintf(stderr, "%s is not found\n", buf);
            exit(1);
        }
    }
#endif
}

void isearch_init()
{
    gInputFileName = STRING_NEW_STACK("");

#if defined(HAVE_MIGEMO_H)
    gMigemoCache = HASH_NEW_STACK(100);
    gReg = NULL;
    gKanjiCode = kUtf8;
    mfiler4_migemo_init();
#endif
}

///////////////////////////////////////////////////
// インクリメンタルサーチ解放
///////////////////////////////////////////////////
void isearch_final()
{
#if defined(HAVE_MIGEMO_H)
    if(gMigemoCache) {
        onig_end();
        migemo_close(gMigemo);
        if(gReg && hash_key(gMigemoCache, gReg) == NULL) onig_free(gReg);
        gReg = NULL;
        hash_it* it = hash_loop_begin(gMigemoCache);
        while(it != NULL) {
            regex_t* reg  = hash_loop_item(it);
            onig_free(reg);

            it = hash_loop_next(it);
        }
    }
#endif
}

///////////////////////////////////////////////////
// インクリメンタルサーチキー入力
///////////////////////////////////////////////////
void isearch_input(int meta, int* keybuf, int keybuf_size)
{
    int key = keybuf[0];

    if(key == 8 || key == KEY_BACKSPACE || key == KEY_DC) {
        string_erase(gInputFileName, string_length(gInputFileName)-1, 1);
#if defined(HAVE_MIGEMO_H)
        if(gReg && hash_key(gMigemoCache, gReg) == NULL) {
            onig_free(gReg);
            gReg = NULL;
        }
        else {
            gReg = NULL;
        }
#endif
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
#if defined(HAVE_MIGEMO_H)
        if(gReg && hash_key(gMigemoCache, gReg) == NULL) {
            onig_free(gReg);
            gReg = NULL;
        }
        else {
            gReg = NULL;
        }
#endif
    }
    else {
        if(key == 9)
            string_push_back2(gInputFileName, ' ');
        else
            string_push_back2(gInputFileName, (char)key);

#if defined(HAVE_MIGEMO_H)
        if(gReg && hash_key(gMigemoCache, gReg) == NULL) {
            onig_free(gReg);
            gReg = NULL;
        }
        else {
            gReg = NULL;
        }
#endif

        if(!gISearchPartMatch) {
            if(!match_next(0)) {
                gISearchPartMatch = TRUE;

                if(!match_next(0)) {
                    string_erase(gInputFileName, string_length(gInputFileName)-1, 1);

#if defined(HAVE_MIGEMO_H)
                    if(gReg && hash_key(gMigemoCache, gReg) == NULL) {
                        onig_free(gReg);
                        gReg = NULL;
                    }
                    else {
                        gReg = NULL;
                    }
#endif
                }

                gISearchPartMatch = FALSE;
            }
        }
        else {
            if(!match_next(0)) {
                string_erase(gInputFileName, string_length(gInputFileName)-1, 1);

#if defined(HAVE_MIGEMO_H)
                if(gReg && hash_key(gMigemoCache, gReg) == NULL) {
                    onig_free(gReg);
                    gReg = NULL;
                }
                else {
                    gReg = NULL;
                }
#endif
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

