/*
** kground.h
** Bindings in the ground environment
** See Copyright Notice in klisp.h
*/

/* TODO: split in different files for each module */
#include "kstate.h"
#include "kobject.h"
#include "kground.h"
#include "kpair.h"
#include "kenvironment.h"
#include "kcontinuation.h"
#include "ksymbol.h"
#include "koperative.h"
#include "kapplicative.h"
#include "kerror.h"

/*
** Some helper macros and functions
*/

#define anytype(obj_) (true)

/*
** NOTE: these are intended to be used at the beginning of a function
**   they expand to more than one statement and may evaluate some of
**   their arguments more than once 
*/
#define bind_1p(K_, n_, ptree_, v_) \
    bind_1tp(K_, n_, ptree_, "any", anytype, v_)

#define bind_1tp(K_, n_, ptree_, tstr_, t_, v_)	\
    TValue v_; \
    if (!ttispair(ptree_) || !ttisnil(kcdr(ptree_))) { \
	klispE_throw(K_, n_ ": Bad ptree (expected one argument)"); \
	return; \
    } \
    v_ = kcar(ptree_); \
    if (!t_(v_)) { \
	klispE_throw(K_, n_ ": Bad type on first argument (expected " \
		     tstr_ ")");				     \
	return; \
    } 


#define bind_2p(K_, n_, ptree_, v1_, v2_)		\
    bind_2tp(K_, n_, ptree_, "any", anytype, v1_, "any", anytype, v2_)

#define bind_2tp(K_, n_, ptree_, tstr1_, t1_, v1_, \
		 tstr2, t2_, v2_)			\
    TValue v1_, v2_;					\
    if (!ttispair(ptree_) || !ttispair(kcdr(ptree_)) || \
	    !ttisnil(kcdr(kcdr(ptree_)))) {		\
	klispE_throw(K_, n_ ": Bad ptree (expected two arguments)"); \
	return; \
    } \
    v1_ = kcar(ptree_); \
    v2_ = kcar(kcdr(ptree_)); \
    if (!t1_(v1_)) { \
	klispE_throw(K_, n_ ": Bad type on first argument (expected " \
		     tstr1_ ")");				     \
	return; \
    } else if (!t2_(v2_)) { \
	klispE_throw(K_, n_ ": Bad type on second argument (expected " \
		     tstr1_ ")");				     \
	return; \
    }

/* TODO: add name and source info */
#define kmake_applicative(K_, fn_, i_, ...) \
    kwrap(K_, kmake_operative(K_, KNIL, KNIL, fn_, i_, ##__VA_ARGS__))


/*
** This section will roughly follow the report and will reference the
** section in which each symbol is defined
*/

/*
**
** 4 Core types and primitive features
**
*/

/*
** 4.1 Booleans
*/

/* 4.1.1 boolean? */
/* TEMP: for now it takes a single argument */
void booleanp(klisp_State *K, TValue *xparams, TValue ptree, TValue denv)
{
    (void) denv;
    (void) xparams;
    bind_1p(K, "boolean?", ptree, o);
    kapply_cc(K, b2tv(ttisboolean(o)));
}

/*
** 4.2 Equivalence under mutation
*/

/* 4.2.1 eq? */
/* TEMP: for now it takes only two argument */
void eqp(klisp_State *K, TValue *xparams, TValue ptree, TValue denv)
{
    (void) denv;
    (void) xparams;
    bind_2p(K, "eq?", ptree, o1, o2);
    /* TEMP: for now this is the same as 
       later it will change with numbers and immutable objects */
    kapply_cc(K, b2tv(tv_equal(o1, o2)));
}

/*
** 4.3 Equivalence up to mutation
*/

/* 4.3.1 equal? */
/* TEMP: for now it takes only two argument */
/* TODO */

/*
** 4.4 Symbols
*/

/* 4.4.1 symbol? */
void symbolp(klisp_State *K, TValue *xparams, TValue ptree, TValue denv)
{
    (void) denv;
    (void) xparams;
    bind_1p(K, "symbol?", ptree, o);
    kapply_cc(K, b2tv(ttissymbol(o)));
}

/*
** 4.5 Control
*/

/* 4.5.1 inert? */
void inertp(klisp_State *K, TValue *xparams, TValue ptree, TValue denv)
{
    (void) denv;
    (void) xparams;
    bind_1p(K, "inert?", ptree, o);
    kapply_cc(K, b2tv(ttisinert(o)));
}

/* 4.5.2 $if */
/* TODO:
void Sif(klisp_State *K, TValue *xparams, TValue ptree, TValue denv)
{
    (void) denv;
    (void) xparams;

    bind_3p(K, "boolean?", ptree, test, consc, altc);
    
    kapply_cc(K, b2tv(ttisboolean(o)));
}
*/

/*
** 4.6 Pairs and lists
*/

/* 4.6.1 pair? */
void pairp(klisp_State *K, TValue *xparams, TValue ptree, TValue denv)
{
    (void) denv;
    (void) xparams;
    bind_1p(K, "pair?", ptree, o);
    kapply_cc(K, b2tv(ttispair(o)));
}

/* 4.6.2 null? */
void nullp(klisp_State *K, TValue *xparams, TValue ptree, TValue denv)
{
    (void) denv;
    (void) xparams;
    bind_1p(K, "null?", ptree, o);
    kapply_cc(K, b2tv(ttisnil(o)));
}
    
/* 4.6.3 cons */
void cons(klisp_State *K, TValue *xparams, TValue ptree, TValue denv)
{
    (void) denv;
    (void) xparams;
    bind_2p(K, "cons", ptree, car, cdr);
    
    TValue new_pair = kcons(K, car, cdr);
    kapply_cc(K, new_pair);
}

/*
** 4.7 Pair mutation
*/

/* 4.7.1 set-car!, set-cdr! */
/* TODO: check if pair is immutable */
void set_carB(klisp_State *K, TValue *xparams, TValue ptree, TValue denv)
{
    (void) denv;
    (void) xparams;
    bind_2tp(K, "set-car!", ptree, "pair", ttispair, pair, 
	     "any", anytype, new_car);
    
    kset_car(pair, new_car);
    kapply_cc(K, KINERT);
}

void set_cdrB(klisp_State *K, TValue *xparams, TValue ptree, TValue denv)
{
    (void) denv;
    (void) xparams;
    bind_2tp(K, "set-cdr!", ptree, "pair", ttispair, pair, 
	     "any", anytype, new_cdr);
    
    kset_cdr(pair, new_cdr);
    kapply_cc(K, KINERT);
}

/* 4.7.2 copy-es-immutable */
/* TODO */

/*
** 4.8 Environments
*/

/* 4.8.1 environment? */
void environmentp(klisp_State *K, TValue *xparams, TValue ptree, TValue denv)
{
    (void) denv;
    (void) xparams;
    bind_1p(K, "environment?", ptree, o);
    kapply_cc(K, b2tv(ttisenvironment(o)));
}

/* 4.8.2 ignore? */
void ignorep(klisp_State *K, TValue *xparams, TValue ptree, TValue denv)
{
    (void) denv;
    (void) xparams;
    bind_1p(K, "ignore?", ptree, o);
    kapply_cc(K, b2tv(ttisignore(o)));
}

/* 4.8.3 eval */
/* TODO */

/* 4.8.4 make-environment */
/* TODO: let it accept any number of parameters */
void make_environment(klisp_State *K, TValue *xparams, TValue ptree, 
		      TValue denv)
{
    (void) denv;
    (void) xparams;
    TValue new_env;
    if (ttisnil(ptree)) {
	new_env = kmake_empty_environment(K);
	kapply_cc(K, new_env);
    } else if (ttispair(ptree) && ttisnil(kcdr(ptree))) {
	TValue parent = kcar(ptree);
	if (ttisenvironment(parent)) {
	    new_env = kmake_environment(K, parent);
	    kapply_cc(K, new_env);
	} else {
	    klispE_throw(K, "make-environment: Bad type on first "
			 "argument (expected environment)");
	    return;
	}
    } else {
	klispE_throw(K, "make-environment: Bad ptree (expected "
		     "zero or one argument");
	return;
    }
}

/*
** 4.9 Environment mutation
*/

/* helper */
void match(klisp_State *K, TValue *xparams, TValue obj);

/* 4.9.1 $define! */
/* TODO: allow general ptrees */
void SdefineB(klisp_State *K, TValue *xparams, TValue ptree, TValue denv)
{
    (void) xparams;
    bind_2p(K, "$define!", ptree, dptree, expr)

    /* TODO: allow general ptrees */
    if (!ttissymbol(dptree) && !ttisignore(dptree)) {
	klispE_throw(K, "$define!: Not a symbol or ignore");
	return;
    } else {
	TValue new_cont = kmake_continuation(K, kget_cc(K), KNIL, KNIL,
					     &match, 2, dptree, denv);
	kset_cc(K, new_cont);
	ktail_call(K, K->eval_op, expr, denv);
    }
}

/* helper */
void match(klisp_State *K, TValue *xparams, TValue obj)
{
    /* 
    ** tparams[0]: ptree
    ** tparams[1]: dynamic environment
    */
    TValue ptree = xparams[0];
    TValue env = xparams[1];

    /* TODO: allow general parameter trees */
    if (!ttisignore(ptree)) {
	kadd_binding(K, env, ptree, obj);
    }
    kapply_cc(K, KINERT);
}

/*
** 4.10 Combiners
*/

/* 4.10.1 operative? */
void operativep(klisp_State *K, TValue *xparams, TValue ptree, TValue denv)
{
    (void) denv;
    (void) xparams;
    bind_1p(K, "operative?", ptree, o);
    kapply_cc(K, b2tv(ttisoperative(o)));
}

/* 4.10.2 applicative? */
void applicativep(klisp_State *K, TValue *xparams, TValue ptree, TValue denv)
{
    (void) denv;
    (void) xparams;
    bind_1p(K, "applicative?", ptree, o);
    kapply_cc(K, b2tv(ttisapplicative(o)));
}

/* 4.10.3 $vau */
/* TODO */

/* 4.10.4 wrap */
void wrap(klisp_State *K, TValue *xparams, TValue ptree, TValue denv)
{
    (void) denv;
    (void) xparams;
    bind_1tp(K, "wrap", ptree, "combiner", ttiscombiner, comb);
    TValue new_app = kwrap(K, comb);
    kapply_cc(K, new_app);
}

/* 4.10.5 unwrap */
void unwrap(klisp_State *K, TValue *xparams, TValue ptree, TValue denv)
{
    (void) denv;
    (void) xparams;
    bind_1tp(K, "unwrap", ptree, "applicative", ttisapplicative, app);
    TValue underlying = kunwrap(K, app);
    kapply_cc(K, underlying);
}

/*
** This is called once to bind all symbols in the ground environment
*/
TValue kmake_ground_env(klisp_State *K)
{
    TValue ground_env = kmake_empty_environment(K);

    TValue symbol, value;

    /*
    ** This section will roughly follow the report and will reference the
    ** section in which each symbol is defined
    */

    /*
    **
    ** 4 Core types and primitive features
    **
    */

    /*
    ** 4.1 Booleans
    */

    /* 4.1.1 boolean? */
    symbol = ksymbol_new(K, "boolean?");
    value = kmake_applicative(K, booleanp, 0);
    kadd_binding(K, ground_env, symbol, value);

    /*
    ** 4.2 Equivalence under mutation
    */

    /* 4.2.1 eq? */
    symbol = ksymbol_new(K, "eq?");
    value = kmake_applicative(K, eqp, 0);
    kadd_binding(K, ground_env, symbol, value);

    /*
    ** 4.3 Equivalence up to mutation
    */

    /* 4.3.1 equal? */
    /* TODO */

    /*
    ** 4.4 Symbols
    */

    /* 4.4.1 symbol? */
    symbol = ksymbol_new(K, "symbol?");
    value = kmake_applicative(K, symbolp, 0);
    kadd_binding(K, ground_env, symbol, value);

    /*
    ** 4.5 Control
    */

    /* 4.5.1 inert? */
    symbol = ksymbol_new(K, "inert?");
    value = kmake_applicative(K, inertp, 0);
    kadd_binding(K, ground_env, symbol, value);

    /* 4.5.2 $if */
/* TODO:
    symbol = ksymbol_new(K, "$if");
    value = kmake_operative(K, KNIL, KNIL, Sif, 0);
    kadd_binding(K, ground_env, symbol, value);
*/

    /*
    ** 4.6 Pairs and lists
    */

    /* 4.6.1 pair? */
    symbol = ksymbol_new(K, "pair?");
    value = kmake_applicative(K, pairp, 0);
    kadd_binding(K, ground_env, symbol, value);

    /* 4.6.2 null? */
    symbol = ksymbol_new(K, "null?");
    value = kmake_applicative(K, nullp, 0);
    kadd_binding(K, ground_env, symbol, value);
    
    /* 4.6.3 cons */
    symbol = ksymbol_new(K, "cons");
    value = kmake_applicative(K, cons, 0);
    kadd_binding(K, ground_env, symbol, value);

    /*
    ** 4.7 Pair mutation
    */

    /* 4.7.1 set-car!, set-cdr! */
    symbol = ksymbol_new(K, "set-car!");
    value = kmake_applicative(K, set_carB, 0);
    kadd_binding(K, ground_env, symbol, value);
    symbol = ksymbol_new(K, "set-cdr!");
    value = kmake_applicative(K, set_cdrB, 0);
    kadd_binding(K, ground_env, symbol, value);

    /* 4.7.2 copy-es-immutable */
    /* TODO */

    /*
    ** 4.8 Environments
    */

    /* 4.8.1 environment? */
    symbol = ksymbol_new(K, "environment?");
    value = kmake_applicative(K, environmentp, 0);
    kadd_binding(K, ground_env, symbol, value);

    /* 4.8.2 ignore? */
    symbol = ksymbol_new(K, "ignore?");
    value = kmake_applicative(K, ignorep, 0);
    kadd_binding(K, ground_env, symbol, value);

    /* 4.8.3 eval */
    /* TODO */

    /* 4.8.4 make-environment */
    symbol = ksymbol_new(K, "make-environment");
    value = kmake_applicative(K, make_environment, 0);
    kadd_binding(K, ground_env, symbol, value);

    /*
    ** 4.9 Environment mutation
    */

    /* 4.9.1 $define! */
    symbol = ksymbol_new(K, "$define!");
    value = kmake_operative(K, KNIL, KNIL, SdefineB, 0);
    kadd_binding(K, ground_env, symbol, value);

    /*
    ** 4.10 Combiners
    */

    /* 4.10.1 operative? */
    symbol = ksymbol_new(K, "operative?");
    value = kmake_applicative(K, operativep, 0);
    kadd_binding(K, ground_env, symbol, value);

    /* 4.10.2 applicative? */
    symbol = ksymbol_new(K, "applicative?");
    value = kmake_applicative(K, applicativep, 0);
    kadd_binding(K, ground_env, symbol, value);

    /* 4.10.3 $vau */
    /* TODO */

    /* 4.10.4 wrap */
    symbol = ksymbol_new(K, "wrap");
    value = kmake_applicative(K, wrap, 0);
    kadd_binding(K, ground_env, symbol, value);

    /* 4.10.5 unwrap */
    symbol = ksymbol_new(K, "unwrap");
    value = kmake_applicative(K, unwrap, 0);
    kadd_binding(K, ground_env, symbol, value);

    return ground_env;
}
