/*
** kgcontrol.c
** Control features for the ground environment
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
#include "kcontinuation.h"
#include "kerror.h"

#include "kghelpers.h"
#include "kgcontrol.h"

/* Continuations */
void do_select_clause(klisp_State *K);
void do_cond(klisp_State *K);
void do_for_each(klisp_State *K);

/* 4.5.1 inert? */
/* uses typep */

/* 4.5.2 $if */

/*  ASK JOHN: both clauses should probably be copied (copy-es-immutable) */
void Sif(klisp_State *K)
{
    TValue *xparams = K->next_xparams;
    TValue ptree = K->next_value;
    TValue denv = K->next_env;
    klisp_assert(ttisenvironment(K->next_env));
    (void) denv;
    (void) xparams;

    bind_3p(K, ptree, test, cons_c, alt_c);

    TValue new_cont = 
	kmake_continuation(K, kget_cc(K), do_select_clause, 
			   3, denv, cons_c, alt_c);
    /* 
    ** Mark as a bool checking cont, not necessary but avoids a continuation
    ** in the last evaluation in the common use of ($if ($or?/$and? ...) ...) 
    */
    kset_bool_check_cont(new_cont);
    kset_cc(K, new_cont);
    ktail_eval(K, test, denv);
}

void do_select_clause(klisp_State *K)
{
    TValue *xparams = K->next_xparams;
    TValue obj = K->next_value;
    klisp_assert(ttisnil(K->next_env));
    /*
    ** xparams[0]: dynamic env
    ** xparams[1]: consequent clause
    ** xparams[2]: alternative clause
    */
    if (ttisboolean(obj)) {
	TValue denv = xparams[0];
	TValue clause = bvalue(obj)? xparams[1] : xparams[2];
	ktail_eval(K, clause, denv);
    } else {
	klispE_throw_simple(K, "test is not a boolean");
	return;
    }
}

/* 5.1.1 $sequence */
void Ssequence(klisp_State *K)
{
    TValue *xparams = K->next_xparams;
    TValue ptree = K->next_value;
    TValue denv = K->next_env;
    klisp_assert(ttisenvironment(K->next_env));
    UNUSED(xparams);

    if (ttisnil(ptree)) {
	kapply_cc(K, KINERT);
    } else {
	/* the list of instructions is copied to avoid mutation */
	/* MAYBE: copy the evaluation structure, ASK John */
	TValue ls = check_copy_list(K, ptree, false, NULL, NULL);
	/* this is needed because seq continuation doesn't check for 
	   nil sequence */
	/* TODO this could be at least in an inlineable function to
	   allow used from $lambda, $vau, $let family, load, etc */
	TValue tail = kcdr(ls);
	if (ttispair(tail)) {
	    krooted_tvs_push(K, ls);
	    TValue new_cont = kmake_continuation(K, kget_cc(K), do_seq, 2, 
						 tail, denv);
	    kset_cc(K, new_cont);
#if KTRACK_SI
	    /* put the source info of the list including the element
	       that we are about to evaluate */
	    kset_source_info(K, new_cont, ktry_get_si(K, ls));
#endif
	    krooted_tvs_pop(K);
	} 
	ktail_eval(K, kcar(ls), denv);
    }
}


/* Helpers for cond */

/*
** Check the clauses structure.
** Each should be a list of at least 1 element.
** Return both a copied list of tests (only list structure is copied)
** and a copied list of bodies (only list structure is copied, see comment
** on $sequence, cf. $let, $vau and $lambda)
** Throw errors if any of the above mentioned checks fail.
*/
/* GC: assumes clauses is rooted, uses dummy 1 & 2 */
TValue split_check_cond_clauses(klisp_State *K, TValue clauses, 
				TValue *bodies)
{
    TValue last_car_pair = kget_dummy1(K);
    TValue last_cdr_pair = kget_dummy2(K);

    TValue tail = clauses;
    int32_t count = 0;

    while(ttispair(tail) && !kis_marked(tail)) {
	count++;
	TValue first = kcar(tail);
	if (!ttispair(first)) {
	    unmark_list(K, clauses);
	    klispE_throw_simple(K, "bad structure in clauses");
	    return KNIL;
	}
	
	TValue new_car = kcons(K, kcar(first), KNIL);
	kset_cdr(last_car_pair, new_car);
	last_car_pair = new_car;
	/* bodies have to be checked later */
	TValue new_cdr = kcons(K, kcdr(first), KNIL);
	kset_cdr(last_cdr_pair, new_cdr);
	last_cdr_pair = new_cdr;

	kset_mark(tail, kcons(K, new_car, new_cdr));
	tail = kcdr(tail);
    }

    /* complete the cycles before unmarking */
    if (ttispair(tail)) {
	TValue mark = kget_mark(tail);
	kset_cdr(last_car_pair, kcar(mark));
	kset_cdr(last_cdr_pair, kcdr(mark));
    }

    unmark_list(K, clauses);

    if (!ttispair(tail) && !ttisnil(tail)) {
	klispE_throw_simple(K, "expected list (clauses)");
	return KNIL;
    } else {
	/* 
	   check all the bodies (should be lists), and
	   make a copy of the list structure.
	   couldn't be done before because this uses
	   marks, count is used because it may be a cyclic list
	*/
	tail = kget_dummy2_tail(K);
	while(count--) {
	    TValue first = kcar(tail);
	    /* this uses dummy3 */
	    TValue copy = check_copy_list(K, first, false, NULL, NULL);
	    kset_car(tail, copy);
	    tail = kcdr(tail);
	}

	*bodies = kcutoff_dummy2(K);
	return  kcutoff_dummy1(K);
    }
}

/* Helper for the $cond continuation */
void do_cond(klisp_State *K)
{
    TValue *xparams = K->next_xparams;
    TValue obj = K->next_value;
    klisp_assert(ttisnil(K->next_env));
    /* 
    ** xparams[0]: the body corresponding to obj
    ** xparams[1]: remaining tests
    ** xparams[2]: remaining bodies
    ** xparams[3]: dynamic environment
    */
    TValue this_body = xparams[0];
    TValue tests = xparams[1];
    TValue bodies = xparams[2];
    TValue denv = xparams[3];

    if (!ttisboolean(obj)) {
	klispE_throw_simple(K, "test evaluated to a non boolean value");
	return;
    } else if (bvalue(obj)) {
	if (ttisnil(this_body)) {
	    kapply_cc(K, KINERT);
	} else {
	    TValue tail = kcdr(this_body);
	    if (ttispair(tail)) {
		TValue new_cont = kmake_continuation(K, kget_cc(K), do_seq, 2, 
						     tail, denv);
		kset_cc(K, new_cont);
#if KTRACK_SI
		/* put the source info of the list including the element
		   that we are about to evaluate */
		kset_source_info(K, new_cont, ktry_get_si(K, this_body));
#endif
	    }
	    ktail_eval(K, kcar(this_body), denv);
	}
    } else {
	/* check next clause if there is any*/
	if (ttisnil(tests)) {
	    kapply_cc(K, KINERT);
	} else {
	    TValue new_cont = 
		kmake_continuation(K, kget_cc(K), do_cond, 4,
				   kcar(bodies), kcdr(tests), kcdr(bodies), 
				   denv);
	    /* 
	    ** Mark as a bool checking cont, not necessary but avoids a 
	    ** continuation in the last evaluation in the common use of 
	    ** ($cond ... (($or?/$and? ...) ...) ...) 
	    */
	    kset_bool_check_cont(new_cont);
	    kset_cc(K, new_cont);
#if KTRACK_SI
	    /* put the source info of the list including the element
	       that we are about to evaluate */
	    kset_source_info(K, new_cont, ktry_get_si(K, tests));
#endif
	    ktail_eval(K, kcar(tests), denv);
	}
    }
}

/* 5.6.1 $cond */
void Scond(klisp_State *K)
{
    TValue *xparams = K->next_xparams;
    TValue ptree = K->next_value;
    TValue denv = K->next_env;
    klisp_assert(ttisenvironment(K->next_env));
    (void) xparams;

    TValue bodies;
    TValue tests = split_check_cond_clauses(K, ptree, &bodies);
    krooted_tvs_push(K, tests);
    krooted_tvs_push(K, bodies);
    
    TValue obj;
    if (ttisnil(tests)) {
	obj = KINERT;
    } else {
	/* pass a dummy body and a #f to the $cond continuation to 
	   avoid code repetition here */
	TValue new_cont = 
	    kmake_continuation(K, kget_cc(K), do_cond, 4, 
			       KNIL, tests, bodies, denv);
	/* there is no need to mark this continuation with bool check
	   because it is just a dummy, no evaluation happens in its
	   dynamic extent, no need for source info either */
	kset_cc(K, new_cont);
	obj = KFALSE; 
    }

    krooted_tvs_pop(K);
    krooted_tvs_pop(K);
    kapply_cc(K, obj);
}

/* Helper continuation for for-each */
void do_for_each(klisp_State *K)
{
    TValue *xparams = K->next_xparams;
    TValue obj = K->next_value;
    klisp_assert(ttisnil(K->next_env));
    /*
    ** xparams[0]: app
    ** xparams[1]: rem-ls
    ** xparams[2]: n
    ** xparams[3]: denv
    */
    TValue app = xparams[0];
    TValue ls = xparams[1];
    int32_t n = ivalue(xparams[2]);
    TValue denv = xparams[3];

    /* the resulting value is just ignored */
    UNUSED(obj);

    if (n == 0) {
        /* return inert as the final result to for-each */
	kapply_cc(K, KINERT);
    } else {
	/* copy the ptree to avoid problems with mutation */
	/* XXX: no check necessary, could just use copy_list if there
	 was such a procedure */
	TValue first_ptree = check_copy_list(K, kcar(ls), false, NULL, NULL);
	krooted_tvs_push(K, first_ptree);
	ls = kcdr(ls);
	n = n-1;

	/* have to unwrap the applicative to avoid extra evaluation of first */
	TValue new_expr = kcons(K, kunwrap(app), first_ptree);
	TValue new_cont = 
	    kmake_continuation(K, kget_cc(K), do_for_each, 4, 
			       app, ls, i2tv(n), denv);
	krooted_tvs_pop(K);
	kset_cc(K, new_cont);
	ktail_eval(K, new_expr, denv);
    }
}

/* 6.9.1 for-each */
void for_each(klisp_State *K)
{
    TValue *xparams = K->next_xparams;
    TValue ptree = K->next_value;
    TValue denv = K->next_env;
    klisp_assert(ttisenvironment(K->next_env));
    (void) xparams;

    bind_al1tp(K, ptree, "applicative", ttisapplicative, app, lss);
    
    if (ttisnil(lss)) {
	klispE_throw_simple(K, "no lists");
	return;
    }

    /* get the metrics of the ptree of each call to app and
       of the result list */
    int32_t app_pairs, app_apairs, app_cpairs;
    int32_t res_pairs, res_apairs, res_cpairs;

    map_for_each_get_metrics(K, lss, &app_apairs, &app_cpairs,
			     &res_apairs, &res_cpairs);
    app_pairs = app_apairs + app_cpairs;
    res_pairs = res_apairs + res_cpairs;

    /* create the list of parameters to app */
    lss = map_for_each_transpose(K, lss, app_apairs, app_cpairs, 
				 res_apairs, res_cpairs);

    krooted_tvs_push(K, lss);

    /* schedule all elements at once, the cycle is just ignored, this
       will also return #inert once done. */
    TValue new_cont = 
	kmake_continuation(K, kget_cc(K), do_for_each, 4, app, lss,
			   i2tv(res_pairs), denv);
    kset_cc(K, new_cont);
    krooted_tvs_pop(K);
    /* this will be a nop */
    kapply_cc(K, KINERT);
}

/* init ground */
void kinit_control_ground_env(klisp_State *K)
{
    TValue ground_env = K->ground_env;
    TValue symbol, value;

    /* 4.5.1 inert? */
    add_applicative(K, ground_env, "inert?", typep, 2, symbol, 
		    i2tv(K_TINERT));
    /* 4.5.2 $if */
    add_operative(K, ground_env, "$if", Sif, 0);
    /* 5.1.1 $sequence */
    add_operative(K, ground_env, "$sequence", Ssequence, 0);
    /* 5.6.1 $cond */
    add_operative(K, ground_env, "$cond", Scond, 0);
    /* 6.9.1 for-each */
    add_applicative(K, ground_env, "for-each", for_each, 0);
}

/* init continuation names */
void kinit_control_cont_names(klisp_State *K)
{
    Table *t = tv2table(K->cont_name_table);

    add_cont_name(K, t, do_select_clause, "select-clause");
    add_cont_name(K, t, do_cond, "eval-cond-list");
    add_cont_name(K, t, do_for_each, "for-each");
}
