/*
** keval.c
** klisp eval function
** See Copyright Notice in klisp.h
*/

#include "klisp.h"
#include "kstate.h"
#include "kobject.h"
#include "kpair.h"
#include "kenvironment.h"
#include "kcontinuation.h"
#include "kerror.h"

/*
** Eval helpers 
*/
void eval_ls_cfn(klisp_State *K, TValue *xparams, TValue obj)
{
    /*
    ** xparams[0]: this argument list pair
    ** xparams[1]: dynamic environment
    ** xparams[2]: first-cycle-pair/NIL 
    ** xparams[3]: combiner
    */
    TValue apair = xparams[0];
    TValue rest = kcdr(apair);
    TValue env = xparams[1];
    TValue tail = xparams[2];
    TValue combiner = xparams[3];

    /* save the result of last evaluation and continue with next pair */
    kset_car(apair, obj); 
    if (ttisnil(rest)) {
	/* argument evaluation complete */
	/* this is necessary to recreate the cycle in operand list */
	kset_cdr(apair, tail);
	kapply_cc(K, combiner);
    } else {
	/* more arguments need to be evaluated */
	/* GC: all objects are rooted at this point */
	TValue new_cont = kmake_continuation(K, kget_cc(K), &eval_ls_cfn, 4, 
					     rest, env, tail, combiner);
	kset_cc(K, new_cont);
	ktail_eval(K, kcar(rest), env);
    }
}

/* TODO: move this to another file, to use it elsewhere */
inline void clear_ls_marks(TValue ls)
{
    while (ttispair(ls) && kis_marked(ls)) {
	kunmark(ls);
	ls = kcdr(ls);
    }
}

/* operands should be a pair, and should be rooted (GC) */
inline TValue make_arg_ls(klisp_State *K, TValue operands, TValue *tail)
{
    TValue arg_ls = kcons(K, kcar(operands), KNIL);
    krooted_tvs_push(K, arg_ls); /* root the constructed list */

    TValue last_pair = arg_ls;
    kset_mark(operands, last_pair);
    TValue rem_op = kcdr(operands);
    
    while(ttispair(rem_op) && kis_unmarked(rem_op)) {
	TValue new_pair = kcons(K, kcar(rem_op), KNIL);
	kset_mark(rem_op, new_pair);
	kset_cdr(last_pair, new_pair);
	last_pair = new_pair;
	rem_op = kcdr(rem_op);
    }
    
    krooted_tvs_pop(K);

    if (ttispair(rem_op)) {
	/* cyclical list */
	*tail = kget_mark(rem_op);
    } else if (ttisnil(rem_op)) {
	*tail = KNIL;
    } else {
	clear_ls_marks(operands);
	klispE_throw_simple(K, "Not a list in applicative combination");
	return KINERT;
    }
    clear_ls_marks(operands);
    return arg_ls;
}

void combine_cfn(klisp_State *K, TValue *xparams, TValue obj)
{
    /* 
    ** xparams[0]: operand list
    ** xparams[1]: dynamic environment
    ** xparams[2]: original_obj_with_si
    */
    TValue operands = xparams[0];
    TValue env = xparams[1];
    TValue si = xparams[2];

    switch(ttype(obj)) {
    case K_TAPPLICATIVE: {
	if (ttisnil(operands)) {
	    /* no arguments => no evaluation, just call the operative */
	    /* NOTE: the while is needed because it may be multiply wrapped */
	    while(ttisapplicative(obj))
		obj = tv2app(obj)->underlying;
	    ktail_call_si(K, obj, operands, env, si);
	} else if (ttispair(operands)) {
	    /* make a copy of the operands (for storing arguments) */
	    TValue tail;
	    TValue arg_ls = make_arg_ls(K, operands, &tail);
	    krooted_tvs_push(K, arg_ls);
	    TValue comb_cont = kmake_continuation(K, kget_cc(K), combine_cfn, 
						  3, arg_ls, env, si);

	    krooted_tvs_pop(K); /* already in cont */
	    krooted_tvs_push(K, comb_cont);
	    TValue els_cont = 
		kmake_continuation(K, comb_cont, eval_ls_cfn, 4, arg_ls, env, 
				   tail, tv2app(obj)->underlying);
	    kset_cc(K, els_cont);
	    krooted_tvs_pop(K);
	    ktail_eval(K, kcar(arg_ls), env);
	} else {
	    klispE_throw_simple(K, "Not a list in applicative combination");
	    return;
	}
    }
    case K_TOPERATIVE:
	ktail_call_si(K, obj, operands, env, si);
    default:
	klispE_throw_simple(K, "Not a combiner in combiner position");
	return;
    }
}

/* the underlying function of the eval operative */
void keval_ofn(klisp_State *K, TValue *xparams, TValue obj, TValue env)
{
    UNUSED(xparams);

    switch(ttype(obj)) {
    case K_TPAIR: {
	TValue new_cont = 
	    kmake_continuation(K, kget_cc(K), combine_cfn, 3, kcdr(obj), 
			       env, ktry_get_si(K, obj));
	kset_cc(K, new_cont);
	ktail_eval(K, kcar(obj), env);
	break;
    }
    case K_TSYMBOL:
	/* error handling happens in kget_binding */
	kapply_cc(K, kget_binding(K, env, obj));
	break;
    default:
	kapply_cc(K, obj);
    }
}


