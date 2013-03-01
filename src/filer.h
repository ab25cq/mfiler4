#ifndef FILER_H
#define FILER_H

#include <oniguruma.h>

///////////////////////////////////////////////////
// sFile -- ファイルオブジェクト
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

///////////////////////////////////////////////////
// sDir -- ディレクトリオブジェクト
///////////////////////////////////////////////////
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

///////////////////////////////////////////////////
// ファイラー
///////////////////////////////////////////////////
extern enum eKanjiCode gKanjiCodeFileName; // ファイル名のエンコード
extern sObject* gDirs;
    // ディレクトリオブジェクトの配列
extern BOOL gExtensionICase;
    // マークファイルの拡張子別実行で大文字小文字を区別するかどうか

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
BOOL filer_history_forward(int dir);
BOOL filer_history_back(int dir);
BOOL filer_get_hitory(sObject* result, int dir); // result --> vector_obj of string_obj
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
int filer_make_size_str(char* result, __off64_t size);

BOOL filer_vd_start(int dir);
BOOL filer_vd_add(int dir, char* fname);
BOOL filer_vd_end(int dir);

BOOL filer_vd2_start(int dir);
BOOL filer_vd2_end(int dir);
BOOL filer_vd2_add(int dir, char* fname, char* linkto, struct stat stat_, struct stat lstat_, char* user, char* group);

void filer_add_keycommand(int meta, int key, sObject* block, char* fname, BOOL externl);
BOOL filer_add_keycommand2(int meta, int key, char* block, char* fname, BOOL externl);
BOOL filer_change_mode(char* mode);

int adir();
int sdir();

extern void str_cut(enum eKanjiCode code, char* mbs, int termsize, char* dest_mbs, int dest_byte);
    // mbsから端末上の文字列分(termsize)のみ残して残りを
    // 捨てた文字列をdest_mbsに返す dest_byte: dest_mbsのサイズ
extern void str_cut2(enum eKanjiCode code, char* mbs, int termsize, char* dest_mbs, int dest_byte);
    // mbsから端末上の文字列分(termsize)のみ残して残りを
    // 捨てた文字列をdest_mbsに返す dest_byte: dest_mbsのサイズ
    // 切り捨てた文字列をスペースで埋める
extern void str_cut3(enum eKanjiCode code, char* mbs, int termsize, char* dest_mbs, int dest_byte);
    // mbsから端末上の文字列分(termsize)のみ残して残りを捨てた文字列を
    // dest_mbsに返す dest_byte: dest_mbsのサイズ
    // 切り捨てた文字列をスペースで埋める
    // 切り捨てるのは文字列の前から


#endif
