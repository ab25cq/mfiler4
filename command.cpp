#include "common.h"

extern "C" {
#include <libgen.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <pwd.h>
}

enum eUndoOp { kUndoNil, kUndoCopy, kUndoMove, kUndoTrashBox };
eUndoOp gUndoOp = kUndoNil;
VALUE gUndoSdir;
VALUE gUndoDdir;
VALUE gUndoFiles;

char gHostName[256];
char gUserName[256];

VALUE mf_minitscr(VALUE self)
{
    if(!mis_curses()) {
        minitscr();
    }

    return Qnil;
}

VALUE mf_mendwin(VALUE self)
{
    if(mis_curses()) {
        mendwin();
    }

    return Qnil;
}

VALUE mf_mmove(VALUE self, VALUE y, VALUE x)
{
    Check_Type(y, T_FIXNUM);
    Check_Type(x, T_FIXNUM);

    mmove(NUM2INT(y), NUM2INT(x));

    return Qnil;
}

VALUE mf_mmove_immediately(VALUE self, VALUE y, VALUE x)
{
    Check_Type(y, T_FIXNUM);
    Check_Type(x, T_FIXNUM);

    mmove_immediately(NUM2INT(y), NUM2INT(x));

    return Qnil;
}

VALUE mf_mprintw(int argc, VALUE* argv, VALUE self)
{
    if(argc < 1) {
        fprintf(stderr, "invalid arguments in mprintw(%d)", argc);
        exit(1);
    }

    /// get str ///
    VALUE str = rb_f_sprintf(argc, argv);
    int ret = mprintw("%s", RSTRING(str)->ptr);

    return INT2NUM(ret);
}

VALUE mf_mmvprintw(int argc, VALUE* argv, VALUE self)
{
    if(argc < 3) {
        fprintf(stderr, "invalid arguments in mmvprintw(%d)", argc);
        exit(1);
    }
        
    /// get x, y ///
    Check_Type(argv[0], T_FIXNUM);
    Check_Type(argv[1], T_FIXNUM);

    int x = NUM2INT(argv[0]);
    int y = NUM2INT(argv[1]);

    /// get str ///
    int new_argc = argc-2;
    VALUE* new_argv = argv + 2;
    
    VALUE str = rb_f_sprintf(new_argc, new_argv);
    int ret = mmvprintw(x, y, "%s", RSTRING(str)->ptr);

    return INT2NUM(ret);
}

VALUE mf_mattron(VALUE self, VALUE attrs)
{
    Check_Type(attrs, T_FIXNUM);

    mattron(NUM2INT(attrs));

    return Qnil;
}

VALUE mf_mattroff(VALUE self)
{
    mattroff();

    return Qnil;
}

VALUE mf_mgetmaxx(VALUE self)
{
    int ret = mgetmaxx();

    return INT2NUM(ret);
}

VALUE mf_mgetmaxy(VALUE self)
{
    int ret = mgetmaxy();

    return INT2NUM(ret);
}

VALUE mf_mclear_immediately(VALUE self)
{
    mclear_immediately();

    return Qnil;
}

VALUE mf_mclear(VALUE self)
{
    mclear();

    return Qnil;
}

VALUE mf_mclear_online(VALUE self, VALUE y)
{
    Check_Type(y, T_FIXNUM);

    mclear_online(NUM2INT(y));

    return Qnil;
}

VALUE mf_mbox(VALUE self, VALUE y, VALUE x, VALUE width, VALUE height)
{
    Check_Type(y, T_FIXNUM);
    Check_Type(x, T_FIXNUM);
    Check_Type(width, T_FIXNUM);
    Check_Type(height, T_FIXNUM);

    mbox(NUM2INT(y), NUM2INT(x), NUM2INT(width), NUM2INT(height));

    return Qnil;
}

VALUE mf_mrefresh(VALUE self)
{
    mrefresh();

    return Qnil;
}

VALUE mf_mgetch(VALUE self)
{
    int meta;
    int key = mgetch(&meta);
    
    VALUE result = rb_ary_new();
    rb_ary_push(result, INT2NUM(meta));
    rb_ary_push(result, INT2NUM(key));

    return result;
}

VALUE mf_mgetch_nonblock(VALUE self)
{
    int meta;
    int key = mgetch_nonblock(&meta);
    
    VALUE result = rb_ary_new();
    rb_ary_push(result, INT2NUM(meta));
    rb_ary_push(result, INT2NUM(key));
    
    return result;
}

VALUE mf_mis_curses(VALUE self)
{
    int result = mis_curses();

    return result ? Qtrue:Qfalse;
}

VALUE mf_dir_move(VALUE self, VALUE dir, VALUE rdir)
{
    Check_Type(dir, T_FIXNUM);
    Check_Type(rdir, T_STRING);

    if(NUM2INT(dir) == 0) {
        gLDir->Move(RSTRING(rdir)->ptr);
    }
    else {
        gRDir->Move(RSTRING(rdir)->ptr);
    }

    return Qnil;
}

VALUE mf_set_sort_kind(VALUE self, VALUE dir_kind, VALUE sort_kind)
{
    Check_Type(dir_kind, T_FIXNUM);
    Check_Type(sort_kind, T_STRING);

    cDirWnd::eSortKind kind = cDirWnd::kNone;

    for(int i=0; i<cDirWnd::kSortMax; i++) {
        if(strcmp(RSTRING(sort_kind)->ptr, cDirWnd::kSortName[i]) == 0) {
            kind = (cDirWnd::eSortKind)i;
        }
    }

    if(NUM2INT(dir_kind) == 0) {
        gLDir->mSortKind = kind;
    }
    else {
        gRDir->mSortKind = kind;
    }

    return Qnil;
}

VALUE mf_sort_kind(VALUE self, VALUE dir_kind)
{
    Check_Type(dir_kind, T_FIXNUM);
    if(NUM2INT(dir_kind) == 0) {
        return rb_str_new2(cDirWnd::kSortName[gLDir->mSortKind]);
    }
    else {
        return rb_str_new2(cDirWnd::kSortName[gRDir->mSortKind]);
    }
}

VALUE mf_sort(VALUE self, VALUE dir)
{
    Check_Type(dir, T_FIXNUM);

    if(NUM2INT(dir) == 0) {
        gLDir->Sort();
    }
    else {
        gRDir->Sort();
    }

    return Qnil;
}


VALUE mf_adir(VALUE self)
{
    if(gLDir->mActive) {
        return INT2NUM(0);
    }
    else {
        return INT2NUM(1);
    }
}

VALUE mf_sdir(VALUE self)
{
    if(gLDir->mActive) {
        return INT2NUM(1);
    }
    else {
        return INT2NUM(0);
    }
}


VALUE mf_set_view_nameonly(VALUE self, VALUE dir_kind, VALUE value)
{
    Check_Type(dir_kind, T_FIXNUM);

    if(NUM2INT(dir_kind) == 0) {
        gLDir->mViewNameOnly = (value == Qtrue)? true: false;
    }
    else {
        gRDir->mViewNameOnly = (value == Qtrue)? true: false;
    }

    return Qnil;
}

VALUE mf_view_nameonly(VALUE self, VALUE dir_kind)
{
    Check_Type(dir_kind, T_FIXNUM);

    if(NUM2INT(dir_kind) == 0) {
        return gLDir->mViewNameOnly ? Qtrue:Qfalse;
    }
    else {
        return gRDir->mViewNameOnly ? Qtrue:Qfalse;
    }
}

VALUE mf_set_view_focusback(VALUE self, VALUE dir_kind, VALUE value)
{
    Check_Type(dir_kind, T_FIXNUM);

    if(NUM2INT(dir_kind) == 0) {
        gLDir->mViewFocusBack = (value == Qtrue) ? true : false;
    }
    else {
        gRDir->mViewFocusBack = (value == Qtrue) ? true : false;
    }

    return Qnil;
}

VALUE mf_view_focusback(VALUE self, VALUE dir_kind)
{
    Check_Type(dir_kind, T_FIXNUM);

    if(NUM2INT(dir_kind) == 0) {
        return gLDir->mViewFocusBack ? Qtrue:Qfalse;
    }
    else {
        return gRDir->mViewFocusBack ? Qtrue:Qfalse;
    }

}

VALUE mf_set_view_removedir(VALUE self, VALUE dir_kind, VALUE value)
{
    Check_Type(dir_kind, T_FIXNUM);

    if(NUM2INT(dir_kind) == 0) {
        gLDir->mViewRemoveDir = (value == Qtrue) ? true : false;
    }
    else {
        gRDir->mViewRemoveDir = (value == Qtrue) ? true : false;
    }

    return Qnil;
}

VALUE mf_view_removedir(VALUE self, VALUE dir_kind)
{
    Check_Type(dir_kind, T_FIXNUM);

    if(NUM2INT(dir_kind) == 0) {
        return gLDir->mViewRemoveDir ? Qtrue:Qfalse;
    }
    else {
        return gRDir->mViewRemoveDir ? Qtrue:Qfalse;
    }

}

VALUE mf_view_filer(VALUE self)
{
    switch(cDirWnd::gViewOption) {
    case cDirWnd::kAll:
        return rb_str_new2("all");

    case cDirWnd::kOneDir:
        return rb_str_new2("one_dir");

    case cDirWnd::kOneDir2:
        return rb_str_new2("one_dir2");

    case cDirWnd::kOneDir3:
        return rb_str_new2("one_dir3");

    case cDirWnd::kOneDir5:
        return rb_str_new2("one_dir5");
    }
}

VALUE mf_set_view_filer(VALUE self, VALUE option)
{
    Check_Type(option, T_STRING);

    if(strcmp(RSTRING(option)->ptr, "all") == 0) {
        cDirWnd::gViewOption = cDirWnd::kAll;

        int cursor = ActiveDir()->Cursor();
        int cursor2 = SleepDir()->Cursor();
        ActiveDir()->MoveCursor(0);
        ActiveDir()->MoveCursor(cursor);
        SleepDir()->MoveCursor(0);
        SleepDir()->MoveCursor(cursor2);
    }
    else if(strcmp(RSTRING(option)->ptr, "one_dir") == 0) {
        cDirWnd::gViewOption = cDirWnd::kOneDir;

        int cursor = ActiveDir()->Cursor();
        
        ActiveDir()->MoveCursor(0);
        ActiveDir()->MoveCursor(cursor);
    }
    else if(strcmp(RSTRING(option)->ptr, "one_dir2") == 0) {
        cDirWnd::gViewOption = cDirWnd::kOneDir2;

        int cursor = ActiveDir()->Cursor();
        
        ActiveDir()->MoveCursor(0);
        ActiveDir()->MoveCursor(cursor);
    }
    else if(strcmp(RSTRING(option)->ptr, "one_dir3") == 0) {
        cDirWnd::gViewOption = cDirWnd::kOneDir3;

        int cursor = ActiveDir()->Cursor();
        
        ActiveDir()->MoveCursor(0);
        ActiveDir()->MoveCursor(cursor);
    }
    else if(strcmp(RSTRING(option)->ptr, "one_dir5") == 0) {
        cDirWnd::gViewOption = cDirWnd::kOneDir5;

        int cursor = ActiveDir()->Cursor();
        
        ActiveDir()->MoveCursor(0);
        ActiveDir()->MoveCursor(cursor);
    }

    return Qnil;
}

VALUE mf_set_view_color(VALUE self, VALUE boolean)
{
    gColor = (boolean == Qtrue);

    return Qnil;
}

VALUE mf_view_color(VALUE self)
{
    return gColor ? Qtrue : Qfalse;
}

VALUE mf_set_view_size(VALUE self, VALUE boolean)
{
    cDirWnd::gViewSize = (boolean == Qtrue);

    return Qnil;
}

VALUE mf_view_size(VALUE self)
{
    return cDirWnd::gViewSize ? Qtrue : Qfalse;
}

VALUE mf_set_view_group(VALUE self, VALUE boolean)
{
    cDirWnd::gViewGroup = (boolean == Qtrue);

    return Qnil;
}

VALUE mf_view_group(VALUE self)
{
    return cDirWnd::gViewGroup ? Qtrue : Qfalse;
}

VALUE mf_set_view_user(VALUE self, VALUE boolean)
{
    cDirWnd::gViewOwner = (boolean == Qtrue);

    return Qnil;
}

VALUE mf_view_user(VALUE self)
{
    return cDirWnd::gViewOwner ? Qtrue : Qfalse;
}

VALUE mf_set_view_nlink(VALUE self, VALUE boolean)
{
    cDirWnd::gViewNLink = (boolean == Qtrue);

    return Qnil;
}

VALUE mf_view_nlink(VALUE self)
{
    return cDirWnd::gViewNLink ? Qtrue : Qfalse;
}

VALUE mf_set_view_permission(VALUE self, VALUE boolean)
{
    cDirWnd::gViewPermission = (boolean == Qtrue);

    return Qnil;
}

VALUE mf_view_permission(VALUE self)
{
    return cDirWnd::gViewPermission ? Qtrue : Qfalse;
}

VALUE mf_set_view_mtime(VALUE self, VALUE boolean)
{
    cDirWnd::gViewMTime = (boolean == Qtrue);

    return Qnil;
}

VALUE mf_view_mtime(VALUE self)
{
    return cDirWnd::gViewMTime ? Qtrue : Qfalse;
}

VALUE mf_set_view_color_mark(VALUE self, VALUE color)
{
    Check_Type(color, T_FIXNUM);

    gColorMark = NUM2INT(color);

    return Qnil;
}

VALUE mf_set_view_color_exe(VALUE self, VALUE color)
{
    Check_Type(color, T_FIXNUM);

    gColorExe = NUM2INT(color);
    

    return Qnil;
}

VALUE mf_set_view_color_dir(VALUE self, VALUE color)
{
    Check_Type(color, T_FIXNUM);

    gColorDir = NUM2INT(color);
    

    return Qnil;
}

VALUE mf_set_view_color_link(VALUE self, VALUE color)
{
    Check_Type(color, T_FIXNUM);

    gColorLink = NUM2INT(color);
    

    return Qnil;
}

VALUE mf_set_view_bold_exe(VALUE self, VALUE boolean)
{
    gBoldExe = (boolean == Qtrue);

    return Qnil;
}

VALUE mf_set_view_bold_dir(VALUE self, VALUE boolean)
{
    gBoldDir = (boolean == Qtrue);

    return Qnil;
}

VALUE mf_set_cmdline_escape_key(VALUE self, VALUE boolean)
{
    gCmdLineEscapeKey = (boolean == Qtrue);

    return Qnil;
}

VALUE mf_cmdline_escape_key(VALUE self)
{
    return gCmdLineEscapeKey ? Qtrue : Qfalse;
}

VALUE mf_set_escape_wait(VALUE self, VALUE value)
{
    Check_Type(value, T_FIXNUM);

    gKeyEscapeWait = NUM2INT(value);

    return Qnil;
}

VALUE mf_kanjicode_filename(VALUE self)
{
    if(gKanjiCodeFileName == kSjis){
        return rb_str_new2("sjis");
    }
    else if(gKanjiCodeFileName == kUtf8) {
        return rb_str_new2("utf8");
    }
    else if(gKanjiCodeFileName == kUtf8Mac) {
        return rb_str_new2("utf8-mac");
    }
    else if(gKanjiCodeFileName == kEucjp) {
        return rb_str_new2("eucjp");
    }
    else {
        return rb_str_new2("unknown");
    }
}

VALUE mf_set_kanjicode_filename(VALUE self, VALUE name)
{
    Check_Type(name, T_STRING);

    if(strcmp(RSTRING(name)->ptr, "sjis") == 0){
        gKanjiCodeFileName = kSjis;
    }
    else if(strcmp(RSTRING(name)->ptr, "utf8") == 0) {
        gKanjiCodeFileName = kUtf8;
    }
    else if(strcmp(RSTRING(name)->ptr, "utf8-mac") == 0) {
        gKanjiCodeFileName = kUtf8Mac;
    }
    else if(strcmp(RSTRING(name)->ptr, "eucjp") == 0) {
        gKanjiCodeFileName = kEucjp;
    }
    else if(strcmp(RSTRING(name)->ptr, "unknown") == 0) {
        gKanjiCodeFileName = kUnknown;
    }

    //isearch_reload_migemo_dict();

    return Qnil;
}

VALUE mf_kanjicode(VALUE self)
{
    if(gKanjiCode == kSjis){
        return rb_str_new2("sjis");
    }
    else if(gKanjiCode == kUtf8) {
        return rb_str_new2("utf8");
    }
    else {
        return rb_str_new2("eucjp");
    }
}

VALUE mf_set_kanjicode(VALUE self, VALUE name)
{
    Check_Type(name, T_STRING);

    if(strcmp(RSTRING(name)->ptr, "sjis") == 0){
        gKanjiCode = kSjis;
    }
    else if(strcmp(RSTRING(name)->ptr, "utf8") == 0) {
        gKanjiCode = kUtf8;
    }
    else if(strcmp(RSTRING(name)->ptr, "eucjp") == 0) {
        gKanjiCode = kEucjp;
    }

    isearch_reload_migemo_dict();

    return Qnil;
}

VALUE mf_tab_max(VALUE self)
{
    return INT2NUM(cDirWnd::TabMax());
}

VALUE mf_tab_new(VALUE self, VALUE num, VALUE path, VALUE scrolltop, VALUE cursor)
{
    Check_Type(num, T_FIXNUM);
    Check_Type(path, T_STRING);
    Check_Type(scrolltop, T_FIXNUM);
    Check_Type(cursor, T_FIXNUM);
    
    cDirWnd::TabNew(NUM2INT(num), RSTRING(path)->ptr, NUM2INT(scrolltop), NUM2INT(cursor));

    return Qnil;
}

VALUE mf_tab_select(VALUE self)
{
    ActiveDir()->SelectTab();

    return Qnil;
}

VALUE mf_tab_close(VALUE self, VALUE num)
{
    Check_Type(num, T_FIXNUM);

    cDirWnd::CloseTab(NUM2INT(num));

    return Qnil;
}

VALUE mf_tab_up(VALUE self, VALUE num)
{
    Check_Type(num, T_FIXNUM);

    ActiveDir()->UpTab(NUM2INT(num));

    return Qnil;
}

VALUE mf_tab_path(VALUE self, VALUE num)
{
    Check_Type(num, T_FIXNUM);

    char* path = cDirWnd::TabPath(NUM2INT(num));

    if(path) 
        return rb_str_new2(path);
    else
        return Qnil;
}

VALUE mf_tab_scrolltop(VALUE self, VALUE num)
{
    Check_Type(num, T_FIXNUM);

    return INT2NUM(cDirWnd::TabScrollTop(NUM2INT(num)));
}

VALUE mf_tab_cursor(VALUE self, VALUE num)
{
    Check_Type(num, T_FIXNUM);

    return INT2NUM(cDirWnd::TabCursor(NUM2INT(num)));
}

VALUE mf_cursor_name(VALUE self)
{
    return rb_str_new2(ActiveDir()->CursorFile()->mName);
}

VALUE mf_cursor_name_convert(VALUE self)
{
    char buf[PATH_MAX];
#if defined(HAVE_ICONV_H) || defined(HAVE_BICONV_H)
    if(kanji_convert(ActiveDir()->CursorFile()->mName, buf, PATH_MAX, gKanjiCodeFileName, gKanjiCode) == -1) {
        strcpy(buf, ActiveDir()->CursorFile()->mName);
    }
#else
    strcpy(buf, ActiveDir()->CursorFile()->mName);
#endif
    return rb_str_new2(buf);
}

VALUE mf_cursor_path(VALUE self)
{
    char buf[PATH_MAX];
    sprintf(buf, "%s%s", ActiveDir()->Path(), ActiveDir()->CursorFile()->mName);
    
    return rb_str_new2(buf);
}

VALUE mf_cursor_path_convert(VALUE self)
{
    char buf[PATH_MAX];
    char buf2[PATH_MAX];
    sprintf(buf, "%s%s", ActiveDir()->Path(), ActiveDir()->CursorFile()->mName);
    
#if defined(HAVE_ICONV_H) || defined(HAVE_BICONV_H)
    if(kanji_convert(buf, buf2, PATH_MAX, gKanjiCodeFileName, gKanjiCode) == -1) {
        strcpy(buf2, buf);
    }
#else
    strcpy(buf2, buf);
#endif

    return rb_str_new2(buf2);
}

VALUE mf_cursor_ext(VALUE self)
{
    char* tmp = extname(ActiveDir()->CursorFile()->mName);
    VALUE result = rb_str_new2(tmp);
    FREE(tmp);

    return result;
}

VALUE mf_cursor_ext_convert(VALUE self)
{
    char buf[PATH_MAX];
    char* tmp = extname(ActiveDir()->CursorFile()->mName);
#if defined(HAVE_ICONV_H) || defined(HAVE_BICONV_H)
    VALUE result;
    if(kanji_convert(tmp, buf, PATH_MAX, gKanjiCodeFileName, gKanjiCode) == -1) {
        result = rb_str_new2(tmp);
    }
    else {
        result = rb_str_new2(buf);
    }
#else
    VALUE result = rb_str_new2(tmp);
#endif

    FREE(tmp);

    return result;
}

VALUE mf_cursor_noext(VALUE self)
{
    char* tmp = noextname(ActiveDir()->CursorFile()->mName);
    VALUE result = rb_str_new2(tmp);
    FREE(tmp);

    return result;
}

VALUE mf_cursor_noext_convert(VALUE self)
{
    char buf[PATH_MAX];
    char* tmp = noextname(ActiveDir()->CursorFile()->mName);
#if defined(HAVE_ICONV_H) || defined(HAVE_BICONV_H)
    VALUE result;
    if(kanji_convert(tmp, buf, PATH_MAX, gKanjiCodeFileName, gKanjiCode) == -1) {
        result = rb_str_new2(tmp);
    }
    else {
        result = rb_str_new2(buf);
    }
#else
    VALUE result = rb_str_new2(tmp);
#endif

    FREE(tmp);

    return result;
}
VALUE mf_active_dir(VALUE self)
{
    char* tmp = STRDUP(ActiveDir()->Path());
    char* tmp2 = basename(tmp);
    FREE(tmp);
    return rb_str_new2(tmp2);
}

VALUE mf_active_dir_convert(VALUE self)
{
    char buf[PATH_MAX];
    char* tmp = STRDUP(ActiveDir()->Path());
    char* tmp2 = basename(tmp);

#if defined(HAVE_ICONV_H) || defined(HAVE_BICONV_H)
    VALUE result;
    if(kanji_convert(tmp2, buf, PATH_MAX, gKanjiCodeFileName, gKanjiCode) == -1) {
        result = rb_str_new2(tmp2);
    }
    else {
        result = rb_str_new2(buf);
    }
#else
    VALUE result = rb_str_new2(tmp2);
#endif

    FREE(tmp);
    return result;
}

VALUE mf_sleep_dir(VALUE self)
{
    char buf[PATH_MAX];
    char* tmp = STRDUP(SleepDir()->Path());
    char* tmp2 = basename(tmp);
    VALUE result = rb_str_new2(tmp2);
    FREE(tmp);
    return result;
}

VALUE mf_sleep_dir_convert(VALUE self)
{
    char buf[PATH_MAX];
    char* tmp = STRDUP(SleepDir()->Path());
    char* tmp2 = basename(tmp);
#if defined(HAVE_ICONV_H) || defined(HAVE_BICONV_H)
    VALUE result;
    if(kanji_convert(tmp2, buf, PATH_MAX, gKanjiCodeFileName, gKanjiCode) == -1) {
        result = rb_str_new2(tmp2);
    }
    else {
        result = rb_str_new2(buf);
    }
#else
    VALUE result = rb_str_new2(tmp2);
#endif
    FREE(tmp);
    return result;
}

VALUE mf_left_dir(VALUE self)
{
    char buf[PATH_MAX];
    char* tmp = STRDUP(gLDir->Path());
    char* tmp2 = basename(tmp);
    VALUE result = rb_str_new2(tmp2);
    FREE(tmp);
    return result;
}

VALUE mf_left_dir_convert(VALUE self)
{
    char buf[PATH_MAX];
    char* tmp = STRDUP(gLDir->Path());
    char* tmp2 = basename(tmp);
#if defined(HAVE_ICONV_H) || defined(HAVE_BICONV_H)
    VALUE result;
    if(kanji_convert(tmp2, buf, PATH_MAX, gKanjiCodeFileName, gKanjiCode) == -1) {
        result = rb_str_new2(tmp2);
    }
    else {
        result = rb_str_new2(buf);
    }
#else
    VALUE result = rb_str_new2(tmp2);
#endif
    FREE(tmp);
    return result;
}

VALUE mf_right_dir(VALUE self)
{
    char buf[PATH_MAX];
    char* tmp = STRDUP(gRDir->Path());
    char* tmp2 = basename(tmp);
    VALUE result = rb_str_new2(tmp2);
    FREE(tmp);
    return result;
}

VALUE mf_right_dir_convert(VALUE self)
{
    char buf[PATH_MAX];
    char* tmp = STRDUP(gRDir->Path());
    char* tmp2 = basename(tmp);
#if defined(HAVE_ICONV_H) || defined(HAVE_BICONV_H)
    VALUE result;
    if(kanji_convert(tmp2, buf, PATH_MAX, gKanjiCodeFileName, gKanjiCode) == -1) {
        result = rb_str_new2(tmp2);
    }
    else {
        result = rb_str_new2(buf);
    }
#else
    VALUE result = rb_str_new2(tmp2);
#endif
    FREE(tmp);
    return result;
}

VALUE mf_adir_path(VALUE self)
{
    char buf[PATH_MAX];
    strcpy(buf, ActiveDir()->Path());

    return rb_str_new2(buf);
}

VALUE mf_adir_path_convert(VALUE self)
{
    char buf[PATH_MAX];
#if defined(HAVE_ICONV_H) || defined(HAVE_BICONV_H)
    VALUE result;
    if(kanji_convert(ActiveDir()->Path(), buf, PATH_MAX, gKanjiCodeFileName, gKanjiCode) == -1) {
        result = rb_str_new2(ActiveDir()->Path());
    }
    else {
        result = rb_str_new2(buf);
    }
#else
    VALUE result = rb_str_new2(ActiveDir()->Path());
#endif

    return result;
}


VALUE mf_sdir_path(VALUE self)
{
    char buf[PATH_MAX];
    strcpy(buf, SleepDir()->Path());

    return rb_str_new2(buf);
}

VALUE mf_sdir_path_convert(VALUE self)
{
    char buf[PATH_MAX];
#if defined(HAVE_ICONV_H) || defined(HAVE_BICONV_H)
    VALUE result;
    if(kanji_convert(SleepDir()->Path(), buf, PATH_MAX, gKanjiCodeFileName, gKanjiCode) == -1) {
        result = rb_str_new2(SleepDir()->Path());
    }
    else {
        result = rb_str_new2(buf);
    }
#else
    VALUE result = rb_str_new2(SleepDir()->Path());
#endif

    return rb_str_new2(buf);
}

VALUE mf_ldir_path(VALUE self)
{
    char buf[PATH_MAX];
    strcpy(buf, gLDir->Path());

    return rb_str_new2(buf);
}

VALUE mf_ldir_path_convert(VALUE self)
{
    char buf[PATH_MAX];
#if defined(HAVE_ICONV_H) || defined(HAVE_BICONV_H)
    VALUE result;
    if(kanji_convert(gLDir->Path(), buf, PATH_MAX, gKanjiCodeFileName, gKanjiCode) == -1) {
        result = rb_str_new2(gLDir->Path());
    }
    else {
        result = rb_str_new2(buf);
    }
#else
    VALUE result = rb_str_new2(gLDir->Path());
#endif
    return rb_str_new2(buf);
}

VALUE mf_rdir_path(VALUE self)
{
    char buf[PATH_MAX];
    strcpy(buf, gRDir->Path());

    return rb_str_new2(buf);
}

VALUE mf_rdir_path_convert(VALUE self)
{
    char buf[PATH_MAX];
#if defined(HAVE_ICONV_H) || defined(HAVE_BICONV_H)
    VALUE result;
    if(kanji_convert(gRDir->Path(), buf, PATH_MAX, gKanjiCodeFileName, gKanjiCode) == -1) {
        result = rb_str_new2(gRDir->Path());
    }
    else {
        result = rb_str_new2(buf);
    }
#else
    VALUE result = rb_str_new2(gRDir->Path());
#endif

    return result;
}

VALUE mf_adir_mark(VALUE self)
{
    return ActiveDir()->MarkFiles();
}

VALUE mf_adir_mark_convert(VALUE self)
{
    return ActiveDir()->MarkFilesConvert();
}

VALUE mf_sdir_mark(VALUE self)
{
    return SleepDir()->MarkFiles();
}

VALUE mf_sdir_mark_convert(VALUE self)
{
    return SleepDir()->MarkFilesConvert();
}

VALUE mf_adir_mark_fullpath(VALUE self)
{
    return ActiveDir()->MarkFilesFullPath();
}

VALUE mf_adir_mark_fullpath_convert(VALUE self)
{
    return ActiveDir()->MarkFilesFullPathConvert();
}

VALUE mf_sdir_mark_fullpath(VALUE self)
{
    return SleepDir()->MarkFilesFullPath();
}

VALUE mf_sdir_mark_fullpath_convert(VALUE self)
{
    return SleepDir()->MarkFilesFullPathConvert();
}

VALUE mf_ldir_mark(VALUE self)
{
    return gLDir->MarkFiles();
}

VALUE mf_ldir_mark_convert(VALUE self)
{
    return gLDir->MarkFilesConvert();
}

VALUE mf_rdir_mark(VALUE self)
{
    return gRDir->MarkFiles();
}

VALUE mf_rdir_mark_convert(VALUE self)
{
    return gRDir->MarkFilesConvert();
}

VALUE mf_ldir_mark_fullpath(VALUE self)
{
    return gLDir->MarkFilesFullPath();
}

VALUE mf_ldir_mark_fullpath_convert(VALUE self)
{
    return gLDir->MarkFilesFullPathConvert();
}

VALUE mf_rdir_mark_fullpath(VALUE self)
{
    return gRDir->MarkFilesFullPath();
}

VALUE mf_rdir_mark_fullpath_convert(VALUE self)
{
    return gRDir->MarkFilesFullPathConvert();
}

VALUE mf_change_terminal_title(VALUE self)
{
    return gChangeTerminalTitle ? Qtrue: Qfalse;
}

VALUE mf_set_change_terminal_title(VALUE self, VALUE value)
{
    gChangeTerminalTitle = (value == Qtrue) ? true:false;

    return Qnil;
}

VALUE mf_change_sort(VALUE self)
{
    ActiveDir()->ChangeSort();
    return Qnil;
}

VALUE mf_set_isearch_option1(VALUE self, VALUE value)
{
    gISearchOption1 = (value == Qtrue) ? true:false;
    
    return Qnil;
}

VALUE mf_isearch_option1(VALUE self)
{
    return gISearchOption1 ? Qtrue : Qfalse;
}

VALUE mf_set_menu_scroll_cycle(VALUE self, VALUE value)
{
    gMenuScrollCycle = (value == Qtrue) ? true : false;

    return Qnil;
}

VALUE mf_menu_scroll_cycle(VALUE self)
{
    return gMenuScrollCycle ? Qtrue : Qfalse;
}


VALUE mf_set_view_fname_divide_ext(VALUE self, VALUE value)
{
    cDirWnd::gViewFnameDivideExt = (value == Qtrue) ? true : false;

    return Qnil;
}

VALUE mf_view_fname_divide_ext(VALUE self)
{
    return cDirWnd::gViewFnameDivideExt ? Qtrue : Qfalse;
}

VALUE mf_keycommand(VALUE self, VALUE rmeta, VALUE rkeycode, VALUE rextension, VALUE rcommand)
{
    Check_Type(rmeta, T_FIXNUM);
    Check_Type(rkeycode, T_FIXNUM);
    Check_Type(rextension, T_STRING);
    Check_Type(rcommand, T_STRING);

    int meta = NUM2INT(rmeta);
    int keycode = NUM2INT(rkeycode);
    char* extension = RSTRING(rextension)->ptr;
    char* command = RSTRING(rcommand)->ptr;

    hash_put(gKeyCommand[meta][keycode], extension, STRDUP(command));

                 
    return Qnil;
}

VALUE mf_exit(VALUE self)
{
    if(gCheckExit) {
        const char* str[] = {
            "Yes", "No"
        };

        int ret = select_str("exit ok?", (char**)str, 2, 1);

        if(ret == 1) return Qnil;
    }

    gMainLoop = false;

    
    return Qnil;
}


VALUE mf_cursor_move(VALUE self, VALUE dir, VALUE rvalue)
{
    Check_Type(dir, T_FIXNUM);
    Check_Type(rvalue, T_FIXNUM);
    
    if(NUM2INT(dir) == 0) {
        gLDir->MoveCursor(NUM2INT(rvalue));
    }
    else {
        gRDir->MoveCursor(NUM2INT(rvalue));
    }
    
    return Qnil;
}

VALUE mf_cursor_left(VALUE self)
{
    if(gRDir->mActive) gLDir->Activate(gRDir);
    
    return Qnil;
}

VALUE mf_cursor_right(VALUE self)
{
    if(gLDir->mActive) gRDir->Activate(gLDir);
    
    return Qnil;
}

VALUE mf_cursor_other(VALUE self)
{
    if(gLDir->mActive)
        gRDir->Activate(gLDir);
    else
        gLDir->Activate(gRDir); 
    
    return Qnil;
}

VALUE mf_version(VALUE self)
{
    return rb_str_new2(gVersion);
}

VALUE mf_mark_all(VALUE self, VALUE dir)
{
    Check_Type(dir, T_FIXNUM);

    if(NUM2INT(dir) == 0) {
        gLDir->MarkAll();
        if(gSortMarkFileUp == Qtrue) {
            gLDir->Sort();
        }
    }
    else {
        gRDir->MarkAll();
        if(gSortMarkFileUp == Qtrue) {
            gRDir->Sort();
        }
    }

    return Qnil;
}

VALUE mf_mark_all_files(VALUE self, VALUE dir)
{
    Check_Type(dir, T_FIXNUM);

    if(NUM2INT(dir) == 0) {
        gLDir->MarkAllFiles();
        if(gSortMarkFileUp == Qtrue) {
            gLDir->Sort();
        }
    }
    else {
        gRDir->MarkAllFiles();
        if(gSortMarkFileUp == Qtrue) {
            gRDir->Sort();
        }
    }

    return Qnil;
}

VALUE mf_mark_clear(VALUE self, VALUE dir)
{
    Check_Type(dir, T_FIXNUM);

    if(NUM2INT(dir) == 0) {
        gLDir->ResetMarks();
        if(gSortMarkFileUp == Qtrue) {
            gLDir->Sort();
        }
    }
    else {
        gRDir->ResetMarks();
        if(gSortMarkFileUp == Qtrue) {
            gRDir->Sort();
        }
    }

    return Qnil;
}

VALUE mf_mark(VALUE self, VALUE dir, VALUE num, VALUE value)
{
    Check_Type(dir, T_FIXNUM);
    Check_Type(num, T_FIXNUM);

    if(NUM2INT(dir) == 0) {
        if(value == Qtrue)
            gLDir->Mark3(NUM2INT(num));
        else
            gLDir->MarkOff(NUM2INT(num));
    }
    else {
        if(value == Qtrue)
            gRDir->Mark3(NUM2INT(num));
        else
            gRDir->MarkOff(NUM2INT(num));
    }
    
    return Qnil;
}

VALUE mf_is_marked(VALUE self, VALUE dir, VALUE num)
{
    Check_Type(dir, T_FIXNUM);
    Check_Type(num, T_FIXNUM);

    VALUE result = Qfalse;

    if(NUM2INT(dir) == 0) {
        sFile* file = gLDir->File(NUM2INT(num));
        if(file) {
            result = file->mMark ? Qtrue:Qfalse;
        }
    }
    else {
        sFile* file = gRDir->File(NUM2INT(num));
        if(file) {
            result = file->mMark ? Qtrue:Qfalse;
        }
    }
    
    return result;
}

VALUE mf_marking(VALUE self, VALUE dir)
{
    Check_Type(dir, T_FIXNUM);

    if(NUM2INT(dir) == 0) {
        return gLDir->Marking() ? Qtrue:Qfalse;
    }
    else {
        return gRDir->Marking() ? Qtrue:Qfalse;
    }
}

VALUE mf_cmdline_c(VALUE self, VALUE command, VALUE position, VALUE cmd)
{
    Check_Type(command, T_STRING);
    Check_Type(position, T_FIXNUM);
    //Check_Type(cmd, T_STRING);

    gCmdAfterCmdlineRun = cmd;

#if defined(HAVE_ICONV_H) || defined(HAVE_BICONV_H)    
    if(gKanjiCodeFileName == kUnknown) {
        gCmdLineEncode = kanji_encode_type(ActiveDir()->CursorFile()->mName);
        cmdline_start(RSTRING(command)->ptr, NUM2INT(position));
    }
    else {
        gCmdLineEncode = gKanjiCodeFileName;
        cmdline_start(RSTRING(command)->ptr, NUM2INT(position));
    }
#else
    gCmdLineEncode = gKanjiCodeFileName;
    cmdline_start(RSTRING(command)->ptr, NUM2INT(position));
#endif
    
    return Qnil;
}

VALUE mf_cmdline_convert(VALUE self, VALUE command, VALUE position, VALUE encode, VALUE cmd)
{
    Check_Type(command, T_STRING);
    Check_Type(position, T_FIXNUM);
    Check_Type(encode, T_STRING);
    //Check_Type(cmd, T_STRING);

    gCmdAfterCmdlineRun = cmd;
    
    if(strcmp(RSTRING(encode)->ptr, "eucjp") == 0) {
        gCmdLineEncode = kEucjp;
        cmdline_start(RSTRING(command)->ptr, NUM2INT(position));
    }
    else if(strcmp(RSTRING(encode)->ptr, "sjis") == 0) {
        gCmdLineEncode = kSjis;
        cmdline_start(RSTRING(command)->ptr, NUM2INT(position));
    }
    else if(strcmp(RSTRING(encode)->ptr, "utf8") == 0) {
        gCmdLineEncode = kUtf8;
        cmdline_start(RSTRING(command)->ptr, NUM2INT(position));
    }
    else if(strcmp(RSTRING(encode)->ptr, "utf8-mac") == 0) {
        gCmdLineEncode = kUtf8Mac;
        cmdline_start(RSTRING(command)->ptr, NUM2INT(position));
    }
    else {
        gCmdLineEncode = kUnknown;
        cmdline_start(RSTRING(command)->ptr, NUM2INT(position));
    }
    
    return Qnil;
}

VALUE mf_cmdline_noconvert(VALUE self, VALUE command, VALUE position, VALUE cmd)
{
    Check_Type(command, T_STRING);
    Check_Type(position, T_FIXNUM);
    //Check_Type(cmd, T_STRING);
    
    gCmdAfterCmdlineRun = cmd;
    gCmdLineEncode = gKanjiCodeFileName;
    cmdline_start(RSTRING(command)->ptr, NUM2INT(position));
    
    return Qnil;
}

VALUE mf_shell(VALUE self, VALUE cmd, VALUE title)
{
    Check_Type(cmd, T_STRING);
    Check_Type(title, T_STRING);
    //Check_Type(cmd2, T_STRING);

    gCmdLineEncode = gKanjiCodeFileName;
    gCmdAfterCmdlineRun = Qnil;
    cmdline_run(RSTRING(cmd)->ptr, RSTRING(title)->ptr);

    return Qnil;
}

VALUE mf_defmenu(int argc, VALUE* argv, VALUE self)
{
    /// check arguments ///
    if(argc%3 != 1) {
        rb_raise(rb_eArgError, "wrong argument in mf_menu");
    }

    /// entry menu ///
    char* menu_name = RSTRING(argv[0])->ptr;
    
    if(hash_item(gMenu, menu_name)) {
        cMenu* menu = (cMenu*)hash_item(gMenu, menu_name);
        delete menu;
        //rb_raise(rb_eArgError, "already entried menu \"%s\"", menu_name);
    }

    cMenu* new_menu = new cMenu((char*)menu_name);

    for(int i=1; i<argc; i+=3) {
        Check_Type(argv[i], T_STRING);
        Check_Type(argv[i+1], T_FIXNUM);
        Check_Type(argv[i+2], T_STRING);

        new_menu->Append(RSTRING(argv[i])->ptr, NUM2INT(argv[i+1]), RSTRING(argv[i+2])->ptr);
    }
    
    hash_put(gMenu, menu_name, new_menu);

    return Qnil;
}

VALUE mf_menu(VALUE self, VALUE menu_name)
{
    Check_Type(menu_name, T_STRING);

    gActiveMenu = (cMenu*)hash_item(gMenu, RSTRING(menu_name)->ptr);
    if(gActiveMenu == NULL) {
        gErrMsgCancel = false; 
        err_msg("not found menu name(%s)", RSTRING(menu_name)->ptr);
        gActiveMenu = NULL;
        return Qnil;
    }
    gActiveMenu->Show();

    return Qnil;
}

VALUE mf_set_mask(VALUE self, VALUE dir, VALUE mask)
{
    Check_Type(mask, T_STRING);
    Check_Type(dir, T_FIXNUM);

    if(NUM2INT(dir) == 0) 
        gLDir->ChangeMask(RSTRING(mask)->ptr);
    else
        gRDir->ChangeMask(RSTRING(mask)->ptr);
    

    return Qnil;
}

VALUE mf_refresh(VALUE self)
{
    gLDir->Reread();
    gRDir->Reread(); 
   
    mclear_immediately();
    
    return Qnil;
}


VALUE mf_reread(VALUE self, VALUE dir)
{
    Check_Type(dir, T_FIXNUM);

    if(NUM2INT(dir) == 0)
        gLDir->Reread();
    else
        gRDir->Reread();
    
    return Qnil;
}


VALUE mf_malias(VALUE self, VALUE alias, VALUE arg_num, VALUE command)
{

    Check_Type(alias, T_STRING);
    Check_Type(arg_num, T_FIXNUM);
    Check_Type(command, T_STRING);

    cmdline_alias_add(RSTRING(alias)->ptr, NUM2INT(arg_num), RSTRING(command)->ptr);
    

    
    return Qnil;
}

VALUE mf_rehash(VALUE self)
{
    cmdline_rehash();
    return Qnil;
}

VALUE mf_help(VALUE self)
{
    help_start();
    
    return Qnil;
}

VALUE mf_set_xterm(VALUE self, VALUE xterm, VALUE opt_title, VALUE opt_eval, VALUE opt_extra, VALUE xterm_type)
{
    Check_Type(xterm, T_STRING);
    Check_Type(opt_title, T_STRING);
    Check_Type(opt_eval, T_STRING);
    Check_Type(opt_extra, T_STRING);
    Check_Type(xterm_type, T_FIXNUM);

    gXtermPrgName = xterm;
    gXtermOptTitle = opt_title;
    gXtermOptEval = opt_eval;
    gXtermOptExtra = opt_extra;
    gXtermType = xterm_type;
    
    return Qnil;
}

VALUE mf_mask(VALUE self, VALUE dir)
{
    Check_Type(dir, T_FIXNUM);

    if(NUM2INT(dir) == 0){
        return rb_str_new2(gLDir->Mask());
    }
    else {
        return rb_str_new2(gRDir->Mask());
    }
}

VALUE mf_cursor(VALUE self, VALUE dir)
{
    Check_Type(dir, T_FIXNUM);

    if(NUM2INT(dir) == 0) {
        return INT2FIX(gLDir->Cursor());
    }
    else {
        return INT2FIX(gRDir->Cursor());
    }
}

VALUE mf_file_name(VALUE self, VALUE dir, VALUE num)
{
    Check_Type(dir, T_FIXNUM);
    Check_Type(num, T_FIXNUM);

    if(NUM2INT(dir) == 0) {
        sFile* file = gLDir->File(NUM2INT(num));
        if(file) {
            return rb_str_new2(file->mName);
        }
        else {
            return Qnil;
        }
    }
    else {
        sFile* file = gRDir->File(NUM2INT(num));
        if(file) {
            return rb_str_new2(file->mName);
        }
        else {
            return Qnil;
        }
    }
}

VALUE mf_file_num(VALUE self, VALUE dir, VALUE fname)
{
    Check_Type(dir, T_FIXNUM);
    Check_Type(fname, T_STRING);
    
    if(NUM2INT(dir) == 0) {
        return INT2FIX(gLDir->FileNum(RSTRING(fname)->ptr));
    }
    else {
        return INT2FIX(gRDir->FileNum(RSTRING(fname)->ptr));
    }
}


VALUE mf_cursor_max(VALUE self, VALUE dir)
{
    Check_Type(dir, T_FIXNUM);

    if(NUM2INT(dir) == 0) {
        return INT2FIX(gLDir->CursorMax());
    }
    else {
        return INT2FIX(gRDir->CursorMax());
    }
}

VALUE mf_scrolltop(VALUE self, VALUE dir)
{
    Check_Type(dir, T_FIXNUM);

    if(NUM2INT(dir) == 0) {
        return INT2FIX(gLDir->ScrollTop());
    }
    else {
        return INT2FIX(gRDir->ScrollTop());
    }
}

VALUE mf_set_scrolltop(VALUE self, VALUE dir, VALUE value)
{
    Check_Type(dir, T_FIXNUM);
    Check_Type(value, T_FIXNUM);

    if(NUM2INT(dir) == 0) {
        gLDir->SetScrollTop(NUM2INT(value));
    }
    else {
        gRDir->SetScrollTop(NUM2INT(value));
    }
    
    return Qnil;
}

VALUE mf_cursor_x(VALUE self)
{
    return INT2FIX(ActiveDir()->CursorX());
}

VALUE mf_cursor_y(VALUE self)
{
    return INT2FIX(ActiveDir()->CursorY());
}

VALUE mf_cursor_maxx(VALUE self)
{
    return INT2FIX(ActiveDir()->CursorMaxX());
}

VALUE mf_cursor_maxy(VALUE self)
{
    return INT2FIX(ActiveDir()->CursorMaxY());
}

VALUE mf_mark_num(VALUE self, VALUE dir)
{
    Check_Type(dir, T_FIXNUM);

    if(NUM2INT(dir) == 0)
        return INT2NUM(gLDir->MarkFileNum());
    else
        return INT2NUM(gRDir->MarkFileNum());
}


VALUE mf_add_file(VALUE self, VALUE dir, VALUE fname)
{
    Check_Type(dir, T_FIXNUM);
    Check_Type(fname, T_STRING);
    
    if(NUM2INT(dir) == 0) {
        gLDir->AddFile(RSTRING(fname)->ptr);
    }
    else {
        gRDir->AddFile(RSTRING(fname)->ptr);
    }
    
    return Qnil;
}

VALUE mf_add_file2(VALUE self, VALUE dir, VALUE fname, VALUE linkto, VALUE kind, VALUE user_rwx, VALUE group_rwx, VALUE other_rwx, VALUE size, VALUE mtime, VALUE user, VALUE group)
{
    Check_Type(dir, T_FIXNUM);

    Check_Type(fname, T_STRING);
    Check_Type(linkto, T_STRING);
    Check_Type(kind, T_FIXNUM);
    Check_Type(size, T_FIXNUM);
//    Check_Type(mtime, T_BIGNUM);
    Check_Type(user, T_STRING);
    Check_Type(user_rwx, T_ARRAY);
    Check_Type(group_rwx, T_ARRAY);
    Check_Type(other_rwx, T_ARRAY);
    Check_Type(group, T_STRING);

    VALUE usr_r = rb_ary_entry(user_rwx, 0);
    VALUE usr_w = rb_ary_entry(user_rwx, 1);
    VALUE usr_x = rb_ary_entry(user_rwx, 2);

    VALUE group_r = rb_ary_entry(group_rwx, 0);
    VALUE group_w = rb_ary_entry(group_rwx, 1);
    VALUE group_x = rb_ary_entry(group_rwx, 2);

    VALUE other_r = rb_ary_entry(other_rwx, 0);
    VALUE other_w = rb_ary_entry(other_rwx, 1);
    VALUE other_x = rb_ary_entry(other_rwx, 2);

    if(NUM2INT(dir) == 0)
        gLDir->AddFile2(RSTRING(fname)->ptr, RSTRING(linkto)->ptr, NUM2INT(kind), (usr_r == Qtrue) ? true: false,(usr_w == Qtrue) ? true: false,(usr_x == Qtrue) ? true: false,(group_r == Qtrue) ? true: false,(group_w == Qtrue) ? true: false,(group_x == Qtrue) ? true: false,(other_r == Qtrue) ? true: false,(other_w == Qtrue) ? true: false,(other_x == Qtrue) ? true: false, NUM2INT(size), NUM2LONG(mtime), RSTRING(user)->ptr, RSTRING(group)->ptr);
    else
        gRDir->AddFile2(RSTRING(fname)->ptr, RSTRING(linkto)->ptr, NUM2INT(kind), (usr_r == Qtrue) ? true: false,(usr_w == Qtrue) ? true: false,(usr_x == Qtrue) ? true: false,(group_r == Qtrue) ? true: false,(group_w == Qtrue) ? true: false,(group_x == Qtrue) ? true: false,(other_r == Qtrue) ? true: false,(other_w == Qtrue) ? true: false,(other_x == Qtrue) ? true: false, NUM2INT(size), NUM2LONG(mtime), RSTRING(user)->ptr, RSTRING(group)->ptr);


    return Qnil;
}

VALUE mf_sisearch(VALUE self, VALUE b)
{
    gSISearch = (b == Qtrue) ? true:false;

    if(gSISearch) {
        if(!sisearch_cache_exist()) {
            msg("super incremental search cache don't exist. pleas make cache with $ menu item(super isearch get cache)");
            gSISearch = false;
        }
        else {
            sisearch_start();
        }
    }

    return Qnil;
}

VALUE mf_sisearch_get_cache(VALUE self)
{
    sisearch_get_cache();

    return Qnil;
}

VALUE mf_sisearch_candidate_max_ascii(VALUE self, VALUE v)
{
    Check_Type(v, T_FIXNUM);

    gCandiateMaxAscii = NUM2INT(v);

    return Qnil;
}

VALUE mf_sisearch_candidate_max_migemo(VALUE self, VALUE v)
{
    Check_Type(v, T_FIXNUM);

    gCandidateMaxMigemo = NUM2INT(v);

    return Qnil;
}

VALUE mf_sisearch_migemo(VALUE self, VALUE v)
{
    gSISearchMigemo = (v == Qtrue)? true:false;

    return Qnil;
}









































VALUE mf_input_box(VALUE self, VALUE msg, VALUE def_input, VALUE def_cursor)
{
    Check_Type(msg, T_STRING);
    Check_Type(def_input, T_STRING);
    Check_Type(def_cursor, T_FIXNUM);
    
    if(mis_curses()) {
        char ret[1024];
        int ret2 = input_box(RSTRING(msg)->ptr, ret, RSTRING(def_input)->ptr, NUM2INT(def_cursor));
        
        VALUE result = rb_ary_new();
        rb_ary_push(result, INT2NUM(ret2));
        rb_ary_push(result, rb_str_new2(ret));
        
        return result;
    }
    else {
        printf("\ninput_box() needs curses. please add %%Q\n");
        
        return Qnil;
    }
}

VALUE mf_is_screen_terminal(VALUE self)
{
    char* term = getenv("TERM");
    if(term == NULL) {
        fprintf(stderr, "$TERM is null");
        exit(1);
    }

    bool flg_screen = strstr(term, "screen") == term || gGnuScreen;
    
    return flg_screen ? Qtrue:Qfalse;
}

VALUE mf_set_signal_clear(VALUE self)
{
    set_signal_clear();
    return Qnil;
}
VALUE mf_set_signal_mfiler2(VALUE self)
{
    set_signal_mfiler2();
    return Qnil;
}

VALUE mf_isearch(VALUE self)
{
    gISearchPartMatch = false;
    gISearch = !gISearch;

    return Qnil;
}

VALUE mf_is_isearch_on(VALUE self)
{
    if(gISearch)
        return Qtrue;
    else
        return Qfalse;
}

VALUE mf_isearch_off(VALUE self)
{
    gISearch = false;

    return Qnil;
}

VALUE mf_isearch_on(VALUE self)
{
    gISearch = true;

    return Qnil;
}

VALUE mf_isearch_explore(VALUE self, VALUE v)
{
    if(v == Qtrue) {
        gISearchExplore = true;
    }
    else {
        gISearchExplore = false;
    }
}

VALUE mf_is_isearch_explore(VALUE self)
{
    if(gISearchExplore)
        return Qtrue;
    else
        return Qfalse;
}

VALUE mf_isearch_partmatch(VALUE self)
{
    gISearchPartMatch = true;
    gISearch = true;

    gNoPartMatchClear = true;

    return Qnil;
}

VALUE mf_isearch_partmatch2(VALUE self, VALUE v)
{
    if(v == Qtrue) 
        gISearchPartMatch = true;
    else
        gISearchPartMatch = false;

    gNoPartMatchClear = true;

    return Qnil;
}

VALUE mf_is_isearch_partmatch2(VALUE self)
{
    if(gISearchPartMatch) 
        return Qtrue;
    else
        return Qfalse;

    return Qnil;
}

VALUE mf_option_remain_marks(VALUE self, VALUE boolean)
{
    cDirWnd::gRemainMarks = (boolean == Qtrue);

    return Qnil;
}

VALUE mf_option_refresh(VALUE self, VALUE boolean)
{
    gUnixDomainSocket= (boolean == Qtrue);

    return Qnil;
}

VALUE mf_is_remain_marks(VALUE self)
{
    return cDirWnd::gRemainMarks ? Qtrue: Qfalse;
}

VALUE mf_option_visible_fname_execute(VALUE self, VALUE boolean)
{
    gExecutiveFileBold = (boolean == Qtrue);

    return Qnil;
}


VALUE mf_option_individual_cursor(VALUE self, VALUE boolean)
{
    gIndividualCursor = (boolean == Qtrue);

    return Qnil;
}

VALUE mf_option_isearch_decision_way(VALUE self, VALUE way)
{
    return Qnil;
}

VALUE mf_option_shift_isearch(VALUE self, VALUE boolean)
{
    return Qnil;
}

VALUE mf_option_check_delete_file(VALUE self, VALUE boolean)
{
    gCheckDeleteFile = (boolean == Qtrue);

    return Qnil;
}

VALUE mf_option_check_copy_file(VALUE self, VALUE boolean)
{
    gCheckCopyFile = (boolean == Qtrue);

    return Qnil;
}

VALUE mf_option_check_exit(VALUE self, VALUE boolean)
{
    gCheckExit = (boolean == Qtrue);

    return Qnil;
}

VALUE mf_option_trashbox_path(VALUE self, VALUE name)
{
    Check_Type(name, T_STRING);

    strcpy(gTrashBoxPath, RSTRING(name)->ptr);

    return Qnil;
}

VALUE mf_encode_type(VALUE self, VALUE string)
{
    Check_Type(string, T_STRING);

#if defined(HAVE_ICONV_H) || defined(HAVE_BICONV_H)
    eKanjiCode code = kanji_encode_type(RSTRING(string)->ptr);

    if(code == kUtf8) {
        return rb_str_new2("utf8");
    }
    else if(code == kUtf8Mac) {
        return rb_str_new2("utf8-mac");
    }
    else if(code == kSjis) {
        return rb_str_new2("sjis");
    }
    else if(code == kEucjp) {
        return rb_str_new2("eucjp");
    }
    else if(code == kUnknown) {
        return rb_str_new2("unknown");
    }

    return Qnil;
#else
    return rb_str_new2("unknown");
#endif
}

VALUE mf_set_iconv_kanji_code_name(VALUE self, VALUE kanjicode, VALUE name)
{
    Check_Type(kanjicode, T_STRING);
    Check_Type(name, T_STRING);

    if(strcmp(RSTRING(kanjicode)->ptr, "sjis") == 0){
        strcpy(gKanjiCodeName[kSjis], RSTRING(name)->ptr);
    }
    else if(strcmp(RSTRING(kanjicode)->ptr, "utf8") == 0) {
        strcpy(gKanjiCodeName[kUtf8], RSTRING(name)->ptr);
    }
    else if(strcmp(RSTRING(kanjicode)->ptr, "utf8-mac") == 0) {
        strcpy(gKanjiCodeName[kUtf8Mac], RSTRING(name)->ptr);
    }
    else if(strcmp(RSTRING(kanjicode)->ptr, "eucjp") == 0) {
        strcpy(gKanjiCodeName[kEucjp], RSTRING(name)->ptr);
    }
}

VALUE mf_option_gnu_screen(VALUE self, VALUE boolean)
{
    gGnuScreen = (boolean == Qtrue);

    return Qnil;
}

VALUE mf_option_xterm(VALUE self, VALUE boolean)
{
    gXterm = (boolean == Qtrue);

    return Qnil;
}

VALUE mf_option_remain_cursor(VALUE self, VALUE boolean)
{
    gRemainCursor = (boolean == Qtrue);

    return Qnil;
}

VALUE mf_option_auto_rehash(VALUE self, VALUE boolean)
{
    gAutoRehash = (boolean == Qtrue);

    return Qnil;
}

VALUE mf_option_extension_icase(VALUE self, VALUE boolean)
{
    gExtensionICase = (boolean == Qtrue);

    return Qnil;
}

VALUE mf_option_read_dir_history(VALUE self, VALUE boolean)
{
    gReadDirHistory = (boolean == Qtrue);

    return Qnil;
}

VALUE mf_option_select(VALUE self, VALUE boolean)
{
    //gSelect = (boolean == Qtrue);

    return Qnil;
}


VALUE mf_dir_copy(VALUE self)
{
    ActiveDir()->Move(SleepDir()->Path());
    
    return Qnil;
}

VALUE mf_dir_exchange(VALUE self)
{
    cDirWnd* wnd = gLDir;
    gLDir = gRDir;
    gRDir = wnd;
    
    return Qnil;
}

VALUE mf_dir_back(VALUE self)
{
    ActiveDir()->MoveBack();

    return Qnil;
}

VALUE mf_sdir_back(VALUE self)
{
    SleepDir()->MoveBack();

    return Qnil;
}



static int gProgressMark = 0;

static void draw_progress_box(int mark_num)
{
    const int maxx = mgetmaxx();
    const int maxy = mgetmaxy();
    
    const int y = maxy/2;
    
    mbox(y, (maxx-22)/2, 22, 3);
    mmvprintw(y, (maxx-22)/2+2, "progress");
    mmvprintw(y+1, (maxx-22)/2+1, "(%d/%d) files", gProgressMark, mark_num);
}


VALUE mf_copy(VALUE self, VALUE sdir, VALUE files, VALUE ddir, VALUE sdir_is_adir)
{

    gCopyOverride = kNone;
    gErrMsgCancel = false;
    gWriteProtected = kWPNone;
    
    if(gCheckCopyFile) {
        const char* str[] = {
            "Yes", "No"
        };

        int ret = select_str("copy ok?", (char**)str, 2, 1);

        if(ret == 1) return Qnil;
    }

    int i = 0;

    Check_Type(sdir, T_STRING);
    Check_Type(files, T_ARRAY);
    Check_Type(ddir, T_STRING);
    
    /// check argument ///
    struct stat dstat;
    if(stat(RSTRING(ddir)->ptr, &dstat) >= 0) {
        if(S_ISDIR(dstat.st_mode) && RSTRING(ddir)->ptr[strlen(RSTRING(ddir)->ptr)-1] != '/') {
            char tmp[PATH_MAX];
            sprintf(tmp, "%s/", RSTRING(ddir)->ptr);

            ddir = rb_str_new2(tmp);
        }
    }
    
    if(RARRAY(files)->len > 1
        && RSTRING(ddir)->ptr[strlen(RSTRING(ddir)->ptr)-1] != '/')
    {
        err_msg("mf_copy: destination_err(%s)", RSTRING(ddir)->ptr);
        return Qnil;
    }
    
    char path2[PATH_MAX];
    if(!correct_path(ActiveDir()->Path(), RSTRING(ddir)->ptr, path2)) {
        err_msg("mf_move: destination_err(%s)", RSTRING(ddir)->ptr);
        return Qnil;
    }
    ddir = rb_str_new2(path2);

    /// go ///
    const int mark_file_num = RARRAY(files)->len;
    gProgressMark = mark_file_num;

    for(int i=0; i < RARRAY(files)->len; i++) {
        VALUE item = rb_ary_entry(files, i);
        Check_Type(item, T_STRING);

        if(strcmp(RSTRING(item)->ptr, ".") != 0
            && strcmp(RSTRING(item)->ptr, "..") != 0)
        {
            char spath[PATH_MAX];
            strcpy(spath, RSTRING(sdir)->ptr);
            if(RSTRING(sdir)->ptr[RSTRING(sdir)->len-1] != '/') {
                strcat(spath, "/");
            }
            strcat(spath, RSTRING(item)->ptr);
    
            char dpath[PATH_MAX];
            strcpy(dpath, RSTRING(ddir)->ptr);

            if(sdir_is_adir == Qtrue) {
                ActiveDir()->MoveCursor(ActiveDir()->FileNum(RSTRING(item)->ptr));
            }
            
            //mclear();
            view(false);
            draw_progress_box(mark_file_num);
            mrefresh();
            
            if(!file_copy(spath, dpath, false, false) ) {
                break;
            }

            gProgressMark--;
            
            if(sdir_is_adir == Qtrue) {
                ActiveDir()->MarkOff(RSTRING(item)->ptr);
            }
        }
    }
    
    mclear();
    view(false);
    mrefresh();
    
    gLDir->Reread();
    gRDir->Reread();
    
    //ActiveDir()->ResetMarks();

    gUndoOp = kUndoCopy;
    gUndoSdir = sdir;
    gUndoDdir = ddir;
    gUndoFiles = files;

    

    return Qnil;
}

VALUE mf_copy_preserve(VALUE self, VALUE sdir, VALUE files, VALUE ddir, VALUE sdir_is_adir)
{

    gCopyOverride = kNone;
    gErrMsgCancel = false;
    gWriteProtected = kWPNone;
    
    if(gCheckCopyFile) {
        const char* str[] = {
            "Yes", "No"
        };

        int ret = select_str("copy ok?", (char**)str, 2, 1);

        if(ret == 1) return Qnil;
    }

    int i = 0;

    Check_Type(sdir, T_STRING);
    Check_Type(files, T_ARRAY);
    Check_Type(ddir, T_STRING);
    
    /// check argument ///
    struct stat dstat;
    if(stat(RSTRING(ddir)->ptr, &dstat) >= 0) {
        if(S_ISDIR(dstat.st_mode) && RSTRING(ddir)->ptr[strlen(RSTRING(ddir)->ptr)-1] != '/') {
            char tmp[PATH_MAX];
            sprintf(tmp, "%s/", RSTRING(ddir)->ptr);

            ddir = rb_str_new2(tmp);
        }
    }
    
    if(RARRAY(files)->len > 1
        && RSTRING(ddir)->ptr[strlen(RSTRING(ddir)->ptr)-1] != '/')
    {
        err_msg("mf_copy: destination_err(%s)", RSTRING(ddir)->ptr);
        return Qnil;
    }
    
    char path2[PATH_MAX];
    if(!correct_path(ActiveDir()->Path(), RSTRING(ddir)->ptr, path2)) {
        err_msg("mf_move: destination_err(%s)", RSTRING(ddir)->ptr);
        return Qnil;
    }
    ddir = rb_str_new2(path2);

    /// go ///
    const int mark_file_num = RARRAY(files)->len;
    gProgressMark = mark_file_num;

    for(int i=0; i < RARRAY(files)->len; i++) {
        VALUE item = rb_ary_entry(files, i);
        Check_Type(item, T_STRING);

        if(strcmp(RSTRING(item)->ptr, ".") != 0
            && strcmp(RSTRING(item)->ptr, "..") != 0)
        {
            char spath[PATH_MAX];
            strcpy(spath, RSTRING(sdir)->ptr);
            if(RSTRING(sdir)->ptr[RSTRING(sdir)->len-1] != '/') {
                strcat(spath, "/");
            }
            strcat(spath, RSTRING(item)->ptr);
    
            char dpath[PATH_MAX];
            strcpy(dpath, RSTRING(ddir)->ptr);

            if(sdir_is_adir == Qtrue) {
                ActiveDir()->MoveCursor(ActiveDir()->FileNum(RSTRING(item)->ptr));
            }

            //mclear();
            view(false);
            draw_progress_box(mark_file_num);
            mrefresh();
            
            if(!file_copy(spath, dpath, false, true)) {
                break;
            }

            gProgressMark--;
            if(sdir_is_adir == Qtrue) {
                ActiveDir()->MarkOff(RSTRING(item)->ptr);
            }
        }
    }

    mclear();
    view(false);
    mrefresh();
    
    //ActiveDir()->ResetMarks();

    gUndoOp = kUndoCopy;
    gUndoSdir = sdir;
    gUndoDdir = ddir;
    gUndoFiles = files;

    

    return Qnil;
}

VALUE mf_move(VALUE self, VALUE sdir, VALUE files, VALUE ddir, VALUE sdir_is_adir)
{

    gCopyOverride = kNone;
    gErrMsgCancel = false;
    gWriteProtected = kWPNone;
    
    if(gCheckCopyFile) {
        const char* str[] = {
            "Yes", "No"
        };

        int ret = select_str("move ok?", (char**)str, 2, 1);

        if(ret == 1) return Qnil;
    }

    int i = 0;

    Check_Type(sdir, T_STRING);
    Check_Type(files, T_ARRAY);
    Check_Type(ddir, T_STRING);

    /// check argument ///
    struct stat dstat;
    if(stat(RSTRING(ddir)->ptr, &dstat) >= 0) {
        if(S_ISDIR(dstat.st_mode) && RSTRING(ddir)->ptr[strlen(RSTRING(ddir)->ptr)-1] != '/') {
            char tmp[PATH_MAX];
            sprintf(tmp, "%s/", RSTRING(ddir)->ptr);

            ddir = rb_str_new2(tmp);
        }
    }
    
    if(RARRAY(files)->len > 1
        && RSTRING(ddir)->ptr[strlen(RSTRING(ddir)->ptr)-1] != '/')
    {
        err_msg("mf_move: destination_err(%s)", RSTRING(ddir)->ptr);
        return Qnil;
    }
    
    char path2[PATH_MAX];
    if(!correct_path(ActiveDir()->Path(), RSTRING(ddir)->ptr, path2)) {
        err_msg("mf_move: destination_err(%s)", RSTRING(ddir)->ptr);
        return Qnil;
    }
    ddir = rb_str_new2(path2);
    
    /// go ///
    const int mark_file_num = RARRAY(files)->len;
    gProgressMark = mark_file_num;

    for(int i=0; i < RARRAY(files)->len; i++) {
        VALUE item = rb_ary_entry(files, i);

        Check_Type(item, T_STRING);

        if(strcmp(RSTRING(item)->ptr, ".") != 0
            && strcmp(RSTRING(item)->ptr, "..") != 0)
        {
            char spath[PATH_MAX];
            strcpy(spath, RSTRING(sdir)->ptr);
            if(RSTRING(sdir)->ptr[RSTRING(sdir)->len-1] != '/') {
                strcat(spath, "/");
            }
            strcat(spath, RSTRING(item)->ptr);

            char dpath[PATH_MAX];
            strcpy(dpath, RSTRING(ddir)->ptr);

            if(sdir_is_adir == Qtrue) {
                ActiveDir()->MoveCursor(ActiveDir()->FileNum(RSTRING(item)->ptr));
            }
            
            //mclear();
            view(false);
            draw_progress_box(mark_file_num);
            mrefresh();
            

            if(!file_copy(spath, dpath, true, true)) {
                break;
            }

            if(sdir_is_adir == Qtrue) {
                ActiveDir()->MarkOff(RSTRING(item)->ptr);
            }
            gProgressMark--;
        }
    }
    
    mclear();
    view(false);
    mrefresh();
    
    //ActiveDir()->ResetMarks();

    gUndoOp = kUndoMove;
    gUndoSdir = sdir;
    gUndoDdir = ddir;
    gUndoFiles = files;

    

    return Qnil;
}

VALUE mf_remove(VALUE self, VALUE sdir, VALUE files, VALUE sdir_is_adir)
{
    gCopyOverride = kNone;
    gWriteProtected = kWPNone;
    gErrMsgCancel = false;

    if(gCheckDeleteFile) {
        const char* str[] = {
            "Yes", "No"
        };

        int ret = select_str("delete ok?", (char**)str, 2, 1);

        if(ret == 1) return Qnil;
    }

    Check_Type(sdir, T_STRING);
    Check_Type(files, T_ARRAY);
    
    const int mark_file_num = RARRAY(files)->len;
    gProgressMark = mark_file_num;

    for(int i= 0; i < RARRAY(files)->len; i++) {
        VALUE item = rb_ary_entry(files, i);
        Check_Type(item, T_STRING);

        if(strcmp(RSTRING(item)->ptr, ".") != 0
            && strcmp(RSTRING(item)->ptr, "..") != 0)
        {
            char spath[PATH_MAX];
            strcpy(spath, RSTRING(sdir)->ptr);
            if(RSTRING(sdir)->ptr[RSTRING(sdir)->len-1] != '/') {
                strcat(spath, "/");
            }
            strcat(spath, RSTRING(item)->ptr);
            
            if(sdir_is_adir == Qtrue) {
                ActiveDir()->MoveCursor(ActiveDir()->FileNum(RSTRING(item)->ptr));
            }
            
            //mclear();
            view(false);
            draw_progress_box(mark_file_num);
            mrefresh();

            if(!file_remove(spath, false, true)) {
                break;
            }
            
            if(sdir_is_adir == Qtrue) {
                ActiveDir()->MarkOff(RSTRING(item)->ptr);
            }
            gProgressMark--;
        }
    }
    
    mclear();
    view(false);
    mrefresh();
    
    //ActiveDir()->ResetMarks();

    return Qnil;
}

VALUE mf_trashbox(VALUE self, VALUE sdir, VALUE files, VALUE sdir_is_adir)
{
    gErrMsgCancel = false;
    gCopyOverride = kYesAll;
    gWriteProtected = kWPYesAll;
    
    char* home = getenv("HOME");
    if(home == NULL) {
        err_msg("trashbox: $HOME is NULL");
        return Qnil;
    }

    char tbpath[PATH_MAX];
    strcpy(tbpath, gTrashBoxPath);

    
    if(gCheckDeleteFile) {
        const char* str[] = {
            "Yes", "No"
        };

        int ret;
        if(strcmp(RSTRING(sdir)->ptr, tbpath) == 0) {
            ret = select_str("delete ok?", (char**)str, 2, 1);
        }
        else {
            ret = select_str("move to the trashbox ok?", (char**)str, 2, 1);
        }

        if(ret == 1) return Qnil;
    }

    Check_Type(sdir, T_STRING);
    Check_Type(files, T_ARRAY);

    /// make mtrashbox ///
    if(access(tbpath, F_OK) == 0) {
        struct stat tbstat;
        if(stat(tbpath, &tbstat) < 0) {
            err_msg("trashbox: stat err(%s)", tbpath);
            return Qnil;
        }
        if(!S_ISDIR(tbstat.st_mode)) {
            char buf[256];
            sprintf(buf, "trashbox: %s is not directory", gTrashBoxPath);
            err_msg("%s", buf);
            return Qnil;
        }
    }
    else {
        if(mkdir(tbpath, 0755) < 0) {
            char buf[256];
            sprintf(buf, "trashbox: can't make %s", gTrashBoxPath);
            err_msg("%s", buf);
            return Qnil;
        }
    }

    strcat(tbpath, "/");

    
    const int mark_file_num = RARRAY(files)->len;
    gProgressMark = mark_file_num;

    for(int i=0; i < RARRAY(files)->len; i++) {
        VALUE item = rb_ary_entry(files, i);

        Check_Type(item, T_STRING);

        if(strcmp(RSTRING(item)->ptr, ".") != 0
            && strcmp(RSTRING(item)->ptr, "..") != 0)
        {
            char spath[PATH_MAX];
            strcpy(spath, RSTRING(sdir)->ptr);
            if(RSTRING(sdir)->ptr[RSTRING(sdir)->len-1] != '/') {
                strcat(spath, "/");
            }
            strcat(spath, RSTRING(item)->ptr);

            char dpath[PATH_MAX];
            strcpy(dpath, tbpath);
            strcat(dpath, RSTRING(item)->ptr);

            
            if(access(dpath, F_OK) == 0) {
                if(!file_remove(dpath, false, false)) {
                    break;
                }
            }
            
            if(sdir_is_adir == Qtrue) {
                ActiveDir()->MoveCursor(ActiveDir()->FileNum(RSTRING(item)->ptr));
            }
            
            //mclear();
            view(false);
            draw_progress_box(mark_file_num);
            mrefresh();
            
            if(!file_copy(spath, tbpath, true, true)) {
                break;
            }

            if(sdir_is_adir == Qtrue) {
                ActiveDir()->MarkOff(RSTRING(item)->ptr);
            }
            gProgressMark--;
        }
    }

    gUndoOp = kUndoTrashBox;
    gUndoSdir = sdir;
    gUndoDdir = Qnil;
    gUndoFiles = files;
    
    mclear();
    view(false);
    mrefresh();
    
//    ActiveDir()->ResetMarks();
    


    return Qnil;
}

VALUE mf_delete_file_from_file_list(VALUE self, VALUE dir, VALUE num)
{
    Check_Type(dir, T_FIXNUM);
    Check_Type(num, T_FIXNUM);

    if(NUM2INT(dir) == 0) {
        gLDir->DeleteFileFromFileList(NUM2INT(num));
    }
    else {
        gRDir->DeleteFileFromFileList(NUM2INT(num));
    }

    return Qnil;
}

const char kCut = 0;
const char kCopy = 1;

VALUE mf_cut(VALUE self)
{
    char buf[8192];
    char* p = buf;

    gErrMsgCancel = false;
    
    const int pid = getpid();
    memcpy(p, &pid, sizeof(pid));
    p += sizeof(pid);
    
    memcpy(p, &kCut, sizeof(kCut));
    p += sizeof(kCut);

    VALUE path2 = mf_adir_path(self);
    sprintf(p, "%s%c", RSTRING(path2)->ptr, 0);
    p += RSTRING(path2)->len+1;

    VALUE files = ActiveDir()->MarkFiles();

    const int n = RARRAY(files)->len;
    memcpy(p, &n, sizeof(n));
    p += sizeof(n);

    for(int i=0; i < RARRAY(files)->len; i++) {
        VALUE item = rb_ary_entry(files, i);
        Check_Type(item, T_STRING);

        sprintf(p, "%s%c", RSTRING(item)->ptr, 0);
        p += RSTRING(item)->len + 1;
    }

    /// write ///    
    char* home = getenv("HOME");
    if(home == NULL) {
        err_msg("cut: $HOME is null");
        return Qnil;
    }

    char path[1024];
    sprintf(path, "%s/.mfiler-cutpast", home);

    int file = open(path, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if(file < 0) {
        err_msg("cut: open err(%s)", path);
        return Qnil;
    }
    if(write(file, buf, p - buf) < 0) {
        err_msg("cut: write err");
        close(file);
        return Qnil;
    }
    close(file);
    
    ActiveDir()->ResetMarks();

    return Qnil;
}

VALUE mf_copy2(VALUE self)
{
    char buf[8192];
    char* p = buf;

    gErrMsgCancel = false;

    const int pid = getpid();
    memcpy(p, &pid, sizeof(pid));
    p += sizeof(pid);
    
    memcpy(p, &kCopy, sizeof(kCopy));
    p += sizeof(kCopy);

    VALUE path2 = mf_adir_path(self);
    sprintf(p, "%s%c", RSTRING(path2)->ptr, 0);
    p += RSTRING(path2)->len+1;

    VALUE files = ActiveDir()->MarkFiles();

    const int n = RARRAY(files)->len;
    memcpy(p, &n, sizeof(n));
    p += sizeof(n);

    for(int i=0; i < RARRAY(files)->len; i++) {
        VALUE item = rb_ary_entry(files, i);
        Check_Type(item, T_STRING);

        sprintf(p, "%s%c", RSTRING(item)->ptr, 0);
        p += RSTRING(item)->len + 1;
    }

    /// write ///    
    char* home = getenv("HOME");
    if(home == NULL) {
        err_msg("copy2: $HOME is null");
        return Qnil;
    }

    char path[PATH_MAX];
    sprintf(path, "%s/.mfiler-cutpast", home);

    int file = open(path, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if(file < 0) {
        err_msg("copy2: open err(%s)", path);
        return Qnil;
    }
    if(write(file, buf, p - buf) < 0) {
        err_msg("copy2: write err");
        close(file);
        return Qnil;
    }
    close(file);
    
    ActiveDir()->ResetMarks();

    return Qnil;
}

VALUE mf_get_mclip(VALUE self)
{
    char buf[8192];

    gErrMsgCancel = false;
    
    /// read ///
    char* home = getenv("HOME");
    if(home == NULL) {
        err_msg("get_mclip: $HOME is null");
        return Qnil;
    }

    char path[PATH_MAX];
    sprintf(path, "%s/.mfiler-cutpast", home);

    if(access(path, F_OK) != 0) {
        return Qnil;
    }
    
    struct stat stat_;
    if(stat(path, &stat_) < 0) {
        err_msg("get_mclip: stat err");
        return Qnil;
    }
        
    int file = open(path, O_RDONLY);
    if(file < 0) {
        err_msg("get_mclip: open err");
        return Qnil;
    }
    
    if(read(file, buf, stat_.st_size) < 0) {
        err_msg("get_mclip: read err");
        close(file);
        return Qnil;
    }
    close(file);
    
    /// reconstruct from file buffer ///
    char* p = buf;

    int pid = *(int*)p;
    p += sizeof(int);

    int cut_mode = (int)*(char*)p;
    p++;

    char buf2[PATH_MAX];
    char* p2 = buf2;
    while(*p) {
        *p2++ = *p++;
    }
    *p2 = 0;
    p++;
    
    VALUE cut_dir = rb_str_new2(buf2);

    VALUE cut_files = rb_ary_new();

    int len = *(int*)p;
    p += sizeof(int);

    for(int i=0; i < len; i++) {
        char buf3[PATH_MAX];
        char* p3 = buf3;
        while(*p) {
            *p3++ = *p++;
        }
        *p3 = 0;
        p++;

        rb_ary_push(cut_files, rb_str_new2(buf3));
    }
    
    VALUE result = rb_ary_new();

    rb_ary_push(result, INT2NUM(cut_mode));
    rb_ary_push(result, cut_dir);
    rb_ary_push(result, cut_files);
    
    return result;
}

VALUE mf_remove_mclip(VALUE self)
{
    char* home = getenv("HOME");
    if(home == NULL) {
        err_msg("remove_mclip: $HOME is null");
        return Qnil;
    }

    /// read ///
    char buf[8192];
    
    char path[PATH_MAX];
    sprintf(path, "%s/.mfiler-cutpast", home);
    
    if(access(path, F_OK) != 0) {
        return Qnil;
    }
    
    struct stat stat_;
    if(stat(path, &stat_) < 0) {
        err_msg("remove_mclip: stat err");
        return Qnil;
    }
        
    int file = open(path, O_RDONLY);
    if(file < 0) {
        err_msg("remove_mclip: open err");
        return Qnil;
    }
    
    if(read(file, buf, stat_.st_size) < 0) {
        err_msg("remove_mclip: read err");
        close(file);
        return Qnil;
    }
    close(file);
    
    /// get pid ////
    char* p = buf;

    int pid = *(int*)p;
    p += sizeof(int);
    
    /// remove mclip ///
    if(unlink(path) < 0) {
        err_msg("remove_mclip: unlink err(%s)", path);
        return Qnil;
    }
    
    ActiveDir()->ResetMarks();

    /// refresh ///
    char buf3[256];

    sprintf(buf3, "mfiler2 -r");
    system(buf3);
    
    return Qnil;
}

VALUE mf_past(VALUE self)
{
    char buf[8192];

    gErrMsgCancel = false;
        
    /// read ///
    char* home = getenv("HOME");
    if(home == NULL) {
        err_msg("past: $HOME is null");
        return Qnil;
    }

    char path[PATH_MAX];
    sprintf(path, "%s/.mfiler-cutpast", home);

    if(access(path, F_OK) != 0) {
        err_msg("past: there are no cutted or copied files");
        return Qnil;
    }
    
    struct stat stat_;
    if(stat(path, &stat_) < 0) {
        err_msg("past: stat err");
        return Qnil;
    }
        
    int file = open(path, O_RDONLY);
    if(file < 0) {
        err_msg("past: open err");
        return Qnil;
    }
    
    if(read(file, buf, stat_.st_size) < 0) {
        err_msg("past: read err");
        close(file);
        return Qnil;
    }
    close(file);
    
    /// reconstruct from file buffer ///
    char* p = buf;

    int pid = *(int*)p;
    p += sizeof(int);

    char cut_mode = *(char*)p;
    p++;


    char buf2[PATH_MAX];
    char* p2 = buf2;
    while(*p) {
        *p2++ = *p++;
    }
    *p2 = 0;
    p++;
    
    VALUE cut_dir = rb_str_new2(buf2);


    VALUE cut_files = rb_ary_new();

    int len = *(int*)p;
    p += sizeof(int);


    for(int i=0; i < len; i++) {
        char buf3[PATH_MAX];
        char* p3 = buf3;
        while(*p) {
            *p3++ = *p++;
        }
        *p3 = 0;
        p++;

        rb_ary_push(cut_files, rb_str_new2(buf3));
    }

    
    switch(cut_mode) {
    case kCopy:
        mf_copy_preserve(self, cut_dir, cut_files, mf_adir_path(self), Qfalse);
        break;

    case kCut: {
        mf_move(self, cut_dir, cut_files, mf_adir_path(self), Qfalse);
        }
        break;
    }

    if(unlink(path) < 0) {
        err_msg("past: unlink err(%s)", path);
        return Qnil;
    }

    /// refresh ///
    char buf3[256];

    sprintf(buf3, "mfiler2 -r");
    system(buf3);
    
    ActiveDir()->ResetMarks();

    return Qnil;
}


VALUE mf_xterm_next_toggle(VALUE self)
{
    gXtermNext = !gXtermNext;
    gXterm2 = false;

    return Qnil;
}

VALUE mf_xterm_toggle(VALUE self)
{
    gXterm2 = !gXterm2;
    gXtermNext = false;

    return Qnil;
}

VALUE mf_xterm(VALUE self, VALUE value)
{
    gXterm2 = (value == Qtrue) ? true : false;
    return Qnil;
}

VALUE mf_xterm_next(VALUE self, VALUE value)
{
    gXtermNext = (value == Qtrue) ? true : false;
    return Qnil;
}

VALUE mf_is_xterm_on(VALUE self)
{
    return gXterm2 ? Qtrue : Qfalse;
}

VALUE mf_is_xterm_next_on(VALUE self)
{
    return gXtermNext ? Qtrue : Qfalse;
}

VALUE mf_keymap(VALUE self, VALUE key, VALUE p1, VALUE p2, VALUE p3, VALUE p4, VALUE p5, VALUE p6, VALUE p7, VALUE p8, VALUE p9, VALUE p10)
{
    char keys[kKeyMapKeysMax];

    Check_Type(key, T_FIXNUM);
    Check_Type(p1, T_FIXNUM);
    Check_Type(p2, T_FIXNUM);
    Check_Type(p3, T_FIXNUM);
    Check_Type(p4, T_FIXNUM);
    Check_Type(p5, T_FIXNUM);
    Check_Type(p6, T_FIXNUM);
    Check_Type(p7, T_FIXNUM);
    Check_Type(p8, T_FIXNUM);
    Check_Type(p9, T_FIXNUM);
    Check_Type(p10, T_FIXNUM);
   

    keys[0] = NUM2INT(p1);
    keys[1] = NUM2INT(p2);
    keys[2] = NUM2INT(p3);
    keys[3] = NUM2INT(p4);
    keys[4] = NUM2INT(p5);
    keys[5] = NUM2INT(p6);
    keys[6] = NUM2INT(p7);
    keys[7] = NUM2INT(p8);
    keys[8] = NUM2INT(p9);
    keys[9] = NUM2INT(p10);
    madd_keymap(NUM2INT(key), keys);
    
    return Qnil;
}

VALUE mf_completion(VALUE self, VALUE all_candidate, VALUE editing, VALUE add_space)
{
    Check_Type(all_candidate, T_ARRAY);
    Check_Type(editing, T_STRING);

    bool add_space2 = (add_space == Qtrue) ? true: false;
        
    cmdline_completion(all_candidate, RSTRING(editing)->ptr, add_space2);

    return Qnil;
}

VALUE mf_completion2(VALUE self, VALUE all_candidate, VALUE editing, VALUE add_space, VALUE candidate_len)
{
    Check_Type(all_candidate, T_ARRAY);
    Check_Type(editing, T_STRING);
    Check_Type(candidate_len, T_FIXNUM);

    bool add_space2 = (add_space == Qtrue) ? true: false;

    cmdline_completion2(all_candidate, RSTRING(editing)->ptr, add_space2, NUM2INT(candidate_len));

    return Qnil;
}

VALUE mf_completion_clear(VALUE self)
{
    cmdline_completion_clear();

    return Qnil;
}

VALUE mf_hostname(VALUE self)
{



    return rb_str_new2(gHostName);
}

VALUE mf_username(VALUE self)
{



    return rb_str_new2(gUserName);
}

VALUE mf_set_shell(VALUE self, VALUE name, VALUE path, VALUE opt_eval)
{

    Check_Type(name, T_STRING);
    Check_Type(path, T_STRING);
    Check_Type(opt_eval, T_STRING);

    strcpy(gShellName, RSTRING(name)->ptr);
    strcpy(gShellPath, RSTRING(path)->ptr);
M(("gShellPath %s", gShellPath));
    strcpy(gShellOptEval, RSTRING(opt_eval)->ptr);
    


    return Qnil;
}

VALUE mf_message(int argc, VALUE* argv, VALUE self)
{

    if(argc < 1) {
        err_msg("invalid arguments in message(%d)", argc);
    }

    int ret = 0;
    if(mis_curses()) {
        VALUE str = rb_f_sprintf(argc, argv);
        msg("%s", RSTRING(str)->ptr);
    }
    else {
        printf("\nmessage() needs curses. please add %%Q\n");
    }



    return Qnil;
}

VALUE mf_message_nonstop(int argc, VALUE* argv, VALUE self)
{

    if(argc < 1) {
        err_msg("invalid arguments in message(%d)", argc);
    }

    int ret = 0;
    if(mis_curses()) {
        VALUE str = rb_f_sprintf(argc, argv);
        msg_nonstop("%s", RSTRING(str)->ptr);
    }
    else {
        printf("\nmessage() needs curses. please add %%Q\n");
    }



    return Qnil;
}

VALUE mf_select_string(VALUE self, VALUE message, VALUE strings, VALUE cancel)
{

    Check_Type(message, T_STRING);
    Check_Type(strings, T_ARRAY);
    Check_Type(cancel, T_FIXNUM);

    int ret = -1;
    if(mis_curses()) {
        char** str = (char**)MALLOC(sizeof(char*)*RARRAY(strings)->len);
    
        for(int i=0; i < RARRAY(strings)->len; i++) {
            VALUE item = rb_ary_entry(strings, i);
            Check_Type(item, T_STRING);

            str[i] = RSTRING(item)->ptr;
        }
    
        ret = select_str(RSTRING(message)->ptr, str, RARRAY(strings)->len, NUM2INT(cancel));

        FREE(str);

        
   return INT2NUM(ret);
   }
    else {
        printf("\nselect_string() needs curses. please add %%Q\n");

        
        return Qnil;
    }
}


VALUE mf_history_size(VALUE self, VALUE size)
{


    Check_Type(size, T_FIXNUM);

    gHistorySize = NUM2INT(size);



    return Qnil;
}






















VALUE mf_is_kanji(VALUE self, VALUE c)
{
    Check_Type(c, T_FIXNUM);
    
    return is_kanji((unsigned char)NUM2INT(c)) ? Qtrue : Qfalse;
}

VALUE mf_undo(VALUE self)
{
    switch(gUndoOp) {
        case kUndoNil:
            break;

        case kUndoCopy: {
            bool tmp = gCheckDeleteFile;
            gCheckDeleteFile = false;
            mf_remove(self, gUndoDdir, gUndoFiles, Qfalse);
            gCheckDeleteFile = tmp;

            gUndoOp = kUndoNil;
            gUndoSdir = Qnil;
            gUndoDdir = Qnil;
            gUndoFiles = Qnil;
            }
            break;

        case kUndoMove: {
            bool tmp = gCheckCopyFile;
            gCheckCopyFile = false;
            mf_move(self, gUndoDdir, gUndoFiles, gUndoSdir, Qfalse);
            gCheckCopyFile = tmp;

            gUndoOp = kUndoNil;
            gUndoSdir = Qnil;
            gUndoDdir = Qnil;
            gUndoFiles = Qnil;
            }
            break;

        case kUndoTrashBox: {
            bool tmp = gCheckCopyFile;
            gCheckCopyFile = false;
            mf_move(self, rb_str_new2(gTrashBoxPath), gUndoFiles, gUndoSdir, Qfalse);
            gCheckCopyFile = tmp;

            gUndoOp = kUndoNil;
            gUndoSdir = Qnil;
            gUndoDdir = Qnil;
            gUndoFiles = Qnil;
            }
            break;
    }
}

VALUE mf_mfiler_view(VALUE self)
{
    view(true);

    return Qnil;
}

VALUE mf_file_kind(VALUE self, VALUE dir, VALUE num)
{
    Check_Type(dir, T_FIXNUM);
    Check_Type(num, T_FIXNUM);

    sFile* file;
    if(NUM2INT(dir) == 0)
        file = gLDir->File(NUM2INT(num));
    else
        file = gRDir->File(NUM2INT(num));

    if(file) {
        if(S_ISDIR(file->mLStat.st_mode)) return INT2NUM(1);
        else if(S_ISLNK(file->mLStat.st_mode)) return INT2NUM(2);
        else if(S_ISCHR(file->mLStat.st_mode)) return INT2NUM(3);
        else if(S_ISBLK(file->mLStat.st_mode)) return INT2NUM(4);
        else if(S_ISFIFO(file->mLStat.st_mode)) return INT2NUM(5);
        else if(S_ISSOCK(file->mLStat.st_mode)) return INT2NUM(6);
        else return INT2NUM(0);
    }
    else {
        return Qnil;
    }
}

VALUE mf_file_linkto(VALUE self, VALUE dir, VALUE num)
{
    Check_Type(dir, T_FIXNUM);
    Check_Type(num, T_FIXNUM);

    sFile* file;
    if(NUM2INT(dir) == 0) 
        file = gLDir->File(NUM2INT(num));
    else
        file = gRDir->File(NUM2INT(num));

    if(file) {
        return rb_str_new2(file->mLinkTo);
    }
    else {
        return Qnil;
    }
}

VALUE mf_file_user(VALUE self, VALUE dir, VALUE num)
{
    Check_Type(dir, T_FIXNUM);
    Check_Type(num, T_FIXNUM);

    sFile* file;
    if(NUM2INT(dir) == 0) 
        file = gLDir->File(NUM2INT(num));
    else
        file = gRDir->File(NUM2INT(num));

    if(file) {
        return rb_str_new2(file->mUser);
    }
    else {
        return Qnil;
    }
}

VALUE mf_file_group(VALUE self, VALUE dir, VALUE num)
{
    Check_Type(dir, T_FIXNUM);
    Check_Type(num, T_FIXNUM);

    sFile* file;
    if(NUM2INT(dir) == 0) 
        file = gLDir->File(NUM2INT(num));
    else
        file = gRDir->File(NUM2INT(num));

    if(file) {
        return rb_str_new2(file->mGroup);
    }
    else {
        return Qnil;
    }
}

VALUE mf_file_permission(VALUE self, VALUE dir, VALUE num)
{
    Check_Type(dir, T_FIXNUM);
    Check_Type(num, T_FIXNUM);

    sFile* file;
    if(NUM2INT(dir) == 0) 
        file = gLDir->File(NUM2INT(num));
    else
        file = gRDir->File(NUM2INT(num));

    if(file) {
        return INT2NUM(file->mLStat.st_mode & S_ALLPERM);
    }
    else {
        return Qnil;
    }
}

VALUE mf_file_mtime(VALUE self, VALUE dir, VALUE num)
{
    Check_Type(dir, T_FIXNUM);
    Check_Type(num, T_FIXNUM);

    sFile* file;
    if(NUM2INT(dir) == 0) 
        file = gLDir->File(NUM2INT(num));
    else
        file = gRDir->File(NUM2INT(num));

    if(file) {
        return INT2NUM(file->mLStat.st_mtime);
    }
    else {
        return Qnil;
    }
}

VALUE mf_forground_job(VALUE self, VALUE num)
{
    Check_Type(num, T_FIXNUM);
    
    shell_forground_job(NUM2INT(num)-1);
    
    return Qnil;
}

VALUE mf_job_num(VALUE self)
{
    return INT2NUM(shell_job_num());
}

VALUE mf_job_title(VALUE self, VALUE num)
{
    Check_Type(num, T_FIXNUM);

    char* str = shell_job_title(NUM2INT(num)-1);

    if(str) {
        return rb_str_new2(str);
    }
    else {
        return Qnil;
    }
}

VALUE mf_kill_job(VALUE self, VALUE num)
{
    Check_Type(num, T_FIXNUM);

    shell_kill_job(NUM2INT(num)-1);

    return Qnil;
}

VALUE mf_wstrcut(VALUE self, VALUE str, VALUE termsize)
{
    Check_Type(str, T_STRING);
    Check_Type(termsize, T_FIXNUM);

    char tmp[4096];
    if(gKanjiCode == kUtf8)
        str_cut(RSTRING(str)->ptr, NUM2INT(termsize), tmp, 4096);
    else 
        cut2(RSTRING(str)->ptr, tmp, NUM2INT(termsize));

    return rb_str_new2(tmp);
}

VALUE mf_completion_program(VALUE self, VALUE editing_dir, VALUE editing_file, VALUE editing, VALUE editing_position)
{
    Check_Type(editing_dir, T_STRING);
    Check_Type(editing_file, T_STRING);
    Check_Type(editing, T_STRING);
    Check_Type(editing_position, T_FIXNUM);

    cmdline_completion_program(RSTRING(editing_dir)->ptr, RSTRING(editing_file)->ptr, RSTRING(editing)->ptr, NUM2INT(editing_position));

    return Qnil;
}

VALUE mf_completion_file(VALUE self, VALUE editing_dir, VALUE editing_file, VALUE editing, VALUE editing_position)
{
    Check_Type(editing_dir, T_STRING);
    Check_Type(editing_file, T_STRING);
    Check_Type(editing, T_STRING);
    Check_Type(editing_position, T_FIXNUM);

    cmdline_completion_file(RSTRING(editing_dir)->ptr, RSTRING(editing_file)->ptr, RSTRING(editing)->ptr, NUM2INT(editing_position));

    return Qnil;
}

void command_init()
{
    gUndoOp = kUndoNil;
    gUndoSdir = Qnil;
    gUndoDdir = Qnil;
    gUndoFiles = Qnil;

    gethostname(gHostName, 256);

    struct passwd* uname = getpwuid(getuid());
    strcpy(gUserName, uname->pw_name);

    rb_global_variable(&gUndoSdir);
    rb_global_variable(&gUndoDdir);
    rb_global_variable(&gUndoFiles);

    rb_define_global_function("minitscr", RUBY_METHOD_FUNC(mf_minitscr), 0);
    rb_define_global_function("mendwin", RUBY_METHOD_FUNC(mf_mendwin), 0);
    rb_define_global_function("mmove", RUBY_METHOD_FUNC(mf_mmove), 2);
    rb_define_global_function("mmove_immediately", RUBY_METHOD_FUNC(mf_mmove_immediately), 2);
    rb_define_global_function("mmvprintw", RUBY_METHOD_FUNC(mf_mmvprintw), -1);
    rb_define_global_function("mprintw", RUBY_METHOD_FUNC(mf_mprintw), -1);
    rb_define_global_function("mattron", RUBY_METHOD_FUNC(mf_mattron), 1);
    rb_define_global_function("mattroff", RUBY_METHOD_FUNC(mf_mattroff), 0);
    rb_define_global_function("mgetmaxx", RUBY_METHOD_FUNC(mf_mgetmaxx), 0);
    rb_define_global_function("mgetmaxy", RUBY_METHOD_FUNC(mf_mgetmaxy), 0);
    rb_define_global_function("mclear_immediately", RUBY_METHOD_FUNC(mf_mclear_immediately), 0);
    rb_define_global_function("mclear", RUBY_METHOD_FUNC(mf_mclear), 0);
    rb_define_global_function("mclear_online", RUBY_METHOD_FUNC(mf_mclear_online), 1);
    rb_define_global_function("mbox", RUBY_METHOD_FUNC(mf_mbox), 4);
    rb_define_global_function("mrefresh", RUBY_METHOD_FUNC(mf_mrefresh), 0);
    rb_define_global_function("mgetch", RUBY_METHOD_FUNC(mf_mgetch), 0);
    rb_define_global_function("mgetch_nonblock", RUBY_METHOD_FUNC(mf_mgetch_nonblock), 0);
    rb_define_global_function("mis_curses", RUBY_METHOD_FUNC(mf_mis_curses), 0);

    rb_define_global_function("dir_move", RUBY_METHOD_FUNC(mf_dir_move), 2);
    rb_define_global_function("adir", RUBY_METHOD_FUNC(mf_adir), 0);
    rb_define_global_function("sdir", RUBY_METHOD_FUNC(mf_sdir), 0);
    rb_define_global_function("set_sort_kind", RUBY_METHOD_FUNC(mf_set_sort_kind), 2);
    rb_define_global_function("sort_kind", RUBY_METHOD_FUNC(mf_sort_kind), 1);
    rb_define_global_function("sort", RUBY_METHOD_FUNC(mf_sort), 1);
    rb_define_global_function("sisearch", RUBY_METHOD_FUNC(mf_sisearch), 1);
    rb_define_global_function("sisearch_get_cache", RUBY_METHOD_FUNC(mf_sisearch_get_cache), 0);
    rb_define_global_function("sisearch_candidate_max_ascii", RUBY_METHOD_FUNC(mf_sisearch_candidate_max_ascii), 1);
    rb_define_global_function("sisearch_candidate_max_migemo", RUBY_METHOD_FUNC(mf_sisearch_candidate_max_migemo), 1);





















    rb_define_global_function("view_nameonly", RUBY_METHOD_FUNC(mf_view_nameonly), 1);
    rb_define_global_function("set_view_nameonly", RUBY_METHOD_FUNC(mf_set_view_nameonly), 2);
    rb_define_global_function("view_focusback", RUBY_METHOD_FUNC(mf_view_focusback), 1);
    rb_define_global_function("set_view_focusback", RUBY_METHOD_FUNC(mf_set_view_focusback), 2);
    rb_define_global_function("view_removedir", RUBY_METHOD_FUNC(mf_view_removedir), 1);
    rb_define_global_function("set_view_removedir", RUBY_METHOD_FUNC(mf_set_view_removedir), 2);
    rb_define_global_function("view_filer", RUBY_METHOD_FUNC(mf_view_filer), 0);
    rb_define_global_function("set_view_filer", RUBY_METHOD_FUNC(mf_set_view_filer), 1);
    rb_define_global_function("view_color", RUBY_METHOD_FUNC(mf_view_color), 0);
    rb_define_global_function("set_view_color", RUBY_METHOD_FUNC(mf_set_view_color), 1);
    rb_define_global_function("set_view_color_mark", RUBY_METHOD_FUNC(mf_set_view_color_mark), 1);
    rb_define_global_function("set_view_color_exe", RUBY_METHOD_FUNC(mf_set_view_color_exe), 1);
    rb_define_global_function("set_view_color_dir", RUBY_METHOD_FUNC(mf_set_view_color_dir), 1);
    rb_define_global_function("set_view_color_link", RUBY_METHOD_FUNC(mf_set_view_color_link), 1);
    rb_define_global_function("set_view_bold_exe", RUBY_METHOD_FUNC(mf_set_view_bold_exe), 1);
    rb_define_global_function("set_view_bold_dir", RUBY_METHOD_FUNC(mf_set_view_bold_dir), 1);
    rb_define_global_function("kanjicode", RUBY_METHOD_FUNC(mf_kanjicode), 0);
    rb_define_global_function("set_kanjicode", RUBY_METHOD_FUNC(mf_set_kanjicode), 1);
    rb_define_global_function("kanjicode_filename", RUBY_METHOD_FUNC(mf_kanjicode_filename), 0);
    rb_define_global_function("set_kanjicode_filename", RUBY_METHOD_FUNC(mf_set_kanjicode_filename), 1);

    rb_define_global_function("tab_select", RUBY_METHOD_FUNC(mf_tab_select), 0);
    rb_define_global_function("tab_close", RUBY_METHOD_FUNC(mf_tab_close), 1);
    rb_define_global_function("tab_up", RUBY_METHOD_FUNC(mf_tab_up), 1);
    rb_define_global_function("tab_path", RUBY_METHOD_FUNC(mf_tab_path), 1);
    rb_define_global_function("tab_scrolltop", RUBY_METHOD_FUNC(mf_tab_scrolltop), 1);
    rb_define_global_function("tab_cursor", RUBY_METHOD_FUNC(mf_tab_cursor), 1);
    rb_define_global_function("tab_max", RUBY_METHOD_FUNC(mf_tab_max), 0);
    rb_define_global_function("tab_new", RUBY_METHOD_FUNC(mf_tab_new), 4);

    rb_define_global_function("cursor_name", RUBY_METHOD_FUNC(mf_cursor_name), 0);
    rb_define_global_function("cursor_name_convert", RUBY_METHOD_FUNC(mf_cursor_name_convert), 0);
    rb_define_global_function("cursor_path", RUBY_METHOD_FUNC(mf_cursor_path), 0);
    rb_define_global_function("cursor_path_convert", RUBY_METHOD_FUNC(mf_cursor_path_convert), 0);
    rb_define_global_function("cursor_ext", RUBY_METHOD_FUNC(mf_cursor_ext), 0);
    rb_define_global_function("cursor_ext_convert", RUBY_METHOD_FUNC(mf_cursor_ext_convert), 0);
    rb_define_global_function("cursor_noext", RUBY_METHOD_FUNC(mf_cursor_noext), 0);
    rb_define_global_function("cursor_noext_convert", RUBY_METHOD_FUNC(mf_cursor_noext_convert), 0);
    rb_define_global_function("active_dir", RUBY_METHOD_FUNC(mf_active_dir), 0);
    rb_define_global_function("active_dir_convert", RUBY_METHOD_FUNC(mf_active_dir_convert), 0);
    rb_define_global_function("sleep_dir", RUBY_METHOD_FUNC(mf_sleep_dir), 0);
    rb_define_global_function("sleep_dir_convert", RUBY_METHOD_FUNC(mf_sleep_dir_convert), 0);
    rb_define_global_function("adir_path", RUBY_METHOD_FUNC(mf_adir_path), 0);
    rb_define_global_function("adir_path_convert", RUBY_METHOD_FUNC(mf_adir_path_convert), 0);
    rb_define_global_function("sdir_path", RUBY_METHOD_FUNC(mf_sdir_path), 0);
    rb_define_global_function("sdir_path_convert", RUBY_METHOD_FUNC(mf_sdir_path_convert), 0);
    rb_define_global_function("left_dir", RUBY_METHOD_FUNC(mf_left_dir), 0);
    rb_define_global_function("left_dir_convert", RUBY_METHOD_FUNC(mf_left_dir_convert), 0);
    rb_define_global_function("right_dir", RUBY_METHOD_FUNC(mf_right_dir), 0);
    rb_define_global_function("right_dir_convert", RUBY_METHOD_FUNC(mf_right_dir_convert), 0);
    rb_define_global_function("ldir_path", RUBY_METHOD_FUNC(mf_ldir_path), 0);
    rb_define_global_function("ldir_path_convert", RUBY_METHOD_FUNC(mf_ldir_path_convert), 0);
    rb_define_global_function("rdir_path", RUBY_METHOD_FUNC(mf_rdir_path), 0);
    rb_define_global_function("rdir_path_convert", RUBY_METHOD_FUNC(mf_rdir_path_convert), 0);
    rb_define_global_function("adir_mark", RUBY_METHOD_FUNC(mf_adir_mark), 0);
    rb_define_global_function("adir_mark_convert", RUBY_METHOD_FUNC(mf_adir_mark_convert), 0);
    rb_define_global_function("sdir_mark", RUBY_METHOD_FUNC(mf_sdir_mark), 0);
    rb_define_global_function("sdir_mark_convert", RUBY_METHOD_FUNC(mf_sdir_mark_convert), 0);
    rb_define_global_function("adir_mark_fullpath", RUBY_METHOD_FUNC(mf_adir_mark_fullpath), 0);
    rb_define_global_function("adir_mark_fullpath_convert", RUBY_METHOD_FUNC(mf_adir_mark_fullpath_convert), 0);
    rb_define_global_function("sdir_mark_fullpath", RUBY_METHOD_FUNC(mf_sdir_mark_fullpath), 0);
    rb_define_global_function("sdir_mark_fullpath_convert", RUBY_METHOD_FUNC(mf_sdir_mark_fullpath_convert), 0);
    rb_define_global_function("ldir_mark", RUBY_METHOD_FUNC(mf_ldir_mark), 0);
    rb_define_global_function("ldir_mark_convert", RUBY_METHOD_FUNC(mf_ldir_mark_convert), 0);
    rb_define_global_function("rdir_mark", RUBY_METHOD_FUNC(mf_rdir_mark), 0);
    rb_define_global_function("rdir_mark_convert", RUBY_METHOD_FUNC(mf_rdir_mark_convert), 0);
    rb_define_global_function("ldir_mark_fullpath", RUBY_METHOD_FUNC(mf_ldir_mark_fullpath), 0);
    rb_define_global_function("ldir_mark_fullpath_convert", RUBY_METHOD_FUNC(mf_ldir_mark_fullpath_convert), 0);
    rb_define_global_function("rdir_mark_fullpath", RUBY_METHOD_FUNC(mf_rdir_mark_fullpath), 0);
    rb_define_global_function("rdir_mark_fullpath_convert", RUBY_METHOD_FUNC(mf_rdir_mark_fullpath_convert), 0);
    rb_define_global_function("change_terminal_title", RUBY_METHOD_FUNC(mf_change_terminal_title), 0);
    rb_define_global_function("set_change_terminal_title", RUBY_METHOD_FUNC(mf_set_change_terminal_title), 1);
    rb_define_global_function("change_sort", RUBY_METHOD_FUNC(mf_change_sort), 0);
    rb_define_global_function("set_isearch_option1", RUBY_METHOD_FUNC(mf_set_isearch_option1), 1);
    rb_define_global_function("isearch_option1", RUBY_METHOD_FUNC(mf_isearch_option1), 0);
    rb_define_global_function("view_mtime", RUBY_METHOD_FUNC(mf_view_mtime), 0);
    rb_define_global_function("set_view_mtime", RUBY_METHOD_FUNC(mf_set_view_mtime), 1);
    rb_define_global_function("view_size", RUBY_METHOD_FUNC(mf_view_size), 0);
    rb_define_global_function("set_view_size", RUBY_METHOD_FUNC(mf_set_view_size), 1);
    rb_define_global_function("view_permission", RUBY_METHOD_FUNC(mf_view_permission), 0);
    rb_define_global_function("set_view_permission", RUBY_METHOD_FUNC(mf_set_view_permission), 1);
    rb_define_global_function("view_nlink", RUBY_METHOD_FUNC(mf_view_nlink), 0);
    rb_define_global_function("set_view_nlink", RUBY_METHOD_FUNC(mf_set_view_nlink), 1);
    rb_define_global_function("view_user", RUBY_METHOD_FUNC(mf_view_user), 0);
    rb_define_global_function("set_view_user", RUBY_METHOD_FUNC(mf_set_view_user), 1);
    rb_define_global_function("view_group", RUBY_METHOD_FUNC(mf_view_group), 0);
    rb_define_global_function("set_view_group", RUBY_METHOD_FUNC(mf_set_view_group), 1);
    rb_define_global_function("set_menu_scroll_cycle", RUBY_METHOD_FUNC(mf_set_menu_scroll_cycle), 1);
    rb_define_global_function("menu_scroll_cycle", RUBY_METHOD_FUNC(mf_menu_scroll_cycle), 0);
    rb_define_global_function("set_view_fname_divide_ext", RUBY_METHOD_FUNC(mf_set_view_fname_divide_ext), 1);
    rb_define_global_function("view_fname_divide_ext", RUBY_METHOD_FUNC(mf_view_fname_divide_ext), 0);
    rb_define_global_function("set_cmdline_escape_key", RUBY_METHOD_FUNC(mf_set_cmdline_escape_key), 1);
    rb_define_global_function("cmdline_escape_key", RUBY_METHOD_FUNC(mf_cmdline_escape_key), 0);
    rb_define_global_function("set_escape_wait", RUBY_METHOD_FUNC(mf_set_escape_wait), 1);

    rb_define_global_function("mark_all", RUBY_METHOD_FUNC(mf_mark_all), 1);
    rb_define_global_function("mark_all_files", RUBY_METHOD_FUNC(mf_mark_all_files), 1);
    rb_define_global_function("mark", RUBY_METHOD_FUNC(mf_mark), 3);
    rb_define_global_function("mark_clear", RUBY_METHOD_FUNC(mf_mark_clear), 1);
    rb_define_global_function("is_marked", RUBY_METHOD_FUNC(mf_is_marked), 2);
    rb_define_global_function("marking", RUBY_METHOD_FUNC(mf_marking), 1);

    rb_define_global_function("mf_exit", RUBY_METHOD_FUNC(mf_exit), 0);
    rb_define_global_function("keycommand", RUBY_METHOD_FUNC(mf_keycommand), 4);
    rb_define_global_function("cursor_move", RUBY_METHOD_FUNC(mf_cursor_move), 2);
    rb_define_global_function("cursor_left", RUBY_METHOD_FUNC(mf_cursor_left), 0);
    rb_define_global_function("cursor_right", RUBY_METHOD_FUNC(mf_cursor_right), 0);
    rb_define_global_function("cursor_other", RUBY_METHOD_FUNC(mf_cursor_other), 0);
    
    
    rb_define_global_function("isearch", RUBY_METHOD_FUNC(mf_isearch), 0);

    rb_define_global_function("option_individual_cursor", RUBY_METHOD_FUNC(mf_option_individual_cursor), 1);
    rb_define_global_function("option_isearch_decision_way", RUBY_METHOD_FUNC(mf_option_isearch_decision_way), 1);
 
    rb_define_global_function("option_remain_marks", RUBY_METHOD_FUNC(mf_option_remain_marks), 1);
    rb_define_global_function("option_refresh", RUBY_METHOD_FUNC(mf_option_refresh), 1);
    rb_define_global_function("is_remain_marks", RUBY_METHOD_FUNC(mf_is_remain_marks), 0);
   rb_define_global_function("option_visible_fname_execute", RUBY_METHOD_FUNC(mf_option_visible_fname_execute), 1);
    rb_define_global_function("option_check_delete_file", RUBY_METHOD_FUNC(mf_option_check_delete_file), 1);
    rb_define_global_function("option_check_copy_file", RUBY_METHOD_FUNC(mf_option_check_copy_file), 1);
    rb_define_global_function("option_check_exit", RUBY_METHOD_FUNC(mf_option_check_exit), 1);
    rb_define_global_function("option_shift_isearch", RUBY_METHOD_FUNC(mf_option_shift_isearch), 1);
    
    rb_define_global_function("option_trashbox_path", RUBY_METHOD_FUNC(mf_option_trashbox_path), 1);
    rb_define_global_function("option_gnu_screen", RUBY_METHOD_FUNC(mf_option_gnu_screen), 1);
    rb_define_global_function("option_xterm", RUBY_METHOD_FUNC(mf_option_xterm), 1);
    rb_define_global_function("option_remain_cursor", RUBY_METHOD_FUNC(mf_option_remain_cursor), 1);
    rb_define_global_function("option_auto_rehash", RUBY_METHOD_FUNC(mf_option_auto_rehash), 1);
    rb_define_global_function("option_extension_icase", RUBY_METHOD_FUNC(mf_option_extension_icase), 1);
    rb_define_global_function("option_read_dir_history", RUBY_METHOD_FUNC(mf_option_read_dir_history), 1);
    
    rb_define_global_function("option_select", RUBY_METHOD_FUNC(mf_option_select), 1);

    rb_define_global_function("cmdline_noconvert", RUBY_METHOD_FUNC(mf_cmdline_noconvert), 3);
    rb_define_global_function("cmdline_c", RUBY_METHOD_FUNC(mf_cmdline_c), 3);
    rb_define_global_function("cmdline_convert", RUBY_METHOD_FUNC(mf_cmdline_convert), 4);
    rb_define_global_function("shell", RUBY_METHOD_FUNC(mf_shell), 2);
    rb_define_global_function("menu", RUBY_METHOD_FUNC(mf_menu), 1);
    rb_define_global_function("defmenu", RUBY_METHOD_FUNC(mf_defmenu), -1);
    
    rb_define_global_function("dir_exchange", RUBY_METHOD_FUNC(mf_dir_exchange), 0);
    rb_define_global_function("dir_copy", RUBY_METHOD_FUNC(mf_dir_copy), 0);
    rb_define_global_function("refresh", RUBY_METHOD_FUNC(mf_refresh), 0);
    rb_define_global_function("reread", RUBY_METHOD_FUNC(mf_reread), 1);
    rb_define_global_function("dir_back", RUBY_METHOD_FUNC(mf_dir_back), 0);
    rb_define_global_function("sdir_back", RUBY_METHOD_FUNC(mf_sdir_back), 0);


    
    rb_define_global_function("copy", RUBY_METHOD_FUNC(mf_copy), 4);
    rb_define_global_function("copy_p", RUBY_METHOD_FUNC(mf_copy_preserve), 4);
    rb_define_global_function("remove", RUBY_METHOD_FUNC(mf_remove), 3);
    rb_define_global_function("move", RUBY_METHOD_FUNC(mf_move), 4);
    rb_define_global_function("trashbox", RUBY_METHOD_FUNC(mf_trashbox), 3);

    rb_define_global_function("file_kind", RUBY_METHOD_FUNC(mf_file_kind), 2);
    rb_define_global_function("file_linkto", RUBY_METHOD_FUNC(mf_file_linkto), 2);
    rb_define_global_function("file_user", RUBY_METHOD_FUNC(mf_file_user), 2);
    rb_define_global_function("file_group", RUBY_METHOD_FUNC(mf_file_group), 2);
    rb_define_global_function("file_permission", RUBY_METHOD_FUNC(mf_file_permission), 2);
    rb_define_global_function("file_mtime", RUBY_METHOD_FUNC(mf_file_mtime), 2);
    rb_define_global_function("add_file", RUBY_METHOD_FUNC(mf_add_file), 2);
    rb_define_global_function("add_file2", RUBY_METHOD_FUNC(mf_add_file2), 11);
    
    rb_define_global_function("set_mask", RUBY_METHOD_FUNC(mf_set_mask), 2);
    rb_define_global_function("cut", RUBY_METHOD_FUNC(mf_cut), 0);
    rb_define_global_function("copy2", RUBY_METHOD_FUNC(mf_copy2), 0);
    rb_define_global_function("past", RUBY_METHOD_FUNC(mf_past), 0);

    rb_define_global_function("is_isearch_on", RUBY_METHOD_FUNC(mf_is_isearch_on), 0);
    rb_define_global_function("isearch_off", RUBY_METHOD_FUNC(mf_isearch_off), 0);
    rb_define_global_function("isearch_on", RUBY_METHOD_FUNC(mf_isearch_on), 0);
    rb_define_global_function("isearch_partmatch", RUBY_METHOD_FUNC(mf_isearch_partmatch), 0);
    rb_define_global_function("mask", RUBY_METHOD_FUNC(mf_mask), 1);
    rb_define_global_function("malias", RUBY_METHOD_FUNC(mf_malias), 3);
    rb_define_global_function("rehash", RUBY_METHOD_FUNC(mf_rehash), 0);

    rb_define_global_function("help", RUBY_METHOD_FUNC(mf_help), 0);
    rb_define_global_function("set_xterm", RUBY_METHOD_FUNC(mf_set_xterm), 5);
    rb_define_global_function("cursor", RUBY_METHOD_FUNC(mf_cursor), 1);
    rb_define_global_function("file_name", RUBY_METHOD_FUNC(mf_file_name), 2);
    rb_define_global_function("cursor_max", RUBY_METHOD_FUNC(mf_cursor_max), 1);
    rb_define_global_function("scrolltop", RUBY_METHOD_FUNC(mf_scrolltop), 1);
    rb_define_global_function("set_scrolltop", RUBY_METHOD_FUNC(mf_set_scrolltop), 2);
    rb_define_global_function("cursor_x", RUBY_METHOD_FUNC(mf_cursor_x), 0);
    rb_define_global_function("cursor_y", RUBY_METHOD_FUNC(mf_cursor_y), 0);
    rb_define_global_function("cursor_maxx", RUBY_METHOD_FUNC(mf_cursor_maxx), 0);
    rb_define_global_function("cursor_maxy", RUBY_METHOD_FUNC(mf_cursor_maxy), 0);
    rb_define_global_function("xterm_next_toggle", RUBY_METHOD_FUNC(mf_xterm_next_toggle), 0);
    rb_define_global_function("xterm_toggle", RUBY_METHOD_FUNC(mf_xterm_toggle), 0);
    rb_define_global_function("keymap", RUBY_METHOD_FUNC(mf_keymap), 11);
    rb_define_global_function("completion", RUBY_METHOD_FUNC(mf_completion), 3);
    rb_define_global_function("completion2", RUBY_METHOD_FUNC(mf_completion2), 4);

    rb_define_global_function("hostname", RUBY_METHOD_FUNC(mf_hostname), 0);
    rb_define_global_function("username", RUBY_METHOD_FUNC(mf_username), 0);
    rb_define_global_function("set_shell", RUBY_METHOD_FUNC(mf_set_shell), 3);
    rb_define_global_function("message", RUBY_METHOD_FUNC(mf_message), -1);
    rb_define_global_function("message_nonstop", RUBY_METHOD_FUNC(mf_message_nonstop), -1);
    rb_define_global_function("history_size", RUBY_METHOD_FUNC(mf_history_size), 1);
    rb_define_global_function("select_string", RUBY_METHOD_FUNC(mf_select_string), 3);
    rb_define_global_function("undo", RUBY_METHOD_FUNC(mf_undo), 0);
    rb_define_global_function("file_num", RUBY_METHOD_FUNC(mf_file_num), 2);
    rb_define_global_function("is_screen_terminal", RUBY_METHOD_FUNC(mf_is_screen_terminal), 0);
    rb_define_global_function("get_mclip", RUBY_METHOD_FUNC(mf_get_mclip), 0);
    rb_define_global_function("remove_mclip", RUBY_METHOD_FUNC(mf_remove_mclip), 0);
    rb_define_global_function("input_box", RUBY_METHOD_FUNC(mf_input_box), 3);
    rb_define_global_function("mark_num", RUBY_METHOD_FUNC(mf_mark_num), 1);
    rb_define_global_function("is_kanji", RUBY_METHOD_FUNC(mf_is_kanji), 1);

    rb_define_global_function("set_iconv_kanji_code_name", RUBY_METHOD_FUNC(mf_set_iconv_kanji_code_name), 2);
    rb_define_global_function("mfiler_view", RUBY_METHOD_FUNC(mf_mfiler_view), 0);
    rb_define_global_function("xterm", RUBY_METHOD_FUNC(mf_xterm), 0);
    rb_define_global_function("xterm_next", RUBY_METHOD_FUNC(mf_xterm_next), 0);
    rb_define_global_function("is_xterm_on", RUBY_METHOD_FUNC(mf_is_xterm_on), 0);
    rb_define_global_function("is_xterm_next_on", RUBY_METHOD_FUNC(mf_is_xterm_next_on), 0);
    rb_define_global_function("encode_type", RUBY_METHOD_FUNC(mf_encode_type), 1);
    rb_define_global_function("version", RUBY_METHOD_FUNC(mf_version), 0);
    rb_define_global_function("isearch_explore", RUBY_METHOD_FUNC(mf_isearch_explore), 1);
    rb_define_global_function("is_isearch_explore", RUBY_METHOD_FUNC(mf_is_isearch_explore), 0);
    rb_define_global_function("isearch_partmatch2", RUBY_METHOD_FUNC(mf_isearch_partmatch2), 1);
    rb_define_global_function("is_isearch_partmatch2", RUBY_METHOD_FUNC(mf_is_isearch_partmatch2), 0);
    rb_define_global_function("forground_job", RUBY_METHOD_FUNC(mf_forground_job), 1);
    rb_define_global_function("job_num", RUBY_METHOD_FUNC(mf_job_num), 0);
    rb_define_global_function("kill_job", RUBY_METHOD_FUNC(mf_kill_job), 1);
    rb_define_global_function("job_title", RUBY_METHOD_FUNC(mf_job_title), 1);
    rb_define_global_function("wstrcut", RUBY_METHOD_FUNC(mf_wstrcut), 2);
    rb_define_global_function("completion_clear", RUBY_METHOD_FUNC(mf_completion_clear), 0);
    rb_define_global_function("completion_program", RUBY_METHOD_FUNC(mf_completion_program), 4);
    rb_define_global_function("completion_file", RUBY_METHOD_FUNC(mf_completion_file), 4);
    rb_define_global_function("set_signal_clear", RUBY_METHOD_FUNC(mf_set_signal_clear), 0);
    rb_define_global_function("set_signal_mfiler2", RUBY_METHOD_FUNC(mf_set_signal_mfiler2), 0);
    rb_define_global_function("delete_file_from_file_list", RUBY_METHOD_FUNC(mf_delete_file_from_file_list), 2);
    rb_define_global_function("sisearch_migemo", RUBY_METHOD_FUNC(mf_sisearch_migemo), 1);
}

void command_final()
{
}
