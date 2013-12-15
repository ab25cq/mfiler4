#include "common.h"
#include <errno.h>
#include <libgen.h>
#include <time.h>

#include "config.h"

#if defined(HAVE_CURSES_H)
#include <curses.h>
#elif defined(HAVE_NCURSES_H)
#include <ncurses.h>
#elif defined(HAVE_NCURSES_NCURSES_H)
#include <ncurses/ncurses.h>
#endif

sObject* gMFiler4;
sObject* gMFiler4System;

BOOL cmd_quit(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(xyzsh_job_num() == 0) {
        if(runinfo->mArgsNumRuntime >= 2) {
            gMainLoop = atoi(runinfo->mArgsRuntime[1]);
        }
        else {
            gMainLoop = 0;
        }
        runinfo->mRCode = 0;
    }
    else {
        err_msg("jobs exist", runinfo->mSName, runinfo->mSLine);
        return FALSE;
    }

    return TRUE;
}

BOOL cmd_keycommand(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mBlocksNum == 1 && runinfo->mArgsNumRuntime == 2) {
        BOOL external = sRunInfo_option(runinfo, "-external");

        char* argv1 = runinfo->mArgsRuntime[1];
        int keycode = atoi(argv1);

        int meta = 0;
        if(sRunInfo_option(runinfo, "-m")) { meta = 1; }

        filer_add_keycommand(meta, keycode, runinfo->mBlocks[0], "run time script", external);

        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_start_color(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(mis_raw_mode()) {
        xstart_color();
    }
    else {
        xinitscr();
        xstart_color();
        xendwin();
    }
    runinfo->mRCode = 0;
    return TRUE;
}

BOOL cmd_cursor_move(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mArgsNumRuntime == 2) {
        char* darg = sRunInfo_option_with_argument(runinfo, "-d");

        int dir;
        if(darg == NULL) {
            dir = adir();
        }
        else if(strcmp(darg, "all") == 0) {
            dir = -1;
        }
        else if(strcmp(darg, "adir") == 0) {
            dir = adir();
        }
        else if(strcmp(darg, "sdir") == 0) {
            dir = sdir();
        }
        else {
            dir = atoi(darg);

            if(dir < 0 || dir >= vector_count(gDirs)) {
                err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine);
                return FALSE;
            }
        }

        char* argv2 = runinfo->mArgsRuntime[1];

        int cursor_num;
        if(argv2[0] == '+') {
            char buf[128];
            xstrncpy(buf, argv2+1, 128);
            sDir* dir2 = filer_dir(dir);
            if(dir2) {
                cursor_num = dir2->mCursor + atoi(buf);
            }
            else {
                cursor_num = 0;
            }
        }
        else if(argv2[0] == '-') {
            char buf[128];
            xstrncpy(buf, argv2+1, 128);
            sDir* dir2 = filer_dir(dir);
            if(dir2) {
                cursor_num = dir2->mCursor - atoi(buf);
            }
        }
        else if(argv2[0] == '/') {
            char buf[128];
            xstrncpy(buf,argv2+1, 128);
            cursor_num = filer_file2(dir, buf);
            if(cursor_num == -1) {
                cursor_num = 0;
            }
        }
        else {
            cursor_num = atoi(argv2);
        }

        if(dir < 0) {
            int j;
            for(j=0; j<vector_count(gDirs); j++) {
                (void)filer_cursor_move(j, cursor_num);
                runinfo->mRCode = 0;
            }
        }
        else {
            if(!filer_cursor_move(dir, cursor_num)) {
                err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine);
                return FALSE;
            }

            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_mcd(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    char* darg = sRunInfo_option_with_argument(runinfo, "-d");

    int dir;
    if(darg == NULL) {
        dir = adir();
    }
    else if(strcmp(darg, "all") == 0) {
        err_msg("can't get option -d all", runinfo->mSName, runinfo->mSLine);
        return FALSE;
    }
    else if(strcmp(darg, "adir") == 0) {
        dir = adir();
    }
    else if(strcmp(darg, "sdir") == 0) {
        dir = sdir();
    }
    else {
        dir = atoi(darg);

        if(dir < 0 || dir >= vector_count(gDirs)) {
            err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine);
            return FALSE;
        }
    }

    if(runinfo->mArgsNumRuntime == 2) {
        char* arg1 = runinfo->mArgsRuntime[1];

        if(strcmp(arg1, "+") == 0) {
            if(!filer_history_forward(dir)) {
                err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine);
                return FALSE;
            }

            runinfo->mRCode = 0;
        }
        else if(strcmp(arg1, "-") == 0) {
            if(!filer_history_back(dir)) {
                err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine);
                return FALSE;
            }

            runinfo->mRCode = 0;
        }
        else {
            if(!filer_cd(dir, arg1)) {
                err_msg("cd failed", runinfo->mSName, runinfo->mSLine);
                return FALSE;
            }
            if(!filer_add_history(dir, arg1)) {
                err_msg("adding hisotry of mcd failed", runinfo->mSName, runinfo->mSLine);
                return FALSE;
            }

            runinfo->mRCode = 0;
        }
    }
    else if(runinfo->mArgsNumRuntime == 1) {
        char* home = getenv("HOME");
        if(home == NULL) {
            fprintf(stderr, "$HOME is NULL.exit");
            exit(1); 
         } 

        if(!filer_cd(dir, home)) {
            err_msg("cd failed", runinfo->mSName, runinfo->mSLine);
            return FALSE;
        }
        if(!filer_add_history(dir, home)) {
            err_msg("adding hisotry of mcd failed", runinfo->mSName, runinfo->mSLine);
            return FALSE;
        }

        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_history(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    char* darg = sRunInfo_option_with_argument(runinfo, "-d");

    int dir;
    if(darg == NULL) {
        dir = adir();
    }
    else if(strcmp(darg, "all") == 0) {
        err_msg("can't get option -d all", runinfo->mSName, runinfo->mSLine);
        return FALSE;
    }
    else if(strcmp(darg, "adir") == 0) {
        dir = adir();
    }
    else if(strcmp(darg, "sdir") == 0) {
        dir = sdir();
    }
    else {
        dir = atoi(darg);

        if(dir < 0 || dir >= vector_count(gDirs)) {
            err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine);
            return FALSE;
        }
    }

    sObject* history = VECTOR_NEW_MALLOC(10);
    (void)filer_get_hitory(history, dir);

    int i;
    for(i=0; i<vector_count(history); i++) {
        char* item = string_c_str((sObject*)vector_item(history, i));

        if(!fd_write(nextout, item, strlen(item))) {
            err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            vector_delete_on_malloc(history);
            return FALSE;
        }
        if(!fd_write(nextout, "\n", 1)) {
            err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            vector_delete_on_malloc(history);
            return FALSE;
        }
    }

    vector_delete_on_malloc(history);

    runinfo->mRCode = 0;

    return TRUE;
}

BOOL cmd_file_name(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(mis_raw_mode() && nextout == gStdout) {
        return TRUE;
    }
    else if(runinfo->mArgsNumRuntime == 2) {
        char* darg = sRunInfo_option_with_argument(runinfo, "-d");

        int dir;
        if(darg == NULL) {
            dir = adir();
        }
        else if(strcmp(darg, "all") == 0) {
            err_msg("can't get option -d all", runinfo->mSName, runinfo->mSLine);
            return FALSE;
        }
        else if(strcmp(darg, "adir") == 0) {
            dir = adir();
        }
        else if(strcmp(darg, "sdir") == 0) {
            dir = sdir();
        }
        else {
            dir = atoi(darg);

            if(dir < 0 || dir >= vector_count(gDirs)) {
                err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine);
                return FALSE;
            }
        }

        char* argv2 = runinfo->mArgsRuntime[1];

        sFile* file = filer_file(dir, atoi(argv2));
        if(file) {
            if(!fd_write(nextout, string_c_str(file->mName), string_length(file->mName))) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            if(!fd_write(nextout, "\n", 1)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }

            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_path(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(mis_raw_mode() && nextout == gStdout) {
        return TRUE;
    }
    else if(runinfo->mArgsNumRuntime == 1) {
        char* darg = sRunInfo_option_with_argument(runinfo, "-d");

        int dir;
        if(darg == NULL) {
            dir = adir();
        }
        else if(strcmp(darg, "all") == 0) {
            err_msg("can't get option -d all", runinfo->mSName, runinfo->mSLine);
            return FALSE;
        }
        else if(strcmp(darg, "adir") == 0) {
            dir = adir();
        }
        else if(strcmp(darg, "sdir") == 0) {
            dir = sdir();
        }
        else {
            dir = atoi(darg);

            if(dir < 0 || dir >= vector_count(gDirs)) {
                err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine);
                return FALSE;
            }
        }

        sDir* dir2 = filer_dir(dir);
        if(dir2) {
            if(!fd_write(nextout, string_c_str(dir2->mPath), string_length(dir2->mPath))) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            if(!fd_write(nextout, "\n", 1)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }

            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_file_ext(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(mis_raw_mode() && nextout == gStdout) {
        return TRUE;
    }
    else if(runinfo->mArgsNumRuntime == 2) {
        char* darg = sRunInfo_option_with_argument(runinfo, "-d");

        int dir;
        if(darg == NULL) {
            dir = adir();
        }
        else if(strcmp(darg, "all") == 0) {
            err_msg("can't get option -d all", runinfo->mSName, runinfo->mSLine);
            return FALSE;
        }
        else if(strcmp(darg, "adir") == 0) {
            dir = adir();
        }
        else if(strcmp(darg, "sdir") == 0) {
            dir = sdir();
        }
        else {
            dir = atoi(darg);

            if(dir < 0 || dir >= vector_count(gDirs)) {
                err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine);
                return FALSE;
            }
        }

        char* argv2 = runinfo->mArgsRuntime[1];
        sFile* file = filer_file(dir, atoi(argv2));
        if(file) {
            char buf[PATH_MAX];
            extname(buf, PATH_MAX, string_c_str(file->mName));
            if(!fd_write(nextout, buf, strlen(buf))) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            if(!fd_write(nextout, "\n", 1)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }

            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_file_index(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(mis_raw_mode() && nextout == gStdout) {
        return TRUE;
    }
    else if(runinfo->mArgsNumRuntime == 2) {
        char* darg = sRunInfo_option_with_argument(runinfo, "-d");

        int dir;
        if(darg == NULL) {
            dir = adir();
        }
        else if(strcmp(darg, "all") == 0) {
            err_msg("can't get option -d all", runinfo->mSName, runinfo->mSLine);
            return FALSE;
        }
        else if(strcmp(darg, "adir") == 0) {
            dir = adir();
        }
        else if(strcmp(darg, "sdir") == 0) {
            dir = sdir();
        }
        else {
            dir = atoi(darg);

            if(dir < 0 || dir >= vector_count(gDirs)) {
                err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine);
                return FALSE;
            }
        }

        char* argv1 = runinfo->mArgsRuntime[1];

        char buf[BUFSIZ];
        int size = snprintf(buf, BUFSIZ, "%d", filer_file2(dir, argv1));

        if(!fd_write(nextout, buf, size)) {
            err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }
        if(!fd_write(nextout, "\n", 1)) {
            err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }

        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_file_user(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(mis_raw_mode() && nextout == gStdout) {
        return TRUE;
    }
    else if(runinfo->mArgsNumRuntime == 2) {
        char* darg = sRunInfo_option_with_argument(runinfo, "-d");

        int dir;
        if(darg == NULL) {
            dir = adir();
        }
        else if(strcmp(darg, "all") == 0) {
            err_msg("can't get option -d all", runinfo->mSName, runinfo->mSLine);
            return FALSE;
        }
        else if(strcmp(darg, "adir") == 0) {
            dir = adir();
        }
        else if(strcmp(darg, "sdir") == 0) {
            dir = sdir();
        }
        else {
            dir = atoi(darg);

            if(dir < 0 || dir >= vector_count(gDirs)) {
                err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine);
                return FALSE;
            }
        }

        int argv1 = atoi(runinfo->mArgsRuntime[1]);

        sFile* file = filer_file(dir, argv1);

        if(file) {
            if(!fd_write(nextout, string_c_str(file->mUser), string_length(file->mUser))) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            if(!fd_write(nextout, "\n", 1)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }

            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_file_group(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(mis_raw_mode() && nextout == gStdout) {
        return TRUE;
    }
    else if(runinfo->mArgsNumRuntime == 2) {
        char* darg = sRunInfo_option_with_argument(runinfo, "-d");

        int dir;
        if(darg == NULL) {
            dir = adir();
        }
        else if(strcmp(darg, "all") == 0) {
            err_msg("can't get option -d all", runinfo->mSName, runinfo->mSLine);
            return FALSE;
        }
        else if(strcmp(darg, "adir") == 0) {
            dir = adir();
        }
        else if(strcmp(darg, "sdir") == 0) {
            dir = sdir();
        }
        else {
            dir = atoi(darg);

            if(dir < 0 || dir >= vector_count(gDirs)) {
                err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine);
                return FALSE;
            }
        }

        int argv1 = atoi(runinfo->mArgsRuntime[1]);

        sFile* file = filer_file(dir, argv1);

        if(file) {
            if(!fd_write(nextout, string_c_str(file->mGroup), string_length(file->mGroup))) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            if(!fd_write(nextout, "\n", 1)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }

            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_file_perm(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(mis_raw_mode() && nextout == gStdout) {
        return TRUE;
    }
    else if(runinfo->mArgsNumRuntime == 2) {
        char* darg = sRunInfo_option_with_argument(runinfo, "-d");

        int dir;
        if(darg == NULL) {
            dir = adir();
        }
        else if(strcmp(darg, "all") == 0) {
            err_msg("can't get option -d all", runinfo->mSName, runinfo->mSLine);
            return FALSE;
        }
        else if(strcmp(darg, "adir") == 0) {
            dir = adir();
        }
        else if(strcmp(darg, "sdir") == 0) {
            dir = sdir();
        }
        else {
            dir = atoi(darg);

            if(dir < 0 || dir >= vector_count(gDirs)) {
                err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine);
                return FALSE;
            }
        }

        int argv1 = atoi(runinfo->mArgsRuntime[1]);

        sFile* file = filer_file(dir, argv1);

        if(file) {
            int n = file->mLStat.st_mode & S_ALLPERM;

            char buf[BUFSIZ];
            int size = snprintf(buf, BUFSIZ, "%o", n);
            if(!fd_write(nextout, buf, size)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            if(!fd_write(nextout, "\n", 1)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }

            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_file_num(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(mis_raw_mode() && nextout == gStdout) {
        return TRUE;
    }
    else if(runinfo->mArgsNumRuntime == 1) {
        char* darg = sRunInfo_option_with_argument(runinfo, "-d");

        int dir;
        if(darg == NULL) {
            dir = adir();
        }
        else if(strcmp(darg, "all") == 0) {
            err_msg("can't get option -d all", runinfo->mSName, runinfo->mSLine);
            return FALSE;
        }
        else if(strcmp(darg, "adir") == 0) {
            dir = adir();
        }
        else if(strcmp(darg, "sdir") == 0) {
            dir = sdir();
        }
        else {
            dir = atoi(darg);

            if(dir < 0 || dir >= vector_count(gDirs)) {
                err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine);
                return FALSE;
            }
        }

        sDir* dir2 = filer_dir(dir);
        if(dir2) {
            char buf[BUFSIZ];
            int size = snprintf(buf, BUFSIZ, "%d", vector_count(dir2->mFiles));
            if(!fd_write(nextout, buf, size)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            if(!fd_write(nextout, "\n", 1)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }

            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_dir_num(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(mis_raw_mode() && nextout == gStdout) {
        return TRUE;
    }
    else if(runinfo->mArgsNumRuntime == 1) {
        char buf[BUFSIZ];
        int size = snprintf(buf, BUFSIZ, "%d", vector_count(gDirs));
        if(!fd_write(nextout, buf, size)) {
            err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }
        if(!fd_write(nextout, "\n", 1)) {
            err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }

        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_cursor_num(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(mis_raw_mode() && nextout == gStdout) {
        return TRUE;
    }
    else if(runinfo->mArgsNumRuntime == 1) {
        char* darg = sRunInfo_option_with_argument(runinfo, "-d");

        int dir;
        if(darg == NULL) {
            dir = adir();
        }
        else if(strcmp(darg, "all") == 0) {
            err_msg("can't get option -d all", runinfo->mSName, runinfo->mSLine);
            return FALSE;
        }
        else if(strcmp(darg, "adir") == 0) {
            dir = adir();
        }
        else if(strcmp(darg, "sdir") == 0) {
            dir = sdir();
        }
        else {
            dir = atoi(darg);

            if(dir < 0 || dir >= vector_count(gDirs)) {
                err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine);
                return FALSE;
            }
        }

        sDir* dir2 = filer_dir(dir);

        if(dir2) {
            char buf[BUFSIZ];
            int size = snprintf(buf, BUFSIZ, "%d", dir2->mCursor);
            if(!fd_write(nextout, buf, size)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            if(!fd_write(nextout, "\n", 1)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }

            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_cursor(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(mis_raw_mode() && nextout == gStdout) {
        return TRUE;
    }
    else if(runinfo->mArgsNumRuntime == 1) {
        char* darg = sRunInfo_option_with_argument(runinfo, "-d");

        int dir;
        if(darg == NULL) {
            dir = adir();
        }
        else if(strcmp(darg, "all") == 0) {
            err_msg("can't get option -d all", runinfo->mSName, runinfo->mSLine);
            return FALSE;
        }
        else if(strcmp(darg, "adir") == 0) {
            dir = adir();
        }
        else if(strcmp(darg, "sdir") == 0) {
            dir = sdir();
        }
        else {
            dir = atoi(darg);

            if(dir < 0 || dir >= vector_count(gDirs)) {
                err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine);
                return FALSE;
            }
        }

        sDir* dir2 = filer_dir(dir);

        if(dir2) {
            sFile* cursor = filer_file(dir, dir2->mCursor);

            if(cursor) {
                char buf[PATH_MAX];
                int size = snprintf(buf, PATH_MAX, "%s", string_c_str(cursor->mName));
                if(!fd_write(nextout, buf, size)) {
                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }
                if(!fd_write(nextout, "\n", 1)) {
                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }

                runinfo->mRCode = 0;
            }
        }
    }

    return TRUE;
}

BOOL cmd_isearch(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    gISearch = TRUE;
    runinfo->mRCode = 0;

    return TRUE;
}

static void cmdline_start(char* cmdline, int cursor, BOOL quick, BOOL continue_)
{
    def_prog_mode();
    endwin();

/*
    int maxy = mgetmaxy();

    mclear_online(maxy-1);
    mclear_online(maxy);
    move(maxy-1, 0);
    refresh();
*/

    mreset_tty();

    int rcode;

    if(continue_) {
        rcode = -2;
        xyzsh_readline_interface(cmdline, cursor, NULL, 0, TRUE, FALSE);
    }
    else {
        if(!xyzsh_readline_interface_onetime(&rcode, cmdline, cursor, "cmdline", NULL, 0, NULL)) {
            rcode = -1;
        }
    }

    if(rcode == 0 || rcode == -2) {
        //filer_reread(0);
        //filer_reread(1);
        //(void)filer_reset_marks(adir());

        char buf[256];
        snprintf(buf, 256, "reread -d 0; reread -d 1; mark -a 0");
        int rcode;
        (void)xyzsh_eval(&rcode, buf, "reread", NULL, gStdin, gStdout, 0, NULL, gMFiler4);
    }

    /// EOF ///
    if(rcode == -2) {
        printf("\x1b[2K");
        //printf("\x1b[1B\x1b[2K");
        reset_prog_mode();
        xinitscr();
    }
    /// an error occures or no quick ///
    else if(rcode != 0 || !quick) {
#ifdef __CYGWIN__
        printf("\x1b[7mHIT ANY KEY\x1b[0m\r\n\x1b[1A");
#else
        printf("\x1b[7mHIT ANY KEY\x1b[0m\n\x1b[1A");
#endif
        reset_prog_mode();

        xinitscr();
        (void)getch();
    }
    /// quick ///
    else {
        reset_prog_mode();

        xinitscr();
    }

    set_signal_mfiler();
}

BOOL cmd_cmdline(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    BOOL quick = sRunInfo_option(runinfo, "-q");
    BOOL continue_ = sRunInfo_option(runinfo, "-c");

    if(runinfo->mArgsNumRuntime == 1) {
        cmdline_start("", 0, quick, continue_);
        runinfo->mRCode = 0;
    }
    else if(runinfo->mArgsNumRuntime == 2) {
        char* init_str = runinfo->mArgsRuntime[1];
        cmdline_start(init_str, -1, quick, continue_);
        runinfo->mRCode = 0;
    }
    else if(runinfo->mArgsNumRuntime == 3) {
        char* init_str = runinfo->mArgsRuntime[1];
        int cursor = atoi(runinfo->mArgsRuntime[2]);
        cmdline_start(init_str, cursor, quick, continue_);
        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_activate(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mArgsNumRuntime == 2) {
        int n = atoi(runinfo->mArgsRuntime[1]);
        if(!filer_activate(n)) {
            err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine);
            return FALSE;
        }

        runinfo->mRCode = 0;
    }


    return TRUE;
}

BOOL cmd_defmenu(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mArgsNumRuntime == 2) {
        char* menu_name = runinfo->mArgsRuntime[1];
        
        menu_create(menu_name);

        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_addmenu(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mArgsNumRuntime == 4 && runinfo->mBlocksNum == 1) {
        char* menu_name = runinfo->mArgsRuntime[1];
        char* title = runinfo->mArgsRuntime[2];
        int key = atoi(runinfo->mArgsRuntime[3]);
        BOOL external = sRunInfo_option(runinfo, "-external");
        
        if(!menu_append(menu_name, title, key, runinfo->mBlocks[0], external))
        {
             char buf[128];
            snprintf(buf, 128, "not found menu of %s", menu_name);
            err_msg(buf, runinfo->mSName, runinfo->mSLine);
            return FALSE;
        }

        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_mmenu(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mArgsNumRuntime == 2) {
        char* menu_name = runinfo->mArgsRuntime[1];

        if(!set_active_menu(menu_name)) {
            char buf[128];
            snprintf(buf, 128, "not found this menu(%s)", menu_name);
            err_msg(buf, runinfo->mSName, runinfo->mSLine);
            return FALSE;
        }

        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_reread(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mArgsNumRuntime == 1) {
        char* darg = sRunInfo_option_with_argument(runinfo, "-d");

        int dir;
        if(darg == NULL) {
            dir = adir();
        }
        else if(strcmp(darg, "all") == 0) {
            dir = -1;
        }
        else if(strcmp(darg, "adir") == 0) {
            dir = adir();
        }
        else if(strcmp(darg, "sdir") == 0) {
            dir = sdir();
        }
        else {
            dir = atoi(darg);

            if(dir < 0 || dir >= vector_count(gDirs)) {
                err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine);
                return FALSE;
            }
        }

        if(dir < 0) {
            int j;
            for(j=0; j<vector_count(gDirs); j++) {
                (void)filer_reread(j);
            }
            runinfo->mRCode = 0;
        }
        else {
            if(!filer_reread(dir)) {
                err_msg("reread error", runinfo->mSName, runinfo->mSLine);
                return FALSE;
            }
            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_mfiler4_sort(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mArgsNumRuntime == 1) {
        char* darg = sRunInfo_option_with_argument(runinfo, "-d");

        int dir;
        if(darg == NULL) {
            dir = adir();
        }
        else if(strcmp(darg, "all") == 0) {
            dir = -1;
        }
        else if(strcmp(darg, "adir") == 0) {
            dir = adir();
        }
        else if(strcmp(darg, "sdir") == 0) {
            dir = sdir();
        }
        else {
            dir = atoi(darg);

            if(dir < 0 || dir >= vector_count(gDirs)) {
                err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine);
                return FALSE;
            }
        }

        if(dir < 0) {
            int j;
            for(j=0; j<vector_count(gDirs); j++) {
                (void)filer_sort(j);
            }
            runinfo->mRCode = 0;
        }
        else {
            if(!filer_sort(dir)) {
                err_msg("reread error", runinfo->mSName, runinfo->mSLine);
                return FALSE;
            }
            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_allfiles(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(mis_raw_mode() && nextout == gStdout) {
        return TRUE;
    }
    else if(sRunInfo_option(runinfo, "-fullpath")) {
        if(runinfo->mArgsNumRuntime == 1) {
            char* darg = sRunInfo_option_with_argument(runinfo, "-d");

            int dir;
            if(darg == NULL) {
                dir = adir();
            }
            else if(strcmp(darg, "all") == 0) {
                dir = -1;
            }
            else if(strcmp(darg, "adir") == 0) {
                dir = adir();
            }
            else if(strcmp(darg, "sdir") == 0) {
                dir = sdir();
            }
            else {
                dir = atoi(darg);

                if(dir < 0 || dir >= vector_count(gDirs)) {
                    err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine);
                    return FALSE;
                }
            }

            if(dir < 0) {
                int i;
                for(i=0; i<vector_count(gDirs); i++) {
                    sDir* dir2 = filer_dir(i);

                    if(dir2) {
                        int j;
                        for(j=0; j<vector_count(dir2->mFiles); j++) {
                            sFile* file = vector_item(dir2->mFiles, j);

                            if(strcmp(string_c_str(file->mName), ".") != 0
                                && strcmp(string_c_str(file->mName), "..") != 0) 
                            {
                                char path[PATH_MAX];
                                xstrncpy(path, string_c_str(dir2->mPath), PATH_MAX);
                                xstrncat(path, string_c_str(file->mName), PATH_MAX);

                                if(!fd_write(nextout, path, strlen(path))) {
                                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                    return FALSE;
                                }
                                if(!fd_write(nextout, "\n", 1)) {
                                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                    return FALSE;
                                }
                            }
                        }
                    }
                }
            }
            else {
                sDir* dir2 = filer_dir(dir);

                if(dir2) {
                    int j;
                    for(j=0; j<vector_count(dir2->mFiles); j++) {
                        sFile* file = vector_item(dir2->mFiles, j);
                        if(strcmp(string_c_str(file->mName), ".") != 0
                            && strcmp(string_c_str(file->mName), "..") != 0) 
                        {
                            char path[PATH_MAX];
                            xstrncpy(path, string_c_str(dir2->mPath), PATH_MAX);
                            xstrncat(path, string_c_str(file->mName), PATH_MAX);

                            if(!fd_write(nextout, path, strlen(path))) {
                                err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                return FALSE;
                            }
                            if(!fd_write(nextout, "\n", 1)) {
                                err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                return FALSE;
                            }
                        }
                    }
                }
            }

            runinfo->mRCode = 0;
        }
    }
    else {
        if(runinfo->mArgsNumRuntime == 1) {
            char* darg = sRunInfo_option_with_argument(runinfo, "-d");

            int dir;
            if(darg == NULL) {
                dir = adir();
            }
            else if(strcmp(darg, "all") == 0) {
                dir = -1;
            }
            else if(strcmp(darg, "adir") == 0) {
                dir = adir();
            }
            else if(strcmp(darg, "sdir") == 0) {
                dir = sdir();
            }
            else {
                dir = atoi(darg);

                if(dir < 0 || dir >= vector_count(gDirs)) {
                    err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine);
                    return FALSE;
                }
            }

            if(dir < 0) {
                int i;
                for(i=0; i<vector_count(gDirs); i++) {
                    sDir* dir2 = filer_dir(i);

                    if(dir2) {
                        int j;
                        for(j=0; j<vector_count(dir2->mFiles); j++) {
                            sFile* file = vector_item(dir2->mFiles, j);

                            if(strcmp(string_c_str(file->mName), ".") != 0
                                && strcmp(string_c_str(file->mName), "..") != 0) 
                            {
                                if(!fd_write(nextout, string_c_str(file->mName), string_length(file->mName))) {
                                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                    return FALSE;
                                }
                                if(!fd_write(nextout, "\n", 1)) {
                                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                    return FALSE;
                                }
                            }
                        }
                    }
                }
            }
            else {
                sDir* dir2 = filer_dir(dir);

                if(dir2) {
                    int j;
                    for(j=0; j<vector_count(dir2->mFiles); j++) {
                        sFile* file = vector_item(dir2->mFiles, j);
                        if(strcmp(string_c_str(file->mName), ".") != 0
                            && strcmp(string_c_str(file->mName), "..") != 0) 
                        {
                            if(!fd_write(nextout, string_c_str(file->mName), string_length(file->mName))) {
                                err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                return FALSE;
                            }
                            if(!fd_write(nextout, "\n", 1)) {
                                err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                return FALSE;
                            }
                        }
                    }
                }
            }

            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_markfiles(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(mis_raw_mode() && nextout == gStdout) {
        return TRUE;
    }
    else if(sRunInfo_option(runinfo, "-fullpath")) {
        if(runinfo->mArgsNumRuntime == 1) {
            char* darg = sRunInfo_option_with_argument(runinfo, "-d");

            int dir;
            if(darg == NULL) {
                dir = adir();
            }
            else if(strcmp(darg, "all") == 0) {
                dir = -1;
            }
            else if(strcmp(darg, "adir") == 0) {
                dir = adir();
            }
            else if(strcmp(darg, "sdir") == 0) {
                dir = sdir();
            }
            else {
                dir = atoi(darg);

                if(dir < 0 || dir >= vector_count(gDirs)) {
                    err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine);
                    return FALSE;
                }
            }

            if(dir < 0) {
                int i;
                for(i=0; i<vector_count(gDirs); i++) {
                    if(filer_marking(i)) {
                        sDir* dir2 = filer_dir(i);

                        if(dir2) {
                            int j;
                            for(j=0; j<vector_count(dir2->mFiles); j++) {
                                sFile* file = vector_item(dir2->mFiles, j);
                                if(file->mMark) {
                                    char path[PATH_MAX];
                                    xstrncpy(path, string_c_str(dir2->mPath), PATH_MAX);
                                    xstrncat(path, string_c_str(file->mName), PATH_MAX);

                                    if(!fd_write(nextout, path, strlen(path))) {
                                        err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                        return FALSE;
                                    }
                                    if(!fd_write(nextout, "\n", 1)) {
                                        err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                        return FALSE;
                                    }
                                }
                            }
                        }
                    }
                    else {
                        sDir* dir2 = filer_dir(i);
                        sFile* file = filer_cursor_file(i);

                        if(dir2 && file) {
                            char path[PATH_MAX];
                            xstrncpy(path, string_c_str(dir2->mPath), PATH_MAX);
                            xstrncat(path, string_c_str(file->mName), PATH_MAX);

                            if(!fd_write(nextout, path, strlen(path))) {
                                err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                return FALSE;
                            }
                            if(!fd_write(nextout, "\n", 1)) {
                                err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                return FALSE;
                            }
                        }
                    }
                }
            }
            else {
                if(filer_marking(dir)) {
                    sDir* dir2 = filer_dir(dir);

                    if(dir2) {
                        int j;
                        for(j=0; j<vector_count(dir2->mFiles); j++) {
                            sFile* file = vector_item(dir2->mFiles, j);
                            if(file->mMark) {
                                char path[PATH_MAX];
                                xstrncpy(path, string_c_str(dir2->mPath), PATH_MAX);
                                xstrncat(path, string_c_str(file->mName), PATH_MAX);

                                if(!fd_write(nextout, path, strlen(path))) {
                                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                    return FALSE;
                                }
                                if(!fd_write(nextout, "\n", 1)) {
                                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                    return FALSE;
                                }
                            }
                        }
                    }
                }
                else {
                    sDir* dir2 = filer_dir(dir);
                    sFile* file = filer_cursor_file(dir);

                    if(dir2 && file) {
                        char path[PATH_MAX];
                        xstrncpy(path, string_c_str(dir2->mPath), PATH_MAX);
                        xstrncat(path, string_c_str(file->mName), PATH_MAX);

                        if(!fd_write(nextout, path, strlen(path))) {
                            err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            return FALSE;
                        }
                        if(!fd_write(nextout, "\n", 1)) {
                            err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            return FALSE;
                        }
                    }
                }
            }

            runinfo->mRCode = 0;
        }
    }
    else {
        if(runinfo->mArgsNumRuntime == 1) {
            char* darg = sRunInfo_option_with_argument(runinfo, "-d");

            int dir;
            if(darg == NULL) {
                dir = adir();
            }
            else if(strcmp(darg, "all") == 0) {
                dir = -1;
            }
            else if(strcmp(darg, "adir") == 0) {
                dir = adir();
            }
            else if(strcmp(darg, "sdir") == 0) {
                dir = sdir();
            }
            else {
                dir = atoi(darg);

                if(dir < 0 || dir >= vector_count(gDirs)) {
                    err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine);
                    return FALSE;
                }
            }

            if(dir < 0) {
                int i;
                for(i=0; i<vector_count(gDirs); i++) {
                    if(filer_marking(i)) {
                        sDir* dir2 = filer_dir(i);

                        if(dir2) {
                            int j;
                            for(j=0; j<vector_count(dir2->mFiles); j++) {
                                sFile* file = vector_item(dir2->mFiles, j);
                                if(file->mMark) {
                                    if(!fd_write(nextout, string_c_str(file->mName), string_length(file->mName))) {
                                        err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                        return FALSE;
                                    }
                                    if(!fd_write(nextout, "\n", 1)) {
                                        err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                        return FALSE;
                                    }
                                }
                            }
                        }
                    }
                    else {
                        sFile* file = filer_cursor_file(i);

                        if(file) {
                            if(!fd_write(nextout, string_c_str(file->mName), string_length(file->mName))) {
                                err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                return FALSE;
                            }
                            if(!fd_write(nextout, "\n", 1)) {
                                err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                return FALSE;
                            }
                        }
                    }
                }
            }
            else {
                if(filer_marking(dir)) {
                    sDir* dir2 = filer_dir(dir);

                    if(dir2) {
                        int j;
                        for(j=0; j<vector_count(dir2->mFiles); j++) {
                            sFile* file = vector_item(dir2->mFiles, j);
                            if(file->mMark) {
                                if(!fd_write(nextout, string_c_str(file->mName), string_length(file->mName))) {
                                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                    return FALSE;
                                }
                                if(!fd_write(nextout, "\n", 1)) {
                                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                    return FALSE;
                                }
                            }
                        }
                    }
                }
                else {
                    sFile* file = filer_cursor_file(dir);

                    if(file) {
                        if(!fd_write(nextout, string_c_str(file->mName), string_length(file->mName))) {
                            err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            return FALSE;
                        }
                        if(!fd_write(nextout, "\n", 1)) {
                            err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            return FALSE;
                        }
                    }
                }
            }

            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_mark(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    BOOL toggle = sRunInfo_option(runinfo, "-t");
    BOOL files = sRunInfo_option(runinfo, "-f");
    BOOL number = sRunInfo_option(runinfo, "-n");
    BOOL all = sRunInfo_option(runinfo, "-a");

    char* darg = sRunInfo_option_with_argument(runinfo, "-d");

    int dir;
    if(darg == NULL) {
        dir = adir();
    }
    else if(strcmp(darg, "all") == 0) {
        dir = -1;
    }
    else if(strcmp(darg, "adir") == 0) {
        dir = adir();
    }
    else if(strcmp(darg, "sdir") == 0) {
        dir = sdir();
    }
    else {
        dir = atoi(darg);

        if(dir < 0 || dir >= vector_count(gDirs)) {
            err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine);
            return FALSE;
        }
    }

    /// 状態参照 ///
    if(!all && !toggle && runinfo->mArgsNumRuntime == 2)
    {
        if(mis_raw_mode() && nextout == gStdout) {
            return TRUE;
        }

        char* arg1 = runinfo->mArgsRuntime[1];

        int arg1_num;
        if(number) {
            arg1_num = atoi(arg1);
        }
        else {
            arg1_num = filer_file2(dir, arg1);
        }

        if(arg1_num >= 0) {
            if(filer_mark(dir, arg1_num)) {
                if(!fd_write(nextout, "1", 1)) {
                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }
                if(!fd_write(nextout, "\n", 1)) {
                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }

                runinfo->mRCode = 0;
            }
            else {
                if(!fd_write(nextout, "0", 1)) {
                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }
                if(!fd_write(nextout, "\n", 1)) {
                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }

                runinfo->mRCode = 0;
            }
        }
    }
    /// 状態セット all toggleオン ///
    else if(all && toggle && runinfo->mArgsNumRuntime == 1) {
        sDir* dir2 = filer_dir(dir);

        if(dir2) {
            int i;
            for(i=0; i<vector_count(dir2->mFiles); i++) {
                sFile* file = vector_item(dir2->mFiles, i);
                if(strcmp(string_c_str(file->mName), ".") != 0
                    && strcmp(string_c_str(file->mName), "..") != 0)
                {
                    if(!files || !S_ISDIR(file->mStat.st_mode)) {
                        file->mMark = !file->mMark;
                    }
                }
            }

            runinfo->mRCode = 0;
        }
    }
    /// 状態セット all toggleオフ 数値指定 ///
    else if(all && !toggle && runinfo->mArgsNumRuntime == 2) {
        int arg1 = atoi(runinfo->mArgsRuntime[1]);

        sDir* dir2 = filer_dir(dir);

        if(dir2) {
            int i;
            for(i=0; i<vector_count(dir2->mFiles); i++) {
                sFile* file = vector_item(dir2->mFiles, i);
                if(strcmp(string_c_str(file->mName), ".") != 0
                    && strcmp(string_c_str(file->mName), "..") != 0)
                {
                    if(!files || !S_ISDIR(file->mStat.st_mode)) {
                        file->mMark = arg1 != 0;
                    }
                }
            }

            runinfo->mRCode = 0;
        }
    }

    /// 状態セット ファイルひとつ ///
    else if(!all && !toggle && runinfo->mArgsNumRuntime == 3) {
        char* arg1 = runinfo->mArgsRuntime[1];

        int arg1_num;
        if(number) {
            arg1_num = atoi(arg1);
        }
        else {
            arg1_num = filer_file2(dir, arg1);
        }

        int arg2 = atoi(runinfo->mArgsRuntime[2]);
        if(arg1_num >= 0) {
            if(!filer_set_mark(dir, arg1_num, arg2 != 0)) {
                err_msg("invalid range", runinfo->mSName, runinfo->mSLine);
                return FALSE;
            }
        }

        runinfo->mRCode = 0;
    }

    /// 状態セット ファイルひとつ ///
    else if(!all && toggle && runinfo->mArgsNumRuntime == 2) {
        char* arg1 = runinfo->mArgsRuntime[1];

        int arg1_num;
        if(number) {
            arg1_num = atoi(arg1);
        }
        else {
            arg1_num = filer_file2(dir, arg1);
        }

        if(arg1_num >= 0) {
            if(!filer_toggle_mark(dir, arg1_num)) {
                err_msg("invalid range", runinfo->mSName, runinfo->mSLine);
                return FALSE;
            }
        }

        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_new_dir(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    char* mask;
    mask = sRunInfo_option_with_argument(runinfo, "-m");
    if(mask == NULL) {
        mask = ".+";
    }

    BOOL dotdir_mask;
    char* dotdir_mask_arg = sRunInfo_option_with_argument(runinfo, "-dotdir");
    if(dotdir_mask_arg == NULL) {
        dotdir_mask = FALSE;
    }
    else {
        dotdir_mask = atoi(dotdir_mask_arg);
    }

    if(runinfo->mArgsNumRuntime == 2) {
        char* dir = runinfo->mArgsRuntime[1];
        if(!filer_new_dir(dir, dotdir_mask, mask)) {
            err_msg("invalid path", runinfo->mSName, runinfo->mSLine);
            return FALSE;
        }

        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_del_dir(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mArgsNumRuntime == 1) {
        char* darg = sRunInfo_option_with_argument(runinfo, "-d");

        int dir;
        if(darg == NULL) {
            dir = adir();
        }
        else if(strcmp(darg, "all") == 0) {
            err_msg("can't get option -d all", runinfo->mSName, runinfo->mSLine);
            return FALSE;
        }
        else if(strcmp(darg, "adir") == 0) {
            dir = adir();
        }
        else if(strcmp(darg, "sdir") == 0) {
            dir = sdir();
        }
        else {
            dir = atoi(darg);

            if(dir < 0 || dir >= vector_count(gDirs)) {
                err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine);
                return FALSE;
            }
        }

        if(!filer_del_dir(dir)) {
            err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine);
            return FALSE;
        }

        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_marking(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(mis_raw_mode() && nextout == gStdout) {
        return TRUE;
    }
    else if(runinfo->mArgsNumRuntime == 1) {
        char* darg = sRunInfo_option_with_argument(runinfo, "-d");

        int dir;
        if(darg == NULL) {
            dir = adir();
        }
        else if(strcmp(darg, "all") == 0) {
            err_msg("can't get option -d all", runinfo->mSName, runinfo->mSLine);
            return FALSE;
        }
        else if(strcmp(darg, "adir") == 0) {
            dir = adir();
        }
        else if(strcmp(darg, "sdir") == 0) {
            dir = sdir();
        }
        else {
            dir = atoi(darg);

            if(dir < 0 || dir >= vector_count(gDirs)) {
                err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine);
                return FALSE;
            }
        }

        if(filer_marking(dir)) {
            if(!fd_write(nextout, "1", 1)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            if(!fd_write(nextout, "\n", 1)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
        }
        else {
            if(!fd_write(nextout, "0", 1)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            if(!fd_write(nextout, "\n", 1)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
        }

        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_mask(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    BOOL dotdir = sRunInfo_option(runinfo, "-dotdir");
    BOOL reread = sRunInfo_option(runinfo, "-r");
    char* darg = sRunInfo_option_with_argument(runinfo, "-d");

    int dir;
    if(darg == NULL) {
        dir = adir();
    }
    else if(strcmp(darg, "all") == 0) {
        dir = -1;
    }
    else if(strcmp(darg, "adir") == 0) {
        dir = adir();
    }
    else if(strcmp(darg, "sdir") == 0) {
        dir = sdir();
    }
    else {
        dir = atoi(darg);

        if(dir < 0 || dir >= vector_count(gDirs)) {
            err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine);
            return FALSE;
        }
    }


    if(dotdir) {
        /// 状態参照 ///
        if(runinfo->mArgsNumRuntime == 1) {
            if(mis_raw_mode() && nextout == gStdout) {
                return TRUE;
            }
            if(dir < 0) {
                err_msg("can't get option -d all", runinfo->mSName, runinfo->mSLine);
                return FALSE;
            }

            if(filer_dotdir_mask(dir)) {
                if(!fd_write(nextout, "1", 1)) {
                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }
                if(!fd_write(nextout, "\n", 1)) {
                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }
            }
            else {
                if(!fd_write(nextout, "0", 1)) {
                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }
                if(!fd_write(nextout, "\n", 1)) {
                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }
            }

            runinfo->mRCode = 0;
        }

        /// 状態セット ///
        else if(runinfo->mArgsNumRuntime == 2) {
            int arg2 = atoi(runinfo->mArgsRuntime[1]);

            if(dir < 0) {
                int i;
                for(i=0; i<vector_count(gDirs); i++) {
                    (void)filer_set_dotdir_mask(i, arg2 != 0);
                }
            }
            else {
                if(!filer_set_dotdir_mask(dir, arg2 != 0)) 
                {
                    err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine);
                    return FALSE;
                }
            }

            runinfo->mRCode = 0;
        }
    }
    else {
        /// 状態参照 ///
        if(runinfo->mArgsNumRuntime == 1) {
            if(mis_raw_mode() && nextout == gStdout) {
                return TRUE;
            }
            if(dir < 0) {
                err_msg("invalid use. mask -d all", runinfo->mSName, runinfo->mSLine);
                return FALSE;
            }

            char* p = filer_mask(dir);
            if(p) {
                if(!fd_write(nextout, p, strlen(p))) {
                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }
                if(!fd_write(nextout, "\n", 1)) {
                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }

                runinfo->mRCode = 0;
            }
        }

        /// 状態セット ///
        else if(runinfo->mArgsNumRuntime == 2) {
            if(dir < 0) {
                int i;
                for(i=0; i<vector_count(gDirs); i++) {
                    if(!filer_set_mask(i, runinfo->mArgsRuntime[1])) {
                        err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine);
                        return FALSE;
                    }
                }

                if(reread) {
                    char buf[256];
                    snprintf(buf, 256, "reread -d 0; reread -d 1");
                    int rcode;
                    (void)xyzsh_eval(&rcode, buf, "reread", NULL, gStdin, gStdout, 0, NULL, gMFiler4);
                    //(void)filer_reread(0);
                    //(void)filer_reread(1);
                }
            }
            else {
                if(!filer_set_mask(dir, runinfo->mArgsRuntime[1])) {
                    err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine);
                    return FALSE;
                }

                if(reread) {
                    if(dir == 0) {
                        char buf[256];
                        snprintf(buf, 256, "reread -d %d", dir);
                        int rcode;
                        (void)xyzsh_eval(&rcode, buf, "reread", NULL, gStdin, gStdout, 0, NULL, gMFiler4);
                        //(void)filer_reread(0);
                    }
                    else if(dir == 1)  {
                        char buf[256];
                        snprintf(buf, 256, "reread -d %d", dir);
                        int rcode;
                        (void)xyzsh_eval(&rcode, buf, "reread", NULL, gStdin, gStdout, 0, NULL, gMFiler4);
                        //(void)filer_reread(1);
                    }
                }
            }

            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_vd(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    char* darg = sRunInfo_option_with_argument(runinfo, "-d");

    int dir;
    if(darg == NULL) {
        dir = adir();
    }
    else if(strcmp(darg, "all") == 0) {
        err_msg("can't get option -d all", runinfo->mSName, runinfo->mSLine);
        return FALSE;
    }
    else if(strcmp(darg, "adir") == 0) {
        dir = adir();
    }
    else if(strcmp(darg, "sdir") == 0) {
        dir = sdir();
    }
    else {
        dir = atoi(darg);

        if(dir < 0 || dir >= vector_count(gDirs)) {
            err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine);
            return FALSE;
        }
    }

    if(runinfo->mFilter) {
        fd_split(nextin, kLF, TRUE, FALSE, FALSE);

        if(!filer_vd_start(dir))  {
            err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine);
            return FALSE;
        }
        if(!filer_vd_add(dir, "..")) {
            err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine);
            return FALSE;
        }

        stack_start_stack();
        
        int i;
        for(i=0; i<vector_count(SFD(nextin).mLines); i++)
        {
            char* line = vector_item(SFD(nextin).mLines, i);
            sObject* line2 = STRING_NEW_STACK(line);
            string_chomp(line2);

            if(strcmp(string_c_str(line2), "") != 0) {
                if(!filer_vd_add(dir, string_c_str(line2))) {
                    stack_end_stack();
                    err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine);
                    return FALSE;
                }
            }

            if(gXyzshSigInt) {
                stack_end_stack();
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                return FALSE;
            }
        }

        stack_end_stack();

        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_mrename(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    BOOL reread = sRunInfo_option(runinfo, "-r");
    BOOL cursor_move = sRunInfo_option(runinfo, "-c");

    if(runinfo->mArgsNumRuntime == 3) {
        char* source = runinfo->mArgsRuntime[1];
        char* destination = runinfo->mArgsRuntime[2];

        if(access(source, F_OK) != 0) {
            char buf[128];
            snprintf(buf, 128, "%s does not exist.", destination);
            err_msg(buf, runinfo->mSName, runinfo->mSLine);
            return FALSE;
        }

        if(access(destination, F_OK) == 0) {
            char buf[128];
            snprintf(buf, 128, "%s exists. can't rename", destination);
            err_msg(buf, runinfo->mSName, runinfo->mSLine);
            return FALSE;
        }

        if(rename(source, destination) < 0) {
            err_msg("rename error", runinfo->mSName, runinfo->mSLine);
            return FALSE;
        }

        if(reread) {
            //(void)filer_reread(adir());
            char buf[256];
            snprintf(buf, 256, "reread -d %d", adir());
            int rcode;
            (void)xyzsh_eval(&rcode, buf, "reread", NULL, gStdin, gStdout, 0, NULL, gMFiler4);
        }
        
        if(cursor_move) {
            int n = filer_file2(adir(), destination);
            if(n >= 0) (void)filer_cursor_move(adir(), n);
        }

        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_row(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(mis_raw_mode() && nextout == gStdout) {
        return TRUE;
    }
    else {
        char buf[BUFSIZ];
        int size = snprintf(buf, BUFSIZ, "%d", filer_row(adir()));
        if(!fd_write(nextout, buf, size)) {
            err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }
        if(!fd_write(nextout, "\n", 1)) {
            err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }
        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_row_max(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(mis_raw_mode() && nextout == gStdout) {
        return TRUE;
    }
    else {
        char buf[BUFSIZ];
        int size = snprintf(buf, BUFSIZ, "%d", filer_row_max(adir()));
        if(!fd_write(nextout, buf, size)) {
            err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }
        if(!fd_write(nextout, "\n", 1)) {
            err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }
        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_line(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(mis_raw_mode() && nextout == gStdout) {
        return TRUE;
    }
    else {
        char buf[BUFSIZ];
        int size = snprintf(buf, BUFSIZ, "%d", filer_line(adir()));
        if(!fd_write(nextout, buf, size)) {
            err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }
        if(!fd_write(nextout, "\n", 1)) {
            err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }
        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_line_max(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(mis_raw_mode() && nextout == gStdout) {
        return TRUE;
    }
    else {
        char buf[BUFSIZ];
        int size = snprintf(buf, BUFSIZ, "%d", filer_line_max(adir()));
        if(!fd_write(nextout, buf, size)) {
            err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }
        if(!fd_write(nextout, "\n", 1)) {
            err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }
        runinfo->mRCode = 0;
    }

    return TRUE;
}

//// 端末制御がかかわるもの ///
static int gProgressMark = 0;

static void draw_progress_box(int mark_num)
{
    const int maxx = mgetmaxx();
    const int maxy = mgetmaxy();
    
    const int y = maxy/2;
    
    mbox(y, (maxx-22)/2, 22, 3);
    mvprintw(y, (maxx-22)/2+2, "progress");
    mvprintw(y+1, (maxx-22)/2+1, "(%d/%d) files", gProgressMark, mark_num);
}

// when returning TRUE, log is opened. you need to call finish_logging or fclose(log);
static BOOL ready_for_logging(FILE** log)
{
    char log_path[PATH_MAX];
    snprintf(log_path, PATH_MAX, "%s/copy_file.log", gHomeDir);
    *log = fopen(log_path, "w");

    if(*log == NULL) {
        merr_msg("can't open the log file");
        return FALSE;
    }

    fprintf(*log, "-+- the path failed with copying(%s/copy_file.log) -+-\n\n", gHomeDir);

    return TRUE;
}

static BOOL finish_logging(FILE* log, int err_num)
{
    /// an error occurs, show log ///
    if(err_num > 0) {
        fprintf(log, "\nThere is %d errors\n", err_num);
        fclose(log);

        if(mis_raw_mode()) {
            merr_msg("There is %d errors. see log with running less %s/copy_file.log", err_num, gHomeDir);

            def_prog_mode();
            endwin();
            mreset_tty();

            int rcode;
            char buf[1024];
            snprintf(buf, 1024, "less '%s/copy_file.log'", gHomeDir);
            (void)xyzsh_eval(&rcode, buf, "less", NULL, gStdin, gStdout, 0, NULL, gMFiler4);

            reset_prog_mode();

            xinitscr();
        }
        else {
            fprintf(stderr, "There is %d errors. see log with running less %s/copy_file.log", err_num, gHomeDir);

            int rcode;
            char buf[1024];
            snprintf(buf, 1024, "cat '%s/copy_file.log'", gHomeDir);
            (void)xyzsh_eval(&rcode, buf, "less", NULL, gStdin, gStdout, 0, NULL, gMFiler4);
        }
    }
    else {
        fclose(log);
    }
}

BOOL cmd_mcp(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    BOOL raw_mode = mis_raw_mode();
    if(!raw_mode) {
        xinitscr();
    }

    BOOL preserve = sRunInfo_option(runinfo, "-p");

    if(runinfo->mArgsNumRuntime == 2) {
        char destination[PATH_MAX];
        xstrncpy(destination, runinfo->mArgsRuntime[1], PATH_MAX);

        /// go ///
        sObject* markfiles = filer_mark_files(adir());
        const int mark_file_num = vector_count(markfiles);
        gProgressMark = mark_file_num;

        sDir* dir = filer_dir(adir());
        if(dir) {
            enum eCopyOverrideWay override_way = kNone;

            /// ready for the log file ///
            FILE* log;
            int err_num = 0;

            if(!ready_for_logging(&log)) {
                if(!raw_mode) { endwin(); }
                return FALSE;
            }

            int j;
            for(j=0; j<mark_file_num; j++) {
                sFile* file = vector_item(markfiles, j);
                char* fname = string_c_str(file->mName);
                char source[PATH_MAX];

                xstrncpy(source, string_c_str(dir->mPath), PATH_MAX);
                xstrncat(source, fname, PATH_MAX);
                
                if(strcmp(fname, ".") != 0 && strcmp(fname, "..") != 0) {
                    int num;
                    if((num = filer_file2(adir(), fname)) < 0) 
                    {
                        merr_msg("%s doesn't exist", fname);
                        break;
                    }
                    else
                    {
                        (void)filer_cursor_move(adir(), num);

                        view(); // これはコピーの上書き確認のときに正しい位置にカーソルがある必要があったり、＊印の描写などが正しく描写されている必要があるので必要
                        //draw_progress_box(mark_file_num);
                        refresh();
                        
                        if(!copy_file(source, destination, FALSE, preserve, &override_way, log, &err_num)) {
                            break;
                        }

                        gProgressMark--;
                        
                        (void)filer_set_mark(adir(), num, FALSE);
                    }
                }
            }
            vector_delete_on_malloc(markfiles);
            
            dir = filer_dir(0);
            if(strcmp(destination, string_c_str(dir->mPath)) == 0) {
                //(void)filer_reread(0);
                char buf[256];
                snprintf(buf, 256, "reread -d %d", 0);
                int rcode;
                (void)xyzsh_eval(&rcode, buf, "reread", NULL, gStdin, gStdout, 0, NULL, gMFiler4);
            }

            dir = filer_dir(1);
            if(strcmp(destination, string_c_str(dir->mPath)) == 0) {
                //(void)filer_reread(1);
                char buf[256];
                snprintf(buf, 256, "reread -d %d", 1);
                int rcode;
                (void)xyzsh_eval(&rcode, buf, "reread", NULL, gStdin, gStdout, 0, NULL, gMFiler4);
            }

            finish_logging(log, err_num);

            runinfo->mRCode = 0;
        }
    }

    if(!raw_mode) {
        endwin();
    }

    return TRUE;
}

BOOL cmd_mbackup(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    BOOL raw_mode = mis_raw_mode();
    if(!raw_mode) {
        xinitscr();
    }

    BOOL preserve = sRunInfo_option(runinfo, "-p");

    if(runinfo->mArgsNumRuntime == 2) {
        sDir* dir = filer_dir(adir());

        if(dir) {
            /// 引数チェック ///
            char destination[PATH_MAX];
            xstrncpy(destination, string_c_str(dir->mPath), PATH_MAX);
            xstrncat(destination, runinfo->mArgsRuntime[1], PATH_MAX);

            /// 対象があるかどうかチェック ///
            if(access(destination, F_OK) == 0) {
                err_msg("destination exists", runinfo->mSName, runinfo->mSLine);
                if(!raw_mode) {
                    endwin();
                }
                return FALSE;
            }

            /// go //
            sFile* file = filer_cursor_file(adir());
            
            if(file) {
                char* fname = string_c_str(file->mName);
                char source[PATH_MAX];

                xstrncpy(source, string_c_str(dir->mPath), PATH_MAX);
                xstrncat(source, fname, PATH_MAX);
                    
                if(strcmp(fname, ".") != 0 && strcmp(fname, "..") != 0) {
                    int num;
                    if((num = filer_file2(adir(), fname)) < 0) 
                    {
                        char buf[128];
                        snprintf(buf, 128, "%s doesn't exist", fname);
                        err_msg(buf, runinfo->mSName, runinfo->mSLine);
                        if(!raw_mode) {
                            endwin();
                        }
                        return FALSE;
                    }
                    else
                    {
                        (void)filer_cursor_move(adir(), num);

                        view(); // これはコピーの上書き確認のときに正しい位置にカーソルがある必要があったり、＊印の描写などが正しく描写されている必要があるので必要
                        //draw_progress_box(mark_file_num);
                        refresh();

                        /// ready for the log file ///
                        FILE* log;
                        int err_num = 0;

                        if(!ready_for_logging(&log)) {
                            if(!raw_mode) { endwin(); }
                            return FALSE;
                        }

                        enum eCopyOverrideWay override_way = kNone;
                        
                        if(!copy_file(source, destination, FALSE, preserve, &override_way, log, &err_num)) {
                            err_msg("", runinfo->mSName, runinfo->mSLine);
                            if(!raw_mode) { endwin(); }
                            fclose(log);
                            return FALSE;
                        }

                        (void)filer_set_mark(adir(), num, FALSE);

                        finish_logging(log, err_num);
                    }
                }

                //(void)filer_reread(adir());
                char buf[256];
                snprintf(buf, 256, "reread -d %d", adir());
                int rcode;
                (void)xyzsh_eval(&rcode, buf, "reread", NULL, gStdin, gStdout, 0, NULL, gMFiler4);
                runinfo->mRCode = 0;
            }
        }
    }

    if(!raw_mode) {
        endwin();
    }
    return TRUE;
}

BOOL cmd_mmv(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    BOOL raw_mode = mis_raw_mode();
    if(!raw_mode) {
        xinitscr();
    }

    BOOL force = sRunInfo_option(runinfo, "-f");
    BOOL preserve = sRunInfo_option(runinfo, "-p");

    if(runinfo->mArgsNumRuntime == 2) {
        char destination[PATH_MAX];
        xstrncpy(destination, runinfo->mArgsRuntime[1], PATH_MAX);

        /// go ///
        sDir* dir = filer_dir(adir());

        if(dir) {
            sObject* markfiles = filer_mark_files(adir());
            const int mark_file_num = vector_count(markfiles);
            gProgressMark = mark_file_num;

            enum eCopyOverrideWay override_way;
            if(force) {
                override_way = kYesAll;
            }
            else {
                override_way = kNone;
            }

            /// ready for the log file ///
            FILE* log;
            int err_num = 0;

            if(!ready_for_logging(&log)) {
                if(!raw_mode) { endwin(); }
                return FALSE;
            }

            /// go ///
            int j;
            for(j=0; j<mark_file_num; j++) {
                sFile* file = vector_item(markfiles, j);
                char* fname = string_c_str(file->mName);
                char source[PATH_MAX];

                xstrncpy(source, string_c_str(dir->mPath), PATH_MAX);
                xstrncat(source, fname, PATH_MAX);

                if(strcmp(fname, ".") != 0 && strcmp(fname, "..") != 0) {
                    int num;
                    if((num = filer_file2(adir(), fname)) < 0) {
                        merr_msg("%s doesn't exist", fname);
                        break;
                    }
                    else
                    {
                        (void)filer_cursor_move(adir(), num);

                        view(); // これはコピーの上書き確認のときに正しい位置にカーソルがある必要があったり、＊印の描写などが正しく描写されている必要があるので必要
                        //draw_progress_box(mark_file_num);
                        refresh();
                        
                        if(!copy_file(source, destination, TRUE, preserve, &override_way, log, &err_num)) {
                            break;
                        }

                        gProgressMark--;
                        
                        (void)filer_set_mark(adir(), num, FALSE);
                    }
                }
            }
            vector_delete_on_malloc(markfiles);

            /*
            mclear();
            view();
            refresh();
            */

            //(void)filer_reread(0);
            //(void)filer_reread(1);
            char buf[256];
            snprintf(buf, 256, "reread -d 0; reread -d 1");
            int rcode;
            (void)xyzsh_eval(&rcode, buf, "reread", NULL, gStdin, gStdout, 0, NULL, gMFiler4);

            runinfo->mRCode = 0;

            finish_logging(log, err_num);
        }
    }

    if(!raw_mode) {
        endwin();
    }
    return TRUE;
}

BOOL cmd_mrm(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    BOOL raw_mode = mis_raw_mode();
    if(!raw_mode) {
        xinitscr();
    }

    if(!mis_raw_mode()) {
        err_msg("invalid terminal setting. not raw mode", runinfo->mSName, runinfo->mSLine);

        if(!raw_mode) {
            endwin();
        }
        return FALSE;
    }

    /// ready for the log file ///
    char log_path[PATH_MAX];
    snprintf(log_path, PATH_MAX, "%s/rm_file.log", gHomeDir);
    FILE* log = fopen(log_path, "w");

    fprintf(log, "-+- failed path of rm files -+-\n\n");
                
    if(runinfo->mArgsNumRuntime == 1) {
        /// go ///
        sObject* markfiles = filer_mark_files(adir());
        const int mark_file_num = vector_count(markfiles);
        gProgressMark = mark_file_num;

        if(log == NULL) {
            merr_msg("can't open the log file");
            return FALSE;
        }

        int j;
        for(j=0; j<mark_file_num; j++) {
            sFile* file = vector_item(markfiles, j);
            char* item = string_c_str(file->mName);

            if(strcmp(item, ".") != 0 && strcmp(item, "..") != 0) {
                char source[PATH_MAX];

                sDir* dir = filer_dir(adir());
                if(dir) {
                    xstrncpy(source, string_c_str(dir->mPath), PATH_MAX);
                    xstrncat(source, item, PATH_MAX);

                    int num;
                    if((num = filer_file2(adir(), item)) < 0) {
                        merr_msg("%s doesn't exist", item);
                        break;
                    }
                    else {
                        (void)filer_cursor_move(adir(), num);

                        view(); // これはコピーの上書き確認のときに正しい位置にカーソルがある必要があったり、＊印の描写などが正しく描写されている必要があるので必要
                        //draw_progress_box(mark_file_num);
                        refresh();

                        int err_num;
                        
                        if(!remove_file(source, FALSE, TRUE, &err_num, log)) {
                            break;
                        }

                        gProgressMark--;
                        
                        (void)filer_set_mark(adir(), num, FALSE);
                    }
                }
            }
        }
        vector_delete_on_malloc(markfiles);
        
        /*
        mclear();
        view();
        refresh();
        */

        //(void)filer_reread(0);
        //(void)filer_reread(1);
        char buf[256];
        snprintf(buf, 256, "reread -d 0; reread -d 1");
        int rcode;
        (void)xyzsh_eval(&rcode, buf, "reread", NULL, gStdin, gStdout, 0, NULL, gMFiler4);

        runinfo->mRCode = 0;
    }

    fclose(log);

    if(!raw_mode) {
        endwin();
    }
    return TRUE;
}

BOOL cmd_mtrashbox(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    BOOL raw_mode = mis_raw_mode();
    if(!raw_mode) {
        xinitscr();
    }

    BOOL force = sRunInfo_option(runinfo, "-f");
    BOOL preserve = sRunInfo_option(runinfo, "-p");

    if(runinfo->mArgsNumRuntime == 1) {
        /// 引数チェック ///
        char destination[PATH_MAX];

        char* env = getenv("TRASHBOX_DIR");
        if(env) {
            xstrncpy(destination, env, PATH_MAX);
        }
        else {
            xstrncpy(destination, getenv("MF4HOME"), PATH_MAX);
            xstrncat(destination, "/trashbox/", PATH_MAX);
        }

        if(destination[strlen(destination)-1] != '/') {
            xstrncat(destination, "/", PATH_MAX);
        }

        /// go ///
        sDir* dir = filer_dir(adir());

        if(dir) {
            sObject* markfiles = filer_mark_files(adir());
            const int mark_file_num = vector_count(markfiles);
            gProgressMark = mark_file_num;

            enum eCopyOverrideWay override_way;
            if(force) {
                override_way = kYesAll;
            }
            else {
                override_way = kNone;
            }

            /// ready for the log file ///
            FILE* log;
            int err_num = 0;

            if(!ready_for_logging(&log)) {
                if(!raw_mode) { endwin(); }
                return FALSE;
            }

            /// go ///
            int j;
            for(j=0; j<mark_file_num; j++) {
                sFile* file = vector_item(markfiles, j);
                char* fname = string_c_str(file->mName);
                char source[PATH_MAX];

                xstrncpy(source, string_c_str(dir->mPath), PATH_MAX);
                xstrncat(source, fname, PATH_MAX);

                if(strcmp(fname, ".") != 0 && strcmp(fname, "..") != 0) {
                    int num;
                    if((num = filer_file2(adir(), fname)) < 0) {
                        merr_msg("%s doesn't exist", fname);
                        break;
                    }
                    else
                    {
                        (void)filer_cursor_move(adir(), num);

                        view(); // これはコピーの上書き確認のときに正しい位置にカーソルがある必要があったり、＊印の描写などが正しく描写されている必要があるので必要
                        //draw_progress_box(mark_file_num);
                        refresh();
                        
                        if(!copy_file(source, destination, TRUE, preserve, &override_way, log, &err_num)) {
                            break;
                        }

                        gProgressMark--;
                        
                        (void)filer_set_mark(adir(), num, FALSE);
                    }
                }
            }
            vector_delete_on_malloc(markfiles);

            /*
            mclear();
            view();
            refresh();
            */

            //(void)filer_reread(0);
            //(void)filer_reread(1);
            
            char buf[256];
            snprintf(buf, 256, "reread -d 0; reread -d 1");
            int rcode;
            (void)xyzsh_eval(&rcode, buf, "reread", NULL, gStdin, gStdout, 0, NULL, gMFiler4);

            runinfo->mRCode = 0;

            finish_logging(log, err_num);
        }
    }

    if(!raw_mode) {
        endwin();
    }
    return TRUE;
}

BOOL cmd_mln(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    BOOL raw_mode = mis_raw_mode();
    if(!raw_mode) {
        xinitscr();
    }

    if(runinfo->mArgsNumRuntime == 2) {
        /// 引数チェック ///
        char destination[PATH_MAX];
        xstrncpy(destination, runinfo->mArgsRuntime[1], PATH_MAX);

        if(destination[strlen(destination)-1] != '/') {
            xstrncat(destination, "/", PATH_MAX);
        }

        /// ディレクトリが無いなら作成 ///
        if(access(destination, F_OK) != 0) {
            char* str[] = {
                "yes", "no"
            };

            char buf[BUFSIZ];
            snprintf(buf, BUFSIZ, "%s doesn't exist. create?", destination);

            if(select_str(buf, str, 2, 1) == 0) {
                snprintf(buf, BUFSIZ, "mkdir -p %s", destination);
                if(system(buf) < 0) {
                    char buf[128];
                    snprintf(buf, 128, "mcp: making directory err(%s)", destination);
                    err_msg(buf, runinfo->mSName, runinfo->mSLine);
                    if(!raw_mode) {
                        endwin();
                    }
                    return FALSE;
                }
            }
            else {
                char buf[128];
                snprintf(buf, 128, "mcp: destination err(%s)", destination);
                err_msg(buf, runinfo->mSName, runinfo->mSLine);
                if(!raw_mode) {
                    endwin();
                }
                return FALSE;
            }
        }

        /// 目標ディレクトリがディレクトリじゃないならエラー ///
        struct stat dstat;
        if(stat(destination, &dstat) < 0 || !S_ISDIR(dstat.st_mode)) {
            err_msg("mcp: destination is not directory", runinfo->mSName, runinfo->mSLine);
            if(!raw_mode) {
                endwin();
            }
            return FALSE;
        }

        /// go ///
        sObject* markfiles = filer_mark_files(adir());
        const int mark_file_num = vector_count(markfiles);
        gProgressMark = mark_file_num;

        sDir* dir = filer_dir(adir());
        if(dir) {
            int j;
            for(j=0; j<mark_file_num; j++) {
                sFile* file = vector_item(markfiles, j);
                char* fname = string_c_str(file->mName);
                char source[PATH_MAX];

                xstrncpy(source, string_c_str(dir->mPath), PATH_MAX);
                xstrncat(source, fname, PATH_MAX);

                if(strcmp(fname, ".") != 0 && strcmp(fname, "..") != 0) {
                    int num;
                    if((num = filer_file2(adir(), fname)) < 0) 
                    {
                        merr_msg("%s doesn't exist", fname);
                        break;
                    }
                    else
                    {
                        (void)filer_cursor_move(adir(), num);

                        view(); // これはコピーの上書き確認のときに正しい位置にカーソルがある必要があったり、＊印の描写などが正しく描写されている必要があるので必要
                        //draw_progress_box(mark_file_num);
                        refresh();

                        char dfile[PATH_MAX];
                        xstrncpy(dfile, destination, PATH_MAX);
                        xstrncat(dfile, fname, PATH_MAX);

                        if(symlink(source, dfile) < 0) {
                            merr_msg("can't make symlink(%s). Is it attempt to make on vfat?", dfile);
                            break;
                        }

                        gProgressMark--;
                        
                        (void)filer_set_mark(adir(), num, FALSE);
                    }
                }
            }
            vector_delete_on_malloc(markfiles);

            /*
            mclear();
            view();
            refresh();
            */

            dir = filer_dir(0);
            if(strcmp(destination, string_c_str(dir->mPath)) == 0) {
                //(void)filer_reread(0);
                char buf[256];
                snprintf(buf, 256, "reread -d 0");
                int rcode;
                (void)xyzsh_eval(&rcode, buf, "reread", NULL, gStdin, gStdout, 0, NULL, gMFiler4);
            }

            dir = filer_dir(1);
            if(strcmp(destination, string_c_str(dir->mPath)) == 0) {
                //(void)filer_reread(1);
                char buf[256];
                snprintf(buf, 256, "reread -d 1");
                int rcode;
                (void)xyzsh_eval(&rcode, buf, "reread", NULL, gStdin, gStdout, 0, NULL, gMFiler4);
            }

            runinfo->mRCode = 0;
        }
    }

    if(!raw_mode) {
        endwin();
    }

    return TRUE;
}

BOOL cmd_mchoice(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mArgsNumRuntime >= 3) {
        BOOL make_raw = FALSE;
        if(!mis_raw_mode()) {
            make_raw = TRUE;
            xinitscr();
        }
        char* msg = runinfo->mArgsRuntime[1];
        char** str = MALLOC(sizeof(char*)*runinfo->mArgsNumRuntime);

        int j;
        for(j=2; j<runinfo->mArgsNumRuntime; j++) {
            str[j-2] = runinfo->mArgsRuntime[j];
        }

        char* smsg = choice(msg, str, runinfo->mArgsNumRuntime -2, -1);
        FREE(str);
        char buf[BUFSIZ];
        int size;
        if(smsg == NULL) {
            size = snprintf(buf, BUFSIZ, "cancel");
        }
        else {
            size = snprintf(buf, BUFSIZ, "%s", smsg);
        }

        if(!fd_write(nextout, buf, size)) {
            err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }
        if(!fd_write(nextout, "\n", 1)) {
            err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }

        runinfo->mRCode = 0;

        if(make_raw) {
            endwin();
        }

        const int maxy = mgetmaxy();
        mmove_immediately(maxy-2, 0);
    }

    return TRUE;
}

BOOL cmd_kanjicode_file_name(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(sRunInfo_option(runinfo, "-byte")) {
        gKanjiCodeFileName = kByte;
        runinfo->mRCode = 0;
    }
    else if(sRunInfo_option(runinfo, "-utf8")) {
        gKanjiCodeFileName = kUtf8;
        runinfo->mRCode = 0;
    }
    else if(sRunInfo_option(runinfo, "-utf8-mac")) {
        gKanjiCodeFileName = kUtf8Mac;
        runinfo->mRCode = 0;
    }
    else {
        if(mis_raw_mode() && nextout == gStdout) {
            return TRUE;
        }
        else if(gKanjiCodeFileName == kByte) {
            if(!fd_write(nextout, "byte", 4)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            if(!fd_write(nextout, "\n", 1)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }

            runinfo->mRCode = 0;
        }
        else if(gKanjiCodeFileName == kUtf8) {
            if(!fd_write(nextout, "utf8", 4)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            if(!fd_write(nextout, "\n", 1)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }

            runinfo->mRCode = 0;
        }
        else if(gKanjiCodeFileName == kUtf8Mac) {
            if(!fd_write(nextout, "utf8-mac", 8)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            if(!fd_write(nextout, "\n", 1)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }

            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_mclear_immediately(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(!mis_raw_mode()) {
        err_msg("invalid terminal setting. not raw mode", runinfo->mSName, runinfo->mSLine);
        return FALSE;
    }

    //xclear_immediately();
    clear();
    view();
    refresh();

    runinfo->mRCode = 0;

    return TRUE;
}

BOOL cmd_adir(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(mis_raw_mode() && nextout == gStdout) {
        return TRUE;
    }
    else {
        char buf[256];
        int size = snprintf(buf, 256, "%d", adir());

        if(!fd_write(nextout, buf, size)) {
            err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }
        if(!fd_write(nextout, "\n", 1)) {
            err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }

        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_sdir(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(mis_raw_mode() && nextout == gStdout) {
        return TRUE;
    }
    else {
        char buf[256];
        int size = snprintf(buf, 256, "%d", sdir());

        if(!fd_write(nextout, buf, size)) {
            err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }
        if(!fd_write(nextout, "\n", 1)) {
            err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }

        runinfo->mRCode = 0;
    }

    return TRUE;
}

sObject* gMFiler4Prompt;

BOOL cmd_mfiler4_prompt(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    if(runinfo->mBlocksNum == 1) {
        gMFiler4Prompt = block_clone_on_gc(runinfo->mBlocks[0], T_BLOCK, FALSE);
        uobject_put(gMFiler4, "_prompt", gMFiler4Prompt);
        runinfo->mRCode = 0;
    }

    return TRUE;
}

void commands_init()
{
    gMFiler4 = UOBJECT_NEW_GC(8, gRootObject, "mfiler4", TRUE);
    uobject_init(gMFiler4);
    uobject_put(gRootObject, "mfiler4", gMFiler4);

    gMFiler4System = UOBJECT_NEW_GC(8, gXyzshObject, "mfiler4", TRUE);
    uobject_init(gMFiler4System);
    uobject_put(gXyzshObject, "mfiler4", gMFiler4System);

    xyzsh_add_inner_command(gMFiler4, "quit", cmd_quit, 0);
    xyzsh_add_inner_command(gMFiler4, "keycommand", cmd_keycommand, 0);
    xyzsh_add_inner_command(gMFiler4, "cursor_move", cmd_cursor_move, 1, "-d");
    xyzsh_add_inner_command(gMFiler4, "mcd", cmd_mcd, 1, "-d");
    xyzsh_add_inner_command(gMFiler4, "file_num", cmd_file_num, 1, "-d");
    xyzsh_add_inner_command(gMFiler4, "file_perm", cmd_file_perm, 1, "-d");
    xyzsh_add_inner_command(gMFiler4, "file_group", cmd_file_group, 1, "-d");
    xyzsh_add_inner_command(gMFiler4, "file_user", cmd_file_user, 1, "-d");
    xyzsh_add_inner_command(gMFiler4, "file_index", cmd_file_index, 1, "-d");
    xyzsh_add_inner_command(gMFiler4, "file_ext", cmd_file_ext, 1, "-d");
    xyzsh_add_inner_command(gMFiler4, "file_name", cmd_file_name, 1, "-d");
    xyzsh_add_inner_command(gMFiler4, "path", cmd_path, 1, "-d");
    xyzsh_add_inner_command(gMFiler4, "cursor_num", cmd_cursor_num, 1, "-d");
    xyzsh_add_inner_command(gMFiler4, "isearch", cmd_isearch, 0);
    xyzsh_add_inner_command(gMFiler4, "mchoice", cmd_mchoice, 0);
    xyzsh_add_inner_command(gMFiler4, "activate", cmd_activate, 0);
    xyzsh_add_inner_command(gMFiler4, "defmenu", cmd_defmenu, 0);
    xyzsh_add_inner_command(gMFiler4, "addmenu", cmd_addmenu, 0);
    xyzsh_add_inner_command(gMFiler4, "mmenu", cmd_mmenu, 0);
    xyzsh_add_inner_command(gMFiler4, "reread", cmd_reread, 1, "-d");
    xyzsh_add_inner_command(gMFiler4, "sort", cmd_mfiler4_sort, 1, "-d");
    xyzsh_add_inner_command(gMFiler4, "markfiles", cmd_markfiles, 1, "-d");
    xyzsh_add_inner_command(gMFiler4, "allfiles", cmd_allfiles, 1, "-d");
    xyzsh_add_inner_command(gMFiler4, "mark", cmd_mark, 1, "-d");
    xyzsh_add_inner_command(gMFiler4, "marking", cmd_marking, 1, "-d");
    xyzsh_add_inner_command(gMFiler4, "mclear_immediately", cmd_mclear_immediately, 0);
    xyzsh_add_inner_command(gMFiler4, "vd", cmd_vd, 1, "-d");
    xyzsh_add_inner_command(gMFiler4, "mcp", cmd_mcp, 0);
    xyzsh_add_inner_command(gMFiler4, "mrm", cmd_mrm, 0);
    xyzsh_add_inner_command(gMFiler4, "mmv", cmd_mmv, 0);
    xyzsh_add_inner_command(gMFiler4, "mtrashbox", cmd_mtrashbox, 0);
    xyzsh_add_inner_command(gMFiler4, "mln", cmd_mln, 0);
    xyzsh_add_inner_command(gMFiler4, "mrename", cmd_mrename, 0);
    xyzsh_add_inner_command(gMFiler4, "mask", cmd_mask, 1, "-d");
    xyzsh_add_inner_command(gMFiler4, "dir_num", cmd_dir_num, 0);
    xyzsh_add_inner_command(gMFiler4, "del_dir", cmd_del_dir, 1, "-d");
    xyzsh_add_inner_command(gMFiler4, "new_dir", cmd_new_dir, 2, "-m", "-dotdir");
    xyzsh_add_inner_command(gMFiler4, "mbackup", cmd_mbackup, 0);

    xyzsh_add_inner_command(gMFiler4, "row", cmd_row, 0);
    xyzsh_add_inner_command(gMFiler4, "row_max", cmd_row_max, 0);
    xyzsh_add_inner_command(gMFiler4, "line", cmd_line, 0);
    xyzsh_add_inner_command(gMFiler4, "line_max", cmd_line_max, 0);
    xyzsh_add_inner_command(gMFiler4, "adir", cmd_adir, 0);
    xyzsh_add_inner_command(gMFiler4, "sdir", cmd_sdir, 0);
    xyzsh_add_inner_command(gMFiler4, "kanjicode_file_name", cmd_kanjicode_file_name, 0);

    xyzsh_add_inner_command(gMFiler4, "cmdline", cmd_cmdline, 0);
    xyzsh_add_inner_command(gMFiler4, "cursor", cmd_cursor, 1, "-d");
    xyzsh_add_inner_command(gMFiler4, "prompt", cmd_mfiler4_prompt, 0);
    xyzsh_add_inner_command(gMFiler4, "start_color", cmd_start_color, 0);
    xyzsh_add_inner_command(gMFiler4, "history", cmd_history, 1, "-d");
}

void commands_final()
{
}


