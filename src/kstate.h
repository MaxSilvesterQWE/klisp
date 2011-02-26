/*
** kstate.h
** klisp vm state
** See Copyright Notice in klisp.h
*/

/*
** SOURCE NOTE: The main structure is from Lua, but because (for now)
** klisp is single threaded, only global state is provided.
*/

#ifndef kstate_h
#define kstate_h

/* TEMP: for error signaling */
#include <assert.h>

#include <stdio.h>
#include <setjmp.h>

#include "kobject.h"
#include "klimits.h"
#include "klisp.h"
#include "ktoken.h"

/* XXX: for now, lines and column names are fixints */
/* MAYBE: this should be in tokenizer */
typedef struct {
    char *filename;
    int32_t tab_width;
    int32_t line;
    int32_t col;
    
    char *saved_filename;
    int32_t saved_line;
    int32_t saved_col;
} ksource_info_t;

struct klisp_State {
    TValue symbol_table;
    Continuation *curr_cont;

    TValue ret_value;     /* the value to be passed to the next function */

    klisp_Alloc frealloc;  /* function to reallocate memory */
    void *ud;         /* auxiliary data to `frealloc' */

    /* TODO: gc info */
    int32_t totalbytes;

    /* TEMP:error handling */
    jmp_buf error_jb;
    bool error_can_cont; /* can continue after error? */

     /* standard input and output */
     /* TODO: eventually these should be ports */
    FILE *curr_in;
    FILE *curr_out;
    char *filename_in;
    char *filename_out;
    
    /* tokenizer */
    /* WORKAROUND for repl */
    bool ktok_seen_eof;
    ksource_info_t ktok_source_info;
    /* tokenizer buffer */
    int32_t ktok_buffer_size;
    int32_t ktok_buffer_idx;
    char *ktok_buffer;

    /* reader */
    /* TODO: replace the list with a hashtable */
    TValue shared_dict;

    /* auxiliary stack */
    int32_t ssize; /* total size of array */
    int32_t stop; /* top of the stack (all elements are below this index) */
    TValue *sbuf;
 };

/* some size related macros */
#define KS_ISSIZE (1024)
#define KS_ITBSIZE (1024)
#define state_size() (sizeof(klisp_State))

 /*
 ** TEMP: for now use inlined functions, later check output in 
 **   different compilers and/or profile to see if it's worthy to 
 **   eliminate it, change it to compiler specific or replace it
 **   with defines 
 */

/*
** Stack functions 
*/
inline void ks_spush(klisp_State *K, TValue obj);
inline TValue ks_spop(klisp_State *K);
/* this is for DISCARDING stack pop (value isn't used, avoid warning) */ 
#define ks_sdpop(st_) (UNUSED(ks_spop(st_)))
inline TValue ks_sget(klisp_State *K);
inline void ks_sclear(klisp_State *K);
inline bool ks_sisempty(klisp_State *K);

/* some stack manipulation macros */
#define ks_ssize(st_) ((st_)->ssize)
#define ks_stop(st_) ((st_)->stop)
#define ks_sbuf(st_) ((st_)->sbuf)
#define ks_selem(st_, i_) ((ks_sbuf(st_))[i_])

inline void ks_spush(klisp_State *K, TValue obj)
{
    if (ks_stop(K) == ks_ssize(K)) { 
	/* TODO: try realloc */ 
	assert(0); 
    }
    ks_selem(K, ks_stop(K)) = obj;
    ++ks_stop(K);
}


inline TValue ks_spop(klisp_State *K)
{
    if (ks_ssize(K) != KS_ISSIZE && ks_stop(K) < (ks_ssize(K) / 4)) {
	/* NOTE: shrink can't fail */
	
	/* TODO: do realloc */
    }
    TValue obj = ks_selem(K, ks_stop(K) - 1);
    --ks_stop(K);
    return obj;
}

inline TValue ks_sget(klisp_State *K)
{
    return ks_selem(K, ks_stop(K) - 1);
}

inline void ks_sclear(klisp_State *K)
{
    if (ks_ssize(K) != KS_ISSIZE) {
	/* NOTE: shrink can't fail */
	/* TODO do realloc */ 
    }
    ks_ssize(K) = KS_ISSIZE; 
    ks_stop(K) = 0;
}

inline bool ks_sisempty(klisp_State *K)
{
    return ks_stop(K) == 0;
}

/*
** Tokenizer char buffer functions
*/

inline void ks_tbadd(klisp_State *K, char ch);
inline char *ks_tbget(klisp_State *K);
inline void ks_tbclear(klisp_State *K);
inline bool ks_tbisempty(klisp_State *K);

/* some buf manipulation macros */
#define ks_tbsize(st_) ((st_)->ktok_buffer_size)
#define ks_tbidx(st_) ((st_)->ktok_buffer_idx)
#define ks_tbuf(st_) ((st_)->ktok_buffer)
#define ks_tbelem(st_, i_) ((ks_tbuf(st_))[i_])

inline void ks_tbadd(klisp_State *K, char ch)
{
    if (ks_tbidx(K) == ks_tbsize(K)) { 
	/* TODO: try realloc */ 
	assert(0); 
    }
    ks_tbelem(K, ks_tbidx(K)) = ch;
    ++ks_tbidx(K);
}

inline char *ks_tbget(klisp_State *K)
{
    assert(ks_tbelem(K, ks_tbidx(K) - 1) == '\0');
    return ks_tbuf(K);
}

inline void ks_tbclear(klisp_State *K)
{
    if (ks_tbsize(K) != KS_ITBSIZE) {
	/* NOTE: shrink can't fail */
	/* TODO do realloc */ 
    }
    ks_tbsize(K) = KS_ITBSIZE; 
    ks_tbidx(K) = 0;
}

inline bool ks_tbisempty(klisp_State *K)
{
    return ks_tbidx(K) == 0;
}

#endif
