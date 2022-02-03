/*  David Anderson.  2022. This small program is hereby
    placed into the public domain to be copied or
    used by anyone for any purpose.

    The C source is src/bin/dwarfexample/showsectiongroups.c
*/

#include "config.h"
#include <stdio.h>
#include <string.h> /* for memset */
#if defined(_WIN32) && defined(HAVE_STDAFX_H)
#include "stdafx.h"
#endif /* HAVE_STDAFX_H */
#include <stdlib.h> /* for exit(), C89 malloc */
#include "dwarf.h"
#include "libdwarf.h"

#define TRUE 1
#define FALSE 0
char trueoutpath[2000];

static int
one_file_show_groups(char  *path_in, int chosengroup)
{
    int              res = 0;
    Dwarf_Debug      dbg = 0;
    Dwarf_Error      error = 0;
    char           * path = 0;
    Dwarf_Unsigned   section_count = 0;
    Dwarf_Unsigned   group_count = 0;
    Dwarf_Unsigned   selected_group = 0;
    Dwarf_Unsigned   map_entry_count = 0;
    Dwarf_Unsigned * group_numbers_array = 0;
    Dwarf_Unsigned * sec_numbers_array = 0;
    const char    ** sec_names_array = 0;

    path =  path_in;
    res = dwarf_init_path(path,
        0,0,
        chosengroup,
        0,0, &dbg, &error);
    if (res == DW_DLV_ERROR) {
        printf("Error from libdwarf opening \"%s\":  %s\n",
             path, dwarf_errmsg(error));
        dwarf_dealloc_error(dbg,error);
        error = 0;
        return res;
    }
    if (res == DW_DLV_NO_ENTRY) {
        printf("There is no such file as \"%s\"\n",
            path);
        return DW_DLV_NO_ENTRY;
    }

    res = dwarf_sec_group_sizes(dbg, &section_count,
        &group_count, &selected_group, &map_entry_count,
        &error);
    if (res == DW_DLV_ERROR) {
        printf("Error from libdwarf getting group "
            "sizes \"%s\":  %s\n",
            path, dwarf_errmsg(error));
        dwarf_dealloc_error(dbg,error);
        error = 0;
        dwarf_finish(dbg);
        return res;
    }
    if (res == DW_DLV_NO_ENTRY) {
        printf("Impossible. libdwarf claims no groups from %s\n",
             path);
        dwarf_finish(dbg);
        return res;
    }
    printf("Group Map data sizes\n");
    printf("  section count   : %4lu\n",
        (unsigned long)section_count);
    printf("  group count     : %4lu\n",
        (unsigned long)group_count);
    printf("  selected group  : %4lu\n",
        (unsigned long)selected_group);
    printf("  map entry count : %4lu\n",
        (unsigned long)map_entry_count);

    group_numbers_array = (Dwarf_Unsigned *)calloc(map_entry_count,
        sizeof(Dwarf_Unsigned));
    if (!group_numbers_array) {
        printf("Error calloc fail, group count %lu\n",
            (unsigned long)group_count);
        dwarf_finish(dbg);
        return DW_DLV_ERROR;
    }
    sec_numbers_array = (Dwarf_Unsigned *)calloc(map_entry_count,
        sizeof(Dwarf_Unsigned));
    if (!sec_numbers_array) {
        free(group_numbers_array);
        printf("Error calloc fail sec numbers, section count %lu\n",
            (unsigned long)section_count);
        dwarf_finish(dbg);
        return DW_DLV_ERROR;
    }
    sec_names_array = (const char **)calloc(map_entry_count,
        sizeof(const char *));
    if (!sec_names_array) {
        free(sec_numbers_array);
        free(group_numbers_array);
        printf("Error calloc fail on names, section count %lu\n",
            (unsigned long)section_count);
        dwarf_finish(dbg);
        return DW_DLV_ERROR;
    }
    res = dwarf_sec_group_map(dbg,map_entry_count,
        group_numbers_array,
        sec_numbers_array, sec_names_array,&error);  
    if (res == DW_DLV_ERROR) {
        free(sec_names_array);
        free(sec_numbers_array);
        free(group_numbers_array);
        printf("Error from libdwarf getting group detals "
            "sizes \"%s\":  %s\n",
            path, dwarf_errmsg(error));
        dwarf_dealloc_error(dbg,error);
        error = 0;
        dwarf_finish(dbg);
        return res;
    }
    if (res == DW_DLV_NO_ENTRY) {
        free(sec_names_array);
        free(sec_numbers_array);
        free(group_numbers_array);
        printf("Impossible. libdwarf claims details from %s\n",
             path);
        dwarf_finish(dbg);
        return res;
    }
    printf("  [index] group   section \n");
    for (Dwarf_Unsigned i = 0; i < map_entry_count;++i) {
        printf("  [%5lu]  %4lu  %4lu %s\n",
            (unsigned long)i,
            (unsigned long)group_numbers_array[i],
            (unsigned long)sec_numbers_array[i],
            sec_names_array[i]);
    } 
    free(sec_names_array);
    free(sec_numbers_array);
    free(group_numbers_array);
    dwarf_finish(dbg);
    return DW_DLV_OK;
}

/*  Does not return */
static void 
usage(void)
{
    printf("Usage: showsectiongroups [-group <n>] "
        "<objectfile> ...\n");
    printf("Usage: group defaults to zero (DW_GROUPNUMBER ANY)\n");
    exit(1);
}
int
main(int argc, char **argv)
{
    int res = 0;
    int i = 1;
    int chosengroup = DW_GROUPNUMBER_ANY;

    if (argc < 2) {
        usage();
        return 0;
    }
    for ( ; i < argc; ++i) {
        char *arg = argv[i];
        if (!strcmp(arg,"-group")) {
            i++;
            if(i >= argc) {
                usage();
            }
            arg = argv[i];
            chosengroup = atoi(arg);
            /*  We are ignoring errors to simplify
                this source. Use strtol, carefully,
                in real code. */
        }
        res = one_file_show_groups(arg,chosengroup);
        printf("=======done with %s, status %s\n",arg,
           (res == DW_DLV_OK)?"DW_DLV_OK":
           (res == DW_DLV_ERROR)?"DW_DLV_ERROR":
           "DW_DLV_NO_ENTRY");
        printf("\n");
    }
    return 0;
}
