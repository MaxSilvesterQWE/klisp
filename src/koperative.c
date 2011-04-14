/*
** koperative.c
** Kernel Operatives
** See Copyright Notice in klisp.h
*/

#include <stdarg.h>

#include "koperative.h"
#include "kobject.h"
#include "kstate.h"
#include "kmem.h"
#include "kgc.h"

TValue kmake_operative(klisp_State *K, TValue name, TValue si, 
		       klisp_Ofunc fn, int32_t xcount, ...)
{
    va_list argp;
    Operative *new_op = (Operative *) 
	klispM_malloc(K, sizeof(Operative) + sizeof(TValue) * xcount);

    /* header + gc_fields */
    klispC_link(K, (GCObject *) new_op, K_TOPERATIVE, 0);

    /* operative specific fields */
    new_op->name = name;
    new_op->si = si;
    new_op->fn = fn;
    new_op->extra_size = xcount;

    va_start(argp, xcount);
    for (int i = 0; i < xcount; i++) {
	new_op->extra[i] = va_arg(argp, TValue);
    }
    va_end(argp);
    return gc2op(new_op);
}
