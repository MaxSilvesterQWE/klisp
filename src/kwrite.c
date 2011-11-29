/*
** kwrite.c
** Writer for the Kernel Programming Language
** See Copyright Notice in klisp.h
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <inttypes.h>
#include <string.h>

#include "kwrite.h"
#include "kobject.h"
#include "kinteger.h"
#include "krational.h"
#include "kreal.h"
#include "kpair.h"
#include "kstring.h"
#include "ksymbol.h"
#include "kstate.h"
#include "kerror.h"
#include "ktable.h"
#include "kport.h"
#include "kenvironment.h"
#include "kbytevector.h"
#include "kvector.h"

/*
** Stack for the write FSM
** 
*/
#define push_data(ks_, data_) (ks_spush(ks_, data_))
#define pop_data(ks_) (ks_sdpop(ks_))
#define get_data(ks_) (ks_sget(ks_))
#define data_is_empty(ks_) (ks_sisempty(ks_))

void kwrite_error(klisp_State *K, char *msg)
{
    /* all cleaning is done in throw 
       (stacks, shared_dict, rooted objs) */
    klispE_throw_simple(K, msg);
}

void kw_printf(klisp_State *K, const char *format, ...)
{
    va_list argp;
    TValue port = K->curr_port;

    if (ttisfport(port)) {
	FILE *file = kfport_file(port);
	va_start(argp, format);
	int ret = vfprintf(file, format, argp);
	va_end(argp);

	if (ret < 0) {
	    clearerr(file); /* clear error for next time */
	    kwrite_error(K, "error writing");
	    return;
	}
    } else if (ttismport(port)) {
	/* bytevector ports shouldn't write chars */
	klisp_assert(kport_is_textual(port));
	/* string port */
	uint32_t size;
	int written;
	uint32_t off = kmport_off(port);

	size = kstring_size(kmport_buf(port)) -
	    kmport_off(port) + 1;

	/* size is always at least 1 (for the '\0') */
	va_start(argp, format);
	written = vsnprintf(kstring_buf(kmport_buf(port)) + off, 
			    size, format, argp);
	va_end(argp);

	if (written >= size) { /* space wasn't enough */
	    kmport_resize_buffer(K, port, off + written);
	    /* size may be greater than off + written, so get again */
	    size = kstring_size(kmport_buf(port)) - off + 1;
	    va_start(argp, format);
	    written = vsnprintf(kstring_buf(kmport_buf(port)) + off, 
				size, format, argp);
	    va_end(argp);
	    if (written < 0 || written >= size) {
		/* shouldn't happen */
		kwrite_error(K, "error writing");
		return;
	    }
	}
	kmport_off(port) = off + written;
    } else {
	kwrite_error(K, "unknown port type");
	return;
    }
}

void kw_flush(klisp_State *K) { kwrite_flush_port(K, K->curr_port); }
    

/* TODO: check for return codes and throw error if necessary */
#define KDEFAULT_NUMBER_RADIX 10
void kw_print_bigint(klisp_State *K, TValue bigint)
{
    int32_t radix = KDEFAULT_NUMBER_RADIX;
    int32_t size = kbigint_print_size(bigint, radix); 
    krooted_tvs_push(K, bigint);
    /* here we are using 1 byte extra, because size already includes
       1 for the terminator, but better be safe than sorry */
    TValue buf_str = kstring_new_s(K, size);
    krooted_tvs_push(K, buf_str);

    char *buf = kstring_buf(buf_str);
    kbigint_print_string(K, bigint, radix, buf, size);
    kw_printf(K, "%s", buf);

    krooted_tvs_pop(K);
    krooted_tvs_pop(K);
}

void kw_print_bigrat(klisp_State *K, TValue bigrat)
{
    int32_t radix = KDEFAULT_NUMBER_RADIX;
    int32_t size = kbigrat_print_size(bigrat, radix); 
    krooted_tvs_push(K, bigrat);
    /* here we are using 1 byte extra, because size already includes
       1 for the terminator, but better be safe than sorry */
    TValue buf_str = kstring_new_s(K, size);
    krooted_tvs_push(K, buf_str);

    char *buf = kstring_buf(buf_str);
    kbigrat_print_string(K, bigrat, radix, buf, size);
    kw_printf(K, "%s", buf);

    krooted_tvs_pop(K);
    krooted_tvs_pop(K);
}

void kw_print_double(klisp_State *K, TValue tv_double)
{
    int32_t size = kdouble_print_size(tv_double); 
    krooted_tvs_push(K, tv_double);
    /* here we are using 1 byte extra, because size already includes
       1 for the terminator, but better be safe than sorry */
    TValue buf_str = kstring_new_s(K, size);
    krooted_tvs_push(K, buf_str);

    char *buf = kstring_buf(buf_str);
    kdouble_print_string(K, tv_double, buf, size);
    kw_printf(K, "%s", buf);

    krooted_tvs_pop(K);
    krooted_tvs_pop(K);
}

/*
** Helper for printing strings (correcly escapes backslashes and
** double quotes & prints embedded '\0's). It includes the surrounding
** double quotes.
*/
void kw_print_string(klisp_State *K, TValue str)
{
    int size = kstring_size(str);
    char *buf = kstring_buf(str);
    char *ptr = buf;
    int i = 0;

    if (!K->write_displayp)
	kw_printf(K, "\"");

    while (i < size) {
	/* find the longest printf-able substring to avoid calling printf
	 for every char */
	for (ptr = buf; 
	     i < size && *ptr != '\0' &&
		 (K->write_displayp || (*ptr != '\\' && *ptr != '"')); 
	     i++, ptr++)
	    ;

	/* NOTE: this work even if ptr == buf (which can only happen the 
	 first or last time) */
	char ch = *ptr;
	*ptr = '\0';
	kw_printf(K, "%s", buf);
	*ptr = ch;

	while(i < size && (*ptr == '\0' || 
        	(!K->write_displayp && (*ptr == '\\' || *ptr == '"')))) {
	    if (*ptr == '\0') {
		kw_printf(K, "%c", '\0'); /* this may not show in the terminal */
	    } else {
		kw_printf(K, "\\%c", *ptr);
	    }
	    i++;
	    ptr++;
	}
	buf = ptr;
    }
			
    if (!K->write_displayp)
	kw_printf(K, "\"");
}

/*
** Mark initialization and clearing
*/
/* GC: root is rooted */
void kw_clear_marks(klisp_State *K, TValue root)
{
    assert(ks_sisempty(K));
    push_data(K, root);

    while(!data_is_empty(K)) {
	TValue obj = get_data(K);
	pop_data(K);
	
	if (ttispair(obj)) {
	    if (kis_marked(obj)) {
		kunmark(obj);
		push_data(K, kcdr(obj));
		push_data(K, kcar(obj));
	    }
	} else if (ttisstring(obj) && (kis_marked(obj))) {
	    kunmark(obj);
	}
    }
    assert(ks_sisempty(K));
}

/*
** NOTE: 
**   - The objects that appear more than once are marked with a -1.
**   that way, the first time they are found in write, a shared def
**   token will be generated and the mark updated with the number;
**   from then on, the writer will generate a shared ref each time
**   it appears again.
**   - The objects that appear only once are marked with a #t to 
**   find repetitions and to allow unmarking after write
*/
/* GC: root is rooted */
void kw_set_initial_marks(klisp_State *K, TValue root)
{
    assert(ks_sisempty(K));
    push_data(K, root);
    
    while(!data_is_empty(K)) {
	TValue obj = get_data(K);
	pop_data(K);

	if (ttispair(obj)) {
	    if (kis_unmarked(obj)) {
		kmark(obj); /* this mark just means visited */
		push_data(K, kcdr(obj));
		push_data(K, kcar(obj));
	    } else {
		/* this mark means it will need a ref number */
		kset_mark(obj, i2tv(-1));
	    }
	} else if (ttisstring(obj)) {
	    if (kis_unmarked(obj)) {
		kmark(obj); /* this mark just means visited */
	    } else {
		/* this mark means it will need a ref number */
		kset_mark(obj, i2tv(-1));
	    }
	}
	/* all other types of object don't matter */
    }
    assert(ks_sisempty(K));
}

#if KTRACK_NAMES
void kw_print_name(klisp_State *K, TValue obj)
{
    kw_printf(K, ": %s", ksymbol_buf(kget_name(K, obj)));
}
#endif /* KTRACK_NAMES */

#if KTRACK_SI
/* Assumes obj has a si */
void kw_print_si(klisp_State *K, TValue obj)
{
    /* should be an improper list of 2 pairs,
       with a string and 2 fixints */
    TValue si = kget_source_info(K, obj);
    kw_printf(K, " @ ");
    /* this is a hack, would be better to change the interface of 
       kw_print_string */
    bool saved_displayp = K->write_displayp; 
    K->write_displayp = true; /* avoid "s and escapes */

    TValue str = kcar(si);
    int32_t row = ivalue(kcadr(si));
    int32_t col = ivalue(kcddr(si));
    kw_print_string(K, str);
    kw_printf(K, " (line: %d, col: %d)", row, col);

    K->write_displayp = saved_displayp;
}
#endif /* KTRACK_SI */

/* obj should be a continuation */
/* REFACTOR: move get cont name to a function somewhere else */
void kw_print_cont_type(klisp_State *K, TValue obj)
{
    bool saved_displayp = K->write_displayp; 
    K->write_displayp = true; /* avoid "s and escapes */

    Continuation *cont = tv2cont(obj);
    const TValue *node = klispH_get(tv2table(K->cont_name_table),
				    p2tv(cont->fn));

    char *type;
    if (node == &kfree) {
	type = "?";
    } else {
	klisp_assert(ttisstring(*node));
	type = kstring_buf(*node);
    }

    kw_printf(K, " (%s)", type);
    K->write_displayp = saved_displayp;
}

/*
** Writes all values except strings and pairs
*/
void kwrite_scalar(klisp_State *K, TValue obj)
{
    switch(ttype(obj)) {
    case K_TSTRING:
	/* shouldn't happen */
	klisp_assert(0);
	/* avoid warning */
	return;
    case K_TFIXINT:
	kw_printf(K, "%" PRId32, ivalue(obj));
	break;
    case K_TBIGINT:
	kw_print_bigint(K, obj);
	break;
    case K_TBIGRAT:
	kw_print_bigrat(K, obj);
	break;
    case K_TEINF:
	kw_printf(K, "#e%cinfinity", tv_equal(obj, KEPINF)? '+' : '-');
	break;
    case K_TIINF:
	kw_printf(K, "#i%cinfinity", tv_equal(obj, KIPINF)? '+' : '-');
	break;
    case K_TDOUBLE: {
	kw_print_double(K, obj);
	break;
    }
    case K_TRWNPV:
	/* ASK John/TEMP: until John tells me what should this be... */
	kw_printf(K, "#real");
	break;
    case K_TUNDEFINED:
	kw_printf(K, "#undefined");
	break;
    case K_TNIL:
	kw_printf(K, "()");
	break;
    case K_TCHAR: {
	if (K->write_displayp) {
	    kw_printf(K, "%c", chvalue(obj));
	} else {
	    char ch_buf[4];
	    char ch = chvalue(obj);
	    char *ch_ptr;

	    if (ch == '\n') {
		ch_ptr = "newline";
	    } else if (ch == ' ') {
		ch_ptr = "space";
	    } else {
		ch_buf[0] = ch;
		ch_buf[1] = '\0';
		ch_ptr = ch_buf;
	    }
	    kw_printf(K, "#\\%s", ch_ptr);
	}
	break;
    }
    case K_TBOOLEAN:
	kw_printf(K, "#%c", bvalue(obj)? 't' : 'f');
	break;
    case K_TSYMBOL:
	if (khas_ext_rep(obj)) {
	    /* TEMP: access symbol structure directly */
	    kw_printf(K, "%s", ksymbol_buf(obj));
	} else {
	    kw_printf(K, "#[symbol]");
	}
	break;
    case K_TINERT:
	kw_printf(K, "#inert");
	break;
    case K_TIGNORE:
	kw_printf(K, "#ignore");
	break;
/* unreadable objects */
    case K_TEOF:
	kw_printf(K, "#[eof]");
	break;
    case K_TENVIRONMENT:
	kw_printf(K, "#[environment");
	#if KTRACK_NAMES
	if (khas_name(obj)) {
	    kw_print_name(K, obj);
	}
	#endif
	kw_printf(K, "]");
	break;
    case K_TCONTINUATION:
	kw_printf(K, "#[continuation");
	#if KTRACK_NAMES
	if (khas_name(obj)) {
	    kw_print_name(K, obj);
	}
	#endif

	kw_print_cont_type(K, obj);

	#if KTRACK_SI
	if (khas_si(obj))
	    kw_print_si(K, obj);
	#endif
	kw_printf(K, "]");
	break;
    case K_TOPERATIVE:
	kw_printf(K, "#[operative");
	#if KTRACK_NAMES
	if (khas_name(obj)) {
	    kw_print_name(K, obj);
	}
	#endif
	#if KTRACK_SI
	if (khas_si(obj))
	    kw_print_si(K, obj);
	#endif
	kw_printf(K, "]");
	break;
    case K_TAPPLICATIVE:
	kw_printf(K, "#[applicative");
	#if KTRACK_NAMES
	if (khas_name(obj)) {
	    kw_print_name(K, obj);
	}
	#endif
	#if KTRACK_SI
	if (khas_si(obj))
	    kw_print_si(K, obj);
	#endif
	kw_printf(K, "]");
	break;
    case K_TENCAPSULATION:
	/* TODO try to get the name */
	kw_printf(K, "#[encapsulation]");
	break;
    case K_TPROMISE:
	/* TODO try to get the name */
	kw_printf(K, "#[promise]");
	break;
    case K_TFPORT:
	/* TODO try to get the filename */
	kw_printf(K, "#[%s %s file port", 
		  kport_is_binary(obj)? "binary" : "textual",
		  kport_is_input(obj)? "input" : "output");
	#if KTRACK_NAMES
	if (khas_name(obj)) {
	    kw_print_name(K, obj);
	}
	#endif
	kw_printf(K, "]");
	break;
    case K_TMPORT:
	kw_printf(K, "#[%s %s port", 
		  kport_is_binary(obj)? "bytevector" : "string",
		  kport_is_input(obj)? "input" : "output");
	#if KTRACK_NAMES
	if (khas_name(obj)) {
	    kw_print_name(K, obj);
	}
	#endif
	kw_printf(K, "]");
	break;
    case K_TERROR: {
	kw_printf(K, "#[error: ");

	/* TEMP for now show only msg */
	bool saved_displayp = K->write_displayp; 
	K->write_displayp = false; /* use "'s and escapes */
	kw_print_string(K, tv2error(obj)->msg);
	K->write_displayp = saved_displayp;

	kw_printf(K, "]");
	break;
    }
    case K_TBYTEVECTOR:
	kw_printf(K, "#[bytevector");
	#if KTRACK_NAMES
	if (khas_name(obj)) {
	    kw_print_name(K, obj);
	}
	#endif
	kw_printf(K, "]");
	break;
    case K_TVECTOR:
        kw_printf(K, "#[vector");
        #if KTRACK_NAMES
        if (khas_name(obj)) {
            kw_print_name(K, obj);
        }
        #endif
        kw_printf(K, "]");
        break;
    default:
	/* shouldn't happen */
	kwrite_error(K, "unknown object type");
	/* avoid warning */
	return;
    }
}


/* GC: obj is rooted */
void kwrite_fsm(klisp_State *K, TValue obj)
{
    /* NOTE: a fixint is more than enough for output */
    int32_t kw_shared_count = 0;

    assert(ks_sisempty(K));
    push_data(K, obj);

    bool middle_list = false;
    while (!data_is_empty(K)) {
	TValue obj = get_data(K);
	pop_data(K);

	if (middle_list) {
	    if (ttisnil(obj)) { /* end of list */
		kw_printf(K, ")");
		/* middle_list = true; */
	    } else if (ttispair(obj) && ttisboolean(kget_mark(obj))) {
		push_data(K, kcdr(obj));
		push_data(K, kcar(obj));
		kw_printf(K, " ");
		middle_list = false;
	    } else { /* improper list is the same as shared ref */
		kw_printf(K, " . ");
		push_data(K, KNIL);
		push_data(K, obj);
		middle_list = false;
	    }
	} else { /* if (middle_list) */
	    switch(ttype(obj)) {
	    case K_TPAIR: {
		TValue mark = kget_mark(obj);
		if (ttisboolean(mark)) { /* simple pair (only once) */
		    kw_printf(K, "(");
		    push_data(K, kcdr(obj));
		    push_data(K, kcar(obj));
		    middle_list = false;
		} else if (ivalue(mark) < 0) { /* pair with no assigned # */
		    /* TEMP: for now only fixints in shared refs */
		    assert(kw_shared_count >= 0);

		    kset_mark(obj, i2tv(kw_shared_count));
		    kw_printf(K, "#%" PRId32 "=(", kw_shared_count);
		    kw_shared_count++;
		    push_data(K, kcdr(obj));
		    push_data(K, kcar(obj));
		    middle_list = false;
		} else { /* pair with an assigned number */
		    kw_printf(K, "#%" PRId32 "#", ivalue(mark));
		    middle_list = true;
		}
		break;
	    }
	    case K_TSTRING: {
		if (kstring_emptyp(obj)) {
                    if (!K->write_displayp)
		        kw_printf(K, "\"\"");
		} else {
		    TValue mark = kget_mark(obj);
		    if (K->write_displayp || ttisboolean(mark)) { 
                        /* simple string (only once) or in display
			   (show all strings) */
			kw_print_string(K, obj);
		    } else if (ivalue(mark) < 0) { /* string with no assigned # */
			/* TEMP: for now only fixints in shared refs */
			assert(kw_shared_count >= 0);
			kset_mark(obj, i2tv(kw_shared_count));
			kw_printf(K, "#%" PRId32 "=", kw_shared_count);
			kw_shared_count++;
			kw_print_string(K, obj);
		    } else { /* string with an assigned number */
			kw_printf(K, "#%" PRId32 "#", ivalue(mark));
		    }
		}
		middle_list = true;
		break;
	    }
	    default:
		kwrite_scalar(K, obj);
		middle_list = true;
	    }
	}
    }

    assert(ks_sisempty(K));
}

/*
** Writer Main function
*/
void kwrite(klisp_State *K, TValue obj)
{
    /* GC: root obj */
    krooted_tvs_push(K, obj);

    kw_set_initial_marks(K, obj);
    kwrite_fsm(K, obj);
    kw_flush(K);
    kw_clear_marks(K, obj);

    krooted_tvs_pop(K);
}

/*
** This is the same as above but will not display
** shared tags (and will hang if there are cycles)
*/
void kwrite_simple(klisp_State *K, TValue obj)
{
    /* GC: root obj */
    krooted_tvs_push(K, obj);
    kwrite_fsm(K, obj);
    kw_flush(K);
    krooted_tvs_pop(K);
}

/*
** Writer Interface
*/
void kwrite_display_to_port(klisp_State *K, TValue port, TValue obj, 
			    bool displayp)
{
    klisp_assert(ttisport(port));
    klisp_assert(kport_is_output(port));
    klisp_assert(kport_is_open(port));
    klisp_assert(kport_is_textual(port));

    K->curr_port = port;
    K->write_displayp = displayp;
    kwrite(K, obj);
}

void kwrite_simple_to_port(klisp_State *K, TValue port, TValue obj)
{
    klisp_assert(ttisport(port));
    klisp_assert(kport_is_output(port));
    klisp_assert(kport_is_open(port));
    klisp_assert(kport_is_textual(port));

    K->curr_port = port;
    K->write_displayp = false;
    kwrite_simple(K, obj);
}

void kwrite_newline_to_port(klisp_State *K, TValue port)
{
    klisp_assert(ttisport(port));
    klisp_assert(kport_is_output(port));
    klisp_assert(kport_is_open(port));
    klisp_assert(kport_is_textual(port));
    K->curr_port = port; /* this isn't needed but all other 
			    i/o functions set it */
    kwrite_char_to_port(K, port, ch2tv('\n'));
}

void kwrite_char_to_port(klisp_State *K, TValue port, TValue ch)
{
    klisp_assert(ttisport(port));
    klisp_assert(kport_is_output(port));
    klisp_assert(kport_is_open(port));
    klisp_assert(kport_is_textual(port));
    K->curr_port = port; /* this isn't needed but all other 
			    i/o functions set it */

    if (ttisfport(port)) {
	FILE *file = kfport_file(port);
	int res = fputc(chvalue(ch), file);

	if (res == EOF) {
	    clearerr(file); /* clear error for next time */
	    kwrite_error(K, "error writing char");
	}
    } else if (ttismport(port)) {
	if (kport_is_binary(port)) {
	    /* bytebuffer port */
	    if (kmport_off(port) >= kbytevector_size(kmport_buf(port))) {
		kmport_resize_buffer(K, port, kmport_off(port) + 1);
	    }
	    kbytevector_buf(kmport_buf(port))[kmport_off(port)] = chvalue(ch);
	    ++kmport_off(port);
	} else {
	    /* string port */
	    if (kmport_off(port) >= kstring_size(kmport_buf(port))) {
		kmport_resize_buffer(K, port, kmport_off(port) + 1);
	    }
	    kstring_buf(kmport_buf(port))[kmport_off(port)] = chvalue(ch);
	    ++kmport_off(port);
	}
    } else {
	kwrite_error(K, "unknown port type");
	return;
    }
}

void kwrite_u8_to_port(klisp_State *K, TValue port, TValue u8)
{
    klisp_assert(ttisport(port));
    klisp_assert(kport_is_output(port));
    klisp_assert(kport_is_open(port));
    klisp_assert(kport_is_binary(port));
    K->curr_port = port; /* this isn't needed but all other 
			    i/o functions set it */
    if (ttisfport(port)) {
	FILE *file = kfport_file(port);
	int res = fputc(ivalue(u8), file);

	if (res == EOF) {
	    clearerr(file); /* clear error for next time */
	    kwrite_error(K, "error writing u8");
	}
    } else if (ttismport(port)) {
	if (kport_is_binary(port)) {
	    /* bytebuffer port */
	    if (kmport_off(port) >= kbytevector_size(kmport_buf(port))) {
		kmport_resize_buffer(K, port, kmport_off(port) + 1);
	    }
	    kbytevector_buf(kmport_buf(port))[kmport_off(port)] = 
		(uint8_t) ivalue(u8);
	    ++kmport_off(port);
	} else {
	    /* string port */
	    if (kmport_off(port) >= kstring_size(kmport_buf(port))) {
		kmport_resize_buffer(K, port, kmport_off(port) + 1);
	    }
	    kstring_buf(kmport_buf(port))[kmport_off(port)] = 
		(char) ivalue(u8);
	    ++kmport_off(port);
	}
    } else {
	kwrite_error(K, "unknown port type");
	return;
    }
}

void kwrite_flush_port(klisp_State *K, TValue port) 
{
    klisp_assert(ttisport(port));
    klisp_assert(kport_is_output(port));
    klisp_assert(kport_is_open(port));
    K->curr_port = port; /* this isn't needed but all other 
			    i/o functions set it */
    if (ttisfport(port)) { /* only necessary for file ports */
	FILE *file = kfport_file(port);
	klisp_assert(file);
	if ((fflush(file)) == EOF) {
	    clearerr(file); /* clear error for next time */
	    kwrite_error(K, "error writing");
	}
    }
}
