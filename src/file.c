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

enum eCopyOverride gCopyOverride = kNone;
enum eWriteProtected gWriteProtected = kWPNone;

static void null_fun()
{
}

void file_copy_override_box(char* spath, struct stat stat_, char* dfile, struct stat dfstat)
{
    const int maxx = mgetmaxx();
    const int maxy = mgetmaxy();
    
    const int y = maxy/2 - 5;
    
    mbox(y, 4, maxx-8, 11);
    
    time_t t = stat_.st_mtime;
    struct tm* tm_ = (struct tm*)localtime(&t);
    
    int year = tm_->tm_year-100;
    if(year < 0) year+=100;
    while(year > 100) year-=100;
    
    mvprintw(y+1, 5, "source");
    char tmp[PATH_MAX];
    xstrncpy(tmp, spath, PATH_MAX);
    if(S_ISDIR(stat_.st_mode)) {
        xstrncat(tmp, "/", PATH_MAX);
    }
    else if(S_ISFIFO(stat_.st_mode)) {
        xstrncat(tmp, "|", PATH_MAX);
    }
    else if(S_ISSOCK(stat_.st_mode)) {
        xstrncat(tmp, "=", PATH_MAX);
    }
    else if(S_ISLNK(stat_.st_mode)) {
        xstrncat(tmp, "@", PATH_MAX);
    }
    
    
    mvprintw(y+2, 5, "path: %s", tmp);
    mvprintw(y+3, 5, "size: %d", stat_.st_size);
    mvprintw(y+4, 5, "time: %02d-%02d-%02d %02d:%02d", year, tm_->tm_mon+1
           , tm_->tm_mday, tm_->tm_hour, tm_->tm_min);
    
    t = dfstat.st_mtime;
    tm_ = (struct tm*)localtime(&t);
    
    year = tm_->tm_year-100;
    if(year < 0) year+=100;
    while(year > 100) year-=100;
    
    mvprintw(y+6, 5, "distination");
    char tmp2[PATH_MAX];
    xstrncpy(tmp2, dfile, PATH_MAX);
    if(S_ISDIR(dfstat.st_mode)) {
        xstrncat(tmp2, "/", PATH_MAX);
    }
    else if(S_ISFIFO(dfstat.st_mode)) {
        xstrncat(tmp, "|", PATH_MAX);
    }
    else if(S_ISSOCK(dfstat.st_mode)) {
        xstrncat(tmp, "=", PATH_MAX);
    }
    else if(S_ISLNK(dfstat.st_mode)) {
        xstrncat(tmp, "@", PATH_MAX);
    }
    
    mvprintw(y+7, 5, "path: %s", tmp2);
    mvprintw(y+8, 5, "size: %d", dfstat.st_size);
    mvprintw(y+9, 5, "time: %02d-%02d-%02d %02d:%02d", year, tm_->tm_mon+1
           , tm_->tm_mday, tm_->tm_hour, tm_->tm_min);
}

static char* gSPath;
static struct stat gStat;
static char* gDFile;
static struct stat gDFStat;

void view_file_copy_override_box()
{
    file_copy_override_box(gSPath, gStat, gDFile, gDFStat);
}

BOOL file_copy(char* spath, char* dpath, BOOL move, BOOL preserve)
{
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

        if(key == 3 || key == 7 || key == 27) {   // CTRL-G and CTRL-C
            merr_msg("file_copy: canceled");
            return FALSE;
        }
    }
    
    /// new file name check ///
    char new_fname[PATH_MAX];
    xstrncpy(new_fname, "", PATH_MAX);
    
    struct stat dstat;
    const int dlen = strlen(dpath);

    if(stat(dpath, &dstat) < 0) {
        if(dpath[dlen -1] == '/') {
            merr_msg("file_copy: destination_err(%s)", dpath);
            return FALSE;
        }
        else {
            char* p = dpath + dlen;
            while(p != dpath && *p != '/') {
                p--;
            }
            xstrncpy(new_fname, p+1, PATH_MAX);

            dpath[p-dpath+1] = 0;
        }
    }
    else if(!S_ISDIR(dstat.st_mode)) {
        char* p = dpath + dlen;
        while(p != dpath && *p != '/') {
            p--;
        }
        xstrncpy(new_fname, p+1, PATH_MAX);

        dpath[p-dpath+1] = 0;
    }
    
    /// add / ///
    char new_dpath[PATH_MAX];
    if(dpath[strlen(dpath) -1] != '/') {
        xstrncpy(new_dpath, dpath, PATH_MAX);
        xstrncat(new_dpath, "/", PATH_MAX);

        dpath = new_dpath;
    }

    /// dpath check ///
    if(stat(dpath, &dstat) < 0) {
        merr_msg("file_copy: destination_err(%s)", dpath);
        return FALSE;
    }
    if(!S_ISDIR(dstat.st_mode)) {
        merr_msg("file_copy: destination err(%s)", dpath);
        return FALSE;
    }

    /// illegal argument check ///
    char spath2[PATH_MAX];

    xstrncpy(spath2, spath, PATH_MAX);
    xstrncat(spath2, "/", PATH_MAX);
        
    if(strstr(dpath, spath2) == dpath) {
        merr_msg("file_copy: destination err(%s)", dpath);
        return FALSE;
    }
    
    /// source stat ///
    struct stat stat_;

    if(lstat(spath, &stat_) < 0) {
        return TRUE;
    }
    else {
        /// get destination file name ///
        char dfile[PATH_MAX];
        
        if(strcmp(new_fname, "") == 0) {
            char sfname[PATH_MAX];
            int i;
            const int len = strlen(spath);
            for(i = len-1;
                i > 0;
                i--)
            {
                if(spath[i] == '/') break;
            }
            xstrncpy(sfname, spath + i + 1, PATH_MAX);

            /// get destination file name ///
            xstrncpy(dfile, dpath, PATH_MAX);
            xstrncat(dfile, sfname, PATH_MAX);
        }
        else {
            xstrncpy(dfile, dpath, PATH_MAX);
            xstrncat(dfile, new_fname, PATH_MAX);
        }

        /// illegal argument check ///
        if(strcmp(spath, dfile) == 0) {
            return FALSE;
        }

        /// dir ///
        if(S_ISDIR(stat_.st_mode)) {
            /// msg ///
            char buf[PATH_MAX+10];
            snprintf(buf, PATH_MAX+10, "entering %s", spath);
            msg_nonstop("%s", buf);

            /// override ///
            BOOL no_mkdir = FALSE;
            BOOL select_newer = FALSE;
            struct stat dfstat;
            if(access(dfile, F_OK) == 0) {
                if(lstat(dfile, &dfstat) < 0) {
                    merr_msg("file_copy: lstat() err2(%s)", dfile);
                    return FALSE;
                }
        
                /// ok ? ///
                if(S_ISDIR(dfstat.st_mode)) {
                    no_mkdir = TRUE;
                }
                else {
                    if(gCopyOverride == kNone) {
override_select_str:
                        
                        gSPath = spath;
                        gStat = stat_;
                        gDFile = dfile;
                        gDFStat = dfstat;

                        gView = view_file_copy_override_box;

                        gView();

                        const char* str[] = {
                            "No", "Yes", "Newer", "Rename", "No(all)", "Yes(all)", "Newer(all)", "Cancel"
                        };

                        char buf[256];
                        snprintf(buf, 256, "override? ");
                        int ret = select_str2(buf, (char**)str, 8, 7);
                        
                        gView = NULL;

                        clear();
                        view();
                        refresh();
                        
                        switch(ret) {
                        case 0:
                            return TRUE;
                        case 1:
                            file_remove(dfile, TRUE, FALSE);
                            break;
                        case 2:
                            select_newer = TRUE;
                            break;
                            
                        case 3: {
                            char result[1024];
                            
                            char dfile_tmp[PATH_MAX];
                        
                            int result2;
                            BOOL exist_rename_file;
                            do {
                                result2 = input_box("input name:", result, 1024, "", 0);
                                
                                if(result2 == 1) {
                                    goto override_select_str;
                                }
                                
                                xstrncpy(dfile_tmp, dpath, PATH_MAX);
                                xstrncat(dfile_tmp, result, PATH_MAX);
                                
                                exist_rename_file = access(dfile_tmp, F_OK) == 0;
                                
                                if(exist_rename_file) {
                                    merr_msg("same name exists");
                                }
                            } while(exist_rename_file);
                            
                            xstrncpy(dfile,dfile_tmp, PATH_MAX);
                            
                            }
                            break;
                            
                        case 4:
                            gCopyOverride = kNoAll;
                            return TRUE;
                        case 5:
                            if(!file_remove(dfile, TRUE, FALSE)) {
                                return FALSE;
                            }
                            gCopyOverride = kYesAll;
                            break;
                        case 6:
                            gCopyOverride = kSelectNewer;
                            select_newer = TRUE;
                            break;
                        case 7:
                            return FALSE;
                        }
                    }
                    else if(gCopyOverride == kYesAll) {
                        if(!file_remove(dfile, TRUE, FALSE)) {
                            return FALSE;
                        }
                    }
                    else if(gCopyOverride == kNoAll) {
                        return TRUE;
                    }
                    else if(gCopyOverride == kSelectNewer) {
                        select_newer = TRUE;
                    }
                }
            }

            if(select_newer) {
                if(stat_.st_mtime > dfstat.st_mtime) {
                    if(!file_remove(dfile, TRUE, FALSE)) {
                        return FALSE;
                    }
                }
                else {
                    return TRUE;
                }
            }

            if(move) {
                if(rename(spath, dfile) < 0) {
                    goto dir_copy;
                }
            }
            else {
dir_copy:
                null_fun();
                DIR* dir = opendir(spath);
                if(dir == NULL) {
                    merr_msg("file_copy: opendir err(%s)", spath);
                    return FALSE;
                }

                if(!no_mkdir) {
                    int result = mkdir(dfile, 0777);
/*
                    mode_t umask_before = umask(0);
                    int result = mkdir(dfile, stat_.st_mode & (S_IRUSR|S_IWUSR|S_IXUSR
                            |S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH));
                    umask(umask_before);
*/
                    if(result <0)
                    {
                        closedir(dir);
                        merr_msg("file_copy: mkdir err(%s)", dfile);
                        return FALSE;
                    }
                }

                struct dirent* entry;
                while(entry = readdir(dir)) {
                    if(strcmp(entry->d_name, ".") != 0
                        && strcmp(entry->d_name, "..") != 0)
                    {
                        char spath2[PATH_MAX];

                        xstrncpy(spath2, spath, PATH_MAX);
                        xstrncat(spath2, "/", PATH_MAX);
                        xstrncat(spath2, entry->d_name, PATH_MAX);

                        char dfile2[PATH_MAX];
        
                        xstrncpy(dfile2, dfile, PATH_MAX);
                        xstrncat(dfile2, "/", PATH_MAX);
        
                        if(!file_copy(spath2, dfile2, move, preserve)) {
                            closedir(dir);
                            return FALSE;
                        }
                    }
                }

                if(!no_mkdir) {
                    mode_t umask_before = umask(0);
                    chmod(dfile, stat_.st_mode & (S_IRUSR|S_IWUSR|S_IXUSR |S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH));
                    umask(umask_before);
                }

                if(preserve || move) {
                    if(getuid() == 0) {
                        if(chown(dfile, stat_.st_uid, stat_.st_gid) <0) {
                            merr_msg("file_copy: chown() err(%s)", dfile);
                            return FALSE;
                        }
                    }

                    struct utimbuf utb;

                    utb.actime = stat_.st_atime;
                    utb.modtime = stat_.st_mtime;

                    if(utime(dfile, &utb) < 0) {
                        merr_msg("file_copy: utime() err(%s)", dfile);
                        return FALSE;
                    }
                }

                mode_t umask_before = umask(0);
#if defined(S_ISTXT)
                if(chmod(dfile, stat_.st_mode & (S_IRUSR|S_IWUSR|S_IXUSR
                    |S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH|S_ISUID|S_ISGID|S_ISTXT)) < 0) 
                {
                    msg_nonstop("file_copy: chmod() err(%s)", dfile);
                    //return FALSE;
                }
#else

#if defined(S_ISVTX)
                if(chmod(dfile, stat_.st_mode & (S_IRUSR|S_IWUSR|S_IXUSR
                            |S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH|S_ISUID|S_ISGID|S_ISVTX)) < 0) 
                {
                    msg_nonstop("file_copy: chmod() err(%s)", dfile);
                    //return FALSE;
                }
#else
                if(chmod(dfile, stat_.st_mode & (S_IRUSR|S_IWUSR|S_IXUSR
                |S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH|S_ISUID|S_ISGID)) < 0) 
                {
                    msg_nonstop("file_copy: chmod() err(%s)", dfile);
                    //return FALSE;
                }
#endif

#endif
                umask(umask_before);
        
                if(closedir(dir) < 0) {
                    merr_msg("file_copy: closedir() err");
                    return FALSE;
                }
                if(move) {
                    if(rmdir(spath) < 0) {
                        merr_msg("file_copy: rmdir() err(%s)", spath);
                        return FALSE;
                    }
                }
            }
        }
        /// file ///
        else {
            /// override ? ///
            if(access(dfile, F_OK) == 0) {
                BOOL select_newer = FALSE;
                if(gCopyOverride == kNone) {
                    struct stat dfstat;
                    
                    if(lstat(dfile, &dfstat) < 0) {
                        merr_msg("file_copy: lstat() err(%s)", dfile);
                        return FALSE;
                    }
        
override_select_str2:
                    gSPath = spath;
                    gStat = stat_;
                    gDFile = dfile;
                    gDFStat = dfstat;

                    gView = view_file_copy_override_box;

                    gView();
                    
                    const char* str[] = {
                        "No", "Yes", "Newer", "Rename", "No(all)", "Yes(all)", "Newer(all)", "Cancel"
                    };
                    
                    char buf[256];
                    snprintf(buf, 256, "override? ");
                    int ret = select_str2(buf, (char**)str, 8, 7);

                    gView = NULL;
                    
                    clear();
                    view();
                    refresh();
                    
                    switch(ret) {
                    case 0:
                        return TRUE;
                    case 1:
                        if(!file_remove(dfile, TRUE, FALSE)) {
                            return FALSE;
                        }
                        break;
                    case 2:
                        select_newer = TRUE;
                        break;
                    case 3: {
                        char result[1024];
                        
                        int result2;
                        BOOL exist_rename_file;
                        
                        char dfile_tmp[PATH_MAX];
                        do {
                            result2 = input_box("input name:", result, 1024, "", 0);
                            
                            if(result2 == 1) {
                                goto override_select_str2;
                            }
                            
                            xstrncpy(dfile_tmp, dpath, PATH_MAX);
                            xstrncat(dfile_tmp, result, PATH_MAX);
                            
                            exist_rename_file = access(dfile_tmp, F_OK) == 0;
                            if(exist_rename_file) {
                                merr_msg("same name exists");
                            }
                        } while(exist_rename_file);
                        
                        xstrncpy(dfile, dfile_tmp, PATH_MAX);
                        
                        }
                        break;
                        
                    case 4:
                        gCopyOverride = kNoAll;
                        return TRUE;
                    case 5:
                        if(!file_remove(dfile, TRUE, FALSE)) {
                            return FALSE;
                        }
                        gCopyOverride = kYesAll;
                        break;
                    case 6:
                        gCopyOverride = kSelectNewer;
                        select_newer = TRUE;
                        break;
                    case 7:
                        return FALSE;
                    }
                }
                else if(gCopyOverride == kYesAll) {
                    if(!file_remove(dfile, TRUE, FALSE)) {
                        return FALSE;
                    }
                }
                else if(gCopyOverride == kNoAll) {
                    return TRUE;
                }
                else if(gCopyOverride == kSelectNewer) {
                    select_newer = TRUE;
                }

                if(select_newer) {
                    struct stat dfstat;

                    if(lstat(dfile, &dfstat) < 0) {
                        merr_msg("file_copy: lstat() err3(%s)", dfile);
                        return FALSE;
                    }

                    if(stat_.st_mtime > dfstat.st_mtime) {
                        if(!file_remove(dfile, TRUE, FALSE)) {
                            return FALSE;
                        }
                    }
                    else {
                        return TRUE;
                    }
                }
            }

            /// msg ///
            char buf[PATH_MAX+10];
            if(move)
                snprintf(buf, PATH_MAX+10, "moving %s", spath);
            else
                snprintf(buf, PATH_MAX+10, "copying %s", spath);
            msg_nonstop("%s", buf);

            if(move) {
                if(rename(spath, dfile) < 0) {
                    goto file_copy;
                }
            }
            else {
file_copy:            
                /// file ///
                if(S_ISREG(stat_.st_mode)) {
                    int fd = open(spath, O_RDONLY);
                    if(fd < 0) {
                        merr_msg("file_copy: open(read) err(%s)", spath);
                        return FALSE;
                    }
                    mode_t umask_before = umask(0);
                    int fd2 = open(dfile, O_WRONLY|O_TRUNC|O_CREAT
                          , stat_.st_mode & (S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP
                                 |S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH)
                        );
                    umask(umask_before);
                    if(fd2 < 0) {
                        merr_msg("file_copy: open(write) err(%s)", dfile);
                        close(fd);
                        return FALSE;
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
                                merr_msg("file_copy: canceled");
                                close(fd);
                                close(fd2);
                                return FALSE;
                            }
                        }
                        
                        int n = read(fd, buf, BUFSIZ);
                        if(n < 0) {
                            merr_msg("file_copy: read err(%s)", spath);
                            close(fd);
                            close(fd2);
                            return FALSE;
                        }
                        
                        if(n == 0) {
                            break;
                        }
        
                        if(write(fd2, buf, n) < 0) {
                            merr_msg("file_copy: write err(%s)", dfile);
                            close(fd);
                            close(fd2);
                            return FALSE;
                        }
                    }
                
                    if(close(fd) < 0) {
                        merr_msg("file_copy: close err(%s)", spath);
                        return FALSE;
                    }
                    if(close(fd2) < 0) {
                        merr_msg("file_copy: close err fd2(%s)", dfile);
                        return FALSE;
                    }

                    
                    if(preserve || move) {
                        if(getuid() == 0) {
                            if(chown(dfile, stat_.st_uid, stat_.st_gid) < 0) {
                                merr_msg("file_copy: chown() err(%s)", dfile);
                                return FALSE;
                            }
                        }

                        struct utimbuf utb;

                        utb.actime = stat_.st_atime;
                        utb.modtime = stat_.st_mtime;

                        if(utime(dfile, &utb) < 0) {
                            merr_msg("file_copy: utime() err(%s)", dfile);
                            return FALSE;
                        }
                        
                    }
                    
                    umask_before = umask(0);
#if defined(S_ISTXT)
                    if(chmod(dfile, stat_.st_mode & (S_IRUSR|S_IWUSR|S_IXUSR
                    |S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH|S_ISUID|S_ISGID|S_ISTXT)) < 0) 
                    {
                        msg_nonstop("file_copy: chmod() err(%s)", dfile);
                        //return FALSE;
                    }
#else

#if defined(S_ISVTX)
                    if(chmod(dfile, stat_.st_mode & (S_IRUSR|S_IWUSR|S_IXUSR
                            |S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH|S_ISUID|S_ISGID|S_ISVTX)) < 0)
                    {
                        msg_nonstop("file_copy: chmod() err(%s)", dfile);
                        //return FALSE;
                    }
#else
                    if(chmod(dfile, stat_.st_mode & (S_IRUSR|S_IWUSR|S_IXUSR
                            |S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH|S_ISUID|S_ISGID)) < 0)
                    {
                        msg_nonstop("file_copy: chmod() err(%s)", dfile);
                        //return FALSE;
                    }
#endif

#endif
                    umask(umask_before);
                }
                /// sym link ///
                else if(S_ISLNK(stat_.st_mode)) {
                    char link[PATH_MAX];
                    int n = readlink(spath, link, PATH_MAX);
                    if(n < 0) {
                        merr_msg("file_copy: readlink err(%s)", spath);
                        return FALSE;
                    }
                    link[n] = 0;
        
                    if(symlink(link, dfile) < 0) {
                        merr_msg("file_copy: symlink err(%s)", dfile);
                        return FALSE;
                    }

                    //chmod(dfile, stat_.st_mode);

                    if(preserve || move) {
                        if(getuid() == 0) {
                            if(lchown(dfile, stat_.st_uid, stat_.st_gid) < 0) {
                                merr_msg("file_copy: lchown err(%s)", dfile);
                                return FALSE;
                            }
                        }
/*
                        struct utimbuf utb;

                        utb.actime = stat_.st_atime;
                        utb.modtime = stat_.st_mtime;

                        utime(dfile, &utb);
*/
                    }
                }
                /// character device ///
                else if(S_ISCHR(stat_.st_mode)) {
                    int major_ = major(stat_.st_rdev);
                    int minor_ = minor(stat_.st_rdev);
                    mode_t umask_before = umask(0);
                    int result = mknod(dfile, stat_.st_mode 
                                 & (S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP
                                 |S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH) 
                                 | S_IFCHR, makedev(major_, minor_));
                    umask(umask_before);

                    if(result < 0) {
                        merr_msg("making character device(%s) is err. only root can do that", dfile);
                        return FALSE;
                    }

                    if(preserve || move) {
                        if(getuid() == 0) {
                            if(chown(dfile, stat_.st_uid, stat_.st_gid) < 0) {
                                merr_msg("file_copy: chown err(%s)", dfile);
                                return FALSE;
                            }
                        }

                        struct utimbuf utb;

                        utb.actime = stat_.st_atime;
                        utb.modtime = stat_.st_mtime;

                        if(utime(dfile, &utb) < 0) {
                            merr_msg("file_copy: utime err(%s)", dfile);
                            return FALSE;
                        }
                    }

                    umask_before = umask(0);
#if defined(S_ISTXT)
                    if(chmod(dfile, stat_.st_mode & (S_IRUSR|S_IWUSR|S_IXUSR
                    |S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH|S_ISUID|S_ISGID|S_ISTXT)) < 0) 
                    {
                        msg_nonstop("file_copy: chmod() err(%s)", dfile);
                        //return FALSE;
                    }
#else

#if defined(S_ISVTX)
                    if(chmod(dfile, stat_.st_mode & (S_IRUSR|S_IWUSR|S_IXUSR
                            |S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH|S_ISUID|S_ISGID|S_ISVTX)) < 0)
                    {
                        msg_nonstop("file_copy: chmod() err(%s)", dfile);
                        //return FALSE;
                    }
#else
                    if(chmod(dfile, stat_.st_mode & (S_IRUSR|S_IWUSR|S_IXUSR
                            |S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH|S_ISUID|S_ISGID)) < 0)
                    {
                        msg_nonstop("file_copy: chmod() err(%s)", dfile);
                        //return FALSE;
                    }
#endif

#endif
                    umask(umask_before);
                    
                }
                /// block device ///
                else if(S_ISBLK(stat_.st_mode)) {
                    int major_ = major(stat_.st_rdev);
                    int minor_ = minor(stat_.st_rdev);
                    mode_t umask_before = umask(0);

                    int result = mknod(dfile, stat_.st_mode 
                                 & (S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP
                                 |S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH) 
                                 | S_IFBLK, makedev(major_, minor_));
                    umask(umask_before);
                    if(result < 0) {
                        merr_msg("file_copy: making block device(%s) is err.only root can do that", dfile);
                        return FALSE;
                    }

                    if(preserve || move) {
                        if(getuid() == 0) {
                            if(chown(dfile, stat_.st_uid, stat_.st_gid) < 0) {
                                merr_msg("file_copy: chown() err (%s)", dfile);
                                return FALSE;
                            }
                        }

                        struct utimbuf utb;

                        utb.actime = stat_.st_atime;
                        utb.modtime = stat_.st_mtime;

                        if(utime(dfile, &utb) < 0) {
                            merr_msg("file_copy: utime err(%s)", dfile);
                            return FALSE;
                        }
                    }

                    umask_before = umask(0);
#if defined(S_ISTXT)
                    if(chmod(dfile, stat_.st_mode & (S_IRUSR|S_IWUSR|S_IXUSR
                            |S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH|S_ISUID|S_ISGID|S_ISTXT)) < 0)
                    {
                        msg_nonstop("file_copy: chmod() err(%s)", dfile);
                        //return FALSE;
                    }
#else

#if defined(S_ISVTX)
                    if(chmod(dfile, stat_.st_mode & (S_IRUSR|S_IWUSR|S_IXUSR
                            |S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH|S_ISUID|S_ISGID|S_ISVTX)) < 0)
                    {
                        msg_nonstop("file_copy: chmod() err(%s)", dfile);
                        //return FALSE;
                    }
#else
                    if(chmod(dfile, stat_.st_mode & (S_IRUSR|S_IWUSR|S_IXUSR
                            |S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH|S_ISUID|S_ISGID)) < 0)
                    {
                        msg_nonstop("file_copy: chmod() err(%s)", dfile);
                        //return FALSE;
                    }
#endif

#endif
                    umask(umask_before);
                    
                }
                /// fifo ///
                else if(S_ISFIFO(stat_.st_mode)) {
                    mode_t umask_before = umask(0);
                    int result = mknod(dfile, stat_.st_mode 
                                 & (S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP
                                 |S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH) 
                                 | S_IFIFO, 0);
                    umask(umask_before);
                    if(result < 0) {
                        merr_msg("making fifo(%s) is err", dfile);
                        return FALSE;
                    }

                    if(preserve || move) {
                        if(getuid() == 0) {
                            if(chown(dfile, stat_.st_uid, stat_.st_gid) < 0) {
                                merr_msg("file_copy: chown() err(%s)", dfile);
                                return FALSE;
                            }
                        }

                        struct utimbuf utb;

                        utb.actime = stat_.st_atime;
                        utb.modtime = stat_.st_mtime;

                        if(utime(dfile, &utb) < 0) 
                        {
                            merr_msg("file_copy: utime err(%s)", dfile);
                            return FALSE;
                        }
                    }

                    umask_before = umask(0);
#if defined(S_ISTXT)
                    if(chmod(dfile, stat_.st_mode & (S_IRUSR|S_IWUSR|S_IXUSR
                            |S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH|S_ISUID|S_ISGID|S_ISTXT)) < 0)
                    {
                        msg_nonstop("file_copy: chmod() err(%s)", dfile);
                        //return FALSE;
                    }
#else

#if defined(S_ISVTX)
                    if(chmod(dfile, stat_.st_mode & (S_IRUSR|S_IWUSR|S_IXUSR
                            |S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH|S_ISUID|S_ISGID|S_ISVTX)) < 0)
                    {
                        msg_nonstop("file_copy: chmod() err(%s)", dfile);
                        //return FALSE;
                    }
#else
                    if(chmod(dfile, stat_.st_mode & (S_IRUSR|S_IWUSR|S_IXUSR
                            |S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH|S_ISUID|S_ISGID)) < 0)
                    {
                        msg_nonstop("file_copy: chmod() err(%s)", dfile);
                        //return FALSE;
                    }
#endif

#endif
                    umask(umask_before);
                }
                /// socket ///
                else if(S_ISSOCK(stat_.st_mode)) {
                    mode_t umask_before = umask(0);
                    int result = mknod(dfile, stat_.st_mode 
                                 & (S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP
                                 |S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH) 
                                 | S_IFSOCK, 0);
                    umask(umask_before);
                    if(result < 0) {
                        merr_msg("making socket(%s) is err", dfile);
                        return FALSE;
                    }

                    if(preserve || move) {
                        if(getuid() == 0) {
                            if(chown(dfile, stat_.st_uid, stat_.st_gid) < 0) {
                                merr_msg("file_copy: chown() err(%s)", dfile);
                                return FALSE;
                            }
                        }

                        struct utimbuf utb;

                        utb.actime = stat_.st_atime;
                        utb.modtime = stat_.st_mtime;

                        if(utime(dfile, &utb) < 0)
                        {
                            merr_msg("file_copy: utime() err(%s)", dfile);
                            return FALSE;
                        }
                    }

                    umask_before = umask(0);
#if defined(S_ISTXT)
                    if(chmod(dfile, stat_.st_mode & (S_IRUSR|S_IWUSR|S_IXUSR
                            |S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH|S_ISUID|S_ISGID|S_ISTXT)) < 0)
                    {
                        msg_nonstop("file_copy: chmod() err(%s)", dfile);
                        //return FALSE;
                    }
#else

#if defined(S_ISVTX)
                    if(chmod(dfile, stat_.st_mode & (S_IRUSR|S_IWUSR|S_IXUSR
                            |S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH|S_ISUID|S_ISGID|S_ISVTX)) < 0)
                    {
                        msg_nonstop("file_copy: chmod() err(%s)", dfile);
                        //return FALSE;
                    }
#else
                    if(chmod(dfile, stat_.st_mode & (S_IRUSR|S_IWUSR|S_IXUSR
                            |S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH|S_ISUID|S_ISGID)) < 0)
                    {
                        msg_nonstop("file_copy: chmod() err(%s)", dfile);
                        //return FALSE;
                    }
#endif

#endif
                    umask(umask_before);
                    
                }
                /// non suport file ///
                else {
                    merr_msg("file_copy: sorry, non support file type(%s)", spath);
                    return FALSE;
                }

                if(move) {
                    if(!file_remove(spath, TRUE, FALSE)) {
                        return FALSE;
                    }
                }
            }
        }

        return TRUE;
    }
}

/// don't put '/' at last

BOOL file_remove(char* path, BOOL no_ctrl_c, BOOL message)
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
                if(gWriteProtected == kWPNoAll) {
                    return TRUE;
                }
                else if(gWriteProtected == kWPYesAll) {
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
                            break;

                        case 1:
                            break;

                        case 2:
                            gWriteProtected = kWPNoAll;
                            return TRUE;
                            break;

                        case 3:
                            gWriteProtected = kWPYesAll;
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
                        merr_msg("file_remove: permission denied. (%s)", path);
                        break;

                    case EROFS:
                        merr_msg("file_remove: file system is readonly. (%s)", path);
                        break;

                    default:
                        merr_msg("file_remove: unlink err(%s)", path);
                        break;
                }
                return FALSE;
            }
        }
        /// directory ///
        else {
            char buf[PATH_MAX+20];
            snprintf(buf, PATH_MAX+20, "entering %s", path);
            msg_nonstop("%s", buf);
            
            DIR* dir = opendir(path);
            if(dir == NULL) {
                merr_msg("file_remove: opendir err(%s)", path);
                return FALSE;
            }
            struct dirent* entry;
            while(entry = readdir(dir)) {
                if(strcmp(entry->d_name, ".") != 0
                    && strcmp(entry->d_name, "..") != 0)
                {
                    char path2[PATH_MAX];
                    xstrncpy(path2, path, PATH_MAX);
                    xstrncat(path2, "/", PATH_MAX);
                    xstrncat(path2, entry->d_name, PATH_MAX);

                    if(!file_remove(path2, no_ctrl_c, FALSE)) {
                        closedir(dir);
                        return FALSE;
                    }
                }
            }
            if(closedir(dir) < 0) {
                merr_msg("file_remove: closedir() err");
                return FALSE;
            }

            if(rmdir(path) < 0) {
                switch(errno) {
                    case EACCES:
                    case EPERM:
                        merr_msg("file_remove: permission denied.(%s)", path);
                        break;

                    case EROFS:
                        merr_msg("file_remove: file system is readonly. (%s)", path);
                        break;

                    default:
                        merr_msg("file_remove: rmdir err(%s)", path);
                        break;
                }
                return FALSE;
            }
        }

        
        return TRUE;
    }
}

