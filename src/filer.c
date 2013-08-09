#include "common.h"

#include "config.h"

#if defined(HAVE_CURSES_H)
#include <curses.h>
#elif defined(HAVE_NCURSES_H)
#include <ncurses.h>
#elif defined(HAVE_NCURSES_NCURSES_H)
#include <ncurses/ncurses.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <dirent.h>
#include <libgen.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <stdarg.h>
#include <time.h>
#include <ctype.h>
#include <sys/types.h>
#include <pwd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>
#include <grp.h>

typedef struct {
    sObject* mBlocks[KEY_MAX*2]; 
    char mExternals[KEY_MAX*2];
} sKeyCommand;

int sKeyCommand_markfun(sObject* self)
{
    int count = 0;
    sKeyCommand* keycommand = SEXTOBJ(self).mObject;

    int i;
    for(i=0; i<KEY_MAX*2; i++) {
        sObject* block = keycommand->mBlocks[i];
        if(block) {
            SET_MARK(block);
            count ++;
            count+= object_gc_children_mark(block);
        }
    }

    return count;
}

void sKeyCommand_freefun(void* obj)
{
    FREE(obj);
}

BOOL sKeyCommand_mainfun(void* obj, sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;
    sKeyCommand* keycommand = obj;

    int i;
    for(i=0; i<KEY_MAX*2; i++) {
        sObject* block = keycommand->mBlocks[i];
        BOOL external = keycommand->mExternals[i];

        if(block) {
            char buf[256];
            snprintf(buf, 256, "-+- key %d meta %d external %d -+-", i%kKeyMetaFirst, i>= kKeyMetaFirst ? 1:0, external);
            if(!fd_write(nextout, buf, strlen(buf))) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }

            if(!fd_write(nextout, SBLOCK(block).mSource, strlen(SBLOCK(block).mSource)))
            {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            if(!fd_write(nextout, "\n", 1)) {
                err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
        }
    }
    return TRUE;
}

static sObject* sKeyCommand_new()
{
    sKeyCommand* obj = MALLOC(sizeof(sKeyCommand));

    memset(obj->mBlocks, 0, sizeof(sObject*)*KEY_MAX*2);
    memset(obj->mExternals, 0, sizeof(char)*KEY_MAX*2);

    return EXTOBJ_NEW_GC(obj, sKeyCommand_markfun, sKeyCommand_freefun, sKeyCommand_mainfun, TRUE);
}

sObject* gDirs;
enum eKanjiCode gKanjiCodeFileName = kByte;

static sObject* gFilerKeyBind;

void filer_init()
{
    gDirs = VECTOR_NEW_MALLOC(2);
    gKanjiCodeFileName = gKanjiCode;

    gFilerKeyBind = sKeyCommand_new();
    uobject_put(gMFiler4, "keycommands", gFilerKeyBind);
}

void filer_final()
{
    int i;
    for(i=0; i<vector_count(gDirs); i++) {
        sDir_delete(vector_item(gDirs, i));
    }
    vector_delete_on_malloc(gDirs);
}

void filer_add_keycommand(int meta, int key, sObject* block, char* fname, BOOL external)
{
    sKeyCommand* keycommand = SEXTOBJ(gFilerKeyBind).mObject;
    keycommand->mBlocks[meta*KEY_MAX+key] = block_clone_on_gc(block, T_BLOCK, TRUE);
    keycommand->mExternals[meta*KEY_MAX+key] = external;
}

BOOL filer_add_keycommand2(int meta, int key, char* cmdline, char* fname, BOOL external)
{
    int sline = 1;
    sObject* block = BLOCK_NEW_GC(TRUE);

    sKeyCommand* keycommand = SEXTOBJ(gFilerKeyBind).mObject;
    keycommand->mBlocks[meta*KEY_MAX+key] = block;
    keycommand->mExternals[meta*KEY_MAX+key] = external;

    if(!parse(cmdline, "keycommand", &sline, block, NULL)) {
        return FALSE;
    }

    return TRUE;
}

BOOL filer_new_dir(char* path, BOOL dotdir_mask, char* mask)
{
    sDir* dir = sDir_new();

    string_put(dir->mPath, path);
    if(path[strlen(path)-1] != '/') {
        string_push_back2(dir->mPath, '/');
    }
    dir->mDotDirMask = dotdir_mask;
    sDir_setmask(dir, mask);

    if(sDir_read(dir) == 0) {
        sDir_mask(dir);
        sDir_sort(dir);

        int i;
        for(i=vector_count(dir->mHistory)-1; i!=dir->mHistoryCursor; i--) {
            string_delete_on_malloc(vector_item(dir->mHistory, i));
            vector_erase(dir->mHistory, i);
        }
        vector_add(dir->mHistory, STRING_NEW_MALLOC(string_c_str(dir->mPath)));
        dir->mHistoryCursor++;

        vector_add(gDirs, dir);
    }
    else {
        return FALSE;
    }

    return TRUE;
}

BOOL filer_del_dir(int dir)
{
    if(dir < 0 || dir >= vector_count(gDirs) || vector_count(gDirs) <= 2) {
        return FALSE;
    }

    sDir* dir2 = vector_item(gDirs, dir);
    sDir_delete(dir2);
    vector_erase(gDirs, dir);

    return TRUE;
}

BOOL filer_vd_start(int dir)
{
    if(dir < 0 || dir >= vector_count(gDirs)) {
        return FALSE;
    }
    sDir* self = (sDir*)vector_item(gDirs, dir);

    self->mVD = TRUE;
    int i;
    for(i=0; i<vector_count(self->mFiles); i++) {
        sFile_delete(vector_item(self->mFiles, i));
    }
    vector_clear(self->mFiles);

    return TRUE;
}

BOOL filer_vd_end(int dir)
{
    if(dir < 0 || dir >= vector_count(gDirs)) {
        return FALSE;
    }

    sDir* self = (sDir*)vector_item(gDirs, dir);

    self->mVD = FALSE;

    char buf[256];
    snprintf(buf, 256, "reread -d %d", dir);
    int rcode;
    return xyzsh_eval(&rcode, buf, "reread", NULL, gStdin, gStdout, 0, NULL, gMFiler4);
}

BOOL filer_vd_add(int dir, char* fname)
{
    if(dir < 0 || dir >= vector_count(gDirs)) {
        return FALSE;
    }
    sDir* self = (sDir*)vector_item(gDirs, dir);

    char fname2[PATH_MAX];
    if(convert_fname(fname, fname2, PATH_MAX) == -1) {
        return FALSE;
    }

    char path[PATH_MAX];
    if(fname[0] == '/') {
        xstrncpy(path, fname, PATH_MAX);
    }
    else {
        snprintf(path, PATH_MAX, "%s%s", string_c_str(self->mPath), fname);
    }
    
    struct stat lstat_;
    memset(&lstat_, 0, sizeof(struct stat));
    if(lstat(path, &lstat_) < 0) {
        return FALSE;
    }

    struct stat stat_;
    memset(&stat_, 0, sizeof(struct stat));
    if(stat(path, &stat_) < 0) {
        if(S_ISLNK(lstat_.st_mode)) {
            stat_ = lstat_;
        }
        else {
            return FALSE;
        }
    }
    
    char linkto[PATH_MAX];

    if(S_ISLNK(lstat_.st_mode)) {
        int bytes = readlink(path, linkto, PATH_MAX);
        if(bytes == -1) {
            linkto[0] = 0;
        }
        else {
            linkto[bytes] = 0;
        }
    }
    else {
        linkto[0] = 0;
    }
#if defined(__DARWIN__)
    char tmp[PATH_MAX + 1];
    if(kanji_convert(linkto, tmp, PATH_MAX, gKanjiCodeFileName, gKanjiCode) != -1) {
        xstrncpy(linkto, tmp, PATH_MAX);
    }
#endif

    char* user = mygetpwuid(&lstat_);
    char* group = mygetgrgid(&lstat_);

    if(strcmp(fname, ".") != 0) {
        BOOL mark = FALSE;
        
        /// add ///
        vector_add(self->mFiles, sFile_new(fname, fname2, linkto, &stat_, &lstat_, mark, user, group, lstat_.st_uid, lstat_.st_gid));
    }

    return TRUE;
}

BOOL filer_history_forward(int dir)
{
    if(dir < 0 || dir >= vector_count(gDirs)) {
        return FALSE;
    }

    sDir* dir2 = vector_item(gDirs, dir);

    if(dir2->mHistoryCursor < vector_count(dir2->mHistory)-1) {
        sObject* path = vector_item(dir2->mHistory, ++dir2->mHistoryCursor);
        filer_cd(dir, string_c_str(path));
    }

    return TRUE;
}

BOOL filer_history_back(int dir)
{
    if(dir < 0 || dir >= vector_count(gDirs)) {
        return FALSE;
    }

    sDir* dir2 = vector_item(gDirs, dir);

    if(dir2->mHistoryCursor > 0) {
        sObject* path = vector_item(dir2->mHistory, --dir2->mHistoryCursor);
        filer_cd(dir, string_c_str(path));
    }

    return TRUE;
}

BOOL filer_add_history(int dir, char* path) 
{
    if(dir < 0 || dir >= vector_count(gDirs)) {
        return FALSE;
    }
    sDir* dir2 = (sDir*)vector_item(gDirs, dir);

    int i;
    for(i=vector_count(dir2->mHistory)-1; i!=dir2->mHistoryCursor; i--) {
        string_delete_on_malloc(vector_item(dir2->mHistory, i));
        vector_erase(dir2->mHistory, i);
    }
    vector_add(dir2->mHistory, STRING_NEW_MALLOC(string_c_str(dir2->mPath)));
    dir2->mHistoryCursor++;

    return TRUE;
}

// result --> vector_obj of string_obj
BOOL filer_get_hitory(sObject* result, int dir)
{
    ASSERT(STYPE(result) == T_VECTOR);

    if(dir < 0 || dir >= vector_count(gDirs)) {
        return FALSE;
    }
    sDir* dir2 = (sDir*)vector_item(gDirs, dir);

    int i;
    for(i=1; i<=dir2->mHistoryCursor; i++) {
        vector_add(result, vector_item(dir2->mHistory, i));
    }

    return TRUE;
}

static int correct_path(char* current_path, char* path, char* path2, int path2_size)
{
    char tmp[PATH_MAX];
    char tmp2[PATH_MAX];
    char tmp3[PATH_MAX];
    char tmp4[PATH_MAX];

    /// 先頭にカレントパスを加える ///
    {
        if(path[0] == '/') {      /// 絶対パスなら必要ない
            xstrncpy(tmp, path, PATH_MAX);
        }
        else {
            if(current_path == NULL) {
                char cwd[PATH_MAX];
                mygetcwd(cwd, PATH_MAX);
                
                xstrncpy(tmp, cwd, PATH_MAX);
                xstrncat(tmp, "/", PATH_MAX);
                xstrncat(tmp, path, PATH_MAX);
            }
            else {
                xstrncpy(tmp, current_path, PATH_MAX);
                if(current_path[strlen(current_path)-1] != '/') {
                    xstrncat(tmp, "/", PATH_MAX);
                }
                xstrncat(tmp, path, PATH_MAX);
            }
        }

    }

    xstrncpy(tmp2, tmp, PATH_MAX);

    /// .を削除する ///
    {
        char* p;
        char* p2;
        int i;

        p = tmp3;
        p2 = tmp2;
        while(*p2) {
            if(*p2 == '/' && *(p2+1) == '.' && *(p2+2) != '.' && ((*p2+3)=='/' || *(p2+3)==0)) {
                p2+=2;
            }
            else {
                *p++ = *p2++;
            }
        }
        *p = 0;

        if(*tmp3 == 0) {
            *tmp3 = '/';
            *(tmp3+1) = 0;
        }

    }

    /// ..を削除する ///
    {
        char* p;
        char* p2;

        p = tmp4;
        p2 = tmp3;

        while(*p2) {
            if(*p2 == '/' && *(p2+1) == '.' && *(p2+2) == '.' 
                && *(p2+3) == '/')
            {
                p2 += 3;

                do {
                    p--;

                    if(p < tmp4) {
                        xstrncpy(path2, "/", path2_size);
                        return FALSE;
                    }
                } while(*p != '/');

                *p = 0;
            }
            else if(*p2 == '/' && *(p2+1) == '.' && *(p2+2) == '.' 
                && *(p2+3) == 0) 
            {
                do {
                    p--;

                    if(p < tmp4) {
                        xstrncpy(path2, "/", path2_size);
                        return FALSE;
                    }
                } while(*p != '/');

                *p = 0;
                break;
            }
            else {
                *p++ = *p2++;
            }
        }
        *p = 0;
    }

    if(*tmp4 == 0) {
        xstrncpy(path2, "/", path2_size);
    }
    else {
        xstrncpy(path2, tmp4, path2_size);
    }

    return TRUE;
}

BOOL filer_cd(int dir, char* path) 
{
    if(dir < 0 || dir >= vector_count(gDirs)) {
        return FALSE;
    }
    sDir* dir2 = (sDir*)vector_item(gDirs, dir);

    dir2->mVD = FALSE;

    char fname[PATH_MAX];
    char buf[PATH_MAX];
    xstrncpy(buf, string_c_str(dir2->mPath), PATH_MAX);
    xstrncpy(fname, basename(buf), PATH_MAX);

    char path2[PATH_MAX];
    if(correct_path(string_c_str(dir2->mPath), path, path2, PATH_MAX) && access(path2, F_OK) == 0) 
    {
        string_put(dir2->mPath, path2);
        if(string_c_str(dir2->mPath)[strlen(string_c_str(dir2->mPath))-1] != '/') {
            string_push_back(dir2->mPath, "/");
        }
        
        (void)filer_reset_marks(dir);

        if(sDir_read(dir2) != 0) {
            return FALSE;;
        }

        sDir_mask(dir2);
        sDir_sort(dir2); 

        if(strcmp(path, "..") == 0) {
            dir2->mScrollTop = 0;
            (void)filer_cursor_move(dir, 0);

            int i;
            for(i=0; i<vector_count(dir2->mFiles); i++) {
                sFile* file = (sFile*)vector_item(dir2->mFiles, i);
                if(strcmp(string_c_str(file->mName), fname) == 0) {
                    dir2->mScrollTop = 0;
                    (void)filer_cursor_move(dir, i);
                    break;
                }
            }
        }
        else {
            dir2->mScrollTop = 0;
            (void)filer_cursor_move(dir, 0);
        }

        /// change pwd ///
        if(dir == adir()) {
            if(chdir(string_c_str(dir2->mPath)) == 0) {
                setenv("PWD", string_c_str(dir2->mPath), 1);
            }
        }

        return TRUE;
    }
    else {
        return FALSE;
    }
}

BOOL filer_reread(int dir)
{
    if(dir < 0 || dir >= vector_count(gDirs)) {
        return FALSE;
    }

    sDir* dir2 = (sDir*)vector_item(gDirs, dir);
    sDir_read(dir2);
    sDir_mask(dir2);
    sDir_sort(dir2);

    (void)filer_cursor_move(dir, dir2->mCursor);

    return TRUE;
}

BOOL filer_marking(int dir)
{
    if(dir < 0 || dir >= vector_count(gDirs)) {
        return FALSE;
    }
    
    if(filer_mark_file_num(dir) != 0) {
        return TRUE;
    }
    else {
        return FALSE;
    }
}

char* filer_mask(int dir)
{
    sDir* dir2 = filer_dir(dir);

    if(dir2) {
        return string_c_str(dir2->mMask);
    }

    return NULL;
}

BOOL filer_set_mask(int dir, char* mask)
{
    sDir* dir2 = filer_dir(dir);

    if(dir2 == NULL) {
        return FALSE;
    }

    sDir_setmask(dir2, mask);

    return TRUE;
}

BOOL filer_cursor_move(int dir, int num)
{
    const int maxy = filer_line_max(dir);

    sDir* dir2 = filer_dir(dir);

    if(dir2 == NULL) {
        return FALSE;
    }

    dir2->mCursor = num;
    
    if(dir2->mCursor < 0) dir2->mCursor = 0;
    if(dir2->mCursor >= vector_count(dir2->mFiles)) 
        dir2->mCursor = vector_count(dir2->mFiles)-1;

    char* env_view_option = getenv("VIEW_OPTION");
    if(env_view_option == NULL) {
        fprintf(stderr, "VIEW_OPTION is NULL\n");
        return FALSE;
    }

    if(strcmp(env_view_option, "2pain") == 0 
        || strcmp(env_view_option, "1pain") == 0) 
    {
         if(dir2->mCursor >= dir2->mScrollTop + maxy) {
             dir2->mScrollTop = (dir2->mCursor/maxy)*maxy;
         }
         if(dir2->mCursor < dir2->mScrollTop) {
             dir2->mScrollTop = (dir2->mCursor/maxy)*maxy;
         }
    }
    else if(strcmp(env_view_option, "1pain2") == 0) {
        if(dir2->mCursor >= dir2->mScrollTop + maxy*2) {
             dir2->mScrollTop = (dir2->mCursor/(maxy*2))*(maxy*2);
        }
        if(dir2->mCursor < dir2->mScrollTop) {
            dir2->mScrollTop = (dir2->mCursor/(maxy*2))*(maxy*2);
        }
    }
    else if(strcmp(env_view_option, "1pain3") == 0) {
        if(dir2->mCursor >= dir2->mScrollTop + maxy*3) {
             dir2->mScrollTop = (dir2->mCursor/(maxy*3))*(maxy*3);
        }
        if(dir2->mCursor < dir2->mScrollTop) {
            dir2->mScrollTop = (dir2->mCursor/(maxy*3))*(maxy*3);
        }
    }
    else if(strcmp(env_view_option, "1pain5") == 0) {
        if(dir2->mCursor >= dir2->mScrollTop + maxy*5) {
             dir2->mScrollTop = (dir2->mCursor/(maxy*5))*(maxy*5);
        }
        if(dir2->mCursor < dir2->mScrollTop) {
            dir2->mScrollTop = (dir2->mCursor/(maxy*5))*(maxy*5);
        }
    }

    return TRUE;
}

BOOL filer_activate(int dir)
{
    if(dir == 0 || dir == 1) {
        sDir* ldir = filer_dir(0);
        sDir* rdir = filer_dir(1);

        if(ldir && rdir) {
            if(dir == 0) {
                ldir->mActive = TRUE;
                rdir->mActive = FALSE;

                if(chdir(string_c_str(ldir->mPath)) == 0) {
                    setenv("PWD", string_c_str(ldir->mPath), 1);
                }
            }
            else {
                ldir->mActive = FALSE;
                rdir->mActive = TRUE;

                if(chdir(string_c_str(rdir->mPath)) == 0) {
                    setenv("PWD", string_c_str(rdir->mPath), 1);
                }
            }
        }

        return TRUE;
    }
    else if(dir >= 2 && dir < vector_count(gDirs)) {
        sDir* adir_ = filer_dir(adir());
        sDir* dir_ = filer_dir(dir);

        if(adir_ && dir_) {
            vector_exchange(gDirs, adir(), dir_);
            vector_exchange(gDirs, dir, adir_);

            adir_->mActive = FALSE;
            dir_->mActive = TRUE;

            if(chdir(string_c_str(dir_->mPath)) == 0) {
                setenv("PWD", string_c_str(dir_->mPath), 1);
            }
        }

        return TRUE;
    }
    else {
        return FALSE;
    }
}

int adir()
{
    sDir* dir0 = (sDir*)vector_item(gDirs, 0);
    sDir* dir1 = (sDir*)vector_item(gDirs, 1);

    if(dir0->mActive)
        return 0;
    else
        return 1;
}

int sdir()
{
    sDir* dir0 = (sDir*)vector_item(gDirs, 0);
    sDir* dir1 = (sDir*)vector_item(gDirs, 1);

    if(!dir0->mActive)
        return 0;
    else
        return 1;
}

sDir* filer_dir(int dir)
{
    if(dir < 0 || dir >= vector_count(gDirs)) {
        return NULL;
    }
    
    return (sDir*)vector_item(gDirs, dir);
}

sFile* filer_cursor_file(int dir)
{
    if(dir < 0 || dir >= vector_count(gDirs)) {
        return NULL;
    }
    
    sDir* dir2 = (sDir*)vector_item(gDirs, dir);

    return (sFile*)vector_item(dir2->mFiles, dir2->mCursor);
}

BOOL filer_toggle_mark(int dir, int num)
{
    if(dir < 0 || dir >= vector_count(gDirs)) {
        return FALSE;
    }
    
    sDir* dir2 = (sDir*)vector_item(gDirs, dir);

    if(num < 0 || num >= vector_count(dir2->mFiles)) {
        return FALSE;
    }

    sFile* file = vector_item(dir2->mFiles, num);

    file->mMark = !file->mMark;

    return TRUE;
}

BOOL filer_set_mark(int dir, int num, int mark)
{
    if(dir < 0 || dir >= vector_count(gDirs)) {
        return FALSE;
    }
    
    sDir* dir2 = (sDir*)vector_item(gDirs, dir);

    if(num < 0 || num >= vector_count(dir2->mFiles)) {
        return FALSE;
    }

    sFile* file = vector_item(dir2->mFiles, num);

    file->mMark = mark;

    return TRUE;
}

BOOL filer_mark(int dir, int num)
{
    if(dir < 0 || dir >= vector_count(gDirs)) {
        return FALSE;
    }
    
    sDir* dir2 = (sDir*)vector_item(gDirs, dir);

    if(num < 0 || num >= vector_count(dir2->mFiles)) {
        return FALSE;
    }

    sFile* file = vector_item(dir2->mFiles, num);

    return file->mMark;
}

int filer_mark_file_num(int dir)
{
    if(dir < 0 || dir >= vector_count(gDirs)) {
        return -1;
    }
    
    sDir* dir2 = (sDir*)vector_item(gDirs, dir);

    int result = 0;
    int i;
    for(i=0; i<vector_count(dir2->mFiles); i++) {
        sFile* file = (sFile*)vector_item(dir2->mFiles, i);

        if(file->mMark) {
            result++;
        }
    }

    return result;
}

double filer_mark_file_size(int dir)
{
    if(dir < 0 || dir >= vector_count(gDirs)) {
        return -1;
    }
    
    sDir* dir2 = (sDir*)vector_item(gDirs, dir);

    double size = 0;
    int i;
    for(i=0; i<vector_count(dir2->mFiles); i++) {
        sFile* file = (sFile*)vector_item(dir2->mFiles, i);

        if(file->mMark && !S_ISDIR(file->mStat.st_mode)) {
            size += file->mLStat.st_size;
        }
    }

    return size;
}

BOOL filer_reset_marks(int dir)
{
    if(dir < 0 || dir >= vector_count(gDirs)) {
        return FALSE;
    }
    
    sDir* dir2 = (sDir*)vector_item(gDirs, dir);
    int i;
    for(i=0; i<vector_count(dir2->mFiles); i++) {
        sFile* file = (sFile*)vector_item(dir2->mFiles, i);
        file->mMark = FALSE;
    }

    return TRUE;
}

///////////////////////////////////////////////////
// filer input
///////////////////////////////////////////////////
void filer_input(int meta, int key)
{
    sDir* dir = filer_dir(adir());
    sFile* cursor = (sFile*)vector_item(dir->mFiles, dir->mCursor);

    char title[128];
    snprintf(title, 128, "keycommand(%d, %d):", meta, key);

    sKeyCommand* keycommand = SEXTOBJ(gFilerKeyBind).mObject;

    sObject* block = keycommand->mBlocks[meta*KEY_MAX+key];
    BOOL external = keycommand->mExternals[meta*KEY_MAX+key];

    if(key >= 0 && key < KEY_MAX && block)
    {
        if(external) {
            const int maxy = mgetmaxy();
            mclear_online(maxy-1);
            mclear_lastline();
            refresh();
            mmove_immediately(maxy-2, 0);
            endwin();

            mreset_tty();

            int rcode;
            if(xyzsh_run(&rcode, block, "external", NULL, gStdin, gStdout, 0, NULL, gMFiler4))
            {
                if(rcode == 0) {
                    xinitscr();

                    char buf[256];
                    snprintf(buf, 256, "reread -d 0; reread -d 1; mark -a 0");
                    int rcode;
                    (void)xyzsh_eval(&rcode, buf, "reread", NULL, gStdin, gStdout, 0, NULL, gMFiler4);
                    //filer_reread(0);
                    //filer_reread(1);
                    //(void)filer_reset_marks(adir());
                }
                else {
                    char str[128];
                    fprintf(stderr, "return code is %d", rcode);

                    printf("\nHIT ANY KEY\n");

                    xinitscr();
                    (void)getch();
                }
            }
            else {
                char str[128];
                fprintf(stderr, "runtime err\n%s", string_c_str(gErrMsg));
                printf("\nHIT ANY KEY\n");

                xinitscr();
                (void)getch();
            }
        }
        else {
            int rcode;
            if(xyzsh_run(&rcode, block, title, NULL, gStdin, gStdout, 0, NULL, gMFiler4)) {
                if(rcode == 0) {
                    //(void)filer_reset_marks(adir());
                }
                else {
                    char str[128];
                    snprintf(str, 128, "return code is %d", rcode);
                    merr_msg(str);
                }
            }
            else {
                merr_msg(string_c_str(gErrMsg));
            }

            ISearchClear();
        }
    }

    set_signal_mfiler();
}

///////////////////////////////////////////////////
// filer view
///////////////////////////////////////////////////

// mbsから端末上の文字列分(termsize)のみ残して残りを捨てた文字列を
// dest_mbsに返す dest_byte: dest_mbsのサイズ
static void str_cut(enum eKanjiCode code, char* mbs, int termsize, char* dest_mbs, int dest_byte)
{
    if(code == kUtf8) {
        wchar_t* wcs;
        wchar_t* wcs_tmp;
        int i;

        wcs = (wchar_t*)MALLOC(sizeof(wchar_t)*(termsize+1)*MB_CUR_MAX);
        wcs_tmp = (wchar_t*)MALLOC(sizeof(wchar_t)*(termsize+1)*MB_CUR_MAX);
        if(mbstowcs(wcs, mbs, (termsize+1)*MB_CUR_MAX) == -1) {
            mbstowcs(wcs, "?????", (termsize+1)*MB_CUR_MAX);
        }

        for(i=0; i<wcslen(wcs); i++) {
            wcs_tmp[i] = wcs[i];
            wcs_tmp[i+1] = 0;

            if(wcswidth(wcs_tmp, wcslen(wcs_tmp)) > termsize) {
                wcs_tmp[i] = 0;
                break;
            }
        }

        wcstombs(dest_mbs, wcs_tmp, dest_byte);

        FREE(wcs);
        FREE(wcs_tmp);
    }
    else {
        int n;
        BOOL kanji = FALSE;
        for(n=0; n<termsize && n<dest_byte-1; n++) {
            if(!kanji && is_kanji(code, mbs[n])) {
                kanji = TRUE;
            }
            else {
                kanji = FALSE;
            }

            dest_mbs[n] = mbs[n];
        }
        
        if(kanji)
            dest_mbs[n-1] = 0;
        else
            dest_mbs[n] = 0;
    }
}

// mbsから端末上の文字列分(termsize)のみ残して残りを捨てた文字列を
// dest_mbsに返す dest_byte: dest_mbsのサイズ
// 切り捨てた文字列をスペースで埋める
void str_cut2(enum eKanjiCode code, char* mbs, int termsize, char* dest_mbs, int dest_byte)
{
    if(code == kUtf8) {
        int i;
        int n;

        wchar_t* wcs 
            = (wchar_t*)MALLOC(sizeof(wchar_t)*(termsize+1)*MB_CUR_MAX);
        wchar_t* tmp 
            = (wchar_t*)MALLOC(sizeof(wchar_t)*(termsize+1)*MB_CUR_MAX);

        if(mbstowcs(wcs, mbs, (termsize+1)*MB_CUR_MAX) == -1) {
            mbstowcs(wcs, "?????", (termsize+1)*MB_CUR_MAX);
        }

        i = 0;
        while(1) {
            if(i < wcslen(wcs)) {
                tmp[i] = wcs[i];
                tmp[i+1] = 0;
            }
            else {
                tmp[i] = ' ';
                tmp[i+1] = 0;
            }

            n = wcswidth(tmp, wcslen(tmp));
            if(n < 0) {
                xstrncpy(dest_mbs, "?????", dest_byte);

                FREE(wcs);
                FREE(tmp);

                return;
            }
            else {
                if(n > termsize) {
                    tmp[i] = 0;

                    if(wcswidth(tmp, wcslen(tmp)) != termsize) {
                        tmp[i] = ' ';
                        tmp[i+1] = 0;
                    }
                    break;
                }
            }

            i++;
        }

        wcstombs(dest_mbs, tmp, dest_byte);

        FREE(wcs);
        FREE(tmp);
    }
    else {
        int n;
        BOOL tmpend = FALSE;
        BOOL kanji = FALSE;
        for(n=0; n<termsize && n<dest_byte-1; n++) {
            if(kanji)
                kanji = FALSE;
            else if(!tmpend && is_kanji(code, mbs[n])) {
                kanji = TRUE;
            }

            if(!tmpend && mbs[n] == 0) tmpend = TRUE;
                        
            if(tmpend)
                dest_mbs[n] = ' ';
            else
                dest_mbs[n] = mbs[n];
        }
        
        if(kanji) dest_mbs[n-1] = ' ';
        dest_mbs[n] = 0;
    }
}

// mbsから端末上の文字列分(termsize)のみ残して残りを捨てた文字列を
// dest_mbsに返す 
// dest_byte: dest_mbsのサイズ
// 切り捨てた文字列をスペースで埋める
// 切り捨てるのは文字列の前から
static void str_cut3(enum eKanjiCode code, char* mbs, int termsize, char* dest_mbs, int dest_byte)
{
    if(code == kUtf8) {
        wchar_t* wcs;
        wchar_t* tmp;
        int i;
        int j;

        wcs = (wchar_t*)MALLOC(sizeof(wchar_t)*(strlen(mbs)+1)*MB_CUR_MAX);
        tmp = (wchar_t*)MALLOC(sizeof(wchar_t)*(strlen(mbs)+1)*MB_CUR_MAX);

        if(str_termlen(code, mbs) <= termsize) {
            str_cut2(code, mbs, termsize, dest_mbs, dest_byte);
        }
        else {
            if(mbstowcs(wcs, mbs, (termsize+1)*MB_CUR_MAX) == -1) {
                mbstowcs(wcs, "?????", (termsize+1)*MB_CUR_MAX);
            }

            i = wcslen(wcs)-1;
            j = 0;
            while(1) {
                tmp[j++] = wcs[i--];
                tmp[j] = 0;

                if(wcswidth(tmp, wcslen(tmp)) > termsize) {
                    tmp[j-1] = 0;
                    i+=2;

                    if(wcswidth(tmp, wcslen(tmp)) == termsize) {
                        wcscpy(tmp, wcs + i);
                        break;
                    }
                    else {
                        tmp[0] = ' ';
                        wcscpy(tmp + 1, wcs + i);
                        break;
                    }
                }
            }

            wcstombs(dest_mbs, tmp, dest_byte);
        }

        FREE(wcs);
        FREE(tmp);
    }
    else {
        BOOL* kanji;
        int i;
        const int len = strlen(mbs);
        char* buf;

        if(len < termsize) {
            xstrncpy(dest_mbs, mbs, dest_byte);
            for(i=len; i<termsize; i++) {
                xstrncat(dest_mbs, " ", dest_byte);
            }
        }
        else {
            kanji = MALLOC(sizeof(BOOL)*len);
            for(i=0; i<len; i++) {
                if(is_kanji(code, mbs[i])) {
                    kanji[i] = TRUE;
                    i++;
                    kanji[i] = FALSE;
                }
                else {
                    kanji[i] = FALSE;
                }
            }

            if(kanji[len-termsize-1]) {
                int i;
                for(i=len-termsize; i<len; i++) {
                    dest_mbs[i-len+termsize] = mbs[i];
                }
                dest_mbs[i-len+termsize] = 0;
                dest_mbs[0] = ' ';
            }
            else {
                int i;
                for(i=len-termsize; i<len; i++) {
                    dest_mbs[i-len+termsize] = mbs[i];
                }
                dest_mbs[i-len+termsize] = 0;
            }

            FREE(kanji);
        }
    }
}

int filer_make_size_str(char* result, long long size)
{
    char* file_size = getenv("VIEW_FILE_SIZE");
    if(strcmp(file_size, "Human") == 0) {
        char tmp[256];
        if(size < 1024) {
            snprintf(tmp, 256, "%lld", size);

            char* end = tmp + strlen(tmp);

            char* p = tmp;
            char* p2 = result;
            
            while(*p) {
                if(end - p == 7) {
                    *p2++ = *p++;
                    *p2++ = ',';
                }
                else if(end - p == 4) {
                    *p2++ = *p++;
                    *p2++ = ',';
                }
                else {
                    *p2++ = *p++;
                }
            }

            *p2 = 0;
        }
        else if(size < 1024*1024) {
            size = size / 1024;
            snprintf(tmp, 256, "%lld", size);

            char* end = tmp + strlen(tmp);

            char* p = tmp;
            char* p2 = result;
            
            while(*p) {
                if(end - p == 9) {
                    *p2++ = *p++;
                    *p2++ = ',';
                }
                else if(end - p == 6) {
                    *p2++ = *p++;
                    *p2++ = ',';
                }
                else {
                    *p2++ = *p++;
                }
            }

            *p2++ = 'k';
            *p2 = 0;
        }
        else if(size < 1024*1024*1024) {
            size = size / (1024*1024);
            snprintf(tmp, 256, "%lld", size);

            char* end = tmp + strlen(tmp);

            char* p = tmp;
            char* p2 = result;
            
            while(*p) {
                if(end - p == 9) {
                    *p2++ = *p++;
                    *p2++ = ',';
                }
                else if(end - p == 6) {
                    *p2++ = *p++;
                    *p2++ = ',';
                }
                else {
                    *p2++ = *p++;
                }
            }

            *p2++ = 'M';
            *p2 = 0;
        }
        else {
            size = size / (1024*1024*1024);
            snprintf(tmp, 256, "%lld", size);

            char* end = tmp + strlen(tmp);

            char* p = tmp;
            char* p2 = result;
            
            while(*p) {
                if(end - p == 9) {
                    *p2++ = *p++;
                    *p2++ = ',';
                }
                else if(end - p == 6) {
                    *p2++ = *p++;
                    *p2++ = ',';
                }
                else {
                    *p2++ = *p++;
                }
            }

            *p2++ = 'G';
            *p2 = 0;
        }

        return 7;
    }
    else if(strcmp(file_size, "Normal") == 0) {
        if(size < 1024*1024) {
            char tmp[256];
            snprintf(tmp, 256, "%lld", size);
            char* end = tmp + strlen(tmp);

            char* p = tmp;
            char* p2 = result;
            
            while(*p) {
                if(end - p == 7) {
                    *p2++ = *p++;
                    *p2++ = ',';
                }
                else if(end - p == 4) {
                    *p2++ = *p++;
                    *p2++ = ',';
                }
                else {
                    *p2++ = *p++;
                }
            }
            *p2 = 0;
        }
        else {
            size = size / (1024*1024);

            char tmp[256];
            snprintf(tmp, 256, "%lld", size);
            char* end = tmp + strlen(tmp);

            char* p = tmp;
            char* p2 = result;
            
            while(*p) {
                if(end - p == 9) {
                    *p2++ = *p++;
                    *p2++ = ',';
                }
                else if(end - p == 6) {
                    *p2++ = *p++;
                    *p2++ = ',';
                }
                else {
                    *p2++ = *p++;
                }
            }
            *p2++ = 'M';
            *p2 = 0;
        }

        return 9;
    }
    else if(strcmp(file_size, "Plane") == 0) {
        char tmp[256];
        snprintf(tmp, 256, "%lld", size);
        char* end = tmp + strlen(tmp);

        char* p = tmp;
        char* p2 = result;
        
        while(*p) {
            if(end - p == 10) {
                *p2++ = *p++;
                *p2++ = ',';
            }
            else if(end - p == 7) {
                *p2++ = *p++;
                *p2++ = ',';
            }
            else if(end - p == 4) {
                *p2++ = *p++;
                *p2++ = ',';
            }
            else {
                *p2++ = *p++;
            }
        }
        *p2 = 0;

        return 14;
    }
    else {
        fprintf(stderr, "VIEW_FILE_SIZE is Normal or Human or Plane.\n");
        exit(1);
    }
}

static void make_size_str2(char* result, long long size)
{
    char tmp[256];
    snprintf(tmp, 256, "%lld", size);
    char* end = tmp + strlen(tmp);

    char* p = tmp;
    char* p2 = result;
    
    while(*p) {
        if(end - p == 13) {
            *p2++ = *p++;
            *p2++ = ',';
        }
        else if(end - p == 10) {
            *p2++ = *p++;
            *p2++ = ',';
        }
        else if(end - p == 7) {
            *p2++ = *p++;
            *p2++ = ',';
        }
        else if(end - p == 4) {
            *p2++ = *p++;
            *p2++ = ',';
        }
        else {
            *p2++ = *p++;
        }
    }
    *p2 = 0;
}

static void make_file_stat(sFile* file, char* buf, int buf_size)
{
    xstrncpy(buf, "", buf_size);

    char* env_permission = getenv("VIEW_PERMISSION");
    if(env_permission && strcmp(env_permission, "1") == 0) {
        char permission[12];
        xstrncpy(permission, "----------", 12);
        if(S_ISDIR(file->mLStat.st_mode)) permission[0] = 'd';
        if(S_ISCHR(file->mLStat.st_mode)) permission[0] = 'c';
        if(S_ISBLK(file->mLStat.st_mode)) permission[0] = 'b';
        if(S_ISFIFO(file->mLStat.st_mode)) permission[0] = 'p';
        if(S_ISLNK(file->mLStat.st_mode)) permission[0] = 'l';
        if(S_ISSOCK(file->mLStat.st_mode)) permission[0] = 's';
    
        mode_t smode = file->mLStat.st_mode;
        if(smode & S_IRUSR) permission[1] = 'r';
        if(smode & S_IWUSR) permission[2] = 'w';
        if(smode & S_IXUSR) permission[3] = 'x';
        if(smode & S_IRGRP) permission[4] = 'r';
        if(smode & S_IWGRP) permission[5] = 'w';
        if(smode & S_IXGRP) permission[6] = 'x';
        if(smode & S_IROTH) permission[7] = 'r';
        if(smode & S_IWOTH) permission[8] = 'w';
        if(smode & S_IXOTH) permission[9] = 'x';
        if(smode & S_ISUID) permission[3] = 's';
        if(smode & S_ISGID) permission[6] = 's';
#if defined(S_ISTXT)
        if(smode & S_ISTXT) permission[9] = 't';
#endif
#if defined(S_ISVTX)
        if(smode & S_ISVTX) permission[9] = 't';
#endif

        snprintf(buf + strlen(buf), buf_size - strlen(buf), " %s", permission);
    }

    char* env_nlink = getenv("VIEW_NLINK");
    if(env_nlink && strcmp(env_nlink, "1") == 0) {
        snprintf(buf + strlen(buf), buf_size -strlen(buf), " "FORMAT_HARDLINK, file->mStat.st_nlink);
    }

    char* env_owner = getenv("VIEW_OWNER");
    if(env_owner && strcmp(env_owner, "1") == 0) {
        char owner[256];
        char* tmp = string_c_str(file->mUser);
        if(tmp) {
            str_cut2(gKanjiCode, tmp, 8, owner, 256);
        }
        else {
            snprintf(owner, 256, "%d", file->mLStat.st_uid);
        }

        snprintf(buf + strlen(buf), buf_size - strlen(buf), " %-8s", owner);
    }

    char* env_group = getenv("VIEW_GROUP");
    if(env_group && strcmp(env_group, "1") == 0) {
        char group[256];
        char* tmp = string_c_str(file->mGroup);
        if(tmp) {
            str_cut2(gKanjiCode, tmp, 8, group, 256);
        }
        else {
            snprintf(group, 256, "%d", file->mLStat.st_gid);
        }

        snprintf(buf + strlen(buf), buf_size - strlen(buf), " %-8s", group);
    }

    char* env_mtime = getenv("VIEW_MTIME");
    if(env_mtime && strcmp(env_mtime, "1") == 0) {
        time_t t = file->mLStat.st_mtime;
        struct tm* tm_ = (struct tm*)localtime(&t);

        int year = tm_->tm_year-100;
        if(year < 0) year+=100;
        while(year > 100) year-=100;

        snprintf(buf + strlen(buf), buf_size - strlen(buf), " %02d-%02d-%02d %02d:%02d"
               , year, tm_->tm_mon+1
               , tm_->tm_mday, tm_->tm_hour, tm_->tm_min);
    }

    char* env_size = getenv("VIEW_SIZE");
    if(env_size && strcmp(env_size, "1") == 0) {
        char size[256];
        int width = filer_make_size_str(size, file->mLStat.st_size);

        if(S_ISDIR(file->mStat.st_mode)) {
            xstrncpy(size, "<DIR>", 256);
        }

        char format[128];
        xstrncpy(format, "%", 128);
        snprintf(format + strlen(format), 126, "%d", width);
        xstrncat(format, "s", 128);

        snprintf(buf + strlen(buf), 256, format, size);
    }

    snprintf(buf + strlen(buf), buf_size - strlen(buf), " ");
}

sFile* filer_file(int dir, int num)
{
    if(dir < 0 || dir >= vector_count(gDirs)) {
        return NULL;
    }
    
    sDir* dir2 = vector_item(gDirs, dir);

    if(num < 0 || num >= vector_count(dir2->mFiles)) {
        return NULL;
    }

    return vector_item(dir2->mFiles, num);
}

sObject* filer_mark_files(int dir) 
{
    if(dir < 0 || dir >= vector_count(gDirs)) {
        return NULL;
    }
    
    sDir* dir2 = vector_item(gDirs, dir);
    sObject* result = VECTOR_NEW_MALLOC(10);
    
    int i;
    for(i=0; i<vector_count(dir2->mFiles); i++) {
        sFile* file = vector_item(dir2->mFiles, i);

        if(file->mMark) {
            vector_add(result, file);
        }
    }

    return result;
}

int filer_file2(int dir, char* name)
{
    if(dir < 0 || dir >= vector_count(gDirs)) {
        return -1;
    }
    
    sDir* dir2 = vector_item(gDirs, dir);
    int i;
    for(i=0; i<vector_count(dir2->mFiles); i++) {
        sFile* file = vector_item(dir2->mFiles, i);
        if(strcmp(string_c_str(file->mName), name) == 0) {
            return i;
        }
    }

    return -1;
}

static void make_file_name(sFile* file, char* fname, int fname_size, char* ext, int ext_size, char* dot, int name_len)
{
    char* env_divide_ext = getenv("VIEW_DIVIDE_EXTENSION");
    BOOL view_divide_ext = FALSE;
    if(env_divide_ext && strcmp(env_divide_ext, "1") == 0) {
        view_divide_ext = TRUE;
    }

    xstrncpy(fname, string_c_str(file->mNameView), fname_size);

    char* env_add_star_exe = getenv("VIEW_ADD_STAR_EXE");

    if(S_ISDIR(file->mStat.st_mode)) {
         xstrncat(fname, "/", fname_size);
    }
    else if(S_ISFIFO(file->mLStat.st_mode)) {
        xstrncat(fname, "|", fname_size);
    }
    else if(S_ISSOCK(file->mLStat.st_mode)) {
        xstrncat(fname, "=", fname_size);
    }
    else if(env_add_star_exe && strcmp(env_add_star_exe, "1") == 0
             &&(file->mStat.st_mode&S_IXUSR
                || file->mStat.st_mode&S_IXGRP
                || file->mStat.st_mode&S_IXGRP))
    {
         xstrncat(fname, "*", fname_size);
    }
    else if(S_ISLNK(file->mLStat.st_mode)) {
        xstrncat(fname, "@", fname_size);
    }

    if(S_ISLNK(file->mLStat.st_mode)) {
        snprintf(fname + strlen(fname), fname_size - strlen(fname), " -> %s", string_c_str(file->mLinkTo));
    }

    if(S_ISDIR(file->mStat.st_mode) || !view_divide_ext) {
        xstrncpy(ext, "", ext_size);
        *dot = ' ';
    }
    else {
        extname(ext, PATH_MAX, fname);

        *dot = ' ';

        if(strcmp(ext, "") != 0) {
            fname[strlen(fname) - strlen(ext) -1] = 0;
            *dot = '.';
        }
        char tmp[128];
        str_cut2(gKanjiCode, ext, 4, tmp, 128);
        xstrncpy(ext, tmp, ext_size);
    }

    if(name_len > 0) {
        char* env_focusback = getenv("VIEW_FOCUSBACK");
        if(env_focusback && strcmp(env_focusback, "1") == 0) {
            str_cut3(gKanjiCode, fname, name_len, fname, PATH_MAX);
        }
        else {
            str_cut2(gKanjiCode, fname, name_len, fname, PATH_MAX);
        }
    }
    else {
        xstrncpy(fname, "", fname_size);
    }
}

static const int kMaxYMinus = 4;

void filer_view(int dir)
{
    if(dir < 0 || dir >= vector_count(gDirs)) {
        return;
    }
    
    sDir* self = (sDir*)vector_item(gDirs, dir);

    char* env_divide_ext = getenv("VIEW_DIVIDE_EXTENSION");
    BOOL view_divide_ext = FALSE;
    if(env_divide_ext && strcmp(env_divide_ext, "1") == 0) {
        view_divide_ext = TRUE;
    }

    char* env_color_dir = getenv("COLOR_DIR");
    int color_dir = 0;
    if(env_color_dir) {
        color_dir = atoi(env_color_dir);
    }

    char* env_color_exe = getenv("COLOR_EXE");
    int color_exe = 0;
    if(env_color_exe) {
        color_exe = atoi(env_color_exe);
    }

    char* env_color_link = getenv("COLOR_LINK");
    int color_link = 0;
    if(env_color_link) {
        color_link = atoi(env_color_link);
    }

    char* env_color_mark = getenv("COLOR_MARK");
    int color_mark = 0;
    if(env_color_mark) {
        color_mark = atoi(env_color_mark);
    }

    const int maxx = mgetmaxx();
    const int maxy = mgetmaxy();

    const int jobs_exist = (xyzsh_job_num() > 0) ? 1 : 0;
    const int dstack_exist = vector_count(gDirs) > 2 ? 1 : 0;

    char* env_page_view_way = getenv("VIEW_PAGE");
    BOOL page_view_way = FALSE;
    if(env_page_view_way && strcmp(env_page_view_way, "1") == 0) {
        page_view_way = TRUE;
    }

    char* env_view_option = getenv("VIEW_OPTION");
    if(env_view_option == NULL) {
        fprintf(stderr, "VIEW_OPTION is NULL\n");
        exit(1);
    }

    ////////////////////////////////////////////////////
    // 1pain 1line
    ///////////////////////////////////////////////////
    if(strcmp(env_view_option, "1pain") == 0) {
        if(self->mActive) {
            mbox(dstack_exist, 0, maxx-1, maxy-2-dstack_exist-jobs_exist);

            char path[PATH_MAX];
            
            xstrncpy(path, string_c_str(self->mPath), PATH_MAX);
            xstrncat(path, string_c_str(self->mMask), PATH_MAX);

#if defined(__DARWIN__)
            char tmp[PATH_MAX];
            if(kanji_convert(path, tmp, PATH_MAX, gKanjiCodeFileName, gKanjiCode) != -1) {
                xstrncpy(path, tmp, PATH_MAX);
            }
#endif

            const int len = strlen(path);
            if(len < maxx-4) {
                attron(A_BOLD);
                mvprintw(0 + dstack_exist, 2, "%s", path);
                attroff(A_BOLD);
            }
            else {
                char buf[PATH_MAX];

                str_cut3(gKanjiCode, path, maxx-4, buf, PATH_MAX);

                attron(A_BOLD);
                mvprintw(0 + dstack_exist, 2, "%s", buf);
                attroff(A_BOLD);
            }

            int i;
            for(i=self->mScrollTop; 
                i-self->mScrollTop 
                    < maxy-kMaxYMinus
                        -dstack_exist-jobs_exist&&i<vector_count(self->mFiles);
                i++)
            {
                int attr = 0;
                sFile* file = (sFile*)vector_item(self->mFiles, i);

                int red = -1;
                int green = -1;
                int blue = -1;
                if(self->mCursor == i && self->mActive) {
                     attr |= A_REVERSE;
                }
        
                if(S_ISDIR(file->mStat.st_mode)) {
                     if(!file->mMark) attr |= color_dir;
                }
                else if(file->mStat.st_mode&S_IXUSR
                            || file->mStat.st_mode&S_IXGRP
                            || file->mStat.st_mode&S_IXGRP)
                {
                     if(!file->mMark) attr |= color_exe;
                }
                else if(S_ISLNK(file->mLStat.st_mode)) {
                    if(!file->mMark) attr |= color_link;
                }

                char buf[1024];
                make_file_stat(file, buf, 1024);

                int name_len;
                if(S_ISDIR(file->mStat.st_mode) || !view_divide_ext) {
                    name_len = maxx-2-2-strlen(buf);
                }
                else {
                    name_len = maxx-2-5-2-strlen(buf);
                }

                char fname[1024];
                char ext[PATH_MAX];
                char dot;
                make_file_name(file, fname, 1024, ext, PATH_MAX, &dot, name_len);

                if(file->mMark) {
                    attr |= color_mark;
                }
                
                attron(attr);
                if(file->mMark) {
                    mvprintw(i-self->mScrollTop + 1 + dstack_exist, 1, "*");
                }
                else {
                    mvprintw(i-self->mScrollTop + 1 + dstack_exist, 1, " ");
                }

                if(S_ISDIR(file->mStat.st_mode) || !view_divide_ext) {
                    mvprintw(i-self->mScrollTop + 1 + dstack_exist, 2, "%s%s", fname,buf);
                }
                else {
                    mvprintw(i-self->mScrollTop + 1 + dstack_exist, 2, "%s%c%-4s%s", fname, dot, ext,buf);
                }
                
                attroff(attr);
            }

            char size[256];
            make_size_str2(size, filer_mark_file_size(dir));
            if(page_view_way) {
                const int p = (((float)self->mCursor+1)
                                    /((float)vector_count(self->mFiles)))*100;
                mvprintw(maxy - kMaxYMinus+1-jobs_exist, 2
                        , "(%d/%d)-%sbytes-(%3d%%)", filer_mark_file_num(dir)
                        , vector_count(self->mFiles)-1, size, p);
            }
            else {
                if(filer_line_max(adir()) > 0) {
                    const int cur_page = self->mCursor/filer_line_max(adir()) + 1;
                    const int max_page = vector_count(self->mFiles)/filer_line_max(adir()) + 1;

                    mvprintw(maxy - kMaxYMinus+1-jobs_exist, 2
                            , "(%d/%d)-%sbytes-(%d/%d)", filer_mark_file_num(dir)
                            , vector_count(self->mFiles)-1, size, cur_page, max_page);
                }
            }
       }
    }
    //////////////////////////////////////////////
    // 1pain 2lines
    /////////////////////////////////////////////
    else if(strcmp(env_view_option, "1pain2") == 0) {
        if(self->mActive) {
            mbox(dstack_exist, 0, maxx-1, maxy-2-dstack_exist-jobs_exist);

            char path[PATH_MAX];

            xstrncpy(path, string_c_str(self->mPath), PATH_MAX);
            xstrncat(path, string_c_str(self->mMask), PATH_MAX);

#if defined(__DARWIN__)
            char tmp[PATH_MAX];
            if(kanji_convert(path, tmp, PATH_MAX, gKanjiCodeFileName, gKanjiCode) != -1) {
                xstrncpy(path, tmp, PATH_MAX);
            }
#endif

            const int len = strlen(path);
            if(len < maxx-4) {
                attron(A_BOLD);
                mvprintw(0 + dstack_exist, 2, "%s", path);
                attroff(A_BOLD);
            }
            else {
                char buf[PATH_MAX];

                str_cut3(gKanjiCode, path, maxx-4, buf, PATH_MAX);

                attron(A_BOLD);
                mvprintw(0 + dstack_exist, 2, "%s", buf);
                attroff(A_BOLD);
            }
            int i;
            for(i=self->mScrollTop;
                i-self->mScrollTop<(maxy-kMaxYMinus-dstack_exist-jobs_exist)*2
                            && i<vector_count(self->mFiles);
                i++)
            {
                int attr = 0;
                sFile* file = (sFile*)vector_item(self->mFiles, i);


                int red = -1;
                int green = -1;
                int blue = -1;

                if(self->mCursor == i && self->mActive) {
                     attr |= A_REVERSE;
                }
        
                if(S_ISDIR(file->mStat.st_mode)) {
                     if(!file->mMark) attr |= color_dir;
                }
                else if(file->mStat.st_mode&S_IXUSR
                            || file->mStat.st_mode&S_IXGRP
                            || file->mStat.st_mode&S_IXGRP)
                {
                     if(!file->mMark) attr |= color_exe;
                }
                else if(S_ISLNK(file->mLStat.st_mode)) {
                    if(!file->mMark) attr |= color_link;
                }

                char buf[1024];
                make_file_stat(file, buf, 1024);

                int name_len;
                if(S_ISDIR(file->mStat.st_mode) || !view_divide_ext) {
                    name_len = maxx/2-2-2-strlen(buf);
                }
                else {
                    name_len = maxx/2-2-5-2-strlen(buf);
                }

                char fname[1024];
                char ext[PATH_MAX];
                char dot;
                make_file_name(file, fname, 1024, ext, PATH_MAX, &dot, name_len);

                const int x = (i-self->mScrollTop)
                                /(maxy-kMaxYMinus-dstack_exist-jobs_exist)
                                *(maxx/2) + 1;
                const int y = (i-self->mScrollTop)
                                %(maxy-kMaxYMinus-dstack_exist-jobs_exist)
                                    + 1 + dstack_exist;

                if(file->mMark) {
                    attr |= color_mark;
                }
                
                attron(attr);
                if(file->mMark) {
                    mvprintw(y, x, "*");
                }
                else {
                    mvprintw(y, x, " ");
                }

                if(S_ISDIR(file->mStat.st_mode) || !view_divide_ext) {
                    mvprintw(y, x+1, "%s%s", fname,buf);
                }
                else {
                    mvprintw(y, x+1, "%s%c%-4s%s", fname, dot, ext,buf);
                }
                
                attroff(attr);
            }

            char size[256];
            make_size_str2(size, filer_mark_file_size(dir));

            if(page_view_way) {
                const int p = (((float)self->mCursor+1)/((float)vector_count(self->mFiles)))*100;
                
                mvprintw(maxy - kMaxYMinus+1-jobs_exist, 2
                        , "(%d/%d)-%sbytes-(%3d%%)"
                        , filer_mark_file_num(dir), vector_count(self->mFiles)-1, size, p);
            }
            else {
                if(filer_line_max(adir()) > 0) {
                    const int cur_page = self->mCursor/(filer_line_max(adir())*2) + 1;
                    const int max_page = vector_count(self->mFiles)/(filer_line_max(adir())*2) + 1;

                    mvprintw(maxy - kMaxYMinus+1-jobs_exist, 2
                          , "(%d/%d)-%sbytes-(%d/%d)", filer_mark_file_num(dir)
                          , vector_count(self->mFiles)-1, size, cur_page, max_page);
                }
            }
        }
    }
    //////////////////////////////////////////////
    // 1pain 3lines
    //////////////////////////////////////////////
    else if(strcmp(env_view_option, "1pain3") == 0) {
        if(self->mActive) {
            mbox(dstack_exist, 0, maxx-1, maxy-2-dstack_exist-jobs_exist);

            char path[PATH_MAX];

            xstrncpy(path, string_c_str(self->mPath), PATH_MAX);
            xstrncat(path, string_c_str(self->mMask), PATH_MAX);

#if defined(__DARWIN__)
            char tmp[PATH_MAX];
            if(kanji_convert(path, tmp, PATH_MAX, gKanjiCodeFileName, gKanjiCode) != -1) {
                xstrncpy(path, tmp, PATH_MAX);
            }
#endif

            const int len = strlen(path);
            if(len < maxx-4) {
                attron(A_BOLD);
                mvprintw(0 + dstack_exist, 2, "%s", path);
                attroff(A_BOLD);
            }
            else {
                char buf[PATH_MAX];

                str_cut3(gKanjiCode, path, maxx-4, buf, PATH_MAX);

                attron(A_BOLD);
                mvprintw(0 + dstack_exist, 2, "%s", buf);
                attroff(A_BOLD);
            }

            int i;
            for(i=self->mScrollTop;
                i-self->mScrollTop<(maxy-kMaxYMinus-dstack_exist-jobs_exist)*3
                            && i<vector_count(self->mFiles);
                i++)
            {
                int attr = 0;
                sFile* file = (sFile*)vector_item(self->mFiles, i);

                int red = -1;
                int green = -1;
                int blue = -1;

                if(self->mCursor == i && self->mActive) {
                     attr |= A_REVERSE;
                }
        
                if(S_ISDIR(file->mStat.st_mode)) {
                     if(!file->mMark) attr |= color_dir;
                }
                else if(file->mStat.st_mode&S_IXUSR
                            || file->mStat.st_mode&S_IXGRP
                            || file->mStat.st_mode&S_IXGRP)
                {
                     if(!file->mMark) attr |= color_exe;
                }
                else if(S_ISLNK(file->mLStat.st_mode)) {
                    if(!file->mMark) attr |= color_link;
                }

                char buf[1024];
                make_file_stat(file, buf, 1024);

                int name_len;
                if(S_ISDIR(file->mStat.st_mode) || !view_divide_ext) {
                    name_len = maxx/3-2-2-strlen(buf);
                }
                else {
                    name_len = maxx/3-2-5-2-strlen(buf);
                }

                char fname[1024];
                char ext[PATH_MAX];
                char dot;
                make_file_name(file, fname, 1024, ext, PATH_MAX, &dot, name_len);

                const int x = (i-self->mScrollTop)
                                /(maxy-kMaxYMinus-dstack_exist-jobs_exist)
                                *(maxx/3) + 1;
                const int y = (i-self->mScrollTop)
                                %(maxy-kMaxYMinus-dstack_exist-jobs_exist)
                                    + 1 + dstack_exist;

                if(file->mMark) {
                    attr |= A_BOLD;
                }
                
                attron(attr);
                if(file->mMark) {
                    mvprintw(y, x, "*");
                }
                else {
                    mvprintw(y, x, " ");
                }

                if(S_ISDIR(file->mStat.st_mode) || !view_divide_ext) {
                    mvprintw(y, x+1, "%s%s", fname,buf);
                }
                else {
                    mvprintw(y, x+1, "%s%c%-4s%s", fname, dot, ext,buf);
                }
                
                attroff(attr);
            }
            
            char size[256];
            make_size_str2(size, filer_mark_file_size(dir));

            if(page_view_way) {
                const int p = (((float)self->mCursor+1)/((float)vector_count(self->mFiles)))*100;
                
                mvprintw(maxy - kMaxYMinus+1-jobs_exist, 2
                        , "(%d/%d)-%sbytes-(%3d%%)", filer_mark_file_num(dir), vector_count(self->mFiles)-1, size, p);
            }
            else {
                if(filer_line_max(adir()) > 0) {
                    const int cur_page = self->mCursor/(filer_line_max(adir())*3) + 1;
                    const int max_page = vector_count(self->mFiles)/(filer_line_max(adir())*3) + 1;

                    mvprintw(maxy - kMaxYMinus+1-jobs_exist, 2
                            , "(%d/%d)-%sbytes-(%d/%d)", filer_mark_file_num(dir)
                            , vector_count(self->mFiles)-1, size, cur_page, max_page);
                }
            }
        }
    }
    
    //////////////////////////////////////////////
    // 1pain 5lines
    //////////////////////////////////////////////
    else if(strcmp(env_view_option, "1pain5") == 0) {
        if(self->mActive) {
            mbox(dstack_exist, 0, maxx-1, maxy-2-dstack_exist-jobs_exist);

            char path[PATH_MAX];

            xstrncpy(path, string_c_str(self->mPath), PATH_MAX);
            xstrncat(path, string_c_str(self->mMask), PATH_MAX);

#if defined(__DARWIN__)
            char tmp[PATH_MAX];
            if(kanji_convert(path, tmp, PATH_MAX, gKanjiCodeFileName, gKanjiCode) != -1) {
                xstrncpy(path, tmp, PATH_MAX);
            }
#endif

            const int len = strlen(path);
            if(len < maxx-4) {
                attron(A_BOLD);
                mvprintw(0 + dstack_exist, 2, "%s", path);
                attroff(A_BOLD);
            }
            else {
                char buf[PATH_MAX];

                str_cut3(gKanjiCode, path, maxx-4, buf, PATH_MAX);

                attron(A_BOLD);
                mvprintw(0 + dstack_exist, 2, "%s", buf);
                attroff(A_BOLD);
            }

            int i;
            for(i=self->mScrollTop;
                i-self->mScrollTop<(maxy-kMaxYMinus-dstack_exist-jobs_exist)*5
                            && i<vector_count(self->mFiles);
                i++)
            {
                int attr = 0;
                sFile* file = (sFile*)vector_item(self->mFiles, i);

                int red = -1;
                int green = -1;
                int blue = -1;

                if(self->mCursor == i && self->mActive) {
                     attr |= A_REVERSE;
                }
        
                if(S_ISDIR(file->mStat.st_mode)) {
                     if(!file->mMark) attr |= color_dir;
                }
                else if(file->mStat.st_mode&S_IXUSR
                            || file->mStat.st_mode&S_IXGRP
                            || file->mStat.st_mode&S_IXGRP)
                {
                     if(!file->mMark) attr |= color_exe;
                }
                else if(S_ISLNK(file->mLStat.st_mode)) {
                    if(!file->mMark) attr |= color_link;
                }

                char buf[1024];
                make_file_stat(file, buf, 1024);

                int name_len;
                if(S_ISDIR(file->mStat.st_mode) || !view_divide_ext) {
                    name_len = maxx/5-2-2-strlen(buf);
                }
                else {
                    name_len = maxx/5-2-5-2-strlen(buf);
                }
                char fname[1024];
                char ext[PATH_MAX];
                char dot;
                make_file_name(file, fname, 1024, ext, PATH_MAX, &dot, name_len);

                if(file->mMark) {
                    attr |= A_BOLD;
                }

                attron(attr);
                const int x = (i-self->mScrollTop)
                                /(maxy-kMaxYMinus-dstack_exist-jobs_exist)
                                *(maxx/5) + 1;
                const int y = (i-self->mScrollTop)
                                %(maxy-kMaxYMinus-dstack_exist-jobs_exist)
                                    + 1 + dstack_exist;
                if(file->mMark) {
                    mvprintw(y, x, "*");
                }
                else {
                    mvprintw(y, x, " ");
                }
                if(S_ISDIR(file->mStat.st_mode) || !view_divide_ext) {
                    mvprintw(y, x+1, "%s%s", fname, buf);
                }
                else {
                    mvprintw(y, x+1, "%s%c%-4s%s", fname, dot, ext, buf);
                }
                
                attroff(attr);
            }

            char size[256];
            make_size_str2(size, filer_mark_file_size(dir));

            if(page_view_way) {
                const int p = (((float)self->mCursor+1)/((float)vector_count(self->mFiles)))*100;
                
                mvprintw(maxy - kMaxYMinus+1-jobs_exist, 2
                        , "(%d/%d)-%sbytes-(%3d%%)", filer_mark_file_num(dir), vector_count(self->mFiles)-1, size, p);
            }
            else {
                if(filer_line_max(adir()) > 0) {
                    const int cur_page = self->mCursor/(filer_line_max(adir())*5) + 1;
                    const int max_page = vector_count(self->mFiles)/(filer_line_max(adir())*5) + 1;

                    mvprintw(maxy - kMaxYMinus+1-jobs_exist, 2
                            , "(%d/%d)-%sbytes-(%d/%d)", filer_mark_file_num(dir)
                            , vector_count(self->mFiles)-1, size, cur_page, max_page);
                }
            }
        }
    }

    //////////////////////////////////////////////
    // 2pain
    //////////////////////////////////////////////
    else {
        int x;
        if(dir == 0)
            x = 0;
        else
            x = maxx / 2; 

        int line = 0;
        mbox(dstack_exist, x, maxx/2-1, maxy-2-dstack_exist-jobs_exist);

        char path[PATH_MAX];

        char* filer_directory_path = getenv("FILER_DIRECTORY_PATH");
        char* filer_directory_path2 = getenv("FILER_DIRECTORY_PATH2");
        if(dir == 0 && filer_directory_path && strcmp(filer_directory_path, "") != 0)
        {
            xstrncpy(path, filer_directory_path, PATH_MAX);
        }
        else if(dir == 1 && filer_directory_path2 && strcmp(filer_directory_path2, "") != 0)
        {
            xstrncpy(path, filer_directory_path2, PATH_MAX);
        }
        else {
            xstrncpy(path, string_c_str(self->mPath), PATH_MAX);
        }
        xstrncat(path, string_c_str(self->mMask), PATH_MAX);

#if defined(__DARWIN__)
        char tmp[PATH_MAX];
        if(kanji_convert(path, tmp, PATH_MAX, gKanjiCodeFileName, gKanjiCode) != -1) {
            xstrncpy(path, tmp, PATH_MAX);
        }
#endif

        const int len = strlen(path);
        if(len < maxx/2-5) {
            if(self->mActive) attron(A_BOLD);
            mvprintw(0 + dstack_exist, x+2, "%s", path);
            if(self->mActive) attroff(A_BOLD);
        }
        else {
            char buf[PATH_MAX];

            str_cut3(gKanjiCode, path, maxx/2-5, buf, PATH_MAX);

            if(self->mActive) attron(A_BOLD);
            mvprintw(0 + dstack_exist, x+2, "%s", buf);
            if(self->mActive) attroff(A_BOLD);
        }

        int i;
        for(i=self->mScrollTop;
                i-self->mScrollTop<maxy-kMaxYMinus-dstack_exist-jobs_exist
                        && i<vector_count(self->mFiles);
                i++)
            {
                int attr = 0;
                sFile* file = (sFile*)vector_item(self->mFiles, i);
                
                int red = -1;
                int green = -1;
                int blue = -1;

                if(self->mCursor == i && self->mActive) {
                     attr |= A_REVERSE;
                }
        
                if(S_ISDIR(file->mStat.st_mode)) {
                     if(!file->mMark) attr |= color_dir;
                }
                else if(file->mStat.st_mode&S_IXUSR
                            || file->mStat.st_mode&S_IXGRP
                            || file->mStat.st_mode&S_IXGRP)
                {
                     if(!file->mMark) attr |= color_exe;
                }
                else if(S_ISLNK(file->mLStat.st_mode)) {
                    if(!file->mMark) attr |= color_link;
                }

                char buf[1024];
                make_file_stat(file, buf, 1024);

                int name_len;
                if(S_ISDIR(file->mStat.st_mode) || !view_divide_ext) {
                    name_len = maxx/2-2-2-strlen(buf);
                }
                else {
                    name_len = maxx/2-2-5-2-strlen(buf);
                }

                char fname[1024];
                char ext[PATH_MAX];
                char dot;
                make_file_name(file, fname, 1024, ext, PATH_MAX, &dot, name_len);

                if(file->mMark) {
                    attr |= color_mark;
                }
                
                attron(attr);
                if(file->mMark) {
                    mvprintw(i-self->mScrollTop + 1 + dstack_exist, x+1, "*");
                }
                else {
                    mvprintw(i-self->mScrollTop + 1 + dstack_exist, x+1, " ");
                }

                if(S_ISDIR(file->mStat.st_mode) || !view_divide_ext) {
                    mvprintw(i-self->mScrollTop + 1 + dstack_exist, x+2, "%s%s", fname,buf);
                }
                else {
                    mvprintw(i-self->mScrollTop + 1 + dstack_exist, x+2, "%s%c%-4s%s", fname, dot, ext,buf);
                }
                
                attroff(attr);
        }

        char size[256];
        make_size_str2(size, filer_mark_file_size(dir));
        
        mvprintw(maxy - kMaxYMinus+1-jobs_exist, x + 2
                    , "(%d/%d)-%sbytes", filer_mark_file_num(dir), vector_count(self->mFiles)-1, size);
        
        if(self->mActive) {
            if(page_view_way) {
                const int p = (((float)self->mCursor+1)/((float)vector_count(self->mFiles)))*100;
                printw("-(%3d%%)", p);
            }
            else {
                if(filer_line_max(adir()) > 0) {
                    const int cur_page = self->mCursor/filer_line_max(adir()) + 1;
                    const int max_page = vector_count(self->mFiles)/(filer_line_max(adir())) + 1;

                    printw("-(%d/%d)", cur_page, max_page);
                }
            }
        }
    }

    /// draw gDirStack ///
    if(dstack_exist) {
        const int width = maxx / (vector_count(gDirs)-2);
        int i;
        for(i=2; i<vector_count(gDirs); i++) {
            sDir* dir = vector_item(gDirs, i);
            char* item = string_c_str(dir->mPath);
            
            char item2[256];
            xstrncpy(item2, "", 256);
            const int len2 = strlen(item);
            int k;
            for(k=len2-2; k>=0; k--) {
                if(item[k] == '/') {
                    int l;
                    for(l=k+1; l<len2-1; l++) {
                        item2[l-k-1] = item[l];
                    }
                    item2[l-k-1] = 0;
                    break;
                }
            }

            xstrncat(item2, "/", 256);
            
            char buf2[256];
            
            str_cut2(gKanjiCode, item2, width-6, buf2, 256);
            
            char buf[256];
            snprintf(buf, 256, "[%d.%s]", i, buf2);

            mvprintw(0, width * (i-2), "%s", buf);
        }
    }
}

void job_view()
{
    const int maxy = mgetmaxy();
    const int maxx = mgetmaxx();

    const int size = xyzsh_job_num();
    int i;
    for(i=0; i<size; i++) {
        char* title = xyzsh_job_title(i);

        const int x = (maxx/size)*i;
        char buf[BUFSIZ];
        snprintf(buf, BUFSIZ, "[%d]%s", i+1, title);
        char buf2[BUFSIZ];
        
        str_cut(gKanjiCode, buf, maxx/size, buf2, BUFSIZ);

        mvprintw(maxy-3, x, "%s", buf2);
    }
}

void cmdline_view_filer()
{
    const int maxx = mgetmaxx();        // 画面サイズ
    const int maxy = mgetmaxy();
    
    stack_start_stack();

    sObject* fun = FUN_NEW_STACK(NULL);
    sObject* stackframe = UOBJECT_NEW_GC(8, gXyzshObject, "_stackframe", FALSE);
    vector_add(gStackFrames, stackframe);
    //uobject_init(stackframe);
    SFUN(fun).mLocalObjects = stackframe;

    sObject* nextout = FD_NEW_STACK();

    int rcode;
    if(!run(gMFiler4Prompt, gStdin, nextout, &rcode, gCurrentObject, fun)) {
        if(rcode == RCODE_BREAK) {
            fprintf(stderr, "invalid break. Not in a loop\n");
        }
        else if(rcode == RCODE_RETURN) {
            fprintf(stderr, "invalid return. Not in a function\n");
        }
        else if(rcode == RCODE_EXIT) {
            fprintf(stderr, "invalid exit. In the prompt\n");
        }
        else {
            fprintf(stderr, "run time error\n");
            fprintf(stderr, "%s", string_c_str(gErrMsg));
        }
    }

    char buf[1024];
    str_cut(gKanjiCode, SFD(nextout).mBuf, maxx, buf, 1024);
    mvprintw(maxy-2, 0, buf);
    //mvprintw(maxy-2, 0, SFD(nextout).mBuf);

    (void)vector_pop_back(gStackFrames);
    stack_end_stack();

    /// カーソル下のファイルの情報を描写 ///          
    sFile* file = filer_cursor_file(adir());

    char permission[12];
    xstrncpy(permission, "----------", 12);

    if(S_ISDIR(file->mLStat.st_mode)) permission[0] = 'd';  // ファイルの種別
    if(S_ISCHR(file->mLStat.st_mode)) permission[0] = 'c';
    if(S_ISBLK(file->mLStat.st_mode)) permission[0] = 'b';
    if(S_ISFIFO(file->mLStat.st_mode)) permission[0] = 'p';
    if(S_ISLNK(file->mLStat.st_mode)) permission[0] = 'l';
    if(S_ISSOCK(file->mLStat.st_mode)) permission[0] = 's';

    mode_t smode = file->mLStat.st_mode;        // パーミッション
    if(smode & S_IRUSR) permission[1] = 'r';
    if(smode & S_IWUSR) permission[2] = 'w';
    if(smode & S_IXUSR) permission[3] = 'x';
    if(smode & S_IRGRP) permission[4] = 'r';
    if(smode & S_IWGRP) permission[5] = 'w';
    if(smode & S_IXGRP) permission[6] = 'x';
    if(smode & S_IROTH) permission[7] = 'r';
    if(smode & S_IWOTH) permission[8] = 'w';
    if(smode & S_IXOTH) permission[9] = 'x';
    if(smode & S_ISUID) permission[3] = 's';
    if(smode & S_ISGID) permission[6] = 's';
#if defined(S_ISTXT)
    if(smode & S_ISTXT) permission[9] = 't';
#endif
#if defined(S_ISVTX)
    if(smode & S_ISVTX) permission[9] = 't';
#endif
    
    permission[10] = 0;

    time_t t = file->mLStat.st_mtime;                   // 時間
    struct tm* tm_ = (struct tm*)localtime(&t);
    
    char buf2[1024];

    char owner[256];
    char* tmp = string_c_str(file->mUser);
    if(tmp) {
        str_cut2(gKanjiCode, tmp, 8, owner, 256);
    }
    else {
        snprintf(owner, 256, "%d", file->mLStat.st_uid);
    }

    char group[256];
    tmp = string_c_str(file->mGroup);
    if(tmp) {
        str_cut2(gKanjiCode, tmp, 7, group, 256);
    }
    else {
        snprintf(group, 256, "%d", file->mLStat.st_gid);
    }

    char size_buf[256];
    int width = filer_make_size_str(size_buf, file->mLStat.st_size);

    if(S_ISDIR(file->mStat.st_mode)) {
        xstrncpy(size_buf, "<DIR>", 256);
    }

    char format[128];
    xstrncpy(format, "%", 128);
    snprintf(format + 1, 126, "%d", width);
    xstrncat(format, "s", 128);

    char size_buf2[256];
    snprintf(size_buf2, 256, format, size_buf);
    if(!S_ISLNK(file->mLStat.st_mode)) {
        int year = tm_->tm_year-100;
        if(year < 0) year+=100;
        while(year > 100) year-=100;
        
        snprintf(buf2, 1024, 
            "%s "FORMAT_HARDLINK" %-8s %-7s%s %02d-%02d-%02d %02d:%02d %s"
            //"%s %3d %-8s %-7s%s %02d-%02d-%02d %02d:%02d %s"
            //, "%s %3d %-8s %-7s%10lld %02d-%02d-%02d %02d:%02d %s"
            , permission, file->mLStat.st_nlink
            , owner, group
                                
            , size_buf2
            
            , year, tm_->tm_mon+1, tm_->tm_mday
            , tm_->tm_hour, tm_->tm_min
            , string_c_str(file->mNameView));
            
        char buf3[1024];
        str_cut2(gKanjiCode, buf2, maxx-1, buf3, 1024);
                            
        mvprintw(maxy-1, 0, "%s", buf3);
    }
    else {
        snprintf(buf, 1024,
           "%s "FORMAT_HARDLINK" %s%s%s %02d-%02d-%02d %02d:%02d %s -> %s"
           //"%s %3d %s%s%s %02d-%02d-%02d %02d:%02d %s -> %s"
           //, "%s %3d %s%s%10lld %02d-%02d-%02d %02d:%02d %s -> %s"
           , permission, file->mLStat.st_nlink
           , owner, group
                                
           , size_buf2

           , tm_->tm_year-100, tm_->tm_mon+1, tm_->tm_mday
           , tm_->tm_hour, tm_->tm_min
           , string_c_str(file->mNameView)
           , string_c_str(file->mLinkTo));
           
        char buf2[1024];
        str_cut2(gKanjiCode, buf, maxx-1, buf2, 1024);
                   
        mvprintw(maxy-1, 0, "%s", buf2);
    }

    move(maxy-1, 0);
}

int filer_line(int dir)
{
    if(dir < 0 || dir >= vector_count(gDirs)) {
        return -1;
    }
    
    const int maxx = mgetmaxx();
    const int maxy = mgetmaxy();

    const int dstack_exist = vector_count(gDirs) > 2 ? 1 : 0;
    const int jobs_exist = xyzsh_job_num() > 0 ? 1 : 0;

    sDir* dir2 = vector_item(gDirs, dir);

    char* env_view_option = getenv("VIEW_OPTION");
    if(env_view_option == NULL) {
        fprintf(stderr, "VIEW_OPTION is NULL\n");
        exit(1);
    }

    if(dir2) {
        if(strcmp(env_view_option, "2pain") == 0)
        {
            return dir2->mCursor - dir2->mScrollTop;
        }
        else {
             return (dir2->mCursor - dir2->mScrollTop)
                         % (maxy-kMaxYMinus-dstack_exist-jobs_exist);
        }
    }

    return -1;
}

int filer_line_max(int dir)
{
    const int jobs_exist = xyzsh_job_num() > 0 ? 1 : 0;
    const int dstack_exist = vector_count(gDirs) > 2 ? 1 : 0;
       
    return mgetmaxy() - kMaxYMinus - jobs_exist - dstack_exist;
}

int filer_row(int dir)
{
    if(dir < 0 || dir >= vector_count(gDirs)) {
        return -1;
    }
    
    const int maxx = mgetmaxx();
    const int maxy = mgetmaxy();

    const int dstack_exist = vector_count(gDirs) > 2 ? 1 : 0;
    const int jobs_exist = xyzsh_job_num() > 0 ? 1 : 0;

    sDir* dir2 = vector_item(gDirs, dir);

    char* env_view_option = getenv("VIEW_OPTION");
    if(env_view_option == NULL) {
        fprintf(stderr, "VIEW_OPTION is NULL\n");
        exit(1);
    }

    if(dir2) {
        if(strcmp(env_view_option, "2pain") == 0)
        {
            return 0;
        }
        else {
            return dir2->mCursor/(maxy-kMaxYMinus-dstack_exist-jobs_exist);
        }
    }

    return -1;
}

int filer_row_max(int dir)
{
    if(dir < 0 || dir >= vector_count(gDirs)) {
        return -1;
    }
    
    const int maxx = mgetmaxx();
    const int maxy = mgetmaxy();

    const int jobs_exist = xyzsh_job_num() > 0 ? 1 : 0;
    const int dstack_exist = vector_count(gDirs) > 2 ? 1 : 0;

    sDir* dir2 = vector_item(gDirs, dir);

    char* env_view_option = getenv("VIEW_OPTION");
    if(env_view_option == NULL) {
        fprintf(stderr, "VIEW_OPTION is NULL\n");
        exit(1);
    }
    if(dir2) {
        if(strcmp(env_view_option, "2pain") == 0) {
            return 1;
        }
        else {
            return (vector_count(dir2->mFiles)-1)
                    /(maxy-kMaxYMinus-dstack_exist-jobs_exist) + 1;
        }
    }

    return -1;
}


BOOL filer_sort(int dir)
{
    if(dir < 0 || dir >= vector_count(gDirs)) {
        return FALSE;
    }
    sDir* self = (sDir*)vector_item(gDirs, dir);

    sDir_sort(self);

    return TRUE;
}


BOOL filer_set_dotdir_mask(int dir, BOOL flg)
{
    sDir* dir2 = filer_dir(dir);

    if(dir2 == NULL) {
        return FALSE;
    }

    dir2->mDotDirMask = flg;

    return TRUE;
}
BOOL filer_dotdir_mask(int dir)
{
    sDir* dir2 = filer_dir(dir);

    if(dir2) {
        return dir2->mDotDirMask;
    }
}

