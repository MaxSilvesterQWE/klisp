/*
** kgenv_mut.c
** Environment mutation features for the ground environment
** See Copyright Notice in klisp.h
*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include "kstate.h"
#include "kobject.h"
#include "kpair.h"
#include "kenvironment.h"
#include "kcontinuation.h"
#include "ksymbol.h"
#include "kerror.h"

#include "kghelpers.h"
#include "kgenv_mut.h"

/* 4.9.1 $define! */
void SdefineB(klisp_State *K, TValue *xparams, TValue ptree, TValue denv)
{
    /*
    ** xparams[0] = define symbol
    */
    bind_2p(K, "$define!", ptree, dptree, expr);
    
    TValue def_sym = xparams[0];

    dptree = check_copy_ptree(K, "$define!", dptree, KIGNORE);
	
    TValue new_cont = kmake_continuation(K, kget_cc(K), KNIL, KNIL,
					 do_match, 3, dptree, denv, 
					 def_sym);
    kset_cc(K, new_cont);
    ktail_eval(K, expr, denv);
}

/* helper */
void do_match(klisp_State *K, TValue *xparams, TValue obj)
{
    /* 
    ** xparams[0]: ptree
    ** xparams[1]: dynamic environment
    ** xparams[2]: combiner symbol
    */
    TValue ptree = xparams[0];
    TValue env = xparams[1];
    char *name = ksymbol_buf(xparams[2]);

    match(K, name, env, ptree, obj);
    kapply_cc(K, KINERT);
}

/* 6.8.1 $set! */
void SsetB(klisp_State *K, TValue *xparams, TValue ptree, TValue denv)
{
    UNUSED(denv);

    TValue sname = xparams[0];

    bind_3p(K, "$set!", ptree, env_exp, raw_formals, eval_exp);

    TValue formals = check_copy_ptree(K, "$set!", raw_formals, KIGNORE);

    TValue new_cont = 
	kmake_continuation(K, kget_cc(K), KNIL, KNIL, do_set_eval_obj, 4, 
			   sname, formals, eval_exp, denv);
    kset_cc(K, new_cont);
    ktail_eval(K, env_exp, denv);
}

/* Helpers for $set! */
void do_set_eval_obj(klisp_State *K, TValue *xparams, TValue obj)
{
    /* 
    ** xparams[0]: name as symbol
    ** xparams[1]: ptree
    ** xparams[2]: expression to be eval'ed
    ** xparams[3]: dynamic environment
    */
    TValue sname = xparams[0];
    TValue formals = xparams[1];
    TValue eval_exp = xparams[2];
    TValue denv = xparams[3];
    
    if (!ttisenvironment(obj)) {
	klispE_throw_extra(K, ksymbol_buf(sname), ": bad type from first "
			   "operand evaluation (expected environment)");
	return;
    } else {
	TValue env = obj;

	TValue new_cont = 
	    kmake_continuation(K, kget_cc(K), KNIL, KNIL, do_match, 3, 
			       formals, env, sname);
	kset_cc(K, new_cont);
	ktail_eval(K, eval_exp, denv);
    }
}

/* Helpers for $provide! & $import! */

inline void unmark_maybe_symbol_list(klisp_State *K, TValue ls)
{
    UNUSED(K);
    while(ttispair(ls) && kis_marked(ls)) {
	TValue first = kcar(ls);
	if (ttissymbol(first))
	    kunmark(first);
	kunmark(ls);
	ls = kcdr(ls);
    }
}

/* 
** Check that obj is a finite list of symbols with no duplicates and
** returns a copy of the list (cf. check_copy_ptree)
*/
TValue check_copy_symbol_list(klisp_State *K, char *name, TValue obj)
{
    TValue tail = obj;
    bool type_errorp = false;
    bool repeated_errorp = false;
    TValue dummy = kcons(K, KNIL, KNIL);
    TValue last_pair = dummy;

    while(ttispair(tail) && !kis_marked(tail)) {
	/* even if there is a type error continue checking the structure */
	TValue first = kcar(tail);
	if (ksymbolp(first)) {
	    repeated_errorp |= kis_marked(first);
	    kmark(first);
	} else {
	    type_errorp = true;
	}
	kmark(tail);

	TValue new_pair = kcons(K, first, KNIL);
	kset_cdr(last_pair, new_pair);
	last_pair = new_pair;

	tail = kcdr(tail);
    }
    unmark_maybe_symbol_list(K, obj);

    if (!ttisnil(tail)) {
	klispE_throw_extra(K, name, ": expected finite list"); 
	return KNIL;
    } else if (type_errorp) {
	/* TODO put type name too */
	klispE_throw_extra(K, name , ": bad operand type (expected list of "
			   "symbols)"); 
	return KNIL;
    } else if (repeated_errorp) {
	klispE_throw_extra(K, name , ": repeated symbols");
    }
    return kcdr(dummy);
}

void do_import(klisp_State *K, TValue *xparams, TValue obj)
{
    /* 
    ** xparams[0]: name as symbol
    ** xparams[1]: symbols
    ** xparams[2]: dynamic environment
    */
    TValue sname = xparams[0];
    TValue symbols = xparams[1];
    TValue denv = xparams[2];
    
    if (!ttisenvironment(obj)) {
	klispE_throw_extra(K, ksymbol_buf(sname), ": bad type from first "
		     "operand evaluation (expected environment)");
	return;
    } else {
	TValue env = obj;
	TValue new_cont = 
	    kmake_continuation(K, kget_cc(K), KNIL, KNIL, do_match, 3, 
			       symbols, denv, sname);
	kset_cc(K, new_cont);
	ktail_eval(K, kcons(K, K->list_app, symbols), env);
    }
}

/* 6.8.2 $provide! */
/* TODO */


/* 6.8.3 $import! */
void SimportB(klisp_State *K, TValue *xparams, TValue ptree, TValue denv)
{
    /* ASK John: The report says that symbols can have repeated symbols
       and even be cyclical (cf $provide!) however this doesn't work
       in the derivation (that uses $set! and so needs a ptree, which are
       acyclical and with no repeated symbols).
       Here I follow $provide! and don't allow repeated symbols or cyclical
       lists, NOTE: is this restriction is to be lifted the code to copy the
       list should guarantee to contruct an acyclical list or do_import be
       changed to work with cyclical lists (at the moment it uses do_match
       that expects a ptree (although it works with repeated symbols provided
       they all have the same value, it loops indefinitely with cyclical ptree) 
    */
    /* 
    ** xparams[0]: name as symbol
    */
    TValue sname = xparams[0];
    char *name = ksymbol_buf(sname);

    bind_al1p(K, name, ptree, env_expr, symbols);

    symbols = check_copy_symbol_list(K, name, symbols);
    
    /* REFACTOR/ASK John: another way for this kind of operative would be
       to first eval the env expression and only then check the type
       of the symbol list (other operatives that could use this model to
       avoid copying are $set!, $define! & $binds?) */

    TValue new_cont =
	    kmake_continuation(K, kget_cc(K), KNIL, KNIL, do_import, 3, 
			       sname, symbols, denv);
    kset_cc(K, new_cont);
    ktail_eval(K, env_expr, denv);
}
