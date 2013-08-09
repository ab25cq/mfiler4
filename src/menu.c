#include "common.h"
#include <unistd.h>
#include <string.h>

#include "config.h"

#if defined(HAVE_CURSES_H)
#include <curses.h>
#elif defined(HAVE_NCURSES_H)
#include <ncurses.h>
#elif defined(HAVE_NCURSES_NCURSES_H)
#include <ncurses/ncurses.h>
#endif

typedef struct {
    char* mName;
    int mKey;
    sObject* mBlock;
    BOOL mExternal;
} sMenuItem;

typedef struct {
    char mTitle[256];

    int mScrollTop;
    int mCursor;

    sObject* mMenuItems; 
} sMenu;

sObject* gActiveMenu = NULL;
static sObject* gMenus;

static sMenuItem* sMenuItem_new(char* name, int key, sObject* block, BOOL external) 
{
    sMenuItem* self = MALLOC(sizeof(sMenuItem));

    self->mName = STRDUP(name);
    self->mKey = key;
    self->mBlock = block_clone_on_gc(block, T_BLOCK, FALSE);
    self->mExternal = external;

    return self;
}

static void sMenuItem_delete(sMenuItem* self)
{
    FREE(self->mName);

    FREE(self);
}

int sMenu_markfun(sObject* self)
{
    int count = 0;

    sMenu* obj = SEXTOBJ(self).mObject;

    int i;
    for(i=0; i<vector_count(obj->mMenuItems); i++)
    {
        sMenuItem* item = vector_item(obj->mMenuItems, i);
        SET_MARK(item->mBlock);
        count++;
        count+= object_gc_children_mark(item->mBlock);
    }

    return count;
}

void sMenu_freefun(void* obj)
{
    sMenu* self = obj;

    int i;
    for(i=0; i<vector_count(self->mMenuItems); i++) {
        sMenuItem_delete(vector_item(self->mMenuItems, i));
    }

    vector_delete_on_malloc(self->mMenuItems);

    FREE(self);
}

BOOL sMenu_mainfun(void* obj, sObject* nextin, sObject* nextout, sRunInfo* runinfo)
{
    sMenu* menu = obj;
    sCommand* command = runinfo->mCommand;

    int i;
    for(i=0; i<vector_count(menu->mMenuItems); i++) {
        sMenuItem* item = vector_item(menu->mMenuItems, i);
        char* name = item->mName;

        char buf[512];
        if(item->mExternal) {
            snprintf(buf, 512, "-+- number: %d name: %s key: %d EXTERNAL -+-\n", i, name, item->mKey);
        }
        else {
            snprintf(buf, 512, "-+- number: %d name: %s key: %d -+-\n", i, name, item->mKey);
        }

        if(!fd_write(nextout, buf, strlen(buf))) {
            err_msg("interrupt", runinfo->mSName, runinfo->mSLine);
            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
            return FALSE;
        }

        sObject* block = item->mBlock;  
        char* source = SBLOCK(block).mSource;

        if(!fd_write(nextout, source, strlen(source))) {
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
    
    return TRUE;
}

static sObject* sMenu_new(char* title)
{
    sMenu* obj = MALLOC(sizeof(sMenu));

    xstrncpy(obj->mTitle, title, 256);
    obj->mScrollTop = 0;
    obj->mCursor = 0;

    obj->mMenuItems = VECTOR_NEW_MALLOC(5);

    return EXTOBJ_NEW_GC(obj, sMenu_markfun, sMenu_freefun, sMenu_mainfun, TRUE);
}

///////////////////////////////////////////////////
// initialization
///////////////////////////////////////////////////
void menu_init()
{
    gMenus = UOBJECT_NEW_GC(10, gMFiler4, "menus", TRUE);
    uobject_init(gMenus);
    uobject_put(gMFiler4, "menus", gMenus);
}

void menu_final()
{
}

void menu_create(char* menu_name)
{
    sObject* menu = sMenu_new(menu_name);
    uobject_put(gMenus, menu_name, menu);
}

BOOL set_active_menu(char* menu_name)
{
    gActiveMenu = uobject_item(gMenus, menu_name);
    if(gActiveMenu) {
        sMenu* menu = SEXTOBJ(gActiveMenu).mObject;
        menu->mScrollTop = 0;
        menu->mCursor = 0;

        return TRUE;
    }
    else {
        return FALSE;
    }
}

BOOL menu_append(char* menu_name, char* title, int key, sObject* block, BOOL external)
{
    sObject* obj = uobject_item(gMenus, menu_name);

    if(obj) {
        sMenu* menu = SEXTOBJ(obj).mObject;
        char buf[256];
        snprintf(buf, 256, " %-40s", title);
        vector_add(menu->mMenuItems, sMenuItem_new(buf, key, block, external));

        return TRUE;
    }
    else {
        return FALSE;
    }
}

void menu_start(char* menu_name)
{
    gActiveMenu = uobject_item(gMenus, menu_name);
    if(gActiveMenu) {
        sMenu* menu = SEXTOBJ(gActiveMenu).mObject;
        menu->mScrollTop = 0;
        menu->mCursor = 0;
    }
}

///////////////////////////////////////////////////
// key input
///////////////////////////////////////////////////
static void menu_run(sMenuItem* item)
{
    clear();
    view();
    refresh();
    
    if(item->mExternal) {
        const int maxy = mgetmaxy();
        mmove_immediately(maxy-2, 0);
        endwin();

        mreset_tty();

        int rcode;
        if(xyzsh_run(&rcode, item->mBlock, item->mName, NULL, gStdin, gStdout, 0, NULL, gMFiler4))
        {
            if(rcode == 0) {
                xinitscr();
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
        if(xyzsh_run(&rcode, item->mBlock, item->mName, NULL, gStdin, gStdout, 0, NULL, gMFiler4)) {
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
    }
}

void menu_input(int meta, int key)
{
    if(gActiveMenu) {
        sMenu* self = SEXTOBJ(gActiveMenu).mObject;

        if(key == 1 || key == KEY_HOME) {        // CTRL-A
            self->mCursor = 0;
        }
        else if(key == 5 || key == KEY_END) {    // CTRL-E
            self->mCursor = vector_count(self->mMenuItems)-1;
        }
        else if(key == 14 || key == 6|| key == KEY_DOWN) {    // CTRL-N CTRL-F
            self->mCursor++;
        }
        else if(key == 16 || key == 2 || key == KEY_UP) {    // CTRL-P CTRL-B
            self->mCursor--;
        }
        else if(key == 4 || key == KEY_NPAGE) { // CTRL-D PAGE DOWN
            self->mCursor+=7;
        }
        else if(key == 21 || key == KEY_PPAGE) { // CTRL-U   PAGE UP
            self->mCursor-=7;
        }
        else if(key == 3 || key == 7) {    // CTRL-C CTRL-G
            gActiveMenu = NULL;
            clear();
        }
        else if(key == 10 || key == 13) {    // CTRL-M CTRL-J
            gActiveMenu = NULL;
            
            sMenuItem* item = vector_item(self->mMenuItems, self->mCursor);
            menu_run(item);
            clear();
        }
        else if(key == 12) {// CTRL-L
            xclear_immediately();
        }
        else {
            gActiveMenu = NULL;
            
            int i;
            for(i=0; i<vector_count(self->mMenuItems); i++) {
                sMenuItem* item = (sMenuItem*)vector_item(self->mMenuItems, i);
                
                if(item->mKey == key) {
                    menu_run(item);
                }
            }
            
            clear();
        }

        if(gActiveMenu) {
            while(self->mCursor < 0) {
                self->mCursor += vector_count(self->mMenuItems);
            }
            while(vector_count(self->mMenuItems) != 0 && self->mCursor >= vector_count(self->mMenuItems)) 
            {
               self->mCursor -= vector_count(self->mMenuItems);
            }

            const int maxy = mgetmaxy() -2;
            
            if(self->mCursor >= (self->mScrollTop+maxy-1)) {
                self->mScrollTop += self->mCursor - (self->mScrollTop+maxy-1) + 1;
            }
            if(self->mCursor < self->mScrollTop) {
                self->mScrollTop = self->mCursor;
            }
        }
    }

    set_signal_mfiler();
}

///////////////////////////////////////////////////
// drawing
///////////////////////////////////////////////////
void menu_view()
{
    if(gActiveMenu) {
        sMenu* self = SEXTOBJ(gActiveMenu).mObject;

        const int maxx = mgetmaxx();
        const int maxy = mgetmaxy();

        int item_maxx = 0;
        int i;
        for(i=0; i<vector_count(self->mMenuItems); i++) {
            sMenuItem* item = (sMenuItem*)vector_item(self->mMenuItems, i);
            
            int len = str_termlen(gKanjiCode, item->mName);

            if(item_maxx < len) {
                item_maxx = len;
            }
        }
        item_maxx+=2;
        
        if(item_maxx < 43) {
            item_maxx = 43;
        }
        else if(item_maxx > maxx-2) {
            item_maxx = maxx-2;
        }
        
        const int size = vector_count(self->mMenuItems) + 2;
        mbox(0, 2, item_maxx, size < maxy ? size : maxy);

        attron(A_BOLD);
        mvprintw(0, 4, self->mTitle);
        attroff(A_BOLD);

        float f;
        if(vector_count(self->mMenuItems) == 0) {
            f = 0.0;
        }
        else {
            f = ((float)(self->mCursor+1)
                    /(float)vector_count(self->mMenuItems))*100;
        }

        mvprintw((size < maxy ? size: maxy) - 1, 2+item_maxx-8, "(%3d%%)", (int)f);
        for(i=self->mScrollTop;
            (i<vector_count(self->mMenuItems) && i-self->mScrollTop<maxy-2); i++) {
            sMenuItem* item = (sMenuItem*)vector_item(self->mMenuItems, i);
            
            char buf[1024];
            if(strlen(item->mName) > maxx-4) {
                str_cut2(gKanjiCode, item->mName, maxx-4, buf, 1024);
            }
            else {
                xstrncpy(buf, item->mName, 1024);
            }
            
            if(i == self->mCursor) {
                attron(A_REVERSE);
                mvprintw(i-self->mScrollTop+1, 3, buf);
                attroff(A_REVERSE);
            }
            else  {
                mvprintw(i-self->mScrollTop+1, 3, buf);
            }
        }

        move(maxy-1, 0);
    }
}
