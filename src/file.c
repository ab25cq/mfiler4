#include "common.h"
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <utime.h>
#include <time.h>
#include <sys/time.h>
#if !defined(__DARWIN__) && !defined(__FREEBSD__)
#include <sys/sysmacros.h>
#endif
#include <libgen.h>

#if defined(HAVE_CURSES_H)
#include <curses.h>
#elif defined(HAVE_NCURSES_H)
#include <ncurses.h>
#elif defined(HAVE_NCURSES_NCURSES_H)
#include <ncurses/ncurses.h>
#endif


void file_copy_override_box(char* spath, struct stat* source_stat, char* dpath, struct stat* dpath_stat)
{
    const int maxx = mgetmaxx();
    const int maxy = mgetmaxy();
    
    const int y = maxy/2 - 5;
    
    mbox(y, 4, maxx-8, 11);
    
    time_t t = source_stat->st_mtime;
    struct tm* tm_ = (struct tm*)localtime(&t);
    
    int year = tm_->tm_year-100;
    if(year < 0) year+=100;
    while(year > 100) year-=100;
    
    mvprintw(y+1, 5, "source");
    char tmp[PATH_MAX];
    xstrncpy(tmp, spath, PATH_MAX);
    if(S_ISDIR(source_stat->st_mode)) {
        xstrncat(tmp, "/", PATH_MAX);
    }
    else if(S_ISFIFO(source_stat->st_mode)) {
        xstrncat(tmp, "|", PATH_MAX);
    }
    else if(S_ISSOCK(source_stat->st_mode)) {
        xstrncat(tmp, "=", PATH_MAX);
    }
    else if(S_ISLNK(source_stat->st_mode)) {
        xstrncat(tmp, "@", PATH_MAX);
    }
    
    
    mvprintw(y+2, 5, "path: %s", tmp);
    mvprintw(y+3, 5, "size: %d", source_stat->st_size);
    mvprintw(y+4, 5, "time: %02d-%02d-%02d %02d:%02d", year, tm_->tm_mon+1
           , tm_->tm_mday, tm_->tm_hour, tm_->tm_min);
    
    t = dpath_stat->st_mtime;
    tm_ = (struct tm*)localtime(&t);
    
    year = tm_->tm_year-100;
    if(year < 0) year+=100;
    while(year > 100) year-=100;
    
    mvprintw(y+6, 5, "destination");
    char tmp2[PATH_MAX];
    xstrncpy(tmp2, dpath, PATH_MAX);
    if(S_ISDIR(dpath_stat->st_mode)) {
        xstrncat(tmp2, "/", PATH_MAX);
    }
    else if(S_ISFIFO(dpath_stat->st_mode)) {
        xstrncat(tmp, "|", PATH_MAX);
    }
    else if(S_ISSOCK(dpath_stat->st_mode)) {
        xstrncat(tmp, "=", PATH_MAX);
    }
    else if(S_ISLNK(dpath_stat->st_mode)) {
        xstrncat(tmp, "@", PATH_MAX);
    }
    
    mvprintw(y+7, 5, "path: %s", tmp2);
    mvprintw(y+8, 5, "size: %d", dpath_stat->st_size);
    mvprintw(y+9, 5, "time: %02d-%02d-%02d %02d:%02d", year, tm_->tm_mon+1
           , tm_->tm_mday, tm_->tm_hour, tm_->tm_min);
}

static char* gOverrideSPath;       // to give the arguments to view_file_copy_override_box
static struct stat* gOverrideStat;
static char* gOverrideDPath;
static struct stat* gOverrideDStat;

void view_file_copy_override_box()
{
    file_copy_override_box(gOverrideSPath, gOverrideStat, gOverrideDPath, gOverrideDStat);
}

static void copy_timestamp_and_permission(char* dpath, struct stat* source_stat, BOOL preserve, FILE* log, int* err_num)
{
    if(preserve || move) {
        if(getuid() == 0) {
            if(chown(dpath, source_stat->st_uid, source_stat->st_gid) <0) {
                fprintf(log, "owner of %s\n", dpath);
                (*err_num)++;
            }
        }

        struct utimbuf utb;

        utb.actime = source_stat->st_atime;
        utb.modtime = source_stat->st_mtime;

        if(utime(dpath, &utb) < 0) {
            fprintf(log, "time stamp of %s\n", dpath);
            (*err_num)++;
        }
    }

    mode_t umask_before = umask(0);
#if defined(S_ISTXT)
    if(chmod(dpath, source_stat->st_mode & (S_IRUSR|S_IWUSR|S_IXUSR
        |S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH|S_ISUID|S_ISGID|S_ISTXT)) < 0) 
    {
        fprintf(log, "permission of %s\n", dpath);
        (*err_num)++;
    }
#else

#if defined(S_ISVTX)
    if(chmod(dpath, source_stat->st_mode & (S_IRUSR|S_IWUSR|S_IXUSR
                |S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH|S_ISUID|S_ISGID|S_ISVTX)) < 0) 
    {
        fprintf(log, "permission of %s\n", dpath);
        (*err_num)++;
    }
#else
    if(chmod(dpath, source_stat->st_mode & (S_IRUSR|S_IWUSR|S_IXUSR
    |S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH|S_ISUID|S_ISGID)) < 0) 
    {
        fprintf(log, "permission of %s\n", dpath);
        (*err_num)++;
    }
#endif

#endif
    umask(umask_before);
}

static BOOL do_copy(char* spath, char* spath_dirname, char* spath_basename, struct stat* source_stat, char* dpath, char* dpath_dirname, char* dpath_basename, BOOL move, BOOL preserve, FILE* log, int* err_num, enum eCopyOverrideWay* override_way);

static BOOL copy_directory_recursively(char* spath, char* spath_basename, char* dpath, char* dpath_dirname, struct stat* source_stat, BOOL move, BOOL preserve, FILE* log, int* err_num, BOOL no_mkdir, enum eCopyOverrideWay* override_way)
{
    DIR* dir = opendir(spath);
    if(dir == NULL) {
        fprintf(log, "%s --> can't open the directory\n", spath);
        (*err_num)++;
        return TRUE;
    }

    if(!no_mkdir && mkdir(dpath, 0777) < 0)  { 
        closedir(dir);
        fprintf(log, "%s --> C can't mkdir %s\n", spath, dpath);
        (*err_num)++;
        return TRUE;
    }

    struct dirent* entry;
    while((entry = readdir(dir)) != 0) {
        if(strcmp(entry->d_name, ".") != 0
            && strcmp(entry->d_name, "..") != 0)
        {
            char spath2[PATH_MAX];

            xstrncpy(spath2, spath, PATH_MAX);
            xstrncat(spath2, "/", PATH_MAX);
            xstrncat(spath2, entry->d_name, PATH_MAX);

            char dpath2[PATH_MAX];

            xstrncpy(dpath2, dpath_dirname, PATH_MAX);
            xstrncat(dpath2, "/", PATH_MAX);
            xstrncat(dpath2, spath_basename, PATH_MAX);
            xstrncat(dpath2, "/", PATH_MAX);
            xstrncat(dpath2, entry->d_name, PATH_MAX);

            struct stat spath2_stat;
            if(lstat(spath2, &spath2_stat) < 0) {
                fprintf(log, "%s --> can't stat\n", spath2);
                (*err_num)++;
            }
            else {
                char spath2_dirname[PATH_MAX];
                xstrncpy(spath2_dirname, spath, PATH_MAX);

                char spath2_basename[PATH_MAX];
                xstrncpy(spath2_basename, entry->d_name, PATH_MAX);

                char dpath2_dirname[PATH_MAX];
                xstrncpy(dpath2_dirname, dpath_dirname, PATH_MAX);
                xstrncat(dpath2_dirname, "/", PATH_MAX);
                xstrncat(dpath2_dirname, spath_basename, PATH_MAX);

                char dpath2_basename[PATH_MAX];
                xstrncpy(dpath2_basename, entry->d_name, PATH_MAX);

                if(!do_copy(spath2, spath2_dirname, spath2_basename, &spath2_stat, dpath2, dpath2_dirname, dpath2_basename, move, preserve, log, err_num, override_way)) {
                    closedir(dir);
                    return FALSE;
                }
            }
        }
    }

    (void)closedir(dir);

    if(!no_mkdir) {
        mode_t umask_before = umask(0);
        chmod(dpath, source_stat->st_mode & (S_IRUSR|S_IWUSR|S_IXUSR |S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH));
        umask(umask_before);
    }

    copy_timestamp_and_permission(dpath, source_stat, preserve, log, err_num);

    if(move) {
        if(rmdir(spath) < 0) {
            fprintf(log, "can't remove %s\n", spath);
            (*err_num)++;
        }
    }

    return TRUE;
}

static BOOL copy(char* spath, char* dpath, struct stat* source_stat, BOOL move, BOOL preserve, FILE* log, int* err_num)
{
    /// regular file ///
    if(S_ISREG(source_stat->st_mode)) {
        int fd = open(spath, O_RDONLY);
        if(fd < 0) {
            fprintf(log, "%s --> can't open this file\n", spath);
            (*err_num)++;
            return TRUE;
        }

        mode_t umask_before = umask(0);
        int fd2 = open(dpath, O_WRONLY|O_TRUNC|O_CREAT
              , source_stat->st_mode & (S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP
                     |S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH)
            );
        umask(umask_before);

        if(fd2 < 0) {
            fprintf(log, "%s --> can't open %s\n", spath, dpath);
            (*err_num)++;
            close(fd);
            return TRUE;
        }

        char buf[BUFSIZ];
        while(1) {
            /// key input? ///
            fd_set mask;

            FD_ZERO(&mask);
            FD_SET(0, &mask);

            struct timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = 0;

            select(1, &mask, NULL, NULL, &tv);
            
            if(FD_ISSET(0, &mask)) {
                int meta;
                int key = xgetch(&meta);

                if(key == 3 || key == 7 || key == 27) { // CTRL-G and CTRL-C
                    merr_msg("canceled");
                    close(fd);
                    close(fd2);
                    return FALSE;
                }
            }
            
            int n = read(fd, buf, BUFSIZ);
            if(n < 0) {
                fprintf(log, "%s --> can't read this file\n", spath);
                (*err_num)++;
                close(fd);
                close(fd2);
                (void)unlink(dpath);
                return TRUE;
            }
            
            if(n == 0) {
                break;
            }

            if(write(fd2, buf, n) < 0) {
                fprintf(log, "%s --> can't write this file\n", spath);
                (*err_num)++;
                close(fd);
                close(fd2);
                (void)unlink(dpath);
                return TRUE;
            }
        }
    
        (void)close(fd);
        (void)close(fd2);

        copy_timestamp_and_permission(dpath, source_stat, preserve, log, err_num);
    }
    /// sym link ///
    else if(S_ISLNK(source_stat->st_mode)) {
        char link[PATH_MAX];
        int n = readlink(spath, link, PATH_MAX);
        if(n < 0) {
            fprintf(log, "%s --> can't readlink this symbolic link\n", spath);
            (*err_num)++;
            return TRUE;
        }
        link[n] = 0;

        if(symlink(link, dpath) < 0) {
            fprintf(log, "%s --> can't symlink this symbolic link\n", spath);
            (*err_num)++;
            return TRUE;
        }

        if(preserve || move) {
            if(getuid() == 0) {
                if(lchown(dpath, source_stat->st_uid, source_stat->st_gid) < 0) {
                    fprintf(log, "owner of %s\n", dpath);
                    (*err_num)++;
                    return TRUE;
                }
            }
        }
    }
    /// character device ///
    else if(S_ISCHR(source_stat->st_mode)) {
        int major_ = major(source_stat->st_rdev);
        int minor_ = minor(source_stat->st_rdev);
        mode_t umask_before = umask(0);
        int result = mknod(dpath
                , (source_stat->st_mode & (S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH)) | S_IFCHR
                        , makedev(major_, minor_));
        umask(umask_before);

        if(result < 0) {
            fprintf(log, "%s --> can't mknod %s\n", spath, dpath);
            (*err_num)++;
            return TRUE;
        }

        copy_timestamp_and_permission(dpath, source_stat, preserve, log, err_num);
    }
    /// block device ///
    else if(S_ISBLK(source_stat->st_mode)) {
        int major_ = major(source_stat->st_rdev);
        int minor_ = minor(source_stat->st_rdev);
        mode_t umask_before = umask(0);

        int result = mknod(dpath
            , (source_stat->st_mode & (S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH)) | S_IFBLK
            , makedev(major_, minor_));
        umask(umask_before);
        if(result < 0) {
            fprintf(log, "%s --> can't mknod %s\n", spath, dpath);
            (*err_num)++;
            return TRUE;
        }

        copy_timestamp_and_permission(dpath, source_stat, preserve, log, err_num);
    }
    /// fifo ///
    else if(S_ISFIFO(source_stat->st_mode)) {
        mode_t umask_before = umask(0);
        int result = mknod(dpath
            , (source_stat->st_mode & (S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP |S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH)) | S_IFIFO
                            , 0);
        umask(umask_before);
        if(result < 0) {
            fprintf(log, "%s --> can't mknod %s\n", spath, dpath);
            (*err_num)++;
            return TRUE;
        }

        copy_timestamp_and_permission(dpath, source_stat, preserve, log, err_num);
    }
    /// socket ///
    else if(S_ISSOCK(source_stat->st_mode)) {
        mode_t umask_before = umask(0);
        int result = mknod(dpath
            , (source_stat->st_mode & (S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP |S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH)) | S_IFSOCK
            , 0);
        umask(umask_before);
        if(result < 0) {
            fprintf(log, "%s --> can't mknod %s\n", spath, dpath);
            (*err_num)++;
            return TRUE;
        }

        copy_timestamp_and_permission(dpath, source_stat, preserve, log, err_num);
    }
    /// non suport file ///
    else {
        fprintf(log, "file type of %s\n", spath);
        (*err_num)++;
        return TRUE;
    }

    return TRUE;
}

static BOOL copy_override(char* spath, char* dpath, char* dpath_dirname, struct stat* source_stat, BOOL* run_copy, BOOL* no_mkdir, FILE* log, int* err_num, enum eCopyOverrideWay* override_way)
{
    int raw_mode = mis_raw_mode();

    if(access(dpath, F_OK) == 0) {
        struct stat dpath_stat;

        if(lstat(dpath, &dpath_stat) < 0) {
            fprintf(log, "%s --> can't lstat %s\n", spath, dpath);
            (*err_num)++;
            return TRUE;
        }
        
        /// can I override ? ///
        BOOL select_newer = FALSE;

        if(S_ISDIR(dpath_stat.st_mode)) {
            *no_mkdir = TRUE;
        }
        else {
            if(*override_way == kNone) {
                int ret;
override_select_str:
                if(raw_mode) {
                    gOverrideSPath = spath;    // give the arguments to view_file_copy_override_box
                    gOverrideStat = source_stat;
                    gOverrideDPath = dpath;
                    gOverrideDStat = &dpath_stat;

                    gView = view_file_copy_override_box;

                    gView();

                    const char* str[] = {
                        "No", "Yes", "Newer", "Rename", "No(all)", "Yes(all)", "Newer(all)", "Cancel"
                    };

                    char buf[256];
                    snprintf(buf, 256, "override? ");
                    ret = select_str2(buf, (char**)str, 8, 7);
                    
                    gView = NULL;

                    clear();
                    view();
                    refresh();
                }
                else {
                    const char* str[] = {
                        "No", "Yes", "Newer", "Rename", "No(all)", "Yes(all)", "Newer(all)", "Cancel"
                    };

                    char buf[256];
                    snprintf(buf, 256, "override? ");
                    ret = select_str_on_readline(buf, (char**)str, 8, 7);
                }
                
                switch(ret) {
                    case 0: // no
                        *run_copy = FALSE;
                        break;

                    case 1: // yes
                        if(!remove_file(dpath, TRUE, FALSE, err_num, log)) {
                            return FALSE;
                        }
                        break;

                    case 2: // newer
                        select_newer = TRUE;
                        break;
                        
                    case 3: { // rename
                        char new_dpath[PATH_MAX];

                        if(raw_mode) {
                            char result[1024];
                            
                            BOOL exist_rename_file;
                            do {
                                if(input_box("input name:", result, 1024, "", 0) == 1) {
                                    goto override_select_str;
                                }
                                
                                xstrncpy(new_dpath, dpath_dirname, PATH_MAX);
                                xstrncat(new_dpath, result, PATH_MAX);
                                
                                exist_rename_file = access(new_dpath, F_OK) == 0;
                                
                                if(exist_rename_file) {
                                    merr_msg("same name exists");
                                }
                            } while(exist_rename_file);
                        }
                        else {
                            char result[1024];
                            
                            BOOL exist_rename_file;
                            do {
                                if(input_box_on_readline("input name:", result, 1024, "", 0) == 1) {
                                    goto override_select_str;
                                }
                                
                                xstrncpy(new_dpath, dpath_dirname, PATH_MAX);
                                xstrncat(new_dpath, result, PATH_MAX);
                                
                                exist_rename_file = access(new_dpath, F_OK) == 0;
                                
                                if(exist_rename_file) {
                                    merr_msg("same name exists\n");
                                }
                            } while(exist_rename_file);
                        }

                        xstrncpy(dpath, new_dpath, PATH_MAX);
                        }
                        break;
                        
                    case 4: // no all
                        *override_way = kNoAll;
                        *run_copy = FALSE;
                        break;

                    case 5: // yes all
                        if(!remove_file(dpath, TRUE, FALSE, err_num, log)) {
                            return FALSE;
                        }
                        *override_way = kYesAll;
                        break;

                    case 6: // newer all
                        *override_way = kSelectNewer;
                        select_newer = TRUE;
                        break;

                    case 7: // cancel
                        return FALSE;
                }
            }
            else if(*override_way == kYesAll) {
                if(!remove_file(dpath, TRUE, FALSE, err_num, log)) {
                    return FALSE;
                }
            }
            else if(*override_way == kNoAll) {
                *run_copy = FALSE;
            }
            else if(*override_way == kSelectNewer) {
                select_newer = TRUE;
            }
        }

        if(select_newer) {
            if(source_stat->st_mtime > dpath_stat.st_mtime) {
                if(!remove_file(dpath, TRUE, FALSE, err_num, log)) {
                    return FALSE;
                }
            }
            else {
                *run_copy = FALSE;
            }
        }
    }

    return TRUE;
}

// copy spath to dpath with checking override 
// spath, spath_dirname, spath_basename, dpath, dpath_dirname, dpath_basename are allocated with PATH_MAX size
// Assuming the file is directory, last character of spath, spath_dirname, spath_basename, dpath, dpath_dirname, dpath_basename is not /.
static BOOL do_copy(char* spath, char* spath_dirname, char* spath_basename, struct stat* source_stat, char* dpath, char* dpath_dirname, char* dpath_basename, BOOL move, BOOL preserve, FILE* log, int* err_num, enum eCopyOverrideWay* override_way)
{
    BOOL raw_mode = mis_raw_mode();

    /// Have the destination file existed already ? ///
    BOOL no_mkdir = FALSE;
    BOOL run_copy = TRUE;

    if(!copy_override(spath, dpath, dpath_dirname, source_stat, &run_copy, &no_mkdir, log, err_num, override_way)) {
        return FALSE;
    }

    if(run_copy) {
        /// dir ///
        if(S_ISDIR(source_stat->st_mode)) {
            /// msg ///
            char buf[PATH_MAX+16];
            snprintf(buf, PATH_MAX+16, "entering %s", spath);
            msg_nonstop("%s", buf);

            if(move) {
                if(rename(spath, dpath) < 0) {
                    if(!copy_directory_recursively(spath, spath_basename, dpath, dpath_dirname, source_stat, move, preserve, log, err_num, no_mkdir, override_way)) {
                        return FALSE;
                    }
                }
            }
            else {
                if(!copy_directory_recursively(spath, spath_basename, dpath, dpath_dirname, source_stat, move, preserve, log, err_num, no_mkdir, override_way)) {
                    return FALSE;
                }
            }
        }
        /// file ///
        else {
            /// msg ///
            char buf[PATH_MAX+16];
            if(move)
                snprintf(buf, PATH_MAX+16, "moving %s", spath);
            else
                snprintf(buf, PATH_MAX+16, "copying %s", spath);

            msg_nonstop("%s", buf);

            /// go ///
            if(move) {
                if(rename(spath, dpath) < 0) {
                    if(!copy(spath, dpath, source_stat, move, preserve, log, err_num)) {
                        return FALSE;
                    }

                    if(!remove_file(spath, TRUE, FALSE, err_num, log)) {
                        return FALSE;
                    }
                }
            }
            else {
                if(!copy(spath, dpath, source_stat, move, preserve, log, err_num)) {
                    return FALSE;
                }
            }
        }
    }
    else {
        fprintf(log, "%s --> can't overwrite %s\n", spath, dpath);
        (*err_num)++;
    }

    return TRUE;
}

static void make_absolute_path_from_relative_path(char* path1, char* dest_path, const int dest_path_size)
{
    const int len = strlen(path1);

    /// add current working directory ///
    char path2[PATH_MAX];

    if(path1[0] == '/') {
        xstrncpy(path2, path1, PATH_MAX);
    }
    else {
        char cwd[PATH_MAX];
        mygetcwd(cwd, PATH_MAX);

        xstrncpy(path2, cwd, PATH_MAX);
        int cwd_len = strlen(cwd);
        if(cwd[cwd_len-1] != '/') {
            xstrncat(path2, "/", PATH_MAX);
        }
        xstrncat(path2, path1, PATH_MAX);
    }

    /// remove continual /
    char path3[PATH_MAX];

    char* p = path2;
    char* p2 = path3;

    while(*p) {
        if(*p == '/') {
            while(*p == '/') {
                p++;
            }

            *p2++ = '/';
        }
        else {
            *p2++ = *p++;
        }
    }
    *p2 = 0;

    /// remove . and ..
    char path4[PATH_MAX];

    p = path3;
    p2 = path4;

    while(*p) {
        if(*p == '/' && *(p+1) == '.' && (*(p+2) == '/' || *(p+2) == 0)) {
            p+=2;
        }
        else if(*p == '/' && *(p+1) == '.' && *(p+2) == '.' && (*(p+3) == '/' || *(p+3) == 0)) {
            /// moving to parent directory ///
            while(1) {
                if(p2 < path4) {  // invalid path, so move to root 
                    p2 = path4;
                    break;
                }
                else if(*p2 == '/') {
                    *p2 = 0;  // delete / for next /..
                    break;
                }
                else {
                    p2--;
                }
            }

            p+=3;
        }
        else {
            *p2++ = *p++;
        }
    }

    if(p2 == path4) {
        *p2++ = '/';
    }
    *p2 = 0;

    xstrncpy(dest_path, path4, dest_path_size);
}

// dest is destination directory, not renaming.
static BOOL copy_file_into_directory(char* source2, struct stat* source_stat, char* dest2, BOOL move, BOOL preserve, FILE* log, int* err_num, enum eCopyOverrideWay* override_way)
{
    /// make absolute path from relative path ///
    char spath[PATH_MAX];
    make_absolute_path_from_relative_path(source2, spath, PATH_MAX);

    char dest3[PATH_MAX];
    make_absolute_path_from_relative_path(dest2, dest3, PATH_MAX);

    /// get dirname and basename
    char spath_dirname[PATH_MAX];
    char spath_basename[PATH_MAX];

    char dpath_dirname[PATH_MAX];
    char dpath_basename[PATH_MAX];

    char* p = spath + strlen(spath) -1;

    while(p >= spath) {
        if(*p == '/') {
            break;
        }
        else {
            p--;
        }
    }

    if(p < spath) {
        xstrncpy(spath_dirname, "", PATH_MAX);
        xstrncpy(spath_basename, spath, PATH_MAX);

        xstrncpy(dpath_dirname, dest3, PATH_MAX);
        xstrncpy(dpath_basename, spath, PATH_MAX);
    }
    else {
        memcpy(spath_dirname, spath, p - spath);
        spath_dirname[p - spath] = 0;

        xstrncpy(spath_basename, p + 1, PATH_MAX);

        xstrncpy(dpath_dirname, dest3, PATH_MAX);
        xstrncpy(dpath_basename, spath_basename, PATH_MAX);
    }

    /// make destination file name ///
    char dpath[PATH_MAX];

    xstrncpy(dpath, dpath_dirname, PATH_MAX);
    xstrncat(dpath, "/", PATH_MAX);
    xstrncat(dpath, dpath_basename, PATH_MAX);

    /// check invalid copy ///
    if(strcmp(spath, dpath) == 0) {  // source and destination are same
        fprintf(log, "%s --> source and destination are same\n", spath);
        (*err_num)++;
        return TRUE;
    }

    if(strcmp(spath_dirname, dpath) == 0) { // a parent of source file and a destination directory are same
        fprintf(log, "%s --> a parent of source file and a destination directory are same\n", spath);
        (*err_num)++;
        return TRUE;
    }

    if(S_ISDIR(source_stat->st_mode) && strstr(dpath, spath) == dpath) {        // copy parent directory to child directory
        fprintf(log, "%s --> can't copy parent directory to child directory.\n", spath);
        (*err_num)++;
        return TRUE;
    }

    /// do copy! ///
    if(!do_copy(spath, spath_dirname, spath_basename, source_stat, dpath, dpath_dirname, dpath_basename, move, preserve, log, err_num, override_way)) {
        return FALSE;
    }

    return TRUE;
}

static BOOL rename_copy(char* source2, struct stat* source_stat, char* dest2, BOOL move, BOOL preserve, FILE* log, int* err_num, enum eCopyOverrideWay* override_way)
{
    /// make absolute path from relative path ///
    char spath[PATH_MAX];
    make_absolute_path_from_relative_path(source2, spath, PATH_MAX);

    char dpath[PATH_MAX];
    make_absolute_path_from_relative_path(dest2, dpath, PATH_MAX);

    /// get spath basename and dirname ///
    char spath_dirname[PATH_MAX];
    char spath_basename[PATH_MAX];

    char* p = spath + strlen(spath) -1;

    while(p >= spath) {
        if(*p == '/') {
            break;
        }
        else {
            p--;
        }
    }

    if(p < spath) {
        strcpy(spath_dirname, "");
        xstrncpy(spath_basename, spath, PATH_MAX);
    }
    else {
        memcpy(spath_dirname, spath, p - spath);
        spath_dirname[p-spath] = 0;
        xstrncpy(spath_basename, p + 1, PATH_MAX);
    }

    /// get dpath_dirname and dpath_basename ///
    char dpath_dirname[PATH_MAX];
    char dpath_basename[PATH_MAX];

    p = dpath + strlen(dpath) - 1;

    while(p >= dpath) {
        if(*p == '/') {
            break;
        }
        else {
            p--;
        }
    }

    if(p < dpath) {
        xstrncpy(dpath_dirname, "", PATH_MAX);
        xstrncpy(dpath_basename, dpath, PATH_MAX);
    }
    else {
        memcpy(dpath_dirname, dpath, p - dpath);
        dpath_dirname[p-dpath] = 0;
        xstrncpy(dpath_basename, p + 1, PATH_MAX);
    }

    /// do copy ///
    if(!do_copy(spath, spath_dirname, spath_basename, source_stat, dpath, dpath_dirname, dpath_basename, move, preserve, log, err_num, override_way)) {
        return FALSE;
    }

    return TRUE;
}


// this function is entrance point from other module

// source and dest must be allocated with PATH_MAX size

// if destination is directory, last character of dest require / character.
// if destination is directory, source is copied into destination directory, not renaming copy
// if destination is file, source is copied with renaming

// log must be oppend. It will be written error log.
// if err_num is setted greater than 0, error occured.
BOOL copy_file(char* source, char* dest, BOOL move, BOOL preserve, enum eCopyOverrideWay* override_way, FILE* log, int* err_num)
{
    BOOL raw_mode = mis_raw_mode();

    /// have the user pressed C-c ? ///
    fd_set mask;

    FD_ZERO(&mask);
    FD_SET(0, &mask);

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;

    select(1, &mask, NULL, NULL, &tv);
    
    if(FD_ISSET(0, &mask)) {
        int meta;
        int key = xgetch(&meta);

        if(key == 3 || key == 7 || key == 27) {   // CTRL-G and CTRL-C
            merr_msg("copy_file: canceled");
            return FALSE;
        }
    }

    /// source stat ///
    struct stat source_stat;

    if(lstat(source, &source_stat) < 0) {
        fprintf(log, "%s --> can't lstat %s\n", source, source);
        (*err_num)++;
        return TRUE;
    }

    /// remove / of last character for system calls ///
    char source2[PATH_MAX];
    xstrncpy(source2, source, PATH_MAX);

    int slen = strlen(source2);

    if(source2[slen-1] == '/') { source2[slen-1] = 0; }

    /// does the destination file exist? ///
    if(access(dest, F_OK) != 0) {
        int dlen = strlen(dest);

        /// is this directory name? ///
        if(dest[dlen-1] == '/') {
            /// make the destination directory ///
            char* str[] = {
                "yes", "no"
            };

            char buf[BUFSIZ];
            snprintf(buf, BUFSIZ, "%s doesn't exist. create?", dest);

            int select_num;
            if(raw_mode) {
                select_num = select_str(buf, str, 2, 1);
            }
            else {
                select_num = select_str_on_readline(buf, str, 2, 1);
            }
                
            if(select_num == 0) {  // yes
                snprintf(buf, BUFSIZ, "mkdir -p \"%s\"", dest);
                if(system(buf) == 0) {
                    char dest2[PATH_MAX];
                    xstrncpy(dest2, dest, PATH_MAX);

                    dest2[dlen-1] = 0; // remove / of last character for system calls

                    if(!copy_file_into_directory(source2, &source_stat, dest2, move, preserve, log, err_num, override_way)) {
                        return FALSE;
                    }
                }
                else {
                    fprintf(log, "%s --> A can't mkdir -p \"%s\"\n", source2, dest);
                    (*err_num)++;
                }
            }
            else {
                fprintf(log, "%s --> B can't mkdir -p \"%s\"\n", source2, dest);
                (*err_num)++;
            }
        }
        else {
            if(!rename_copy(source2, &source_stat, dest, move, preserve, log, err_num, override_way)) {
                return FALSE;
            }
        }
    }
    else {
        struct stat dest_stat;
        if(stat(dest, &dest_stat) == 0) {
            int dlen = strlen(dest);
            char dest2[PATH_MAX];
            xstrncpy(dest2, dest, PATH_MAX);

            if(dest2[dlen-1] == '/') { dest2[dlen-1] = 0; }  // remove / of last character for system calls

            /// dest is directory ///
            if(S_ISDIR(dest_stat.st_mode)) {
                if(!copy_file_into_directory(source2, &source_stat, dest2, move, preserve, log, err_num, override_way)) {
                    return FALSE;
                }
            }
            /// dest is file ///
            else {
                if(!rename_copy(source2, &source_stat, dest2, move, preserve, log, err_num, override_way)) {
                    return FALSE;
                }
            }
        } else {
            fprintf(log, "%s --> can't stat %s\n", source2, dest);
            (*err_num)++;
        }
    }

    return TRUE;
}

enum eRemoveWriteProtected { kWPNone, kWPYesAll, kWPNoAll, kWPCancel };

BOOL remove_file_core(char* path, BOOL no_ctrl_c, BOOL message, enum eRemoveWriteProtected* remove_write_protected, int* err_num, FILE* log)
{
    /// key input ? ///
    if(!no_ctrl_c) {
        fd_set mask;

        FD_ZERO(&mask);
        FD_SET(0, &mask);

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 0;

        select(1, &mask, NULL, NULL, &tv);
    
        if(FD_ISSET(0, &mask)) {
            int meta;
            int key = xgetch(&meta);

            if(key == 3 || key == 7 || key == 27) { // CTRL-C and CTRL-G
                return FALSE;
            }
        }
    }

    /// cancel ///
    struct stat stat_;

    if(lstat(path, &stat_) < 0) {
        return TRUE;
    }
    else {
        /// file ///
        if(!S_ISDIR(stat_.st_mode)) {
            if(!(stat_.st_mode&S_IWUSR) && !(stat_.st_mode&S_IWGRP) &&
                !(stat_.st_mode&S_IWOTH) )
            {
                if(*remove_write_protected == kWPNoAll) {
                    return TRUE;
                }
                else if(*remove_write_protected == kWPYesAll) {
                }
                else {
                    const char* str[] = {
                        "No", "Yes", "No(all)", "Yes(all)", "Cancel"
                    };
                    
                    char buf[PATH_MAX+20];
                    snprintf(buf, PATH_MAX+20, "remove write-protected file(%s)", path);
                    int ret = select_str2(buf, (char**)str, 5, 4);

                    switch(ret) {
                        case 0:
                            return TRUE;

                        case 1:
                            break;

                        case 2:
                            *remove_write_protected = kWPNoAll;
                            return TRUE;

                        case 3:
                            *remove_write_protected = kWPYesAll;
                            break;

                        case 4:
                            return FALSE;

                        default:
                            break;
                    }
                }
            }

            /// msg ///
            if(message) {
                char buf[PATH_MAX+20];
                snprintf(buf, PATH_MAX+20, "deleting %s", path);
                msg_nonstop("%s", buf);
            }

            /// go ///
            if(unlink(path) < 0) {
                switch(errno) {
                    case EACCES:
                    case EPERM:
                        fprintf(log, "can't remove. permission denied. (%s)\n", path);
                        break;

                    case EROFS:
                        fprintf(log, "can't removepermission denied. (%s)\n", path);
                        break;

                    default:
                        fprintf(log, "unlink err. (%s)\n", path);
                        break;
                }
                return TRUE;
            }
        }
        /// directory ///
        else {
            char buf[PATH_MAX+20];
            snprintf(buf, PATH_MAX+20, "entering %s", path);
            msg_nonstop("%s", buf);
            
            DIR* dir = opendir(path);
            if(dir == NULL) {
                fprintf(log, "can't remove opendir err(%s)\n", path);
                return TRUE;
            }
            struct dirent* entry;
            while((entry = readdir(dir)) != 0) {
                if(strcmp(entry->d_name, ".") != 0
                    && strcmp(entry->d_name, "..") != 0)
                {
                    char path2[PATH_MAX];
                    xstrncpy(path2, path, PATH_MAX);
                    xstrncat(path2, "/", PATH_MAX);
                    xstrncat(path2, entry->d_name, PATH_MAX);

                    if(!remove_file(path2, no_ctrl_c, FALSE, err_num, log)) {
                        (void)closedir(dir);
                        return FALSE;
                    }
                }
            }
            (void)closedir(dir);

            if(rmdir(path) < 0) {
                switch(errno) {
                    case EACCES:
                    case EPERM:
                        fprintf(log, "can't remove. permission denied.(%s)", path);
                        break;

                    case EROFS:
                        fprintf(log, "can't remove. file system is readonly. (%s)", path);
                        break;

                    default:
                        fprintf(log, "rmdir err(%s)", path);
                        break;
                }

                return TRUE;
            }
        }

        return TRUE;
    }
}

// path must be allocated with PATH_MAX size
BOOL remove_file(char* path, BOOL no_ctrl_c, BOOL message, int* err_num, FILE* log)
{
    enum eRemoveWriteProtected remove_write_protected = kWPNone;

    if(!remove_file_core(path, no_ctrl_c, message, &remove_write_protected, err_num, log)) {
        return FALSE;
    }

    return TRUE;
}
