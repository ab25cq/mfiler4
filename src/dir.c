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

char* mygetpwuid(struct stat* statbuf)
{
    struct passwd* ud;
    
    ud = getpwuid(statbuf->st_uid);
        
    if(ud)
        return ud->pw_name;
    else
        return NULL;
}

char* mygetgrgid(struct stat* statbuf)
{
    struct group* gd;

    gd = getgrgid(statbuf->st_gid);
    if(gd)
        return gd->gr_name;
    else
        return NULL;
}

///////////////////////////////////////////////////
// sFile
///////////////////////////////////////////////////
sFile* sFile_new(char* name, char* name_view, char* linkto, struct stat* stat_, struct stat* lstat_, BOOL mark, char* user, char* group, int uid, int gid)
{
    sFile* self = (sFile*)MALLOC(sizeof(sFile));
    
    self->mName = STRING_NEW_MALLOC(name);
    self->mNameView = STRING_NEW_MALLOC(name_view);
    self->mLinkTo = STRING_NEW_MALLOC(linkto);
    self->mStat = *stat_;
    self->mLStat = *lstat_;
    self->mMark = mark;
    if(user == NULL) {
        char tmp[128];
        snprintf(tmp, 128, "%d", uid);
        self->mUser = STRING_NEW_MALLOC(tmp);
    }
    else {
        self->mUser = STRING_NEW_MALLOC(user);
    }

    if(group == NULL) {
        char tmp[128];
        snprintf(tmp, 128, "%d", gid);
        self->mGroup = STRING_NEW_MALLOC(tmp);
    }
    else {
        self->mGroup = STRING_NEW_MALLOC(group);
    }

    self->mSortRandom = 0;

    return self;
}

void sFile_delete(sFile* self) 
{
    string_delete_on_malloc(self->mName);
    string_delete_on_malloc(self->mNameView);
    string_delete_on_malloc(self->mLinkTo);
    string_delete_on_malloc(self->mUser);
    string_delete_on_malloc(self->mGroup);

    FREE(self);
}

///////////////////////////////////////////////////
// sDir
///////////////////////////////////////////////////
sDir* sDir_new()
{
    sDir* self = (sDir*)MALLOC(sizeof(sDir));

    self->mPath = STRING_NEW_MALLOC("");

    self->mMask = STRING_NEW_MALLOC("");
    self->mActive = FALSE;

    self->mFiles = VECTOR_NEW_MALLOC(50);

    self->mScrollTop = 0;
    self->mCursor = 0;

    self->mHistory = VECTOR_NEW_MALLOC(10);
    vector_add(self->mHistory, STRING_NEW_MALLOC(string_c_str(self->mPath)));
    self->mHistoryCursor = 0;

#ifdef HAVE_ONIGURUMA_H
    self->mMaskReg = NULL;
#endif

    sDir_setmask(self, "");

    self->mVD = FALSE;

    return self;
}

void sDir_delete(sDir* self)
{
    string_delete_on_malloc(self->mPath);
    string_delete_on_malloc(self->mMask);
    int i;
    for(i=0; i<vector_count(self->mFiles); i++) {
        sFile_delete(vector_item(self->mFiles, i));
    }
    vector_delete_on_malloc(self->mFiles);

    for(i=0; i<vector_count(self->mHistory); i++) {
        string_delete_on_malloc(vector_item(self->mHistory, i));
    }
    vector_delete_on_malloc(self->mHistory);
    FREE(self);
}

void sDir_setmask(sDir* self, char* mask) 
{
    if(self->mMaskReg) onig_free(self->mMaskReg);

    OnigUChar* regex = (OnigUChar*)mask;
    OnigErrorInfo err_info;

    int r = onig_new(&self->mMaskReg, regex
        , regex + strlen((char*)regex)
        , ONIG_OPTION_IGNORECASE
        , ONIG_ENCODING_UTF8
        , ONIG_SYNTAX_DEFAULT
        ,  &err_info);

    if(r == ONIG_NORMAL) {
        string_put(self->mMask, mask);
    }
    else {
        string_put(self->mMask, "");
        self->mMaskReg = NULL;
    }
}

void sDir_mask(sDir* self)
{
    if(strcmp(string_c_str(self->mMask), "") != 0) {
        int i;
        for(i=0; i<vector_count(self->mFiles); i++) {
            sFile* file = vector_item(self->mFiles, i);
            char* fname = string_c_str(file->mName);

            if(!S_ISDIR(file->mStat.st_mode) 
                && strcmp(fname, ".index") != 0) 
            {
                OnigRegion* region = onig_region_new();

                int r2 = onig_search(self->mMaskReg, fname
                   , fname + strlen(fname)
                   , fname, fname + strlen(fname)
                   , region, ONIG_OPTION_IGNORECASE);

                if(r2 < 0) {
                    sFile* f = vector_item(self->mFiles, i);
                    sFile_delete(f);
                    vector_erase(self->mFiles, i);
                    i--;
                }

                onig_region_free(region, 1);
            } 
            else if(S_ISDIR(file->mStat.st_mode)) {
                if(strcmp(fname, "..") != 0 && fname[0] == '.' && self->mDotDirMask != 0) {
                    sFile* f = vector_item(self->mFiles, i);
                    sFile_delete(f);
                    vector_erase(self->mFiles, i);
                    i--;
                }
            }
        }
    }
}

int convert_fname(char* src, char* des, int des_size) 
{
#if defined(__DARWIN__)
    if(is_all_ascii(kByte, src)) {
        xstrncpy(des, src, des_size);
    }
    else {
        if(kanji_convert(src, des, des_size, gKanjiCodeFileName, gKanjiCode) == -1) {
            return -1;
        }
    }
    return 0;
#else
    xstrncpy(des, src, des_size);
    return 0;
#endif
}

int convert_fname2(char* src, char* des, int des_size) 
{
    int i;
    char* p = src;
    char* p2 = des;
    while(*p && (p2 - des) < des_size-1) {
        if(*p >= 0 && *p < ' ' || *p == 127) {
            *p2++ = '^';
            *p2++ = *p + '@';
            p++;
        }
        else {
            *p2++ = *p++;
        }
    }
    *p2 = 0;
}

int sDir_read(sDir* self)
{
    if(self->mVD) {
        int i;
        for(i=0; i<vector_count(self->mFiles); i++) {
            sFile* file = vector_item(self->mFiles, i);
            char* fname1 = string_c_str(file->mName);

            char fname[4089];
            if(convert_fname(fname1, fname, 4089) == -1) {
                sFile* f = vector_item(self->mFiles, i);
                sFile_delete(f);
                vector_erase(self->mFiles, i);
                i--;
                continue;
            }

            char fname2[4089];
            convert_fname2(fname, fname2, 4089);

            char path[PATH_MAX];
            if(fname1[0] == '/') {
                xstrncpy(path, fname1, PATH_MAX);
            }
            else {
                snprintf(path, PATH_MAX, "%s%s", string_c_str(self->mPath),fname1);
            }
            
            struct stat lstat_;
            memset(&lstat_, 0, sizeof(struct stat));
            if(lstat(path, &lstat_) < 0) {
                sFile* f = vector_item(self->mFiles, i);
                sFile_delete(f);
                vector_erase(self->mFiles, i);
                i--;
                continue;
            }

            struct stat stat_;
            memset(&stat_, 0, sizeof(struct stat));
            if(stat(path, &stat_) < 0) {
                if(S_ISLNK(lstat_.st_mode)) {
                    stat_ = lstat_;
                }
                else {
                    sFile* f = vector_item(self->mFiles, i);
                    sFile_delete(f);
                    vector_erase(self->mFiles, i);
                    i--;
                    continue;
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

            /// refresh ///
            string_put(file->mLinkTo, linkto);
            file->mStat = stat_;
            file->mLStat = lstat_;
            if(user == NULL) {
                char tmp[128];
                snprintf(tmp, 128, "%d", lstat_.st_uid);
                string_put(file->mUser, tmp);
            }
            else {
                string_put(file->mUser, user);
            }
            if(group == NULL) {
                char tmp[128];
                snprintf(tmp, 128, "%d", lstat_.st_gid);
                string_put(file->mGroup, tmp);
            }
            else {
                string_put(file->mGroup, group);
            }

            file->mSortRandom = 0;
        }

        if(vector_count(self->mFiles) == 0) {
            string_put(self->mPath, "/");
            self->mVD = FALSE;
            return sDir_read(self);
        }
    }
    else {
        if(access(string_c_str(self->mPath), F_OK) != 0) {
            string_put(self->mPath, "/");
            return sDir_read(self);
        }

        sObject* mark_files = HASH_NEW_MALLOC(10);
        int i;
        for(i=0; i<vector_count(self->mFiles); i++) {
            sFile* file = vector_item(self->mFiles, i);

            if(file->mMark) {
                hash_put(mark_files, string_c_str(file->mName), (void*)1);
            }
        }

        for(i=0; i<vector_count(self->mFiles); i++) {
            sFile_delete(vector_item(self->mFiles, i));
        }
        vector_clear(self->mFiles);

        DIR* dir = opendir(string_c_str(self->mPath));
        if(dir == NULL) {
            string_put(self->mPath, "/");
            hash_delete_on_malloc(mark_files);
            return sDir_read(self);
        }
        
        struct dirent* entry;
        while(entry = readdir(dir)) {
            char fname[4089];
            if(convert_fname(entry->d_name, fname, 4089) == -1) {
                continue;
            }
            char fname2[4089];
            convert_fname2(fname, fname2, 4089);

            char path[PATH_MAX];
            snprintf(path, PATH_MAX, "%s%s", string_c_str(self->mPath), entry->d_name);

            struct stat lstat_;
            memset(&lstat_, 0, sizeof(struct stat));
            if(lstat(path, &lstat_) < 0) {
                continue;
            }

            struct stat stat_;
            memset(&stat_, 0, sizeof(struct stat));
            if(stat(path, &stat_) < 0) {
                if(S_ISLNK(lstat_.st_mode)) {
                    stat_ = lstat_;
                }
                else {
                    continue;
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

            if(strcmp(entry->d_name, ".") != 0) {
                BOOL mark = FALSE;

                if(hash_item(mark_files, entry->d_name) != NULL) {
                    mark = TRUE;
                }
                
                /// add ///
                vector_add(self->mFiles, sFile_new(entry->d_name, fname2, linkto, &stat_, &lstat_, mark, user, group, lstat_.st_uid, lstat_.st_gid));
            }
        }
        
        closedir(dir);

        hash_delete_on_malloc(mark_files);
    }

    return 0;
}

///////////////////////////////////////////////////
// directory sort
///////////////////////////////////////////////////
static BOOL sort_name(void* left, void* right)
{
    sFile* l = (sFile*) left;
    sFile* r = (sFile*) right;

    char* lfname = string_c_str(l->mName);
    char* rfname = string_c_str(r->mName);

    if(strcmp(lfname, ".") == 0) return TRUE;
    if(strcmp(lfname, "..") == 0) {
        if(strcmp(rfname, ".") == 0) return FALSE;

        return TRUE;          
    }
    if(strcmp(rfname, ".") == 0) return FALSE;
    if(strcmp(rfname, "..") == 0) return FALSE;
    
    char* mark_up = getenv("SORT_MARK_UP");
    if(mark_up && strcmp(mark_up,"1") == 0) {
        if(l->mMark && !r->mMark) {
            return TRUE;
        }
        if(!l->mMark && r->mMark) {
            return FALSE;
        }
    }

    char* dir_up = getenv("SORT_DIR_UP");
    if(dir_up && strcmp(dir_up, "1") == 0) {
        if(!S_ISDIR(l->mStat.st_mode) && !S_ISDIR(r->mStat.st_mode)
            || S_ISDIR(l->mStat.st_mode) && S_ISDIR(r->mStat.st_mode) )
        {
            return strcasecmp(lfname, rfname) < 0;
        }

        if(S_ISDIR(l->mStat.st_mode))
            return TRUE;
        else
            return FALSE;
    }
    else {
        return strcasecmp(lfname, rfname) < 0;
    }
}

static BOOL sort_name_reverse(void* left, void* right)
{
    sFile* l = (sFile*) left;
    sFile* r = (sFile*) right;

    char* lfname = string_c_str(l->mName);
    char* rfname = string_c_str(r->mName);

    if(strcmp(lfname, ".") == 0) return TRUE;
    if(strcmp(lfname, "..") == 0) {
        if(strcmp(rfname, ".") == 0) return FALSE;

        return TRUE;          
    }
    if(strcmp(rfname, ".") == 0) return FALSE;
    if(strcmp(rfname, "..") == 0) return FALSE;

    char* mark_up = getenv("SORT_MARK_UP");
    if(mark_up && strcmp(mark_up, "1") == 0) {
        if(!l->mMark && r->mMark) {
            return TRUE;
        }
        if(l->mMark && !r->mMark) {
            return FALSE;
        }
    }
    char* dir_up = getenv("SORT_DIR_UP");
    if(dir_up && strcmp(dir_up, "1") == 0) {
        if(!S_ISDIR(l->mStat.st_mode) && !S_ISDIR(r->mStat.st_mode)
            || S_ISDIR(l->mStat.st_mode) && S_ISDIR(r->mStat.st_mode) )
            {
            return strcasecmp(lfname, rfname) > 0;
            }
    
        if(S_ISDIR(r->mStat.st_mode)) return FALSE;
        return TRUE;
    }
    else {
        return strcasecmp(lfname, rfname) > 0;
    }
}
    
static BOOL sort_ext(void* left, void* right)
{
     sFile* l = (sFile*) left;
     sFile* r = (sFile*) right;

     char* lfname = string_c_str(l->mName);
     char* rfname = string_c_str(r->mName);

     if(strcmp(lfname, ".") == 0) return TRUE;
     if(strcmp(lfname, "..") == 0) {
          if(strcmp(rfname, ".") == 0) return FALSE;

          return TRUE;          
     }
     if(strcmp(rfname, ".") == 0) return FALSE;
     if(strcmp(rfname, "..") == 0) return FALSE;

    char* mark_up = getenv("SORT_MARK_UP");
    if(mark_up && strcmp(mark_up, "1") == 0) {
        if(l->mMark && !r->mMark) {
            return TRUE;
        }
        if(!l->mMark && r->mMark) {
            return FALSE;
        }
    }
     char lext[PATH_MAX];
     extname(lext, PATH_MAX, lfname);
     char rext[PATH_MAX];
     extname(rext, PATH_MAX, rfname);
    
    BOOL result;
    char* dir_up = getenv("SORT_DIR_UP");
    if(dir_up && strcmp(dir_up, "1") == 0) {
         if(S_ISDIR(l->mStat.st_mode)) {
              if(S_ISDIR(r->mStat.st_mode))
                    result = strcasecmp(lfname, rfname) < 0;
              else
                    result = TRUE;
         }
         else if(S_ISDIR(r->mStat.st_mode)) {
              result = FALSE;
         }
         else {
              const int cmp = strcasecmp(lext, rext);
              if(cmp == 0) {
                  result = strcasecmp(lfname, rfname) < 0;
              }
              else {
                  result = cmp < 0;
              }
         }
     }
     else {
         const int cmp = strcasecmp(lext, rext);
         if(cmp == 0) {
             result = strcasecmp(lfname, rfname) < 0;
         }
         else {
             result = cmp < 0;
         }
     }
    
     return result;
}
    
static BOOL sort_ext_reverse(void* left, void* right)
{
     sFile* l = (sFile*) left;
     sFile* r = (sFile*) right;

     char* lfname = string_c_str(l->mName);
     char* rfname = string_c_str(r->mName);

     if(strcmp(lfname, ".") == 0) return TRUE;
     if(strcmp(lfname, "..") == 0) {
          if(strcmp(rfname, ".") == 0) return FALSE;

          return TRUE;          
     }
     if(strcmp(rfname, ".") == 0) return FALSE;
     if(strcmp(rfname, "..") == 0) return FALSE;

    char* mark_up = getenv("SORT_MARK_UP");
    if(mark_up && strcmp(mark_up, "1") == 0) {
        if(!l->mMark && r->mMark) {
            return TRUE;
        }
        if(l->mMark && !r->mMark) {
            return FALSE;
        }
    }
     char lext[PATH_MAX];
     extname(lext, PATH_MAX, lfname);
     char rext[PATH_MAX];
     extname(rext, PATH_MAX, rfname);
    
     BOOL result;
    char* dir_up = getenv("SORT_DIR_UP");
    if(dir_up && strcmp(dir_up, "1") == 0) {
         if(S_ISDIR(r->mStat.st_mode)) {
            if(S_ISDIR(l->mStat.st_mode))
                result= strcasecmp(lfname, rfname) >0;
            else
                result = FALSE;
         }
         else if(S_ISDIR(l->mStat.st_mode)) {
              result = TRUE;
         }
         else {
              const int cmp = strcasecmp(lext, rext);
              if(cmp == 0) {
                  result = strcasecmp(lfname, rfname) > 0;
              }
              else {
                  result = cmp > 0;
              }
         }
     }
     else {
         const int cmp = strcmp(lext, rext);
         if(cmp == 0) {
             result = strcasecmp(lfname, rfname) > 0;
         }
         else {
             result = cmp > 0;
         }
     }
    
     return result;
}

static BOOL sort_size(void* left, void* right)
{
     sFile* l = (sFile*) left;
     sFile* r = (sFile*) right;

     char* lfname = string_c_str(l->mName);
     char* rfname = string_c_str(r->mName);

     if(strcmp(lfname, ".") == 0) return TRUE;
     if(strcmp(lfname, "..") == 0) {
          if(strcmp(rfname, ".") == 0) return FALSE;

          return TRUE;          
     }
     if(strcmp(rfname, ".") == 0) return FALSE;
     if(strcmp(rfname, "..") == 0) return FALSE;
     
    char* mark_up = getenv("SORT_MARK_UP");
    if(mark_up && strcmp(mark_up, "1") == 0) {
        if(l->mMark && !r->mMark) {
            return TRUE;
        }
        if(!l->mMark && r->mMark) {
            return FALSE;
        }
    }
    char* dir_up = getenv("SORT_DIR_UP");
    if(dir_up && strcmp(dir_up, "1") == 0) {
         if(S_ISDIR(l->mStat.st_mode)) {
             if(S_ISDIR(r->mStat.st_mode)) return strcasecmp(lfname, rfname) < 0;
             return TRUE;
         }
         if(S_ISDIR(r->mStat.st_mode)) return FALSE;
     }
     
     const int lsize = S_ISDIR(l->mStat.st_mode) ? 0 : l->mLStat.st_size; 
     const int rsize = S_ISDIR(r->mStat.st_mode) ? 0 : r->mLStat.st_size;
    
     if(lsize == rsize) {
         return strcasecmp(lfname, rfname) < 0;
     }
     else {
         return lsize < rsize;
     }
}
  
static BOOL sort_size_reverse(void* left, void* right)
{
     sFile* l = (sFile*) left;
     sFile* r = (sFile*) right;

     char* lfname = string_c_str(l->mName);
     char* rfname = string_c_str(r->mName);

     if(strcmp(lfname, ".") == 0) return TRUE;
     if(strcmp(lfname, "..") == 0) {
          if(strcmp(rfname, ".") == 0) return FALSE;

          return TRUE;          
     }
     if(strcmp(rfname, ".") == 0) return FALSE;
     if(strcmp(rfname, "..") == 0) return FALSE;

    char* mark_up = getenv("SORT_MARK_UP");
    if(mark_up && strcmp(mark_up, "1") == 0) {
        if(!l->mMark && r->mMark) {
            return TRUE;
        }
        if(l->mMark && !r->mMark) {
            return FALSE;
        }
    }
    char* dir_up = getenv("SORT_DIR_UP");
    if(dir_up && strcmp(dir_up, "1") == 0) {
         if(S_ISDIR(r->mStat.st_mode)) {
             if(S_ISDIR(l->mStat.st_mode)) return strcasecmp(lfname, rfname) > 0;
             return FALSE;
         }
         if(S_ISDIR(l->mStat.st_mode)) return TRUE;
     }
     
     const int lsize = S_ISDIR(l->mStat.st_mode) ? 0 : l->mLStat.st_size; 
     const int rsize = S_ISDIR(r->mStat.st_mode) ? 0 : r->mLStat.st_size;

     if(lsize == rsize) {
         return strcasecmp(lfname, rfname) > 0;
     }
     else {
         return lsize > rsize;
     }
}
    
static BOOL sort_time(void* left, void* right)
{ 
     sFile* l = (sFile*) left;
     sFile* r = (sFile*) right;

     char* lfname = string_c_str(l->mName);
     char* rfname = string_c_str(r->mName);

     if(strcmp(lfname, ".") == 0) return TRUE;
     if(strcmp(lfname, "..") == 0) {
          if(strcmp(rfname, ".") == 0) return FALSE;

          return TRUE;
     }
     if(strcmp(rfname, ".") == 0) return FALSE;
     if(strcmp(rfname, "..") == 0) return FALSE;

    char* mark_up = getenv("SORT_MARK_UP");
    if(mark_up && strcmp(mark_up, "1") == 0) {
        if(l->mMark && !r->mMark) {
            return TRUE;
        }
        if(!l->mMark && r->mMark) {
            return FALSE;
        }
    }
    char* dir_up = getenv("SORT_DIR_UP");
    if(dir_up && strcmp(dir_up, "1") == 0) {
         if(S_ISDIR(l->mStat.st_mode)) {
             if(S_ISDIR(r->mStat.st_mode)) {
                 double cmp = difftime(l->mLStat.st_mtime, r->mLStat.st_mtime);
                 if(cmp == 0) {
                     return strcasecmp(lfname, rfname) < 0;
                 }
                 else {
                    return cmp < 0;
                }
             }
             return TRUE;
         }
         if(S_ISDIR(r->mStat.st_mode)) return FALSE;
     }
     
     double cmp = difftime(l->mLStat.st_mtime, r->mLStat.st_mtime);
     if(cmp == 0) {
         return strcasecmp(lfname, rfname) < 0;
     }
     else {
        return cmp < 0;
     }
}
    
static BOOL sort_time_reverse(void* left, void* right)
{ 
     sFile* l = (sFile*) left;
     sFile* r = (sFile*) right;

     char* lfname = string_c_str(l->mName);
     char* rfname = string_c_str(r->mName);

     if(strcmp(lfname, ".") == 0) return TRUE;
     if(strcmp(lfname, "..") == 0) {
          if(strcmp(rfname, ".") == 0) return FALSE;

          return TRUE;
     }
     if(strcmp(rfname, ".") == 0) return FALSE;
     if(strcmp(rfname, "..") == 0) return FALSE;

    char* mark_up = getenv("SORT_MARK_UP");
    if(mark_up && strcmp(mark_up, "1") == 0) {
        if(!l->mMark && r->mMark) {
            return TRUE;
        }
        if(l->mMark && !r->mMark) {
            return FALSE;
        }
    }
    char* dir_up = getenv("SORT_DIR_UP");
    if(dir_up && strcmp(dir_up, "1") == 0) {
         if(S_ISDIR(r->mStat.st_mode)) {
             if(S_ISDIR(l->mStat.st_mode)) {
                 double cmp = difftime(l->mLStat.st_mtime, r->mLStat.st_mtime);
                 if(cmp == 0) {
                     return strcasecmp(lfname, rfname) > 0;
                 }
                 else {
                    return cmp > 0;
                 }
             }
             return FALSE;
         }
         if(S_ISDIR(l->mStat.st_mode)) return TRUE;
     }
     
     double cmp = difftime(l->mLStat.st_mtime, r->mLStat.st_mtime);
     if(cmp == 0) {
         return strcasecmp(lfname, rfname) > 0;
     }
     else {
        return cmp > 0;
    }
}

static BOOL sort_user(void* left, void* right)
{ 
     sFile* l = (sFile*) left;
     sFile* r = (sFile*) right;

     char* lfname = string_c_str(l->mName);
     char* rfname = string_c_str(r->mName);

     if(strcmp(lfname, ".") == 0) return TRUE;
     if(strcmp(lfname, "..") == 0) {
          if(strcmp(rfname, ".") == 0) return FALSE;

          return TRUE;
     }
     if(strcmp(rfname, ".") == 0) return FALSE;
     if(strcmp(rfname, "..") == 0) return FALSE;

    char* mark_up = getenv("SORT_MARK_UP");
    if(mark_up && strcmp(mark_up, "1") == 0) {
        if(l->mMark && !r->mMark) {
            return TRUE;
        }
        if(!l->mMark && r->mMark) {
            return FALSE;
        }
    }
    char* dir_up = getenv("SORT_DIR_UP");
    if(dir_up && strcmp(dir_up, "1") == 0) {
         if(S_ISDIR(l->mStat.st_mode)) {
             if(S_ISDIR(r->mStat.st_mode)) {
                 if(l->mLStat.st_uid == r->mLStat.st_uid) {
                     return strcasecmp(lfname, rfname) < 0;
                 }
                 else {
                     return l->mLStat.st_uid < r->mLStat.st_uid;
                 }
             }
             return TRUE;
         }
         if(S_ISDIR(r->mStat.st_mode)) return FALSE;
     }
     
     if(l->mLStat.st_uid == r->mLStat.st_uid) {
         return strcasecmp(lfname, rfname) < 0;
     }
     else {
         return l->mLStat.st_uid < r->mLStat.st_uid;
     }
}
    
static BOOL sort_user_reverse(void* left, void* right)
{ 
     sFile* l = (sFile*) left;
     sFile* r = (sFile*) right;

     char* lfname = string_c_str(l->mName);
     char* rfname = string_c_str(r->mName);

     if(strcmp(lfname, ".") == 0) return TRUE;
     if(strcmp(lfname, "..") == 0) {
          if(strcmp(rfname, ".") == 0) return FALSE;

          return TRUE;
     }
     if(strcmp(rfname, ".") == 0) return FALSE;
     if(strcmp(rfname, "..") == 0) return FALSE;

    char* mark_up = getenv("SORT_MARK_UP");
    if(mark_up && strcmp(mark_up, "1") == 0) {
        if(!l->mMark && r->mMark) {
            return TRUE;
        }
        if(l->mMark && !r->mMark) {
            return FALSE;
        }
    }
    char* dir_up = getenv("SORT_DIR_UP");
    if(dir_up && strcmp(dir_up, "1") == 0) {
         if(S_ISDIR(r->mStat.st_mode)) {
             if(S_ISDIR(l->mStat.st_mode)) {
                 if(l->mLStat.st_uid == r->mLStat.st_uid) {
                     return strcasecmp(lfname, rfname) > 0;
                 }
                 else {
                    return l->mLStat.st_uid > r->mLStat.st_uid;
                 }
             }
//             if(S_ISDIR(l->mLStat.st_mode)) return strcasecmp(rfname, lfname) < 0;
             return FALSE;
         }
         if(S_ISDIR(l->mStat.st_mode)) return TRUE;
     }
     
     if(l->mLStat.st_uid == r->mLStat.st_uid) {
         return strcasecmp(lfname, rfname) > 0;
     }
     else {
        return l->mLStat.st_uid > r->mLStat.st_uid;
     }
}

static BOOL sort_group(void* left, void* right)
{ 
     sFile* l = (sFile*) left;
     sFile* r = (sFile*) right;

     char* lfname = string_c_str(l->mName);
     char* rfname = string_c_str(r->mName);

     if(strcmp(lfname, ".") == 0) return TRUE;
     if(strcmp(lfname, "..") == 0) {
          if(strcmp(rfname, ".") == 0) return FALSE;

          return TRUE;
     }
     if(strcmp(rfname, ".") == 0) return FALSE;
     if(strcmp(rfname, "..") == 0) return FALSE;

    char* mark_up = getenv("SORT_MARK_UP");
    if(mark_up && strcmp(mark_up, "1") == 0) {
        if(l->mMark && !r->mMark) {
            return TRUE;
        }
        if(!l->mMark && r->mMark) {
            return FALSE;
        }
    }
    char* dir_up = getenv("SORT_DIR_UP");
    if(dir_up && strcmp(dir_up, "1") == 0) {
         if(S_ISDIR(l->mStat.st_mode)) {
             if(S_ISDIR(r->mStat.st_mode)) {
                 if(l->mLStat.st_gid == r->mLStat.st_gid) {
                     return strcasecmp(lfname, rfname) < 0;
                 }
                 else {
                    return l->mLStat.st_gid < r->mLStat.st_gid;
                 }
             }
//             if(S_ISDIR(r->mLStat.st_mode)) return strcasecmp(lfname, rfname) < 0;
             return TRUE;
         }
         if(S_ISDIR(r->mStat.st_mode)) return FALSE;
     }
     
     if(l->mLStat.st_gid == r->mLStat.st_gid) {
         return strcasecmp(lfname, rfname) < 0;
     }
     else {
        return l->mLStat.st_gid < r->mLStat.st_gid;
     }
}
    
static BOOL sort_group_reverse(void* left, void* right)
{ 
     sFile* l = (sFile*) left;
     sFile* r = (sFile*) right;

     char* lfname = string_c_str(l->mName);
     char* rfname = string_c_str(r->mName);

     if(strcmp(lfname, ".") == 0) return TRUE;
     if(strcmp(lfname, "..") == 0) {
          if(strcmp(rfname, ".") == 0) return FALSE;

          return TRUE;
     }
     if(strcmp(rfname, ".") == 0) return FALSE;
     if(strcmp(rfname, "..") == 0) return FALSE;

    char* mark_up = getenv("SORT_MARK_UP");
    if(mark_up && strcmp(mark_up, "1") == 0) {
        if(!l->mMark && r->mMark) {
            return TRUE;
        }
        if(l->mMark && !r->mMark) {
            return FALSE;
        }
    }
    char* dir_up = getenv("SORT_DIR_UP");
    if(dir_up && strcmp(dir_up, "1") == 0) {
         if(S_ISDIR(r->mStat.st_mode)) {
             if(S_ISDIR(l->mStat.st_mode)) {
                 if(l->mLStat.st_gid == r->mLStat.st_gid) {
                     return strcasecmp(lfname, rfname) > 0;
                 }
                 else {
                    return l->mLStat.st_gid > r->mLStat.st_gid;
                 }
             }
//             if(S_ISDIR(l->mLStat.st_mode)) return strcasecmp(rfname, lfname) < 0;
             return FALSE;
         }
         if(S_ISDIR(l->mStat.st_mode)) return TRUE;
     }
     
     if(l->mLStat.st_gid == r->mLStat.st_gid) {
         return strcasecmp(lfname, rfname) > 0;
     }
     else {
        return l->mLStat.st_gid > r->mLStat.st_gid;
     }
}

static BOOL sort_permission(void* left, void* right)
{ 
     sFile* l = (sFile*) left;
     sFile* r = (sFile*) right;

     char* lfname = string_c_str(l->mName);
     char* rfname = string_c_str(r->mName);

     if(strcmp(lfname, ".") == 0) return TRUE;
     if(strcmp(lfname, "..") == 0) {
          if(strcmp(rfname, ".") == 0) return FALSE;

          return TRUE;
     }
     if(strcmp(rfname, ".") == 0) return FALSE;
     if(strcmp(rfname, "..") == 0) return FALSE;

    char* mark_up = getenv("SORT_MARK_UP");
    if(mark_up && strcmp(mark_up, "1") == 0) {
        if(l->mMark && !r->mMark) {
            return TRUE;
        }
        if(!l->mMark && r->mMark) {
            return FALSE;
        }
    }
    char* dir_up = getenv("SORT_DIR_UP");
    if(dir_up && strcmp(dir_up, "1") == 0) {
         if(S_ISDIR(l->mStat.st_mode)) {
             if(S_ISDIR(r->mStat.st_mode)) {
                 if((l->mLStat.st_mode&S_ALLPERM) == (r->mLStat.st_mode&S_ALLPERM)) {
                     return strcasecmp(lfname, rfname) < 0;
                 }
                 else {
                     return (l->mLStat.st_mode&S_ALLPERM) < (r->mLStat.st_mode&S_ALLPERM);
                 }
             }
             return TRUE;
         }
         if(S_ISDIR(r->mStat.st_mode)) return FALSE;
     }
     
     if((l->mLStat.st_mode&S_ALLPERM) == (r->mLStat.st_mode&S_ALLPERM)) {
         return strcasecmp(lfname, rfname) < 0;
     }
     else {
         return (l->mLStat.st_mode&S_ALLPERM) < (r->mLStat.st_mode&S_ALLPERM);
     }
}
    
static BOOL sort_permission_reverse(void* left, void* right)
{ 
     sFile* l = (sFile*) left;
     sFile* r = (sFile*) right;

     char* lfname = string_c_str(l->mName);
     char* rfname = string_c_str(r->mName);

     if(strcmp(lfname, ".") == 0) return TRUE;
     if(strcmp(lfname, "..") == 0) {
          if(strcmp(rfname, ".") == 0) return FALSE;

          return TRUE;
     }
     if(strcmp(rfname, ".") == 0) return FALSE;
     if(strcmp(rfname, "..") == 0) return FALSE;

    char* mark_up = getenv("SORT_MARK_UP");
    if(mark_up && strcmp(mark_up, "1") == 0) {
        if(!l->mMark && r->mMark) {
            return TRUE;
        }
        if(l->mMark && !r->mMark) {
            return FALSE;
        }
    }
    char* dir_up = getenv("SORT_DIR_UP");
    if(dir_up && strcmp(dir_up, "1") == 0) {
         if(S_ISDIR(r->mStat.st_mode)) {
             if(S_ISDIR(l->mStat.st_mode)) {
                 if((l->mLStat.st_mode&S_ALLPERM) == (r->mLStat.st_mode&S_ALLPERM)) {
                     return strcasecmp(lfname, rfname) > 0;
                 }
                 else {
                     return (l->mLStat.st_mode&S_ALLPERM) > (r->mLStat.st_mode&S_ALLPERM);
                 }
             }
             return FALSE;
         }
         if(S_ISDIR(l->mStat.st_mode)) return TRUE;
     }
     
     if((l->mLStat.st_mode&S_ALLPERM) == (r->mLStat.st_mode&S_ALLPERM)) {
         return strcasecmp(lfname, rfname) > 0;
     }
     else {
         return (l->mLStat.st_mode&S_ALLPERM) > (r->mLStat.st_mode&S_ALLPERM);
     }
}

static BOOL sort_random(void* left, void* right)
{
    sFile* l = (sFile*) left;
    sFile* r = (sFile*) right;

    char* lfname = string_c_str(l->mName);
    char* rfname = string_c_str(r->mName);

    if(strcmp(lfname, ".") == 0) return TRUE;
    if(strcmp(lfname, "..") == 0) {
        if(strcmp(rfname, ".") == 0) return FALSE;

        return TRUE;          
    }
    if(strcmp(rfname, ".") == 0) return FALSE;
    if(strcmp(rfname, "..") == 0) return FALSE;

    char* mark_up = getenv("SORT_MARK_UP");
    if(mark_up && strcmp(mark_up, "1") == 0) {
        if(l->mMark && !r->mMark) {
            return TRUE;
        }
        if(!l->mMark && r->mMark) {
            return FALSE;
        }
    }

    char* dir_up = getenv("SORT_DIR_UP");
    if(dir_up && strcmp(dir_up, "1") == 0) {
        if(!S_ISDIR(l->mStat.st_mode) && !S_ISDIR(r->mStat.st_mode)
            || S_ISDIR(l->mStat.st_mode) && S_ISDIR(r->mStat.st_mode) )
        {
            return l->mSortRandom < r->mSortRandom;
        }
    
        if(S_ISDIR(l->mStat.st_mode))
            return TRUE;
        else
            return FALSE;
    }
    else {
        return l->mSortRandom < r->mSortRandom;
    }
}

void sDir_sort(sDir* self)
{
    char* sort_kind = getenv("SORT_KIND");

    if(sort_kind == NULL || strcmp(sort_kind, "name") == 0) {
        vector_sort(self->mFiles, sort_name);
    }
    else if(strcmp(sort_kind, "random") == 0) {
        int i;
        for(i=0; i<vector_count(self->mFiles); i++) {
            sFile* f = (sFile*)vector_item(self->mFiles, i);

            f->mSortRandom = random()%vector_count(self->mFiles);
        }

        vector_sort(self->mFiles, sort_random);
    }
    else if(strcmp(sort_kind, "name_rev") == 0) {
        vector_sort(self->mFiles, sort_name_reverse);
    }
    else if(strcmp(sort_kind, "ext") == 0) {
        vector_sort(self->mFiles, sort_ext);
    }
    else if(strcmp(sort_kind, "ext_rev") == 0) {
        vector_sort(self->mFiles, sort_ext_reverse);
    }
    else if(strcmp(sort_kind, "size") == 0) {
        vector_sort(self->mFiles, sort_size);
    }
    else if(strcmp(sort_kind, "size_rev") == 0) {
        vector_sort(self->mFiles, sort_size_reverse);
    }
    else if(strcmp(sort_kind, "time") == 0) {
        vector_sort(self->mFiles, sort_time);
    }
    else if(strcmp(sort_kind, "time_rev") == 0) {
        vector_sort(self->mFiles, sort_time_reverse);
    }
    else if(strcmp(sort_kind, "user") == 0) {
        vector_sort(self->mFiles, sort_user);
    }
    else if(strcmp(sort_kind, "user_rev") == 0) {
        vector_sort(self->mFiles, sort_user_reverse);
    }
    else if(strcmp(sort_kind, "group") == 0) {
        vector_sort(self->mFiles, sort_group);
    }
    else if(strcmp(sort_kind, "group_rev") == 0) {
        vector_sort(self->mFiles, sort_group_reverse);
    }
    else if(strcmp(sort_kind, "perm") == 0) {
        vector_sort(self->mFiles, sort_permission);
    }
    else if(strcmp(sort_kind, "perm_rev") == 0) {
        vector_sort(self->mFiles, sort_permission_reverse);
    }
}


