#ifndef COMMON_H
#define COMMON_H

#include "config.h"
#include <xyzsh/xyzsh.h>

#include <sys/stat.h>
#include <unistd.h>

#ifndef	NSIG
# ifdef	_NSIG
# define	NSIG		_NSIG
# else
#  ifdef	DJGPP
#  define	NSIG		301
#  else
#  define	NSIG		64
#  endif
# endif
#endif

#include <limits.h>
#include <signal.h>
#include <sys/wait.h>

#include <oniguruma.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(S_ISTXT)
    #define S_ALLPERM (S_IRWXU|S_IRWXG|S_IRWXO|S_ISUID|S_ISGID|S_ISTXT)
#elif defined(S_ISVTX)
    #define S_ALLPERM (S_IRWXU|S_IRWXG|S_IRWXO|S_ISUID|S_ISGID|S_ISVTX)
#else
    #define S_ALLPERM (S_IRWXU|S_IRWXG|S_IRWXO|S_ISUID|S_ISGID)
#endif

#define S_IXUGO (S_IXOTH | S_IXGRP | S_IXUSR)

#if defined(__x86_64__) && !defined(__DARWIN__)
#define FORMAT_HARDLINK "%3ld"
#else
#define FORMAT_HARDLINK "%3d"
#endif

#define kKeyMetaFirst 128

//////////////////////////////////////////////
// main.c
///////////////////////////////////////////////
void extname(char* result, int result_size, char* name);
void mygetcwd(char* result, int result_size);

extern char gHomeDir[PATH_MAX];     // ~/.mfiler4/のパス
extern char gTempDir[PATH_MAX];     // 一時ディレクトリのパス
extern int gMainLoop;               // メインループを回すかどうか -1の間回す。終了時はリターンコードとなる
extern BOOL gExitCode;              // 終了コード

extern void (*gView)();             // 登録描写関数(関数を登録しておけばviewで実行される)

void set_signal_mfiler();

void view();                        // 全体の描写関数

///////////////////////////////////////////////
// menu.c
///////////////////////////////////////////////
extern sObject* gActiveMenu;

void menu_init();
void menu_final();

void menu_create(char* menu_name);
void menu_view();
void menu_input(int meta, int key);
BOOL set_active_menu(char* menu_name);
BOOL menu_append(char* menu_name, char* name, int key, sObject* block, BOOL external);
void menu_start(char* menu_name);

///////////////////////////////////////////////////
// isearch.c
///////////////////////////////////////////////////
extern BOOL gISearch;        // インクリメンタルサーチ中かどうか
void isearch_init();         // インクリメンタルサーチ初期化
void isearch_final();        // インクリメンタルサーチ解放

void isearch_input(int meta, int* keybuf, int keybuf_size);
void isearch_view();                  // インクリメンタルサーチ描写

BOOL IsISearchExploreChar(int meta, int key);
BOOL IsISearchNULL();
void ISearchClear();

///////////////////////////////////////////////////
// filer.c
///////////////////////////////////////////////////
typedef struct {
    sObject* mName;                // ディスク内部の名前
    sObject* mNameView;            // 表示する時の名前(iconvで変換後)
    sObject* mLinkTo;              // リンク先
    struct stat mStat;              // stat()のキャッシュ
    struct stat mLStat;             // lstat()のキャッシュ
    BOOL mMark;                       // マークされているかどうか
    sObject* mUser;                // ユーザー名
    sObject* mGroup;               // グループ名

    int mSortRandom;                  // ランダムソート時の数値
} sFile;

sFile* sFile_new(char* name, char* name_view, char* linkto, struct stat* stat_, struct stat* lstat_, BOOL mark, char* user, char* group, int uid, int gid);

typedef struct  {
    sObject* mPath;          // 現在のディレクトリ最後は/
    sObject* mMask;          // マスク
    BOOL mDotDirMask;           // ドットディレクトリのマスク

    regex_t* mMaskReg;          // マスクの正規表現のキャッシュ

    int mScrollTop;             // スクロールトップ
    int mCursor;                // カーソル位置

    sObject* mFiles;
    BOOL mActive;

    BOOL mVD;                   // 仮想ディレクトリ開始

    sObject* mHistory;
    int mHistoryCursor;
} sDir;

extern enum eKanjiCode gKanjiCodeFileName; // ファイル名のエンコード
extern sObject* gDirs;
    // ディレクトリオブジェクトの配列

void filer_init();
void filer_final();

BOOL filer_new_dir(char* path, BOOL dot_dir_mask, char* mask);
BOOL filer_del_dir(int dir);

sDir* filer_dir(int dir);
sFile* filer_file(int dir, int num);
int filer_file2(int dir, char* name); // カーソル番号で返す
ALLOC sObject* filer_mark_files(int dir);
char* filer_mask(int dir);
BOOL filer_set_mask(int dir, char* mask);
BOOL filer_set_dotdir_mask(int dir, BOOL flg);
BOOL filer_dotdir_mask(int dir);
int filer_mark_file_num(int dir);
BOOL filer_marking(int dir);
int filer_cursor(int dir);
sFile* filer_cursor_file(int dir);
BOOL filer_mark(int dir, int num);
BOOL filer_set_mark(int dir, int num, int mark);

BOOL filer_sort(int dir);
BOOL filer_cd(int dir, char* path);
BOOL filer_add_history(int dir, char* path);
BOOL filer_history_forward(int dir);
BOOL filer_history_back(int dir);
BOOL filer_reread(int dir);
BOOL filer_cursor_move(int dir, int num);
BOOL filer_activate(int dir);
BOOL filer_reset_marks(int dir);
BOOL filer_toggle_mark(int dir, int num);

int filer_line(int dir);
int filer_line_max(int dir);
int filer_row(int dir);
int filer_row_max(int dir);

void filer_view(int dir);
void filer_input(int meta, int key);
int filer_make_size_str(char* result, long long size);

BOOL filer_vd_start(int dir);
BOOL filer_vd_add(int dir, char* fname);
BOOL filer_vd_end(int dir);

void filer_add_keycommand(int meta, int key, sObject* block, char* fname, BOOL externl);
BOOL filer_add_keycommand2(int meta, int key, char* block, char* fname, BOOL externl);
BOOL filer_change_mode(char* mode);

int adir();
int sdir();

void cmdline_view_filer();
void job_view();
void str_cut2(enum eKanjiCode code, char* mbs, int termsize, char* dest_mbs, int dest_byte);

///////////////////////////////////////////////////
// gui.c
///////////////////////////////////////////////////
void gui_init();

void xstart_color();
int xgetch(int* meta);
void xgetch_utf8(int* meta, ALLOC int** key, int* key_size);
void xinitscr();
void xendwin();
void xclear();
void xclear_immediately();
void mclear_lastline();
char* choise(char* msg, char* str[], int len, int cancel);
void merr_msg(char* msg, ...);
void msg_nonstop(char* msg, ...);
int select_str(char* msg, char* str[], int len, int cancel);
int select_str2(char* msg, char* str[], int len, int cancel);
int input_box(char* msg, char* result, int result_size, char* def_input, int def_cursor);
void mbox(int y, int x, int width, int height);

///////////////////////////////////////////////////
// commands.c
///////////////////////////////////////////////////
void commands_init();
void commands_final();

///////////////////////////////////////////////////
// dir.c
///////////////////////////////////////////////////
void sDir_setmask(sDir* self, char* mask);
int sDir_read(sDir* self);
void sDir_mask(sDir* self);
char* mygetpwuid(struct stat* statbuf);
char* mygetgrgid(struct stat* statbuf);
void sFile_delete(sFile* self);
sDir* sDir_new();
void sDir_delete(sDir* self);
void sDir_mask(sDir* self);
 int sDir_read(sDir* self);
void sDir_sort(sDir* self);
int convert_fname(char* src, char* des, int des_size);

///////////////////////////////////////////////////
// file.c
///////////////////////////////////////////////////
enum eCopyOverride { kNone, kYesAll, kNoAll, kCancel, kSelectNewer, kYesAllRemainPermission };
extern enum eCopyOverride gCopyOverride;

enum eWriteProtected { kWPNone, kWPYesAll, kWPNoAll, kWPCancel };
extern enum eWriteProtected gWriteProtected;

BOOL file_copy(char* spath, char* dpath, BOOL move, BOOL preserve);
BOOL file_remove(char* path, BOOL no_ctrl_c, BOOL msg);

extern sObject* gMFiler4;
extern sObject* gMFiler4System;
extern sObject* gMFiler4Prompt;

#endif

