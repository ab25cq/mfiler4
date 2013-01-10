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
    sCommand* command = runinfo->mCommand;

    if(xyzsh_job_num() == 0) {
        if(command->mArgsNumRuntime >= 2) {
            gMainLoop = atoi(command->mArgsRuntime[1]);
        }
        else {
            gMainLoop = 0;
        }
        runinfo->mRCode = 0;
    }
    else {
        err_msg("jobs exist", runinfo->mSName, runinfo->mSLine, "quit");
        return FALSE;
    }

    return TRUE;
}

BOOL cmd_keycommand(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    if(command->mBlocksNum == 1 && command->mArgsNumRuntime == 2) {
        BOOL external = sCommand_option_item(command, "-external");

        char* argv1 = command->mArgsRuntime[1];
        int keycode = atoi(argv1);

        int meta = 0;
        if(sCommand_option_item(command, "-m")) { meta = 1; }

        filer_add_keycommand(meta, keycode, command->mBlocks[0], "run time script", external);

        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_cursor_move(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    if(command->mArgsNumRuntime == 2) {
        char* darg = sCommand_option_with_argument_item(command, "-d");

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
                err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                return FALSE;
            }
        }

        char* argv2 = command->mArgsRuntime[1];

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
                err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                return FALSE;
            }

            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_mcd(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    char* darg = sCommand_option_with_argument_item(command, "-d");

    int dir;
    if(darg == NULL) {
        dir = adir();
    }
    else if(strcmp(darg, "all") == 0) {
        err_msg("can't get option -d all", runinfo->mSName, runinfo->mSLine, "mcd");
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
            err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            return FALSE;
        }
    }

    if(command->mArgsNumRuntime == 2) {
        char* arg1 = command->mArgsRuntime[1];

        if(strcmp(arg1, "+") == 0) {
            if(!filer_history_forward(dir)) {
                err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                return FALSE;
            }

            runinfo->mRCode = 0;
        }
        else if(strcmp(arg1, "-") == 0) {
            if(!filer_history_back(dir)) {
                err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                return FALSE;
            }

            runinfo->mRCode = 0;
        }
        else {
            if(!filer_cd(dir, arg1)) {
                err_msg("cd failed", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                return FALSE;
            }
            if(!filer_add_history(dir, arg1)) {
                err_msg("adding hisotry of mcd failed", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                return FALSE;
            }

            runinfo->mRCode = 0;
        }
    }
    else if(command->mArgsNumRuntime == 1) {
        char* home = getenv("HOME");
        if(home == NULL) {
            fprintf(stderr, "$HOME is NULL.exit");
            exit(1); 
         } 
	
        if(!filer_cd(dir, home)) {
            err_msg("cd failed", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            return FALSE;
        }
        if(!filer_add_history(dir, home)) {
            err_msg("adding hisotry of mcd failed", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            return FALSE;
        }

        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_file_name(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    if(mis_raw_mode() && nextout == gStdout) {
        return TRUE;
    }
    else if(command->mArgsNumRuntime == 2) {
        char* darg = sCommand_option_with_argument_item(command, "-d");

        int dir;
        if(darg == NULL) {
            dir = adir();
        }
        else if(strcmp(darg, "all") == 0) {
            err_msg("can't get option -d all", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                return FALSE;
            }
        }

        char* argv2 = command->mArgsRuntime[1];

        sFile* file = filer_file(dir, atoi(argv2));
        if(file) {
            if(!fd_write(nextout, string_c_str(file->mName), string_length(file->mName))) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            if(!fd_write(nextout, "\n", 1)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
    sCommand* command = runinfo->mCommand;

    if(mis_raw_mode() && nextout == gStdout) {
        return TRUE;
    }
    else if(command->mArgsNumRuntime == 1) {
        char* darg = sCommand_option_with_argument_item(command, "-d");

        int dir;
        if(darg == NULL) {
            dir = adir();
        }
        else if(strcmp(darg, "all") == 0) {
            err_msg("can't get option -d all", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                return FALSE;
            }
        }

        sDir* dir2 = filer_dir(dir);
        if(dir2) {
            if(!fd_write(nextout, string_c_str(dir2->mPath), string_length(dir2->mPath))) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            if(!fd_write(nextout, "\n", 1)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
    sCommand* command = runinfo->mCommand;

    if(mis_raw_mode() && nextout == gStdout) {
        return TRUE;
    }
    else if(command->mArgsNumRuntime == 2) {
        char* darg = sCommand_option_with_argument_item(command, "-d");

        int dir;
        if(darg == NULL) {
            dir = adir();
        }
        else if(strcmp(darg, "all") == 0) {
            err_msg("can't get option -d all", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                return FALSE;
            }
        }

        char* argv2 = command->mArgsRuntime[1];
        sFile* file = filer_file(dir, atoi(argv2));
        if(file) {
            char buf[PATH_MAX];
            extname(buf, PATH_MAX, string_c_str(file->mName));
            if(!fd_write(nextout, buf, strlen(buf))) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            if(!fd_write(nextout, "\n", 1)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
    sCommand* command = runinfo->mCommand;

    if(mis_raw_mode() && nextout == gStdout) {
        return TRUE;
    }
    else if(command->mArgsNumRuntime == 2) {
        char* darg = sCommand_option_with_argument_item(command, "-d");

        int dir;
        if(darg == NULL) {
            dir = adir();
        }
        else if(strcmp(darg, "all") == 0) {
            err_msg("can't get option -d all", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                return FALSE;
            }
        }

        char* argv1 = command->mArgsRuntime[1];

        char buf[BUFSIZ];
        int size = snprintf(buf, BUFSIZ, "%d", filer_file2(dir, argv1));

        if(!fd_write(nextout, buf, size)) {
            err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }
        if(!fd_write(nextout, "\n", 1)) {
            err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }

        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_file_user(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    if(mis_raw_mode() && nextout == gStdout) {
        return TRUE;
    }
    else if(command->mArgsNumRuntime == 2) {
        char* darg = sCommand_option_with_argument_item(command, "-d");

        int dir;
        if(darg == NULL) {
            dir = adir();
        }
        else if(strcmp(darg, "all") == 0) {
            err_msg("can't get option -d all", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                return FALSE;
            }
        }

        int argv1 = atoi(command->mArgsRuntime[1]);

        sFile* file = filer_file(dir, argv1);

        if(file) {
            if(!fd_write(nextout, string_c_str(file->mUser), string_length(file->mUser))) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            if(!fd_write(nextout, "\n", 1)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
    sCommand* command = runinfo->mCommand;

    if(mis_raw_mode() && nextout == gStdout) {
        return TRUE;
    }
    else if(command->mArgsNumRuntime == 2) {
        char* darg = sCommand_option_with_argument_item(command, "-d");

        int dir;
        if(darg == NULL) {
            dir = adir();
        }
        else if(strcmp(darg, "all") == 0) {
            err_msg("can't get option -d all", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                return FALSE;
            }
        }

        int argv1 = atoi(command->mArgsRuntime[1]);

        sFile* file = filer_file(dir, argv1);

        if(file) {
            if(!fd_write(nextout, string_c_str(file->mGroup), string_length(file->mGroup))) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            if(!fd_write(nextout, "\n", 1)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
    sCommand* command = runinfo->mCommand;

    if(mis_raw_mode() && nextout == gStdout) {
        return TRUE;
    }
    else if(command->mArgsNumRuntime == 2) {
        char* darg = sCommand_option_with_argument_item(command, "-d");

        int dir;
        if(darg == NULL) {
            dir = adir();
        }
        else if(strcmp(darg, "all") == 0) {
            err_msg("can't get option -d all", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                return FALSE;
            }
        }

        int argv1 = atoi(command->mArgsRuntime[1]);

        sFile* file = filer_file(dir, argv1);

        if(file) {
            int n = file->mLStat.st_mode & S_ALLPERM;

            char buf[BUFSIZ];
            int size = snprintf(buf, BUFSIZ, "%o", n);
            if(!fd_write(nextout, buf, size)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            if(!fd_write(nextout, "\n", 1)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
    sCommand* command = runinfo->mCommand;

    if(mis_raw_mode() && nextout == gStdout) {
        return TRUE;
    }
    else if(command->mArgsNumRuntime == 1) {
        char* darg = sCommand_option_with_argument_item(command, "-d");

        int dir;
        if(darg == NULL) {
            dir = adir();
        }
        else if(strcmp(darg, "all") == 0) {
            err_msg("can't get option -d all", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                return FALSE;
            }
        }

        sDir* dir2 = filer_dir(dir);
        if(dir2) {
            char buf[BUFSIZ];
            int size = snprintf(buf, BUFSIZ, "%d", vector_count(dir2->mFiles));
            if(!fd_write(nextout, buf, size)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            if(!fd_write(nextout, "\n", 1)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
    sCommand* command = runinfo->mCommand;

    if(mis_raw_mode() && nextout == gStdout) {
        return TRUE;
    }
    else if(command->mArgsNumRuntime == 1) {
        char buf[BUFSIZ];
        int size = snprintf(buf, BUFSIZ, "%d", vector_count(gDirs));
        if(!fd_write(nextout, buf, size)) {
            err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }
        if(!fd_write(nextout, "\n", 1)) {
            err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }

        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_cursor_num(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    if(mis_raw_mode() && nextout == gStdout) {
        return TRUE;
    }
    else if(command->mArgsNumRuntime == 1) {
        char* darg = sCommand_option_with_argument_item(command, "-d");

        int dir;
        if(darg == NULL) {
            dir = adir();
        }
        else if(strcmp(darg, "all") == 0) {
            err_msg("can't get option -d all", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                return FALSE;
            }
        }

        sDir* dir2 = filer_dir(dir);

        if(dir2) {
            char buf[BUFSIZ];
            int size = snprintf(buf, BUFSIZ, "%d", dir2->mCursor);
            if(!fd_write(nextout, buf, size)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            if(!fd_write(nextout, "\n", 1)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
    sCommand* command = runinfo->mCommand;

    if(mis_raw_mode() && nextout == gStdout) {
        return TRUE;
    }
    else if(command->mArgsNumRuntime == 1) {
        char* darg = sCommand_option_with_argument_item(command, "-d");

        int dir;
        if(darg == NULL) {
            dir = adir();
        }
        else if(strcmp(darg, "all") == 0) {
            err_msg("can't get option -d all", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }
                if(!fd_write(nextout, "\n", 1)) {
                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
    //endwin();
    //mreset_tty();
    def_prog_mode();
    endwin();

    int rcode;

    if(continue_) {
        rcode = 0;
        xyzsh_readline_interface(cmdline, cursor, NULL, 0);
    }
    else {
        if(!xyzsh_readline_interface_onetime(&rcode, cmdline, cursor, "cmdline", NULL, 0, NULL)) {
            rcode = -1;
        }
    }

    if(rcode == 0) {
        filer_reread(0);
        filer_reread(1);
        (void)filer_reset_marks(adir());
    }

    if(rcode != 0 || !quick) {
        printf("\x1b[7mHIT ANY KEY\x1b[0m", 27, 27);
    }

    //xinitscr();
    reset_prog_mode();

    if(rcode != 0 || !quick) {
        (void)getch();
    }
}

BOOL cmd_cmdline(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;
    BOOL quick = sCommand_option_item(command, "-q");
    BOOL continue_ = sCommand_option_item(command, "-c");

    if(command->mArgsNumRuntime == 1) {
        cmdline_start("", 0, quick, continue_);
        runinfo->mRCode = 0;
    }
    else if(command->mArgsNumRuntime == 2) {
        char* init_str = command->mArgsRuntime[1];
        cmdline_start(init_str, -1, quick, continue_);
        runinfo->mRCode = 0;
    }
    else if(command->mArgsNumRuntime == 3) {
        char* init_str = command->mArgsRuntime[1];
        int cursor = atoi(command->mArgsRuntime[2]);
        cmdline_start(init_str, cursor, quick, continue_);
        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_activate(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;
    if(command->mArgsNumRuntime == 2) {
        int n = atoi(command->mArgsRuntime[1]);
        if(!filer_activate(n)) {
            err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            return FALSE;
        }

        runinfo->mRCode = 0;
    }


    return TRUE;
}

BOOL cmd_defmenu(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    if(command->mArgsNumRuntime == 2) {
        char* menu_name = command->mArgsRuntime[1];
        
        menu_create(menu_name);

        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_addmenu(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    if(command->mArgsNumRuntime == 4 && command->mBlocksNum == 1) {
        char* menu_name = command->mArgsRuntime[1];
        char* title = command->mArgsRuntime[2];
        int key = atoi(command->mArgsRuntime[3]);
        BOOL external = sCommand_option_item(command, "-external");
        
        if(!menu_append(menu_name, title, key, command->mBlocks[0], external))
        {
             char buf[128];
            snprintf(buf, 128, "not found menu of %s", menu_name);
            err_msg(buf, runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            return FALSE;
        }

        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_mmenu(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    if(command->mArgsNumRuntime == 2) {
        char* menu_name = command->mArgsRuntime[1];

        if(!set_active_menu(menu_name)) {
            char buf[128];
            snprintf(buf, 128, "not found this menu(%s)", menu_name);
            err_msg(buf, runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            return FALSE;
        }

        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_reread(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    if(command->mArgsNumRuntime == 1) {
        char* darg = sCommand_option_with_argument_item(command, "-d");

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
                err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                err_msg("reread error", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                return FALSE;
            }
            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_allfiles(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    if(mis_raw_mode() && nextout == gStdout) {
        return TRUE;
    }
    else if(sCommand_option_item(command, "-fullpath")) {
        if(command->mArgsNumRuntime == 1) {
            char* darg = sCommand_option_with_argument_item(command, "-d");

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
                    err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                    return FALSE;
                                }
                                if(!fd_write(nextout, "\n", 1)) {
                                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                return FALSE;
                            }
                            if(!fd_write(nextout, "\n", 1)) {
                                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
        if(command->mArgsNumRuntime == 1) {
            char* darg = sCommand_option_with_argument_item(command, "-d");

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
                    err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                    return FALSE;
                                }
                                if(!fd_write(nextout, "\n", 1)) {
                                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                return FALSE;
                            }
                            if(!fd_write(nextout, "\n", 1)) {
                                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
    sCommand* command = runinfo->mCommand;

    if(mis_raw_mode() && nextout == gStdout) {
        return TRUE;
    }
    else if(sCommand_option_item(command, "-fullpath")) {
        if(command->mArgsNumRuntime == 1) {
            char* darg = sCommand_option_with_argument_item(command, "-d");

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
                    err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                                        err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                        return FALSE;
                                    }
                                    if(!fd_write(nextout, "\n", 1)) {
                                        err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                return FALSE;
                            }
                            if(!fd_write(nextout, "\n", 1)) {
                                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                    return FALSE;
                                }
                                if(!fd_write(nextout, "\n", 1)) {
                                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                            err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            return FALSE;
                        }
                        if(!fd_write(nextout, "\n", 1)) {
                            err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
        if(command->mArgsNumRuntime == 1) {
            char* darg = sCommand_option_with_argument_item(command, "-d");

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
                    err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                                        err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                                        runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                        return FALSE;
                                    }
                                    if(!fd_write(nextout, "\n", 1)) {
                                        err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                return FALSE;
                            }
                            if(!fd_write(nextout, "\n", 1)) {
                                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                                    return FALSE;
                                }
                                if(!fd_write(nextout, "\n", 1)) {
                                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                            err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            return FALSE;
                        }
                        if(!fd_write(nextout, "\n", 1)) {
                            err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
    sCommand* command = runinfo->mCommand;

    BOOL toggle = sCommand_option_item(command, "-t");
    BOOL files = sCommand_option_item(command, "-f");
    BOOL number = sCommand_option_item(command, "-n");
    BOOL all = sCommand_option_item(command, "-a");

    char* darg = sCommand_option_with_argument_item(command, "-d");

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
            err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            return FALSE;
        }
    }

    /// 状態参照 ///
    if(!all && !toggle && command->mArgsNumRuntime == 2)
    {
        if(mis_raw_mode() && nextout == gStdout) {
            return TRUE;
        }

        char* arg1 = command->mArgsRuntime[1];

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
                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }
                if(!fd_write(nextout, "\n", 1)) {
                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }

                runinfo->mRCode = 0;
            }
            else {
                if(!fd_write(nextout, "0", 1)) {
                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }
                if(!fd_write(nextout, "\n", 1)) {
                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }

                runinfo->mRCode = 0;
            }
        }
    }
    /// 状態セット all toggleオン ///
    else if(all && toggle && command->mArgsNumRuntime == 1) {
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
    else if(all && !toggle && command->mArgsNumRuntime == 2) {
        int arg1 = atoi(command->mArgsRuntime[1]);

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
    else if(!all && !toggle && command->mArgsNumRuntime == 3) {
        char* arg1 = command->mArgsRuntime[1];

        int arg1_num;
        if(number) {
            arg1_num = atoi(arg1);
        }
        else {
            arg1_num = filer_file2(dir, arg1);
        }

        int arg2 = atoi(command->mArgsRuntime[2]);
        if(arg1_num >= 0) {
            if(!filer_set_mark(dir, arg1_num, arg2 != 0)) {
                err_msg("invalid range", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                return FALSE;
            }
        }

        runinfo->mRCode = 0;
    }

    /// 状態セット ファイルひとつ ///
    else if(!all && toggle && command->mArgsNumRuntime == 2) {
        char* arg1 = command->mArgsRuntime[1];

        int arg1_num;
        if(number) {
            arg1_num = atoi(arg1);
        }
        else {
            arg1_num = filer_file2(dir, arg1);
        }

        if(arg1_num >= 0) {
            if(!filer_toggle_mark(dir, arg1_num)) {
                err_msg("invalid range", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                return FALSE;
            }
        }

        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_new_dir(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    char* mask;
    mask = sCommand_option_with_argument_item(command, "-m");
    if(mask == NULL) {
        mask = ".+";
    }

    BOOL dotdir_mask;
    char* dotdir_mask_arg = sCommand_option_with_argument_item(command, "-dotdir");
    if(dotdir_mask_arg == NULL) {
        dotdir_mask = FALSE;
    }
    else {
        dotdir_mask = atoi(dotdir_mask_arg);
    }

    if(command->mArgsNumRuntime == 2) {
        char* dir = command->mArgsRuntime[1];
        if(!filer_new_dir(dir, dotdir_mask, mask)) {
            err_msg("invalid path", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            return FALSE;
        }

        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_del_dir(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    if(command->mArgsNumRuntime == 1) {
        char* darg = sCommand_option_with_argument_item(command, "-d");

        int dir;
        if(darg == NULL) {
            dir = adir();
        }
        else if(strcmp(darg, "all") == 0) {
            err_msg("can't get option -d all", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                return FALSE;
            }
        }

        if(!filer_del_dir(dir)) {
            err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            return FALSE;
        }

        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_marking(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    if(mis_raw_mode() && nextout == gStdout) {
        return TRUE;
    }
    else if(command->mArgsNumRuntime == 1) {
        char* darg = sCommand_option_with_argument_item(command, "-d");

        int dir;
        if(darg == NULL) {
            dir = adir();
        }
        else if(strcmp(darg, "all") == 0) {
            err_msg("can't get option -d all", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                return FALSE;
            }
        }

        if(filer_marking(dir)) {
            if(!fd_write(nextout, "1", 1)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            if(!fd_write(nextout, "\n", 1)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
        }
        else {
            if(!fd_write(nextout, "0", 1)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            if(!fd_write(nextout, "\n", 1)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
    sCommand* command = runinfo->mCommand;

    BOOL dotdir = sCommand_option_item(command, "-dotdir");
    BOOL reread = sCommand_option_item(command, "-r");
    char* darg = sCommand_option_with_argument_item(command, "-d");

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
            err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            return FALSE;
        }
    }


    if(dotdir) {
        /// 状態参照 ///
        if(command->mArgsNumRuntime == 1) {
            if(mis_raw_mode() && nextout == gStdout) {
                return TRUE;
            }
            if(dir < 0) {
                err_msg("can't get option -d all", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                return FALSE;
            }

            if(filer_dotdir_mask(dir)) {
                if(!fd_write(nextout, "1", 1)) {
                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }
                if(!fd_write(nextout, "\n", 1)) {
                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }
            }
            else {
                if(!fd_write(nextout, "0", 1)) {
                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }
                if(!fd_write(nextout, "\n", 1)) {
                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }
            }

            runinfo->mRCode = 0;
        }

        /// 状態セット ///
        else if(command->mArgsNumRuntime == 2) {
            int arg2 = atoi(command->mArgsRuntime[1]);

            if(dir < 0) {
                int i;
                for(i=0; i<vector_count(gDirs); i++) {
                    (void)filer_set_dotdir_mask(i, arg2 != 0);
                }
            }
            else {
                if(!filer_set_dotdir_mask(dir, arg2 != 0)) 
                {
                    err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                    return FALSE;
                }
            }

            runinfo->mRCode = 0;
        }
    }
    else {
        /// 状態参照 ///
        if(command->mArgsNumRuntime == 1) {
            if(mis_raw_mode() && nextout == gStdout) {
                return TRUE;
            }
            if(dir < 0) {
                err_msg("invalid use. mask -d all", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                return FALSE;
            }

            char* p = filer_mask(dir);
            if(p) {
                if(!fd_write(nextout, p, strlen(p))) {
                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }
                if(!fd_write(nextout, "\n", 1)) {
                    err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                    runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                    return FALSE;
                }

                runinfo->mRCode = 0;
            }
        }

        /// 状態セット ///
        else if(command->mArgsNumRuntime == 2) {
            if(dir < 0) {
                int i;
                for(i=0; i<vector_count(gDirs); i++) {
                    if(!filer_set_mask(i, command->mArgsRuntime[1])) {
                        err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                        return FALSE;
                    }
                }

                if(reread) {
                    (void)filer_reread(0);
                    (void)filer_reread(1);
                }
            }
            else {
                if(!filer_set_mask(dir, command->mArgsRuntime[1])) {
                    err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                    return FALSE;
                }

                if(reread) {
                    if(dir == 0)
                        (void)filer_reread(0);
                    else if(dir == 1) 
                        (void)filer_reread(1);
                }
            }

            runinfo->mRCode = 0;
        }
    }

    return TRUE;
}

BOOL cmd_vd(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    char* darg = sCommand_option_with_argument_item(command, "-d");

    int dir;
    if(darg == NULL) {
        dir = adir();
    }
    else if(strcmp(darg, "all") == 0) {
        err_msg("can't get option -d all", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
            err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            return FALSE;
        }
    }

    if(runinfo->mFilter) {
        fd_split(nextin, kLF);

        if(!filer_vd_start(dir))  {
            err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            return FALSE;
        }
        if(!filer_vd_add(dir, "..")) {
            err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                    err_msg("invalid dir number", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                    return FALSE;
                }
            }

            if(gXyzshSigInt) {
                stack_end_stack();
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
    sCommand* command = runinfo->mCommand;

    BOOL reread = sCommand_option_item(command, "-r");
    BOOL cursor_move = sCommand_option_item(command, "-c");

    if(command->mArgsNumRuntime == 3) {
        char* source = command->mArgsRuntime[1];
        char* distination = command->mArgsRuntime[2];

        if(access(source, F_OK) != 0) {
            char buf[128];
            snprintf(buf, 128, "%s does not exist.", distination);
            err_msg(buf, runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            return FALSE;
        }

        if(access(distination, F_OK) == 0) {
            char buf[128];
            snprintf(buf, 128, "%s exists. can't rename", distination);
            err_msg(buf, runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            return FALSE;
        }

        if(rename(source, distination) < 0) {
            err_msg("rename error", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            return FALSE;
        }

        if(reread) {
            (void)filer_reread(adir());
        }
        
        if(cursor_move) {
            int n = filer_file2(adir(), distination);
            if(n >= 0) (void)filer_cursor_move(adir(), n);
        }

        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_row(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    if(mis_raw_mode() && nextout == gStdout) {
        return TRUE;
    }
    else {
        char buf[BUFSIZ];
        int size = snprintf(buf, BUFSIZ, "%d", filer_row(adir()));
        if(!fd_write(nextout, buf, size)) {
            err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }
        if(!fd_write(nextout, "\n", 1)) {
            err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }
        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_row_max(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;
    
    if(mis_raw_mode() && nextout == gStdout) {
        return TRUE;
    }
    else {
        char buf[BUFSIZ];
        int size = snprintf(buf, BUFSIZ, "%d", filer_row_max(adir()));
        if(!fd_write(nextout, buf, size)) {
            err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }
        if(!fd_write(nextout, "\n", 1)) {
            err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }
        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_line(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    if(mis_raw_mode() && nextout == gStdout) {
        return TRUE;
    }
    else {
        char buf[BUFSIZ];
        int size = snprintf(buf, BUFSIZ, "%d", filer_line(adir()));
        if(!fd_write(nextout, buf, size)) {
            err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }
        if(!fd_write(nextout, "\n", 1)) {
            err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }
        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_line_max(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    if(mis_raw_mode() && nextout == gStdout) {
        return TRUE;
    }
    else {
        char buf[BUFSIZ];
        int size = snprintf(buf, BUFSIZ, "%d", filer_line_max(adir()));
        if(!fd_write(nextout, buf, size)) {
            err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }
        if(!fd_write(nextout, "\n", 1)) {
            err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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

BOOL cmd_mcp(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    BOOL raw_mode = mis_raw_mode();
    if(!raw_mode) {
        xinitscr();
    }

    BOOL preserve = sCommand_option_item(command, "-p");

    if(command->mArgsNumRuntime == 2) {
        gCopyOverride = kNone;
        gWriteProtected = kWPNone;

        /// 引数チェック ///
        char distination[PATH_MAX];
        xstrncpy(distination, command->mArgsRuntime[1], PATH_MAX);

        /// 対象がディレクトリかチェック ///
        if(distination[strlen(distination)-1] != '/')
        {
            xstrncat(distination, "/", PATH_MAX);
        }

        /// ディレクトリが無いなら作成 ///
        if(access(distination, F_OK) != 0) {
            char* str[] = {
                "yes", "no"
            };

            char buf[BUFSIZ];
            snprintf(buf, BUFSIZ, "%s doesn't exist. create?", distination);

            if(select_str(buf, str, 2, 1) == 0) {
                snprintf(buf, BUFSIZ, "mkdir -p %s", distination);
                if(system(buf) < 0) {
                    char buf[128];
                    snprintf(buf, 128, "mcp: making directory err(%s)", distination);
                    err_msg(buf, runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                    if(!raw_mode) {
                        endwin();
                    }
                    return FALSE;
                }
            }
            else {
                char buf[128];
                snprintf(buf, 128, "mcp: destination err(%s)", distination);
                err_msg(buf, runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                if(!raw_mode) {
                    endwin();
                }
                return FALSE;
            }
        }

        /// 目標ディレクトリがディレクトリじゃないならエラー ///
        struct stat dstat;
        if(stat(distination, &dstat) < 0 || !S_ISDIR(dstat.st_mode)) {
            char buf[128];
            snprintf(buf, 128, "mcp: distination is not directory");
            err_msg(buf, runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                        
                        if(!file_copy(source, distination, FALSE, preserve)) 
                        {
                            break;
                        }

                        gProgressMark--;
                        
                        (void)filer_set_mark(adir(), num, FALSE);
                    }
                }
            }
            vector_delete_malloc(markfiles);
            
            /*
            mclear();
            view();
            refresh();
            */

            dir = filer_dir(0);
            if(strcmp(distination, string_c_str(dir->mPath)) == 0) {
                (void)filer_reread(0);
            }

            dir = filer_dir(1);
            if(strcmp(distination, string_c_str(dir->mPath)) == 0) {
                (void)filer_reread(1);
            }

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
    sCommand* command = runinfo->mCommand;

    BOOL raw_mode = mis_raw_mode();
    if(!raw_mode) {
        xinitscr();
    }

    BOOL preserve = sCommand_option_item(command, "-p");

    if(command->mArgsNumRuntime == 2) {
        gCopyOverride = kNone;
        gWriteProtected = kWPNone;

        sDir* dir = filer_dir(adir());

        if(dir) {
            /// 引数チェック ///
            char distination[PATH_MAX];
            xstrncpy(distination, string_c_str(dir->mPath), PATH_MAX);
            xstrncat(distination, command->mArgsRuntime[1], PATH_MAX);

            /// 対象があるかどうかチェック ///
            if(access(distination, F_OK) == 0) {
                err_msg("distination exists", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                        err_msg(buf, runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                        
                        if(!file_copy(source, distination, FALSE, preserve)) {
                            err_msg("", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                            if(!raw_mode) {
                                endwin();
                            }
                            return FALSE;
                        }

                        (void)filer_set_mark(adir(), num, FALSE);
                    }
                }

                (void)filer_reread(adir());
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
    sCommand* command = runinfo->mCommand;

    BOOL raw_mode = mis_raw_mode();
    if(!raw_mode) {
        xinitscr();
    }

    BOOL force = sCommand_option_item(command, "-f");
    BOOL preserve = sCommand_option_item(command, "-p");

    if(command->mArgsNumRuntime == 2) {
        if(force) {
            gCopyOverride = kYesAll;
        }
        else {
            gCopyOverride = kNone;
        }

        gWriteProtected = kWPNone;

        /// 引数チェック ///
        char distination[PATH_MAX];
        xstrncpy(distination, command->mArgsRuntime[1], PATH_MAX);

        /// 対象がディレクトリ ///
        if(distination[strlen(distination)-1] != '/') {
            xstrncat(distination, "/", PATH_MAX);
        }

        /// ディレクトリが無いなら作成 ///
        if(access(distination, F_OK) != 0)
        {
            char* str[] = {
                "yes", "no"
            };

            char buf[BUFSIZ];
            snprintf(buf, BUFSIZ, "%s doesn't exist. create?", distination);

            if(select_str(buf, str, 2, 1) == 0) {
                snprintf(buf, BUFSIZ, "mkdir -p %s", distination);
                if(system(buf) < 0) {
                    char buf[128];
                    snprintf(buf, 128, "mmv: making directory err(%s)", distination);
                    err_msg(buf, runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                    if(!raw_mode) {
                        endwin();
                    }
                    return FALSE;
                }
            }
            else {
                char buf[128];
                snprintf(buf, 128, "mmv: destination err(%s)", distination);
                err_msg(buf, runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                if(!raw_mode) {
                    endwin();
                }
                return FALSE;
            }
        }

        /// 目標ファイルがディレクトリかどうかチェック ///
        struct stat dstat;
        if(stat(distination, &dstat) < 0 || !S_ISDIR(dstat.st_mode)) {
            err_msg("mmv: distination is not directory", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            if(!raw_mode) {
                endwin();
            }
            return FALSE;
        }

        /// go ///
        sDir* dir = filer_dir(adir());

        if(dir) {
            sObject* markfiles = filer_mark_files(adir());
            const int mark_file_num = vector_count(markfiles);
            gProgressMark = mark_file_num;

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
                        
                        if(!file_copy(source, distination, TRUE, preserve)) {
                            break;
                        }

                        gProgressMark--;
                        
                        (void)filer_set_mark(adir(), num, FALSE);
                    }
                }
            }
            vector_delete_malloc(markfiles);

            /*
            mclear();
            view();
            refresh();
            */

            (void)filer_reread(0);
            (void)filer_reread(1);

            runinfo->mRCode = 0;
        }
    }

    if(!raw_mode) {
        endwin();
    }
    return TRUE;
}

BOOL cmd_mrm(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    BOOL raw_mode = mis_raw_mode();
    if(!raw_mode) {
        xinitscr();
    }

    if(!mis_raw_mode()) {
        err_msg("invalid terminal setting. not raw mode", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);

        if(!raw_mode) {
            endwin();
        }
        return FALSE;
    }
                
    if(command->mArgsNumRuntime == 1) {
        gCopyOverride = kNone;
        gWriteProtected = kWPNone;

        /// go ///
        sObject* markfiles = filer_mark_files(adir());
        const int mark_file_num = vector_count(markfiles);
        gProgressMark = mark_file_num;

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
                        
                        if(!file_remove(source, FALSE, TRUE)) {
                            break;
                        }

                        gProgressMark--;
                        
                        (void)filer_set_mark(adir(), num, FALSE);
                    }
                }
            }
        }
        vector_delete_malloc(markfiles);
        
        /*
        mclear();
        view();
        refresh();
        */

        (void)filer_reread(0);
        (void)filer_reread(1);

        runinfo->mRCode = 0;
    }

    if(!raw_mode) {
        endwin();
    }
    return TRUE;
}

BOOL cmd_mtrashbox(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    BOOL raw_mode = mis_raw_mode();
    if(!raw_mode) {
        xinitscr();
    }

    BOOL force = sCommand_option_item(command, "-f");
    BOOL preserve = sCommand_option_item(command, "-p");

    if(command->mArgsNumRuntime == 1) {
        if(force) {
            gCopyOverride = kYesAll;
        }
        else {
            gCopyOverride = kNone;
        }

        gWriteProtected = kWPNone;

        /// 引数チェック ///
        char distination[PATH_MAX];

        char* env = getenv("TRASHBOX_DIR");
        if(env) {
            xstrncpy(distination, env, PATH_MAX);
        }
        else {
            xstrncpy(distination, getenv("MF4HOME"), PATH_MAX);
            xstrncat(distination, "/trashbox/", PATH_MAX);
        }

        if(distination[strlen(distination)-1] != '/') {
            xstrncat(distination, "/", PATH_MAX);
        }

        /// ディレクトリが無いなら作成 ///
        if(access(distination, F_OK) != 0)
        {
            char* str[] = {
                "yes", "no"
            };

            char buf[BUFSIZ];
            snprintf(buf, BUFSIZ, "%s doesn't exist. create?", distination);

            if(select_str(buf, str, 2, 1) == 0) {
                snprintf(buf, BUFSIZ, "mkdir -p %s", distination);
                if(system(buf) < 0) {
                    char buf[128];
                    snprintf(buf, 128, "mmv: making directory err(%s)", distination);
                    err_msg(buf, runinfo->mSName, runinfo->mSLine, command->mArgs[0]);

                    if(!raw_mode) {
                        endwin();
                    }
                    return FALSE;
                }
            }
            else {
                char buf[128];
                snprintf(buf, 128, "mmv: destination err(%s)", distination);
                err_msg(buf, runinfo->mSName, runinfo->mSLine, command->mArgs[0]);

                if(!raw_mode) {
                    endwin();
                }
                return FALSE;
            }
        }

        /// 目標ファイルがディレクトリかどうかチェック ///
        struct stat dstat;
        if(stat(distination, &dstat) < 0 || !S_ISDIR(dstat.st_mode)) {
            err_msg("mmv: distination is not directory", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);

            if(!raw_mode) {
                endwin();
            }
            return FALSE;
        }

        /// go ///
        sDir* dir = filer_dir(adir());

        if(dir) {
            sObject* markfiles = filer_mark_files(adir());
            const int mark_file_num = vector_count(markfiles);
            gProgressMark = mark_file_num;

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
                        
                        if(!file_copy(source, distination, TRUE, preserve)) {
                            break;
                        }

                        gProgressMark--;
                        
                        (void)filer_set_mark(adir(), num, FALSE);
                    }
                }
            }
            vector_delete_malloc(markfiles);

            /*
            mclear();
            view();
            refresh();
            */

            (void)filer_reread(0);
            (void)filer_reread(1);

            runinfo->mRCode = 0;
        }
    }

    if(!raw_mode) {
        endwin();
    }
    return TRUE;
}

BOOL cmd_mln(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    BOOL raw_mode = mis_raw_mode();
    if(!raw_mode) {
        xinitscr();
    }

    if(command->mArgsNumRuntime == 2) {
        /// 引数チェック ///
        char distination[PATH_MAX];
        xstrncpy(distination, command->mArgsRuntime[1], PATH_MAX);

        if(distination[strlen(distination)-1] != '/') {
            xstrncat(distination, "/", PATH_MAX);
        }

        /// ディレクトリが無いなら作成 ///
        if(access(distination, F_OK) != 0) {
            char* str[] = {
                "yes", "no"
            };

            char buf[BUFSIZ];
            snprintf(buf, BUFSIZ, "%s doesn't exist. create?", distination);

            if(select_str(buf, str, 2, 1) == 0) {
                snprintf(buf, BUFSIZ, "mkdir -p %s", distination);
                if(system(buf) < 0) {
                    char buf[128];
                    snprintf(buf, 128, "mcp: making directory err(%s)", distination);
                    err_msg(buf, runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                    if(!raw_mode) {
                        endwin();
                    }
                    return FALSE;
                }
            }
            else {
                char buf[128];
                snprintf(buf, 128, "mcp: destination err(%s)", distination);
                err_msg(buf, runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                if(!raw_mode) {
                    endwin();
                }
                return FALSE;
            }
        }

        /// 目標ディレクトリがディレクトリじゃないならエラー ///
        struct stat dstat;
        if(stat(distination, &dstat) < 0 || !S_ISDIR(dstat.st_mode)) {
            err_msg("mcp: distination is not directory", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
                        xstrncpy(dfile, distination, PATH_MAX);
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
            vector_delete_malloc(markfiles);

            /*
            mclear();
            view();
            refresh();
            */

            dir = filer_dir(0);
            if(strcmp(distination, string_c_str(dir->mPath)) == 0) {
                (void)filer_reread(0);
            }

            dir = filer_dir(1);
            if(strcmp(distination, string_c_str(dir->mPath)) == 0) {
                (void)filer_reread(1);
            }

            runinfo->mRCode = 0;
        }
    }

    if(!raw_mode) {
        endwin();
    }

    return TRUE;
}

BOOL cmd_mchoise(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    if(command->mArgsNumRuntime >= 3) {
        BOOL make_raw = FALSE;
        if(!mis_raw_mode()) {
            make_raw = TRUE;
            xinitscr();
        }
        char* msg = command->mArgsRuntime[1];
        char** str = MALLOC(sizeof(char*)*command->mArgsNumRuntime);

        int j;
        for(j=2; j<command->mArgsNumRuntime; j++) {
            str[j-2] = command->mArgsRuntime[j];
        }

        char* smsg = choise(msg, str, command->mArgsNumRuntime -2, -1);
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
            err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }
        if(!fd_write(nextout, "\n", 1)) {
            err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
    sCommand* command = runinfo->mCommand;

    if(sCommand_option_item(command, "-byte")) {
        gKanjiCodeFileName = kByte;
        runinfo->mRCode = 0;
    }
    else if(sCommand_option_item(command, "-utf8")) {
        gKanjiCodeFileName = kUtf8;
        runinfo->mRCode = 0;
    }
    else if(sCommand_option_item(command, "-utf8-mac")) {
        gKanjiCodeFileName = kUtf8Mac;
        runinfo->mRCode = 0;
    }
    else {
        if(mis_raw_mode() && nextout == gStdout) {
            return TRUE;
        }
        else if(gKanjiCodeFileName == kByte) {
            if(!fd_write(nextout, "byte", 4)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            if(!fd_write(nextout, "\n", 1)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }

            runinfo->mRCode = 0;
        }
        else if(gKanjiCodeFileName == kUtf8) {
            if(!fd_write(nextout, "utf8", 4)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            if(!fd_write(nextout, "\n", 1)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }

            runinfo->mRCode = 0;
        }
        else if(gKanjiCodeFileName == kUtf8Mac) {
            if(!fd_write(nextout, "utf8-mac", 8)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
                runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                return FALSE;
            }
            if(!fd_write(nextout, "\n", 1)) {
                err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
    sCommand* command = runinfo->mCommand;

    if(!mis_raw_mode()) {
        err_msg("invalid terminal setting. not raw mode", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
        return FALSE;
    }

    xclear_immediately();
    runinfo->mRCode = 0;

    return TRUE;
}

BOOL cmd_adir(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    if(mis_raw_mode() && nextout == gStdout) {
        return TRUE;
    }
    else {
        char buf[256];
        int size = snprintf(buf, 256, "%d", adir());

        if(!fd_write(nextout, buf, size)) {
            err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }
        if(!fd_write(nextout, "\n", 1)) {
            err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }

        runinfo->mRCode = 0;
    }

    return TRUE;
}

BOOL cmd_sdir(sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sCommand* command = runinfo->mCommand;

    if(mis_raw_mode() && nextout == gStdout) {
        return TRUE;
    }
    else {
        char buf[256];
        int size = snprintf(buf, 256, "%d", sdir());

        if(!fd_write(nextout, buf, size)) {
            err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }
        if(!fd_write(nextout, "\n", 1)) {
            err_msg("interrupt", runinfo->mSName, runinfo->mSLine, command->mArgs[0]);
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
    sCommand* command = runinfo->mCommand;
    if(command->mBlocksNum == 1) {
        gMFiler4Prompt = block_clone_gc(command->mBlocks[0], T_BLOCK, FALSE);
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
    xyzsh_add_inner_command(gMFiler4, "mchoise", cmd_mchoise, 0);
    xyzsh_add_inner_command(gMFiler4, "activate", cmd_activate, 0);
    xyzsh_add_inner_command(gMFiler4, "defmenu", cmd_defmenu, 0);
    xyzsh_add_inner_command(gMFiler4, "addmenu", cmd_addmenu, 0);
    xyzsh_add_inner_command(gMFiler4, "mmenu", cmd_mmenu, 0);
    xyzsh_add_inner_command(gMFiler4, "reread", cmd_reread, 1, "-d");
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
}

void commands_final()
{
}


