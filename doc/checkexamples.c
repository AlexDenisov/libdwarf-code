/*! @file checkexamples.c
    @page checkexamples.c
    checkexamples.c contains what user code should be,
    hence the code typed here is PUBLIC DOMAIN.

    It need not be compiled routinely nor
    should it ever be executed.

    To verify syntatic correctness compile with

cc -c -Wall -O0 -Wpointer-arith  -Wdeclaration-after-statement \
-Wextra -Wcomment -Wformat -Wpedantic -Wuninitialized \
-Wno-long-long -Wshadow -Wbad-function-cast \
-Wmissing-parameter-type -Wnested-externs \
-I../src/lib/libdwarf checkexamples.c
*/

#include <stdio.h> /* for printf */
#include <stdlib.h> /* for free() */
#include <string.h> /* for memcmp() */
#include "dwarf.h"
#include "libdwarf.h"

#define TRUE  1
#define FALSE 0

/*! @defgroup exampleinit Example of dwarf_init_path
    @brief exampleinit

    An example calling  dwarf_init_path() and dwarf_finish()
    @param path
    Path to an object we wish to open.
    @param groupnumber
    @code
*/
void exampleinit(const char *path, unsigned groupnumber)
{
    static char true_pathbuf[FILENAME_MAX];
    unsigned tpathlen = FILENAME_MAX;
    Dwarf_Handler errhand = 0;
    Dwarf_Ptr errarg = 0;
    Dwarf_Error error = 0;
    Dwarf_Debug dbg = 0;
    int res = 0;

    res = dwarf_init_path(path,true_pathbuf,
        tpathlen,groupnumber,errhand,
        errarg,&dbg, &error);
    if (res == DW_DLV_ERROR) {
        /* Valid call even though dbg is null! */
        dwarf_dealloc_error(dbg,error);
        return;
    }
    if (res == DW_DLV_NO_ENTRY) {
        /*  Nothing we can do */
        return;
    }
    printf("The file we actually opened is %s\n",
        true_pathbuf);
    /* Call libdwarf functions here */
    dwarf_finish(dbg);
}
/*! @endcode */

/*! @defgroup exampleinit_dl Example of dwarf_init_path_dl
    @brief Example calling the debuglink init.

    In case GNU debuglink data is followed the true_pathbuf
    content will not match path.
    The path actually used is copied to true_path_out.
    In the case of MacOS dSYM the true_path_out
    may not match path.
    If debuglink missing from the Elf executable
    or shared-object (ie, it is a normal
    object!) or unusable by libdwarf or
    true_path_buffer len is zero or true_path_out_buffer
    is zero libdwarf accepts the path given as the object
    to report on, no debuglink or dSYM processing will be used.

    @sa https://sourceware.org/gdb/onlinedocs/\
    gdb/Separate-Debug-Files.html

    An example calling  dwarf_init_path_dl() and dwarf_finish()
    @code
*/
int exampleinit_dl(const char *path, unsigned groupnumber,
    Dwarf_Error *error)
{
    static char true_pathbuf[FILENAME_MAX];
    static const char *glpath[3] = {
        "/usr/local/debug",
        "/usr/local/private/debug",
        "/usr/local/libdwarfdd/debug"
    };
    unsigned tpathlen = FILENAME_MAX;
    Dwarf_Handler errhand = 0;
    Dwarf_Ptr errarg = 0;
    Dwarf_Debug dbg = 0;
    int res = 0;
    unsigned char path_source = 0;

    res = dwarf_init_path_dl(path,true_pathbuf,
        tpathlen,groupnumber,errhand,
        errarg,&dbg,
        (char **)glpath,
        3,
        &path_source,
        error);
    if (res == DW_DLV_ERROR) {
        return res;
    }
    if (res == DW_DLV_NO_ENTRY) {
        return res;
    }
    printf("The file we actually opened is %s\n",
        true_pathbuf);
    /* Call libdwarf functions here */
    dwarf_finish(dbg);
    return DW_DLV_OK;
}
/*! @endcode */

/*! @defgroup example1 Example of dwarf_attrlist
    @brief Showing dwarf_attrlist()

    @code
*/
int example1(Dwarf_Die somedie,Dwarf_Error *error)
{
    Dwarf_Debug dbg = 0;
    Dwarf_Signed atcount;
    Dwarf_Attribute *atlist;
    Dwarf_Signed i = 0;
    int errv;

    errv = dwarf_attrlist(somedie, &atlist,&atcount, error);
    if (errv != DW_DLV_OK) {
        return errv;
    }
    for (i = 0; i < atcount; ++i) {
        Dwarf_Half attrnum = 0;
        const char *attrname = 0;

        /*  use atlist[i], likely calling
            libdwarf functions and likely
            returning DW_DLV_ERROR if
            what you call gets DW_DLV_ERROR */
        errv = dwarf_whatattr(atlist[i],&attrnum,error);
        if (errv != DW_DLV_OK) {
            /* Something really bad happened. */
            return errv;
        }
        dwarf_get_AT_name(attrnum,&attrname);
        printf("Attribute[%ld], value %u name %s\n",
            (long int)i,attrnum,attrname);
        dwarf_dealloc_attribute(atlist[i]);
        atlist[i] = 0;
    }
    dwarf_dealloc(dbg, atlist, DW_DLA_LIST);
    return DW_DLV_OK;
}
/*! @endcode */

/*! @defgroup example2 Attaching a tied dbg
    @brief Attaching a tied dbg

    By convention, open the base Dwarf_Debug using
    a dwarf_init call.  Then open
    the executable as the tied object.
    Then call dwarf_set_tied_dbg()
    so the library can look for relevant data
    in the tied-dbg (the executable).

    With split dwarf your libdwarf calls after
    than the initial open
    are done against the base Dwarf_Dbg and
    libdwarf automatically looks in the open tied dbg
    when and as appropriate.
    the tied-dbg can be detached too, see
    example3 link, though you must call
    dwarf_finish() on the detached dw_tied_dbg,
    the library will not do that for you..

    @param tieddbg
    @param error
    @return
    Returns whatever DW_DLV appropriate
    to the caller to deal with.
    @code
*/
int example2(Dwarf_Debug dbg, Dwarf_Debug tieddbg,
    Dwarf_Error *error)
{
    int res = 0;

    /*  The caller should have opened dbg
        on the debug shared object/dwp,
        an object with DWARF, but no executable
        code.
        And it should have opened tieddbg on the
        runnable shared object or executable. */
    res = dwarf_set_tied_dbg(dbg,tieddbg,error);
    /*  Let your caller (who initialized the dbg
        values) deal with doing dwarf_finish()
    */
    return res;

}
/*! @endcode */

/*! @defgroup example3 Detaching a tied dbg
    @brief Detaching a tied dbg

    With split dwarf your libdwarf calls after
    than the initial open
    are done against the base Dwarf_Dbg and
    libdwarf automatically looks in the open tied dbg
    when and as appropriate.
    the tied-dbg can be detached too, see
    example3 link, though you must call
    dwarf_finish() on the detached dw_tied_dbg,
    the library will not do that for you..

    @code
*/
int example3(Dwarf_Debug dbg,Dwarf_Error *error)
{
    int res = 0;
    res = dwarf_set_tied_dbg(dbg,NULL,error);
    if (res != DW_DLV_OK) {
        /* Something went wrong*/
        return res;
    }
    return res;
}
/*! @endcode */

void examplesecgroup(Dwarf_Debug dbg)
{
    int res = 0;
    Dwarf_Unsigned  section_count = 0;
    Dwarf_Unsigned  group_count;
    Dwarf_Unsigned  selected_group = 0;
    Dwarf_Unsigned  group_map_entry_count = 0;
    Dwarf_Unsigned *sec_nums = 0;
    Dwarf_Unsigned *group_nums = 0;
    const char **   sec_names = 0;
    Dwarf_Error     error = 0;
    Dwarf_Unsigned  i = 0;

    res = dwarf_sec_group_sizes(dbg,&section_count,
        &group_count,&selected_group, &group_map_entry_count,
        &error);
    if (res != DW_DLV_OK) {
        /* Something is badly wrong*/
        return;
    }
    /*  In an object without split-dwarf sections
        or COMDAT sections we now have
        selected_group == 1. */
    sec_nums = calloc(group_map_entry_count,sizeof(Dwarf_Unsigned));
    if (!sec_nums) {
        /* FAIL. out of memory */
        return;
    }
    group_nums = calloc(group_map_entry_count,sizeof(Dwarf_Unsigned));
    if (!group_nums) {
        free(group_nums);
        /* FAIL. out of memory */
        return;
    }
    sec_names = calloc(group_map_entry_count,sizeof(char*));
    if (!sec_names) {
        free(group_nums);
        free(sec_nums);
        /* FAIL. out of memory */
        return;
    }

    res = dwarf_sec_group_map(dbg,group_map_entry_count,
        group_nums,sec_nums,sec_names,&error);
    if (res != DW_DLV_OK) {
        /* FAIL. Something badly wrong. */
    }
    for ( i = 0; i < group_map_entry_count; ++i) {
        /*  Now do something with
            group_nums[i],sec_nums[i],sec_names[i] */
    }
    free(group_nums);
    free(sec_nums);
    /*  The strings are in Elf data.
        Do not free() the strings themselves.*/
    free(sec_names);
}
void example4(Dwarf_Debug dbg,Dwarf_Die in_die,Dwarf_Bool is_info)
{
    Dwarf_Die return_sib = 0;
    Dwarf_Error error = 0;
    int res = 0;

    /* in_die might be NULL or a valid Dwarf_Die */
    res = dwarf_siblingof_b(dbg,in_die,is_info,&return_sib, &error);
    if (res == DW_DLV_OK) {
        /* Use return_sib here. */
        dwarf_dealloc_die(return_sib);
        /*  return_sib is no longer usable for anything, we
            ensure we do not use it accidentally with: */
        return_sib = 0;
    }
}

void example5(Dwarf_Die in_die)
{
    Dwarf_Die return_kid = 0;
    Dwarf_Error error = 0;
    int res = 0;

    res = dwarf_child(in_die,&return_kid, &error);
    if (res == DW_DLV_OK) {
        /* Use return_kid here. */
        dwarf_dealloc_die(return_kid);
        /*  The original form of dealloc still works
            dwarf_dealloc(dbg, return_kid, DW_DLA_DIE);
            */
        /*  return_die is no longer usable for anything, we
            ensure we do not use it accidentally with: */
        return_kid = 0;
    }
}

void example6(Dwarf_Debug dbg,Dwarf_Off die_offset,Dwarf_Bool is_info)
{
    Dwarf_Error error = 0;
    Dwarf_Die return_die = 0;
    int res = 0;

    res = dwarf_offdie_b(dbg,die_offset,is_info,&return_die, &error);
    if (res == DW_DLV_OK) {
        /* Use return_die here. */
        dwarf_dealloc_die(return_die);
        /*  The original form still works:
            dwarf_dealloc(dbg, return_die, DW_DLA_DIE);
        */
        /*  return_die is no longer usable for anything, we
            ensure we do not use it accidentally with: */
        return_die = 0;
    } else {
        /*  res could be NO ENTRY or ERROR, so no
            dealloc necessary.  */
    }
}

void example7(Dwarf_Debug dbg, Dwarf_Die in_die,Dwarf_Bool is_info)
{
    int res = 0;
    Dwarf_Off cudieoff = 0;
    Dwarf_Die cudie = 0;
    Dwarf_Error error = 0;

    res = dwarf_CU_dieoffset_given_die(in_die,&cudieoff,&error);
    if (res != DW_DLV_OK) {
        /*  FAIL */
        return;
    }
    res = dwarf_offdie_b(dbg,cudieoff,is_info,&cudie,&error);
    if (res != DW_DLV_OK) {
        /* FAIL */
        return;
    }
    /* do something with cu_die */
    dwarf_dealloc_die(cudie);
    /*  The original form still works.
        dwarf_dealloc(dbg,cudie, DW_DLA_DIE);
    */
}

/* See also example1, which is more complete */
int example8(Dwarf_Debug dbg, Dwarf_Die somedie, Dwarf_Error *error)
{
    Dwarf_Signed atcount = 0;
    Dwarf_Attribute *atlist = 0;
    int errv = 0;
    Dwarf_Signed i = 0;

    errv = dwarf_attrlist(somedie, &atlist,&atcount, error);
    if (errv != DW_DLV_OK) {
        return errv;
    }
    for (i = 0; i < atcount; ++i) {
        /* use atlist[i] */
        dwarf_dealloc_attribute(atlist[i]);
        atlist[i] = 0;
    }
    dwarf_dealloc(dbg, atlist, DW_DLA_LIST);
    return DW_DLV_OK;
}

/*! @defgroup exampleoffsetlist Example using dwarf_offset_list
    @brief Using dwarf_offset_list

    An example calling  dwarf_offset_list
    @param dbg
    @param dieoffset
    @param error
    @return
    Returns DW_DLV_OK etc
    @code
*/
int exampleoffset_list(Dwarf_Debug dbg, Dwarf_Off dieoffset,
    Dwarf_Bool is_info,Dwarf_Error * error)
{
    Dwarf_Unsigned offcnt = 0;
    Dwarf_Off *offbuf = 0;
    int errv = 0;
    Dwarf_Unsigned i = 0;

    errv = dwarf_offset_list(dbg,dieoffset, is_info,
        &offbuf,&offcnt, error);
    if (errv != DW_DLV_OK) {
        return errv;
    }
    for (i = 0; i < offcnt; ++i) {
        /* use offbuf[i] */
        /*  No need to free the offbuf entry, it
            is just an offset value. */
    }
    dwarf_dealloc(dbg, offbuf, DW_DLA_LIST);
    return DW_DLV_OK;
}
/*! @endcode */

/*! @defgroup explainformblock Documenting Form_Block
    @brief Documents Form_Block content

    Used with certain location information functions,
    a frame expression function, expanded
    frame instructions, and
    DW_FORM_block<> functions and more.

    @see dwarf_formblock
    @see Dwarf_Block_s

    @code
struct Dwarf_Block_s fields:

    Length of block bl_data points at
Dwarf_Unsigned  bl_len;

    Uninterpreted data bytes
Dwarf_Ptr       bl_data;

    See libdwarf.h DW_LKIND, defaults to
    DW_LKIND_expression and except in certain
    location expressions the field is ignored.
Dwarf_Small     bl_from_loclist;

    Section offset of what bl_data points to
Dwarf_Unsigned  bl_section_offset;
    @endcode
*/

/*! @defgroup examplediscrlist Example using dwarf_discr_list
    @brief Using dwarf_discr_list and dwarf_formblock

    An example calling dwarf_get_form_class,
    dwarf_discr_list, and dwarf_formblock.
    and the dwarf_deallocs applicable.

    @see dwarf_discr_list
    @see dwarf_get_form_class
    @see dwarf_formblock

    @param dw_dbg
    The applicable Dwarf_Debug
    @param dw_die
    The applicable Dwarf_Die
    @param dw_attr
    The applicable Dwarf_Attribute
    @param dw_attrnum,
    The attribute number passed in to shorten
    this example a bit.
    @param dw_isunsigned,
    The attribute number passed in to shorten
    this example a bit.
    @param dw_theform,
    The form number passed in to shorten
    this example a bit.
    @param dw_error
    The usual error pointer.
    @return
    Returns DW_DLV_OK etc
    @code
*/
int example_discr_list(Dwarf_Debug dbg,
    Dwarf_Die die,
    Dwarf_Attribute attr,
    Dwarf_Half attrnum,
    Dwarf_Bool isunsigned,
    Dwarf_Half theform,
    Dwarf_Error *error)
{
    /*  The example here assumes that
        attribute attr is a DW_AT_discr_list.
        isunsigned should be set from the signedness
        of the parent of 'die' per DWARF rules for
        DW_AT_discr_list. */
    enum Dwarf_Form_Class fc = DW_FORM_CLASS_UNKNOWN;
    Dwarf_Half version = 0;
    Dwarf_Half offset_size = 0;
    int wres = 0;

    wres = dwarf_get_version_of_die(die,&version,&offset_size);
    if (wres != DW_DLV_OK) {
        /* FAIL */
        return wres;
    }
    fc = dwarf_get_form_class(version,attrnum,offset_size,theform);
    if (fc == DW_FORM_CLASS_BLOCK) {
        int fres = 0;
        Dwarf_Block *tempb = 0;
        fres = dwarf_formblock(attr, &tempb, error);
        if (fres == DW_DLV_OK) {
            Dwarf_Dsc_Head h = 0;
            Dwarf_Unsigned u = 0;
            Dwarf_Unsigned arraycount = 0;
            int sres = 0;

            sres = dwarf_discr_list(dbg,
                (Dwarf_Small *)tempb->bl_data,
                tempb->bl_len,
                &h,&arraycount,error);
            if (sres == DW_DLV_NO_ENTRY) {
                /* Nothing here. */
                dwarf_dealloc(dbg, tempb, DW_DLA_BLOCK);
                return sres;
            }
            if (sres == DW_DLV_ERROR) {
                /* FAIL . */
                dwarf_dealloc(dbg, tempb, DW_DLA_BLOCK);
                return sres ;
            }
            for (u = 0; u < arraycount; u++) {
                int u2res = 0;
                Dwarf_Half dtype = 0;
                Dwarf_Signed dlow = 0;
                Dwarf_Signed dhigh = 0;
                Dwarf_Unsigned ulow = 0;
                Dwarf_Unsigned uhigh = 0;

                if (isunsigned) {
                    u2res = dwarf_discr_entry_u(h,u,
                        &dtype,&ulow,&uhigh,error);
                } else {
                    u2res = dwarf_discr_entry_s(h,u,
                        &dtype,&dlow,&dhigh,error);
                }
                if (u2res == DW_DLV_ERROR) {
                    /* Something wrong */
                    dwarf_dealloc(dbg,h,DW_DLA_DSC_HEAD);
                    dwarf_dealloc(dbg, tempb, DW_DLA_BLOCK);
                    return u2res ;
                }
                if (u2res == DW_DLV_NO_ENTRY) {
                    /* Impossible. u < arraycount. */
                    dwarf_dealloc(dbg,h,DW_DLA_DSC_HEAD);
                    dwarf_dealloc(dbg, tempb, DW_DLA_BLOCK);
                    return u2res;
                }
                /*  Do something with dtype, and whichever
                    of ulow, uhigh,dlow,dhigh got set.
                    Probably save the values somewhere.
                    Simple casting of dlow to ulow (or vice versa)
                    will not get the right value due to the nature
                    of LEB values. Similarly for uhigh, dhigh.
                    One must use the right call.  */
            }
            dwarf_dealloc(dbg,h,DW_DLA_DSC_HEAD);
            dwarf_dealloc(dbg, tempb, DW_DLA_BLOCK);
        }
    }
    return DW_DLV_OK;
}
/*! @endcode */

void example_loclistcv5(Dwarf_Debug dbg,Dwarf_Attribute someattr)
{
    Dwarf_Unsigned lcount = 0;
    Dwarf_Loc_Head_c loclist_head = 0;
    Dwarf_Error error = 0;
    int lres = 0;

    lres = dwarf_get_loclist_c(someattr,&loclist_head,
        &lcount,&error);
    if (lres == DW_DLV_OK) {
        Dwarf_Unsigned i = 0;

        /*  Before any return remember to call
            dwarf_loc_head_c_dealloc(loclist_head); */
        for (i = 0; i < lcount; ++i) {
            Dwarf_Small loclist_lkind = 0;
            Dwarf_Small lle_value = 0;
            Dwarf_Unsigned rawval1 = 0;
            Dwarf_Unsigned rawval2 = 0;
            Dwarf_Bool debug_addr_unavailable = FALSE;
            Dwarf_Addr lopc = 0;
            Dwarf_Addr hipc = 0;
            Dwarf_Unsigned loclist_expr_op_count = 0;
            Dwarf_Locdesc_c locdesc_entry = 0;
            Dwarf_Unsigned expression_offset = 0;
            Dwarf_Unsigned locdesc_offset = 0;

            lres = dwarf_get_locdesc_entry_d(loclist_head,
                i,
                &lle_value,
                &rawval1,&rawval2,
                &debug_addr_unavailable,
                &lopc,&hipc,
                &loclist_expr_op_count,
                &locdesc_entry,
                &loclist_lkind,
                &expression_offset,
                &locdesc_offset,
                &error);
            if (lres == DW_DLV_OK) {
                Dwarf_Unsigned j = 0;
                int opres = 0;
                Dwarf_Small op = 0;

                for (j = 0; j < loclist_expr_op_count; ++j) {
                    Dwarf_Unsigned raw1 = 0;
                    Dwarf_Unsigned raw2 = 0;
                    Dwarf_Unsigned raw3 = 0;
                    Dwarf_Unsigned opd1 = 0;
                    Dwarf_Unsigned opd2 = 0;
                    Dwarf_Unsigned opd3 = 0;
                    Dwarf_Unsigned offsetforbranch = 0;

                    opres = dwarf_get_location_op_value_d(
                        locdesc_entry,
                        j,&op,
                        &raw1,&raw2,&raw3,
                        &opd1, &opd2,&opd3,&offsetforbranch,
                        &error);
                    if (opres == DW_DLV_OK) {
                        /*  Do something with the operators.
                            Usually you want to use opd1,2,3
                            as appropriate. Calculations
                            involving base addresses etc
                            have already been incorporated
                            in opd1,2,3.  */
                    } else {
                        dwarf_dealloc_error(dbg,error);
                        dwarf_loc_head_c_dealloc(loclist_head);
                        /*Something is wrong. */
                        return;
                    }
                }
            } else {
                /* Something is wrong. Do something. */
                dwarf_loc_head_c_dealloc(loclist_head);
                dwarf_dealloc_error(dbg,error);
                return;
            }
        }
    }
    /*  Always call dwarf_loc_head_c_dealloc()
        to free all the memory associated with loclist_head.  */
    if (error) {
        dwarf_dealloc_error(dbg,error);
    }
    dwarf_loc_head_c_dealloc(loclist_head);
    loclist_head = 0;
    return;
}

void example_locexprc(Dwarf_Debug dbg,Dwarf_Ptr expr_bytes,
    Dwarf_Unsigned expr_len,
    Dwarf_Half addr_size,
    Dwarf_Half offset_size,
    Dwarf_Half version)
{
    Dwarf_Loc_Head_c head = 0;
    Dwarf_Locdesc_c locentry = 0;
    int res2 = 0;
    Dwarf_Unsigned rawlopc = 0;
    Dwarf_Unsigned rawhipc = 0;
    Dwarf_Bool debug_addr_unavail = FALSE;
    Dwarf_Unsigned lopc = 0;
    Dwarf_Unsigned hipc = 0;
    Dwarf_Unsigned ulistlen = 0;
    Dwarf_Unsigned ulocentry_count = 0;
    Dwarf_Unsigned section_offset = 0;
    Dwarf_Unsigned locdesc_offset = 0;
    Dwarf_Small lle_value = 0;
    Dwarf_Small loclist_source = 0;
    Dwarf_Unsigned i = 0;
    Dwarf_Error error = 0;

    res2 = dwarf_loclist_from_expr_c(dbg,
        expr_bytes,expr_len,
        addr_size,
        offset_size,
        version,
        &head,
        &ulistlen,
        &error);
    if (res2 != DW_DLV_OK) {
        return;
    }
    /*  These are a location expression, not loclist.
        So we just need the 0th entry. */
    res2 = dwarf_get_locdesc_entry_d(head,
        0, /* Data from 0th because it is a loc expr,
            there is no list */
        &lle_value,
        &rawlopc, &rawhipc, &debug_addr_unavail, &lopc, &hipc,
        &ulocentry_count, &locentry,
        &loclist_source, &section_offset, &locdesc_offset,
        &error);
    if (res2 == DW_DLV_ERROR) {
        dwarf_loc_head_c_dealloc(head);
        return;
    } else if (res2 == DW_DLV_NO_ENTRY) {
        dwarf_loc_head_c_dealloc(head);
        return;
    }
    /*  ASSERT: ulistlen == 1 */
    for (i = 0; i < ulocentry_count;++i) {
        Dwarf_Small op = 0;
        Dwarf_Unsigned opd1 = 0;
        Dwarf_Unsigned opd2 = 0;
        Dwarf_Unsigned opd3 = 0;
        Dwarf_Unsigned rawop1 = 0;
        Dwarf_Unsigned rawop2 = 0;
        Dwarf_Unsigned rawop3 = 0;
        Dwarf_Unsigned offsetforbranch = 0;

        res2 = dwarf_get_location_op_value_d(locentry,
            i, &op,&opd1,&opd2,&opd3,
            &rawop1,&rawop2,&rawop3,&offsetforbranch,
            &error);
        /* Do something with the expression operator and operands */
        if (res2 != DW_DLV_OK) {
            dwarf_loc_head_c_dealloc(head);
            return;
        }
    }
    dwarf_loc_head_c_dealloc(head);
}

/* examplea */
Dwarf_Unsigned
examplea(Dwarf_Attribute someattr)
{
    Dwarf_Loc_Head_c loclisthead = 0;
    Dwarf_Unsigned   loc_count = 0;
    Dwarf_Error    error = 0;
    Dwarf_Unsigned i = 0;
    Dwarf_Small    lle_value = 0;
    Dwarf_Unsigned rawlowpc = 0;
    Dwarf_Unsigned rawhipc  = 0;
    Dwarf_Locdesc_c locentry = 0;
    Dwarf_Bool     debug_addr_unavailable = 0;
    Dwarf_Addr     lowpc = 0;
    Dwarf_Addr     hipc = 0;
    Dwarf_Unsigned loclist_count  =  0;
    Dwarf_Small    loclist_source_out = 0;
    Dwarf_Unsigned expression_offset = 0;
    Dwarf_Unsigned locdesc_offset = 0;
    int lres = 0;
    Dwarf_Unsigned meaninglesstotal = 0;

    lres = dwarf_get_loclist_c(someattr, &loclisthead,
        &loc_count,&error);
    if (lres != DW_DLV_OK) {
        return 0;
    }
    for (i=0 ; ;++i) {
        lres = dwarf_get_locdesc_entry_d(loclisthead,i,
            &lle_value,&rawlowpc,&rawhipc,&debug_addr_unavailable,
            &lowpc,&hipc,
            &loclist_count, &locentry,
            &loclist_source_out, &expression_offset,&locdesc_offset,
            &error);
        if (lres == DW_DLV_ERROR) {
            dwarf_loc_head_c_dealloc(loclisthead);
            return 0;
        }
        if (lres == DW_DLV_NO_ENTRY) {
            /* done */
            break;
        }
        /* Do something with the values. We fake to avoid
            compiler complaints about unused args.*/
        meaninglesstotal += lle_value + rawlowpc+
            rawhipc+(int)debug_addr_unavailable +
            lowpc+ hipc+loclist_count+loclist_source_out+
            expression_offset+locdesc_offset;

    }
    dwarf_loc_head_c_dealloc(loclisthead);
    return meaninglesstotal;
}

/*! @defgroup examplec Example of dwarf_srclines_b etc
    @brief examplesrclines

    An example calling dwarf_srclines_b
    dwarf_srclines_dealloc_b
    dwarf_srclines_from_linecontext
    dwarf_srclines_files_indexes
    dwarf_srclines_files_data_b
    dwarf_srclines_two_level_from_linecontext

    @param path
    Path to an object we wish to open.
    @param groupnumber
    @code
*/
int examplec(Dwarf_Die cu_die,Dwarf_Error *error)
{
    /* EXAMPLE: DWARF5 style access.  */
    Dwarf_Line  *linebuf = 0;
    Dwarf_Signed linecount = 0;
    Dwarf_Line  *linebuf_actuals = 0;
    Dwarf_Signed linecount_actuals = 0;
    Dwarf_Line_Context line_context = 0;
    Dwarf_Small  table_count = 0;
    Dwarf_Unsigned lineversion = 0;
    int sres = 0;
    /* ... */
    /*  we use 'return' here to signify we can do nothing more
        at this point in the code. */
    sres = dwarf_srclines_b(cu_die,&lineversion,
        &table_count,&line_context,error);
    if (sres != DW_DLV_OK) {
        /*  Handle the DW_DLV_NO_ENTRY  or DW_DLV_ERROR
            No memory was allocated so there nothing
            to dealloc. */
        return sres;
    }
    if (table_count == 0) {
        /*  A line table with no actual lines.  */
        /*...do something, see dwarf_srclines_files_count()
            etc below. */

        dwarf_srclines_dealloc_b(line_context);
        /*  All the memory is released, the line_context
            and linebuf zeroed now
            as a reminder they are stale. */
        linebuf = 0;
        line_context = 0;
    } else if (table_count == 1) {
        Dwarf_Signed i = 0;
        Dwarf_Signed baseindex = 0;
        Dwarf_Signed file_count = 0;
        Dwarf_Signed endindex = 0;
        /*  Standard dwarf 2,3,4, or 5 line table */
        /*  Do something. */

        /*  First let us index through all the files listed
            in the line table header. */
        sres = dwarf_srclines_files_indexes(line_context,
            &baseindex,&file_count,&endindex,error);
        if (sres != DW_DLV_OK) {
            /* Something badly wrong! */
            return sres;
        }
        /*  Works for DWARF2,3,4 (one-based index)
            and DWARF5 (zero-based index) */
        for (i = baseindex; i < endindex; i++) {
            Dwarf_Unsigned dirindex = 0;
            Dwarf_Unsigned modtime = 0;
            Dwarf_Unsigned flength = 0;
            Dwarf_Form_Data16 *md5data = 0;
            int vres = 0;
            const char *name = 0;

            vres = dwarf_srclines_files_data_b(line_context,i,
                &name,&dirindex, &modtime,&flength,
                &md5data,error);
            if (vres != DW_DLV_OK) {
                /* something very wrong. */
                return vres;
            }
            /* do something */
        }

        /*  For this case where we have a line table we will likely
            wish to get the line details: */
        sres = dwarf_srclines_from_linecontext(line_context,
            &linebuf,&linecount,
            error);
        if (sres != DW_DLV_OK) {
            /* Error. Clean up the context information. */
            dwarf_srclines_dealloc_b(line_context);
            return sres;
        }
        /* The lines are normal line table lines. */
        for (i = 0; i < linecount; ++i) {
            /* use linebuf[i] */
        }
        dwarf_srclines_dealloc_b(line_context);
        /*  All the memory is released, the line_context
            and linebuf zeroed now as a reminder they are stale */
        linebuf = 0;
        line_context = 0;
        linecount = 0;
    } else {
        Dwarf_Signed i = 0;
        /*  ASSERT: table_count == 2,
            Experimental two-level line table. Version 0xf006
            We do not define the meaning of this non-standard
            set of tables here. */

        /*  For 'something C' (two-level line tables)
            one codes something like this
            Note that we do not define the meaning or
            use of two-level line
            tables as these are experimental, not standard DWARF. */
        sres = dwarf_srclines_two_level_from_linecontext(line_context,
            &linebuf,&linecount,
            &linebuf_actuals,&linecount_actuals,
            error);
        if (sres == DW_DLV_OK) {
            for (i = 0; i < linecount; ++i) {
                /*  use linebuf[i], these are the 'logicals'
                    entries. */
            }
            for (i = 0; i < linecount_actuals; ++i) {
                /*  use linebuf_actuals[i], these are the
                    actuals entries */
            }
            dwarf_srclines_dealloc_b(line_context);
            line_context = 0;
            linebuf = 0;
            linecount = 0;
            linebuf_actuals = 0;
            linecount_actuals = 0;
        } else if (sres == DW_DLV_NO_ENTRY) {
            /* This should be impossible, but do something.   */
            /* Then Free the line_context */
            dwarf_srclines_dealloc_b(line_context);
            line_context = 0;
            linebuf = 0;
            linecount = 0;
            linebuf_actuals = 0;
            linecount_actuals = 0;
        } else {
            /*  ERROR, show the error or something.
                Free the line_context. */
            dwarf_srclines_dealloc_b(line_context);
            line_context = 0;
            linebuf = 0;
            linecount = 0;
            linebuf_actuals = 0;
            linecount_actuals = 0;
        }
    }
    return DW_DLV_OK;
}
/*! @endcode */

/*! @defgroup exampled Example of dwarf_srclines_b use
    @see dwarf_srclines_b
    @see dwarf_srclines_from_linecontext
    @see dwarf_srclines_dealloc_b
    @code
*/
int exampled(Dwarf_Die somedie,Dwarf_Error *error)
{
    Dwarf_Signed count = 0;
    Dwarf_Line_Context context = 0;
    Dwarf_Line *linebuf = 0;
    Dwarf_Signed i = 0;
    Dwarf_Line *line;
    Dwarf_Small table_count =0;
    Dwarf_Unsigned version = 0;
    int sres = 0;

    sres = dwarf_srclines_b(somedie,
        &version, &table_count,&context,error);
    if (sres != DW_DLV_OK) {
        return sres;
    }
    sres = dwarf_srclines_from_linecontext(context,
        &linebuf,&count,error);
    if (sres != DW_DLV_OK) {
        dwarf_srclines_dealloc_b(context);
        return sres;
    }
    line = linebuf;
    for (i = 0; i < count; ++line) {
        /* use line */
    }
    dwarf_srclines_dealloc_b(context);
    return DW_DLV_OK;
}
/*! @endcode */

/*! @defgroup examplee Example of dwarf_srcfiles use
    @code
*/
int examplee(Dwarf_Debug dbg,Dwarf_Die somedie,Dwarf_Error *error)
{
    Dwarf_Signed count = 0;
    char **srcfiles = 0;
    Dwarf_Signed i = 0;
    int res = 0;

    res = dwarf_srcfiles(somedie, &srcfiles,&count,error);
    if (res != DW_DLV_OK) {
        return res;
    }
    for (i = 0; i < count; ++i) {
        /* use srcfiles[i] */
        dwarf_dealloc(dbg, srcfiles[i], DW_DLA_STRING);
    }
    dwarf_dealloc(dbg, srcfiles, DW_DLA_LIST);
    return DW_DLV_OK;
}
/*! @endcode */

void examplef(Dwarf_Debug dbg)
{
    Dwarf_Signed count = 0;
    Dwarf_Global *globs = 0;
    Dwarf_Signed i = 0;
    Dwarf_Error  error = 0;
    int res = 0;

    res = dwarf_get_globals(dbg, &globs,&count, &error);
    if (res == DW_DLV_OK) {
        for (i = 0; i < count; ++i) {
            /* use globs[i] */
        }
        dwarf_globals_dealloc(dbg, globs, count);
    }
}

void exampleg(Dwarf_Debug dbg)
{
    Dwarf_Error error = 0;
    Dwarf_Signed count = 0;
    Dwarf_Type *types = 0;
    Dwarf_Signed i = 0;
    int res = 0;

    res = dwarf_get_pubtypes(dbg, &types,&count, &error);
    if (res == DW_DLV_OK) {
        for (i = 0; i < count; ++i) {
            /* use types[i] */
        }
        dwarf_types_dealloc(dbg, types, count);
    }
}

void exampleh(Dwarf_Debug dbg)
{
    Dwarf_Error error = 0;
    Dwarf_Signed count = 0;
    Dwarf_Weak *weaks = 0;
    Dwarf_Signed i = 0;
    int res = 0;

    res = dwarf_get_weaks(dbg, &weaks, &count, &error);
    if (res == DW_DLV_OK) {
        for (i = 0; i < count; ++i) {
            /* use weaks[i] */
        }
        dwarf_weaks_dealloc(dbg, weaks, count);
    }
}

/* example of what not to do. */
void examplei(Dwarf_Debug dbg)
{
    Dwarf_Error error = 0;
    Dwarf_Signed count = 0;
    Dwarf_Weak *weaks = 0;
    Dwarf_Signed i = 0;
    int res = 0;

    /* Obsolete, see exampleh instead */
    res = dwarf_get_weaks(dbg, &weaks, &count, &error);
    if (res == DW_DLV_OK) {
        /*  OBSOLETE: do not use dealloc for this.
            See above */
        for (i = 0; i < count; ++i) {
            /* use weaks[i] */
            dwarf_dealloc(dbg, weaks[i], DW_DLA_WEAK);
        }
        dwarf_dealloc(dbg, weaks, DW_DLA_LIST);
    }
}

void examplej(Dwarf_Debug dbg)
{
    Dwarf_Error error = 0;
    Dwarf_Signed count = 0;
    Dwarf_Func *funcs = 0;
    Dwarf_Signed i = 0;
    int fres = 0;

    fres = dwarf_get_funcs(dbg, &funcs, &count, &error);
    if (fres == DW_DLV_OK) {
        for (i = 0; i < count; ++i) {
            /* use funcs[i] */
        }
        dwarf_funcs_dealloc(dbg, funcs, count);
    }
}

void examplek(Dwarf_Debug dbg)
{
    Dwarf_Error error = 0;
    Dwarf_Func *funcs = 0;
    Dwarf_Signed count = 0;
    Dwarf_Signed i = 0;
    int fres = 0;

    fres = dwarf_get_funcs(dbg, &funcs,&count, &error);
    if (fres == DW_DLV_OK) {
        /* OBSOLETE: see dwarf_funcs_dealloc() above */
        for (i = 0; i < count; ++i) {
            /* use funcs[i] */
            dwarf_dealloc(dbg, funcs[i], DW_DLA_FUNC);
        }
        dwarf_dealloc(dbg, funcs, DW_DLA_LIST);
    }
}

void examplel(Dwarf_Debug dbg)
{
    Dwarf_Error error = 0;
    Dwarf_Signed count = 0;
    Dwarf_Type *types = 0;
    Dwarf_Signed i = 0;
    int res = 0;

    res = dwarf_get_types(dbg, &types,&count, &error);
    if (res == DW_DLV_OK) {
        for (i = 0; i < count; ++i) {
            /* use types[i] */
        }
        dwarf_types_dealloc(dbg, types, count);
    }
}

void examplem(Dwarf_Debug dbg)
{
    Dwarf_Error error = 0;
    Dwarf_Signed count = 0;
    Dwarf_Type *types = 0;
    Dwarf_Signed i = 0;
    int res = 0;

    /*  OBSOLETE: see dwarf_types_dealloc() above */
    res = dwarf_get_types(dbg, &types,&count, &error);
    if (res == DW_DLV_OK) {
        for (i = 0; i < count; ++i) {
            /* use types[i] */
            dwarf_dealloc(dbg, types[i], DW_DLA_TYPENAME);
        }
        dwarf_dealloc(dbg, types, DW_DLA_LIST);
    }
}

void examplen(Dwarf_Debug dbg)
{
    Dwarf_Error error = 0;
    Dwarf_Signed count = 0;
    Dwarf_Var *vars = 0;
    Dwarf_Signed i = 0;
    int res = 0;
    res = dwarf_get_vars(dbg, &vars,&count,&error);
    if (res == DW_DLV_OK) {
        for (i = 0; i < count; ++i) {
            /* use vars[i] */
        }
        dwarf_vars_dealloc(dbg, vars, count);
    }
}

void exampleo(Dwarf_Debug dbg)
{
    Dwarf_Error error = 0;
    Dwarf_Signed count = 0;
    Dwarf_Var *vars = 0;
    Dwarf_Signed i = 0;
    int res = 0;
    res = dwarf_get_vars(dbg, &vars,&count,&error);
    if (res == DW_DLV_OK) {
        /* DO NOT USE: see dwarf_vars_dealloc() above */
        for (i = 0; i < count; ++i) {
            /* use vars[i] */
            dwarf_dealloc(dbg, vars[i], DW_DLA_VAR);
        }
        dwarf_dealloc(dbg, vars, DW_DLA_LIST);
    }
}

void exampledebugnames()
{
    /* FIXME need extended example of debugnames use. */
}

/*! @defgroup examplep5 An example reading .debug_macro

    @brief An example reading DWARF5 macro data
    This builds an list or some other data structure
    (not defined) to give an import somewhere to list
    the import offset and then later to enquire
    if the list has unexamined offsets.
    The code compiles but is not yet tested.

    This example does not actually do the import at
    the correct time as this is just checking
    import offsets, not creating
    a proper full list (in the proper order) of the
    macros with the imports inserted.

    A candidate set of hypothetical functions that
    callers would write for this special checking
    purpose:

    @code
*/
int  has_unchecked_import_in_list(void);
Dwarf_Unsigned get_next_import_from_list(void);
void mark_this_offset_as_examined(Dwarf_Unsigned macro_unit_offset);
void add_offset_to_list(Dwarf_Unsigned offset);
int  examplep5(Dwarf_Die cu_die,Dwarf_Error *error)
{
    int lres = 0;
    Dwarf_Unsigned version = 0;
    Dwarf_Macro_Context macro_context = 0;
    Dwarf_Unsigned macro_unit_offset = 0;
    Dwarf_Unsigned number_of_ops = 0;
    Dwarf_Unsigned ops_total_byte_len = 0;
    Dwarf_Bool is_primary = TRUE;
    unsigned k = 0;

    for (;;) {
        if (is_primary) {
            lres = dwarf_get_macro_context(cu_die,
                &version,&macro_context,
                &macro_unit_offset,
                &number_of_ops,
                &ops_total_byte_len,
                error);
            is_primary = FALSE;
        } else {
            if (has_unchecked_import_in_list()) {
                macro_unit_offset = get_next_import_from_list();
            } else {
                /* We are done */
                break;
            }
            lres = dwarf_get_macro_context_by_offset(cu_die,
                macro_unit_offset,
                &version,
                &macro_context,
                &number_of_ops,
                &ops_total_byte_len,
                error);
            mark_this_offset_as_examined(macro_unit_offset);
        }

        if (lres == DW_DLV_ERROR) {
            /* Something is wrong. */
            return lres;
        }
        if (lres == DW_DLV_NO_ENTRY) {
            /* We are done. */
            break;
        }
        /* lres ==  DW_DLV_OK) */
        for (k = 0; k < number_of_ops; ++k) {
            Dwarf_Unsigned  section_offset = 0;
            Dwarf_Half      macro_operator = 0;
            Dwarf_Half      forms_count = 0;
            const Dwarf_Small *formcode_array = 0;
            Dwarf_Unsigned  line_number = 0;
            Dwarf_Unsigned  index = 0;
            Dwarf_Unsigned  offset =0;
            const char    * macro_string =0;
            int lres2 = 0;

            lres2 = dwarf_get_macro_op(macro_context,
                k, &section_offset,&macro_operator,
                &forms_count, &formcode_array,error);
            if (lres2 != DW_DLV_OK) {
                /* Some error. Deal with it */
                dwarf_dealloc_macro_context(macro_context);
                return lres2;
            }
            switch(macro_operator) {
            case 0:
                /* Nothing to do. */
                break;
            case DW_MACRO_end_file:
                /* Do something */
                break;
            case DW_MACRO_define:
            case DW_MACRO_undef:
            case DW_MACRO_define_strp:
            case DW_MACRO_undef_strp:
            case DW_MACRO_define_strx:
            case DW_MACRO_undef_strx:
            case DW_MACRO_define_sup:
            case DW_MACRO_undef_sup: {
                lres2 = dwarf_get_macro_defundef(macro_context,
                    k,
                    &line_number,
                    &index,
                    &offset,
                    &forms_count,
                    &macro_string,
                    error);
                if (lres2 != DW_DLV_OK) {
                    /* Some error. Deal with it */
                    dwarf_dealloc_macro_context(macro_context);
                    return lres2;
                }
                /* do something */
                }
                break;
            case DW_MACRO_start_file: {
                lres2 = dwarf_get_macro_startend_file(macro_context,
                    k,&line_number,
                    &index,
                    &macro_string,error);
                if (lres2 != DW_DLV_OK) {
                    /* Some error. Deal with it */
                    dwarf_dealloc_macro_context(macro_context);
                    return lres2;
                }
                /* do something */
                }
                break;
            case DW_MACRO_import: {
                lres2 = dwarf_get_macro_import(macro_context,
                    k,&offset,error);
                if (lres2 != DW_DLV_OK) {
                    /* Some error. Deal with it */
                    dwarf_dealloc_macro_context(macro_context);
                    return lres2;
                }
                add_offset_to_list(offset);
                }
                break;
            case DW_MACRO_import_sup: {
                lres2 = dwarf_get_macro_import(macro_context,
                    k,&offset,error);
                if (lres2 != DW_DLV_OK) {
                    /* Some error. Deal with it */
                    dwarf_dealloc_macro_context(macro_context);
                    return lres2;
                }
                /* do something */
                }
                break;
            default:
                /*  This is an error or an omission
                    in the code here.  We do not
                    know what to do.
                    Do something appropriate, print something?. */
                break;
            }
        }
        dwarf_dealloc_macro_context(macro_context);
        macro_context = 0;
    }
    return DW_DLV_OK;
}
/* @endcode */

/*! @defgroup examplep2 Example of reading .debug_macinfo
    @brief examplep2 Reading .debug_macinfo, DWARF2-4

    @code */

void functionusingsigned(Dwarf_Signed s);

int examplep2(Dwarf_Debug dbg, Dwarf_Off cur_off,
    Dwarf_Error*error)
{
    Dwarf_Signed count = 0;
    Dwarf_Macro_Details *maclist = 0;
    Dwarf_Signed i = 0;
    Dwarf_Unsigned max = 500000; /* sanity limit */
    int errv = 0;

    /*  This is for DWARF2,DWARF3, and DWARF4
        .debug_macinfo section only.*/
    /*  Given an offset from a compilation unit,
        start at that offset (from DW_AT_macroinfo)
        and get its macro details. */
    errv = dwarf_get_macro_details(dbg, cur_off,max,
        &count,&maclist,error);
    if (errv == DW_DLV_OK) {
        for (i = 0; i < count; ++i) {
            Dwarf_Macro_Details *  mentry = maclist +i;
            /* example of use */
            Dwarf_Signed lineno = mentry->dmd_lineno;
            functionusingsigned(lineno);
        }
        dwarf_dealloc(dbg, maclist, DW_DLA_STRING);
    }
    /*  Loop through all the compilation units macro info from zero.
        This is not guaranteed to work because DWARF does not
        guarantee every byte in the section is meaningful:
        there can be garbage between the macro info
        for CUs.  But this loop will sometimes work.
    */
    cur_off = 0;
    while((errv = dwarf_get_macro_details(dbg, cur_off,max,
        &count,&maclist,error))== DW_DLV_OK) {
        for (i = 0; i < count; ++i) {
            Dwarf_Macro_Details *  mentry = maclist +i;
            /* example of use */
            Dwarf_Signed lineno = mentry->dmd_lineno;
            functionusingsigned(lineno);
        }
        cur_off = maclist[count-1].dmd_offset + 1;
        dwarf_dealloc(dbg, maclist, DW_DLA_STRING);
    }
    return DW_DLV_OK;
}
/*! @endcode */

void exampleq(Dwarf_Debug dbg)
{
    Dwarf_Cie *cie_data = 0;
    Dwarf_Signed cie_count = 0;
    Dwarf_Fde *fde_data = 0;
    Dwarf_Signed fde_count = 0;
    Dwarf_Error error = 0;
    int fres = 0;

    fres = dwarf_get_fde_list(dbg,&cie_data,&cie_count,
        &fde_data,&fde_count,&error);
    if (fres == DW_DLV_OK) {
        dwarf_dealloc_fde_cie_list(dbg, cie_data, cie_count,
            fde_data,fde_count);
    }
}

/* OBSOLETE EXAMPLE */
void exampleqb(Dwarf_Debug dbg)
{
    Dwarf_Cie *cie_data = 0;
    Dwarf_Signed cie_count = 0;
    Dwarf_Fde *fde_data = 0;
    Dwarf_Signed fde_count = 0;
    Dwarf_Error error = 0;
    Dwarf_Signed i = 0;
    int fres = 0;

    fres = dwarf_get_fde_list(dbg,&cie_data,&cie_count,
        &fde_data,&fde_count,&error);
    if (fres == DW_DLV_OK) {
        for (i = 0; i < cie_count; ++i) {
            /* use cie[i] */
            dwarf_dealloc(dbg, cie_data[i], DW_DLA_CIE);
        }
        for (i = 0; i < fde_count; ++i) {
            /* use fde[i] */
            dwarf_dealloc(dbg, fde_data[i], DW_DLA_FDE);
        }
        dwarf_dealloc(dbg, cie_data, DW_DLA_LIST);
        dwarf_dealloc(dbg, fde_data, DW_DLA_LIST);
    }
}

void exampler(Dwarf_Debug dbg,Dwarf_Addr mypcval)
{
    /*  Given a pc value
        for a function find the FDE and CIE data for
        the function.
        Example shows basic access to FDE/CIE plus
        one way to access details given a PC value.
        dwarf_get_fde_n() allows accessing all FDE/CIE
        data so one could build up an application-specific
        table of information if that is more useful.  */
    Dwarf_Cie *cie_data = 0;
    Dwarf_Signed cie_count = 0;
    Dwarf_Fde *fde_data = 0;
    Dwarf_Signed fde_count = 0;
    Dwarf_Error error = 0;
    int fres = 0;

    fres = dwarf_get_fde_list_eh(dbg,&cie_data,&cie_count,
        &fde_data,&fde_count,&error);
    if (fres == DW_DLV_OK) {
        Dwarf_Fde myfde = 0;
        Dwarf_Addr low_pc = 0;
        Dwarf_Addr high_pc = 0;
        fres = dwarf_get_fde_at_pc(fde_data,mypcval,
            &myfde,&low_pc,&high_pc,
            &error);
        if (fres == DW_DLV_OK) {
            Dwarf_Cie mycie = 0;
            fres = dwarf_get_cie_of_fde(myfde,&mycie,&error);
            if (fres == DW_DLV_OK) {
                /*  Now we can access a range of information
                    about the fde and cie applicable. */
            }
        }
        dwarf_dealloc_fde_cie_list(dbg, cie_data, cie_count,
            fde_data,fde_count);
    }
    /* ERROR or NO ENTRY. Do something */
}

void examples(Dwarf_Cie cie,
    Dwarf_Ptr instruction,Dwarf_Unsigned len)
{
    Dwarf_Frame_Instr_Head head = 0;
    Dwarf_Unsigned         count = 0;
    Dwarf_Error            error = 0;
    int res = 0;

    res = dwarf_expand_frame_instructions(cie,instruction,len,
        &head,&count, &error);
    if (res == DW_DLV_OK) {
        Dwarf_Unsigned i = 0;

        for (i = 0; i < count; ++i) {
            Dwarf_Unsigned  instr_offset_in_instrs = 0;
            Dwarf_Small     cfa_operation          = 0;
            const char     *fields_description     = 0;
            Dwarf_Unsigned  u0 = 0;
            Dwarf_Unsigned  u1 = 0;
            Dwarf_Signed    s0 = 0;
            Dwarf_Signed    s1 = 0;
            Dwarf_Unsigned  code_alignment_factor = 0;
            Dwarf_Signed    data_alignment_factor = 0;
            Dwarf_Block     expression_block;
            const char *    op_name = 0;

            memset(&expression_block,0,sizeof(expression_block));
            res = dwarf_get_frame_instruction(head,i,
                &instr_offset_in_instrs,&cfa_operation,
                &fields_description,&u0,&u1,
                &s0,&s1,
                &code_alignment_factor,
                &data_alignment_factor,
                &expression_block,&error);
            if (res == DW_DLV_OK) {
                res = dwarf_get_CFA_name(cfa_operation,
                    &op_name);
                if (res != DW_DLV_OK) {
                    op_name = "unknown op";
                }
                printf("Instr %2lu %-22s %s\n",
                    (unsigned long)i,
                    op_name,
                    fields_description);
                /*  Do something with the various data
                    as guided by the fields_description. */
            }
        }
        dwarf_dealloc_frame_instr_head(head);
    }
}

/*  We'll do in one function and not worry about
    memory leakage to keep the example short.
    dwarf_finish() will deallocate  if we do not
    do so here. */
void examplestrngoffsets(Dwarf_Debug dbg)
{
    int res = 0;
    Dwarf_Str_Offsets_Table sot = 0;
    Dwarf_Unsigned wasted_byte_count = 0;
    Dwarf_Unsigned table_count = 0;
    Dwarf_Error error = 0;

    res = dwarf_open_str_offsets_table_access(dbg, &sot,&error);
    if (res == DW_DLV_NO_ENTRY) {
        /* No such table */
        return;
    }
    if (res == DW_DLV_ERROR) {
        /* Something is very wrong. Print the error? */
        return;
    }
    for (;;) {
        Dwarf_Unsigned unit_length =0;
        Dwarf_Unsigned unit_length_offset =0;
        Dwarf_Unsigned table_start_offset =0;
        Dwarf_Half     entry_size = 0;
        Dwarf_Half     version =0;
        Dwarf_Half     padding =0;
        Dwarf_Unsigned table_value_count =0;
        Dwarf_Unsigned i = 0;
        Dwarf_Unsigned table_entry_value = 0;

        res = dwarf_next_str_offsets_table(sot,
            &unit_length, &unit_length_offset,
            &table_start_offset,
            &entry_size,&version,&padding,
            &table_value_count,&error);
        if (res == DW_DLV_NO_ENTRY) {
            /* We have dealt with all tables */
            break;
        }
        if (res == DW_DLV_ERROR) {
            /* Something badly wrong. Do something. */
            return;
        }
        /*  One could call dwarf_str_offsets_statistics to
            get the wasted bytes so far, but we do not do that
            in this example. */
        /*  Possibly print the various table-related values
            returned just above. */
        for (i=0; i < table_value_count; ++i) {
            res = dwarf_str_offsets_value_by_index(sot,i,
                &table_entry_value,&error);
            if (res != DW_DLV_OK) {
                /* Something is badly wrong. Do something. */
                return;
            }
            /*  Do something with the table_entry_value
                at this index. Maybe just print it.
                It is an offset in .debug_str. */
        }
    }
    res = dwarf_str_offsets_statistics(sot,&wasted_byte_count,
        &table_count,&error);
    if (res == DW_DLV_OK) {
        /*  The wasted byte count is set. Print it or something.
            One hopes zero bytes are wasted.
            Print the table count if one is interested. */
    }
    res = dwarf_close_str_offsets_table_access(sot,&error);
    /*  There is little point in checking the return value
        as little can be done about any error. */
    sot = 0;
}

void exampleu(Dwarf_Debug dbg)
{
    Dwarf_Signed count = 0;
    Dwarf_Arange *arang = 0;
    int res = 0;
    Dwarf_Error error = 0;

    res = dwarf_get_aranges(dbg, &arang,&count, &error);
    if (res == DW_DLV_OK) {
        Dwarf_Signed i = 0;

        for (i = 0; i < count; ++i) {
            /* use arang[i] */
            dwarf_dealloc(dbg, arang[i], DW_DLA_ARANGE);
        }
        dwarf_dealloc(dbg, arang, DW_DLA_LIST);
    }
}
void functionusingrange(Dwarf_Ranges *r);
void examplev(Dwarf_Debug dbg,Dwarf_Off rangesoffset,Dwarf_Die die)
{
    Dwarf_Signed count = 0;
    Dwarf_Off  realoffset = 0;
    Dwarf_Ranges *rangesbuf = 0;
    Dwarf_Unsigned bytecount = 0;
    Dwarf_Error error = 0;
    int res = 0;

    res = dwarf_get_ranges_b(dbg,rangesoffset,die,
        &realoffset,
        &rangesbuf,&count,&bytecount,&error);
    if (res == DW_DLV_OK) {
        Dwarf_Signed i = 0;
        for ( i = 0; i < count; ++i ) {
            Dwarf_Ranges *cur = rangesbuf+i;
            /* Use cur. */
            functionusingrange(cur);
        }
        dwarf_dealloc_ranges(dbg,rangesbuf,count);
    }
}
void examplew(Dwarf_Debug dbg)
{
    Dwarf_Gdbindex gindexptr = 0;
    Dwarf_Unsigned version = 0;
    Dwarf_Unsigned cu_list_offset = 0;
    Dwarf_Unsigned types_cu_list_offset = 0;
    Dwarf_Unsigned address_area_offset = 0;
    Dwarf_Unsigned symbol_table_offset = 0;
    Dwarf_Unsigned constant_pool_offset = 0;
    Dwarf_Unsigned section_size = 0;
    Dwarf_Error error = 0;
    const char * section_name = 0;
    int res = 0;
    res = dwarf_gdbindex_header(dbg,&gindexptr,
        &version,&cu_list_offset, &types_cu_list_offset,
        &address_area_offset,&symbol_table_offset,
        &constant_pool_offset, &section_size,
        &section_name,&error);
    if (res == DW_DLV_NO_ENTRY) {
        return;
    } else if (res == DW_DLV_ERROR) {
        return;
    }
    {
        /* do something with the data */
        Dwarf_Unsigned length = 0;
        Dwarf_Unsigned typeslength = 0;
        Dwarf_Unsigned i = 0;
        res = dwarf_gdbindex_culist_array(gindexptr,
            &length,&error);
        /* Example actions. */
        if (res == DW_DLV_OK) {
            for (i = 0; i < length; ++i) {
                Dwarf_Unsigned cuoffset = 0;
                Dwarf_Unsigned culength = 0;
                res = dwarf_gdbindex_culist_entry(gindexptr,
                    i,&cuoffset,&culength,&error);
                if (res == DW_DLV_OK) {
                    /* Do something with cuoffset, culength */
                }
            }
        }
        res = dwarf_gdbindex_types_culist_array(gindexptr,
            &typeslength,&error);
        if (res == DW_DLV_OK) {
            for (i = 0; i < typeslength; ++i) {
                Dwarf_Unsigned cuoffset = 0;
                Dwarf_Unsigned tuoffset = 0;
                Dwarf_Unsigned type_signature  = 0;
                res = dwarf_gdbindex_types_culist_entry(gindexptr,
                    i,&cuoffset,&tuoffset,&type_signature,&error);
                if (res == DW_DLV_OK) {
                    /* Do something with cuoffset etc. */
                }
            }
        }
        dwarf_gdbindex_free(gindexptr);
    }
}
void examplewgdbindex(Dwarf_Gdbindex gdbindex)
{
    Dwarf_Unsigned list_len = 0;
    Dwarf_Unsigned i = 0;
    int res = 0;
    Dwarf_Error err = 0;

    res = dwarf_gdbindex_addressarea(gdbindex, &list_len,&err);
    if (res != DW_DLV_OK) {
        /* Something wrong, ignore the addressarea */
    }
    /* Iterate through the address area. */
    for (i  = 0; i < list_len; i++) {
        Dwarf_Unsigned lowpc = 0;
        Dwarf_Unsigned highpc = 0;
        Dwarf_Unsigned cu_index = 0;
        res = dwarf_gdbindex_addressarea_entry(gdbindex,i,
            &lowpc,&highpc,
            &cu_index,
            &err);
        if (res != DW_DLV_OK) {
            /* Something wrong, ignore the addressarea */
            return;
        }
        /*  We have a valid address area entry, do something
            with it. */
    }
}
void examplex(Dwarf_Gdbindex gdbindex)
{
    Dwarf_Unsigned symtab_list_length = 0;
    Dwarf_Unsigned i = 0;
    Dwarf_Error err = 0;
    int res = 0;

    res = dwarf_gdbindex_symboltable_array(gdbindex,
        &symtab_list_length,&err);
    if (res != DW_DLV_OK) {
        return;
    }
    for (i  = 0; i < symtab_list_length; i++) {
        Dwarf_Unsigned symnameoffset = 0;
        Dwarf_Unsigned cuvecoffset = 0;
        Dwarf_Unsigned cuvec_len = 0;
        Dwarf_Unsigned ii = 0;
        const char *name = 0;
        int resl = 0;

        resl = dwarf_gdbindex_symboltable_entry(gdbindex,i,
            &symnameoffset,&cuvecoffset,
            &err);
        if (resl != DW_DLV_OK) {
            return;
        }
        resl = dwarf_gdbindex_string_by_offset(gdbindex,
            symnameoffset,&name,&err);
        if (resl != DW_DLV_OK) {
            return;
        }
        resl = dwarf_gdbindex_cuvector_length(gdbindex,
            cuvecoffset,&cuvec_len,&err);
        if (resl != DW_DLV_OK) {
            return;
        }
        for (ii = 0; ii < cuvec_len; ++ii ) {
            Dwarf_Unsigned attributes = 0;
            Dwarf_Unsigned cu_index = 0;
            Dwarf_Unsigned symbol_kind = 0;
            Dwarf_Unsigned is_static = 0;
            int res2 = 0;

            res2 = dwarf_gdbindex_cuvector_inner_attributes(
                gdbindex,cuvecoffset,ii,
                &attributes,&err);
            if (res2 != DW_DLV_OK) {
                return;
            }
            /*  'attributes' is a value with various internal
                fields so we expand the fields. */
            res2 = dwarf_gdbindex_cuvector_instance_expand_value(
                gdbindex, attributes, &cu_index,
                &symbol_kind, &is_static,
                &err);
            if (res2 != DW_DLV_OK) {
                return;
            }
            /* Do something with the attributes. */
        }
    }
}

void exampley(Dwarf_Debug dbg, const char *type)
{
    /* type is "tu" or "cu" */
    int res = 0;
    Dwarf_Xu_Index_Header xuhdr = 0;
    Dwarf_Unsigned version_number = 0;
    Dwarf_Unsigned offsets_count = 0; /*L */
    Dwarf_Unsigned units_count = 0; /* M */
    Dwarf_Unsigned hash_slots_count = 0; /* N */
    Dwarf_Error err = 0;
    const char * section_name = 0;

    res = dwarf_get_xu_index_header(dbg,
        type,
        &xuhdr,
        &version_number,
        &offsets_count,
        &units_count,
        &hash_slots_count,
        &section_name,
        &err);
    if (res == DW_DLV_NO_ENTRY) {
        /* No such section. */
        return;
    }
    if (res == DW_DLV_ERROR) {
        /* Something wrong. */
        return;
    }
    /* Do something with the xuhdr here . */
    dwarf_xu_header_free(xuhdr);
}

void examplez( Dwarf_Xu_Index_Header xuhdr,
    Dwarf_Unsigned hash_slots_count)
{
    /*  hash_slots_count returned by
        dwarf_get_xu_index_header(), see above. */
    static Dwarf_Sig8 zerohashval;

    Dwarf_Error err = 0;
    Dwarf_Unsigned h = 0;

    for (h = 0; h < hash_slots_count; h++) {
        Dwarf_Sig8 hashval;
        Dwarf_Unsigned index = 0;
        int res = 0;

        res = dwarf_get_xu_hash_entry(xuhdr,h,
            &hashval,&index,&err);
        if (res == DW_DLV_ERROR) {
            /* Oops. hash_slots_count wrong. */
            return;
        } else if (res == DW_DLV_NO_ENTRY) {
            /* Impossible */
            return;
        } else if (!memcmp(&hashval,&zerohashval,
            sizeof(Dwarf_Sig8))
            && index == 0 ) {
            /* An unused hash slot */
            continue;
        }
        /*  Here, hashval and index (a row index into
            offsets and lengths) are valid. */
    }
}

void exampleza(Dwarf_Xu_Index_Header xuhdr,
    Dwarf_Unsigned offsets_count, Dwarf_Unsigned index )
{
    Dwarf_Error err = 0;
    Dwarf_Unsigned col = 0;
    /*  We use  'offsets_count' returned by
        a dwarf_get_xu_index_header() call.
        We use 'index' returned by a
        dwarf_get_xu_hash_entry() call. */
    for (col = 0; col < offsets_count; col++) {
        Dwarf_Unsigned off = 0;
        Dwarf_Unsigned len = 0;
        const char * name = 0;
        Dwarf_Unsigned num = 0;
        int res = 0;

        res = dwarf_get_xu_section_names(xuhdr,
            col,&num,&name,&err);
        if (res != DW_DLV_OK) {
            break;
        }
        res = dwarf_get_xu_section_offset(xuhdr,
            index,col,&off,&len,&err);
        if (res != DW_DLV_OK) {
            break;
        }
        /*  Here we have the DW_SECT_ name and number
            and the base offset and length of the
            section data applicable to the hash
            that got us here.
            Use the values.*/
    }
}

void examplezb(void)
{
    const char * out = 0;
    int res = 0;

    /* The following is wrong, do not do it! */
    res = dwarf_get_ACCESS_name(DW_TAG_entry_point,&out);
    /*  Nothing one does here with 'res' or 'out'
        is meaningful. */

    /* The following is meaningful.*/
    res = dwarf_get_TAG_name(DW_TAG_entry_point,&out);
    if ( res == DW_DLV_OK) {
        /*  Here 'out' is a pointer one can use which
            points to the string "DW_TAG_entry_point". */
    } else {
        /*  Here 'out' has not been touched, it is
            uninitialized.  Do not use it. */
    }
}

void exampledebuglink(Dwarf_Debug dbg)
{
    int      res = 0;
    char    *debuglink_path = 0;
    unsigned char *crc = 0;
    char    *debuglink_fullpath = 0;
    unsigned debuglink_fullpath_strlen = 0;
    unsigned buildid_type = 0;
    char *   buildidowner_name = 0;
    unsigned char *buildid_itself = 0;
    unsigned buildid_length = 0;
    char **  paths = 0;
    unsigned paths_count = 0;
    Dwarf_Error error = 0;
    unsigned i = 0;

    /*  This is just an example if one knows
        of another place full-DWARF objects
        may be. "/usr/lib/debug" is automatically
        set. */
    res = dwarf_add_debuglink_global_path(dbg,
        "/some/path/debug",&error);
    if (res != DW_DLV_OK) {
        /*  Something is wrong, but we'll ignore that
            here. */
    }
    res = dwarf_gnu_debuglink(dbg,
        &debuglink_path,
        &crc,
        &debuglink_fullpath,
        &debuglink_fullpath_strlen,
        &buildid_type,
        &buildidowner_name,
        &buildid_itself,
        &buildid_length,
        &paths,
        &paths_count,
        &error);
    if (res == DW_DLV_ERROR) {
        /* Do something with the error */
        return;
    }
    if (res == DW_DLV_NO_ENTRY) {
        /*  No such sections as .note.gnu.build-id
            or .gnu_debuglink  */
        return;
    }
    if (debuglink_fullpath_strlen) {
        printf("debuglink     path: %s\n",debuglink_path);
        printf("crc length        : %u  crc: ",4);
        for (i = 0; i < 4;++i ) {
            printf("%02x",crc[i]);
        }
        printf("\n");
        printf("debuglink fullpath: %s\n",debuglink_fullpath);
    }
    if (buildid_length) {
        printf("buildid type      : %u\n",buildid_type);
        printf("Buildid owner     : %s\n",buildidowner_name);
        printf("buildid byte count: %u\n",buildid_length);
        printf(" ");
        /*   buildid_length should be 20. */
        for (i = 0; i < buildid_length;++i) {
            printf("%02x",buildid_itself[i]);
        }
        printf("\n");
    }
    printf("Possible paths count %u\n",paths_count);
    for ( ; i < paths_count; ++i ){
        printf("%2u: %s\n",i,paths[i]);
    }
    free(debuglink_fullpath);
    free(paths);
    return;
}
int example_raw_rnglist(Dwarf_Debug dbg,Dwarf_Error *error)
{
    Dwarf_Unsigned count = 0;
    int res = 0;
    Dwarf_Unsigned i = 0;

    res = dwarf_load_rnglists(dbg,&count,error);
    if (res != DW_DLV_OK) {
        return res;
    }
    for (i =0  ; i < count ; ++i) {
        Dwarf_Unsigned header_offset = 0;
        Dwarf_Small   offset_size = 0;
        Dwarf_Small   extension_size = 0;
        unsigned      version = 0; /* 5 */
        Dwarf_Small   address_size = 0;
        Dwarf_Small   segment_selector_size = 0;
        Dwarf_Unsigned offset_entry_count = 0;
        Dwarf_Unsigned offset_of_offset_array = 0;
        Dwarf_Unsigned offset_of_first_rangeentry = 0;
        Dwarf_Unsigned offset_past_last_rangeentry = 0;

        res = dwarf_get_rnglist_context_basics(dbg,i,
            &header_offset,&offset_size,&extension_size,
            &version,&address_size,&segment_selector_size,
            &offset_entry_count,&offset_of_offset_array,
            &offset_of_first_rangeentry,
            &offset_past_last_rangeentry,error);
        if (res != DW_DLV_OK) {
            return res;
        }
        {
            Dwarf_Unsigned e = 0;
            unsigned colmax = 4;
            unsigned col = 0;
            Dwarf_Unsigned global_offset_of_value = 0;

            for ( ; e < offset_entry_count; ++e) {
                Dwarf_Unsigned value = 0;
                int resc = 0;

                resc = dwarf_get_rnglist_offset_index_value(dbg,
                    i,e,&value,
                    &global_offset_of_value,error);
                if (resc != DW_DLV_OK) {
                    return resc;
                }
                /*  Do something */
                col++;
                if (col == colmax) {
                    col = 0;
                }
            }
        }
        {
            Dwarf_Unsigned curoffset = offset_of_first_rangeentry;
            Dwarf_Unsigned endoffset = offset_past_last_rangeentry;
            int rese = 0;
            Dwarf_Unsigned ct = 0;

            for ( ; curoffset < endoffset; ++ct ) {
                unsigned entrylen = 0;
                unsigned code = 0;
                Dwarf_Unsigned v1 = 0;
                Dwarf_Unsigned v2 = 0;
                rese = dwarf_get_rnglist_rle(dbg,i,
                    curoffset,endoffset,
                    &entrylen,
                    &code,&v1,&v2,error);
                if (rese != DW_DLV_OK) {
                    return rese;
                }
                /*  Do something with the values */
                curoffset += entrylen;
                if (curoffset > endoffset) {
                    return DW_DLV_ERROR;
                }
            }
        }
    }
    return DW_DLV_OK;
}
int example_rnglist_for_attribute(Dwarf_Attribute attr,
    Dwarf_Unsigned attrvalue,Dwarf_Error *error)
{
    /*  attrvalue must be the DW_AT_ranges
        DW_FORM_rnglistx or DW_FORM_sec_offset value
        extracted from attr. */
    int res = 0;
    Dwarf_Half theform = 0;
    Dwarf_Unsigned    entries_count;
    Dwarf_Unsigned    global_offset_of_rle_set;
    Dwarf_Rnglists_Head rnglhead = 0;
    Dwarf_Unsigned i = 0;

    res = dwarf_rnglists_get_rle_head(attr,
        theform,
        attrvalue,
        &rnglhead,
        &entries_count,
        &global_offset_of_rle_set,
        error);
    if (res != DW_DLV_OK) {
        return res;
    }
    for (i = 0; i < entries_count; ++i) {
        unsigned entrylen     = 0;
        unsigned code         = 0;
        Dwarf_Unsigned rawlowpc  = 0;
        Dwarf_Unsigned rawhighpc = 0;
        Dwarf_Bool debug_addr_unavailable = FALSE;
        Dwarf_Unsigned lowpc  = 0;
        Dwarf_Unsigned highpc = 0;

        /*  Actual addresses are most likely what one
            wants to know, not the lengths/offsets
            recorded in .debug_rnglists. */
        res = dwarf_get_rnglists_entry_fields_a(rnglhead,
            i,&entrylen,&code,
            &rawlowpc,&rawhighpc,
            &debug_addr_unavailable,
            &lowpc,&highpc,error);
        if (res != DW_DLV_OK) {
            dwarf_dealloc_rnglists_head(rnglhead);
            return res;
        }
        if (code == DW_RLE_end_of_list) {
            /* we are done */
            break;
        }
        if (code == DW_RLE_base_addressx ||
            code == DW_RLE_base_address) {
            /*  We do not need to use these, they
                have been accounted for already. */
            continue;
        }
        if (debug_addr_unavailable) {
            /* lowpc and highpc are not real addresses */
            continue;
        }
        /*  Here do something with lowpc and highpc, these
            are real addresses */
    }
    dwarf_dealloc_rnglists_head(rnglhead);
    return DW_DLV_OK;
}
