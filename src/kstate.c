/*
** kstate.c
** klisp vm state
** See Copyright Notice in klisp.h
*/

/*
** SOURCE NOTE: this is mostly from Lua.
*/

#include <stddef.h>
#include <setjmp.h>

#include "klisp.h"
#include "kstate.h"
#include "kobject.h"
#include "kstring.h"
#include "kpair.h"
#include "kmem.h"
#include "keval.h"
#include "koperative.h"
#include "kground.h"


/*
** State creation and destruction
*/
klisp_State *klisp_newstate (klisp_Alloc f, void *ud) {
    klisp_State *K;
    void *k = (*f)(ud, NULL, 0, state_size());
    if (k == NULL) return NULL;
    void *s = (*f)(ud, NULL, 0, KS_ISSIZE * sizeof(TValue));
    if (s == NULL) { 
	(*f)(ud, k, state_size(), 0); 
	return NULL;
    }
    void *b = (*f)(ud, NULL, 0, KS_ITBSIZE);
    if (b == NULL) {
	(*f)(ud, k, state_size(), 0); 
	(*f)(ud, s, KS_ISSIZE * sizeof(TValue), 0); 
	return NULL;
    }

    K = (klisp_State *) k;

    K->symbol_table = KNIL;
    /* TODO: create a continuation */
    K->curr_cont = KNIL;

    K->next_func = NULL;
    K->next_value = KINERT;
    K->next_env = KNIL;
    K->next_xparams = NULL;

    K->eval_op = KINERT;
    K->ground_env = KINERT;

    K->frealloc = f;
    K->ud = ud;

    /* current input and output */
    K->curr_in = stdin;
    K->curr_out = stdout;
    K->filename_in = "*STDIN*";
    K->filename_out = "*STDOUT*";

    /* TODO: more gc info */
    K->totalbytes = KS_ISSIZE + state_size();

    /* TEMP: err */
    /* do nothing for now */

    /* initialize strings */
    /* Empty string */
    /* TODO: make it uncollectible */
    K->empty_string = kstring_new_empty(K);

    /* initialize tokenizer */

    /* WORKAROUND: for stdin line buffering & reading of EOF */
    K->ktok_seen_eof = false;

    ks_tbsize(K) = KS_ITBSIZE;
    ks_tbidx(K) = 0; /* buffer is empty */
    ks_tbuf(K) = (char *)b;

    /* Special Tokens */
    K->ktok_lparen = kcons(K, ch2tv('('), KNIL);
    K->ktok_rparen = kcons(K, ch2tv(')'), KNIL);
    K->ktok_dot = kcons(K, ch2tv('.'), KNIL);

    /* TEMP: For now just hardcode it to 8 spaces tab-stop */
    K->ktok_source_info.tab_width = 8;
    K->ktok_source_info.filename = "*STDIN*";
    ktok_init(K);
    ktok_reset_source_info(K);

    /* initialize reader */
    K->shared_dict = KNIL;

    /* initialize writer */

    /* initialize temp stack */
    K->ssize = KS_ISSIZE;
    K->stop = 0; /* stack is empty */
    K->sbuf = (TValue *)s;

    /* create the ground environment and the eval operative */
    K->eval_op = kmake_operative(K, KNIL, KNIL, keval_ofn, 0);
    K->ground_env = kmake_ground_env(K);

    return K;
}

void klisp_close (klisp_State *K)
{
    /* TODO: free memory for all objects */
    klispM_freemem(K, ks_sbuf(K), ks_ssize(K));
    klispM_freemem(K, ks_tbuf(K), ks_tbsize(K));
    /* NOTE: this needs to be done "by hand" */
    (*(K->frealloc))(K->ud, K, state_size(), 0);
}


void kcall_cont(klisp_State *K, TValue dst_cont, TValue obj)
{
    /* TODO: interceptions */
    Continuation *cont = tv2cont(dst_cont);
    K->next_func = cont->fn;
    K->next_value = obj;
    /* NOTE: this is needed to differentiate a return from a tail call */
    K->next_env = KNIL;
    K->next_xparams = cont->extra;
    K->curr_cont = cont->parent;

    longjmp(K->error_jb, 1);
}
