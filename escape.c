#include <stdio.h>
#include <string.h>
#include "util.h"
#include "symbol.h"
#include "absyn.h"
#include "escape.h"
#include "table.h"
#include "env.h"
#include "helper.h"

static void traverseDec(S_table env, int depth, A_dec d);
static void traverseVar(S_table env, int depth, A_var v);
static void traverseExp(S_table env, int depth, A_exp e)
{
	switch (e->kind) {
		case A_varExp: {
			traverseVar(env, depth, e->u.var);
			break;
		}
		case A_letExp: {
			S_beginScope(env);
			A_decList decs = get_letexp_decs(e);
			for (; decs; decs = decs->tail) {
				traverseDec(env, depth, decs->head);
			}
			traverseExp(env, depth, get_letexp_body(e));
			S_endScope(env);
			break;
		}
		case A_callExp: {
			A_expList args = e->u.call.args;
			for (; args; args = args->tail) {
				traverseExp(env, depth, args->head);
			}
			break;
		}
		case A_opExp: {
			traverseExp(env, depth, get_opexp_left(e));
			traverseExp(env, depth, get_opexp_right(e));
			break;
		}
		case A_recordExp: {
			A_efieldList el = get_recordexp_fields(e);
			for (; el; el=el->tail) {
				traverseExp(env, depth, el->head->exp);
			}
			break;
		}
		case A_seqExp: {
			A_expList expList = get_seqexp_seq(e);
			for (; expList; expList=expList->tail) {
				traverseExp(env, depth, expList->head);
			}
			break;
		}
		case A_assignExp: {
			traverseExp(env, depth, get_assexp_exp(e));
			traverseVar(env, depth, get_assexp_var(e));
			break;
		}
		case A_ifExp: {
			traverseExp(env, depth, get_ifexp_test(e));
			traverseExp(env, depth, get_ifexp_then(e));
			if (get_ifexp_else(e)) {
				traverseExp(env, depth, get_ifexp_else(e));
			}
			break;
		}
		case A_whileExp: {
			traverseExp(env, depth, get_whileexp_test(e));
			traverseExp(env, depth, get_whileexp_body(e));
			break;
		}
		case A_forExp: {
			traverseExp(env, depth, get_forexp_lo(e));
			traverseExp(env, depth, get_forexp_hi(e));
			S_beginScope(env);
			e->u.forr.escape = FALSE;
			S_enter(env, get_forexp_var(e), E_EscapeEntry(depth, &(e->u.forr.escape)));
			traverseExp(env, depth, get_forexp_body(e));
			S_endScope(env);
			break;
		}
		case A_arrayExp: {
			traverseExp(env, depth, get_arrayexp_size(e));
			traverseExp(env, depth, get_arrayexp_init(e));
			break;
		}
		default: break;
	}
}

static void traverseDec(S_table env, int depth, A_dec d)
{
	switch (d->kind) {
		case A_functionDec: {
			A_fundecList fl;
			for (fl = get_funcdec_list(d); fl; fl=fl->tail) {
				S_beginScope(env);
				A_fieldList l;
				for (l=fl->head->params; l; l=l->tail) {
					l->head->escape = FALSE;
					S_enter(env, l->head->name, E_EscapeEntry(depth + 1, &(l->head->escape)));  //深度加1
				}
				traverseExp(env, depth + 1, fl->head->body);
				S_endScope(env);
			}
			break;
		}
		case A_varDec: {
			traverseExp(env, depth, get_vardec_init(d));
			d->u.var.escape = FALSE;
			S_enter(env, get_vardec_var(d), E_EscapeEntry(depth, &(d->u.var.escape)));
			break;
		}
		default:
		    break;
	}
}

static void traverseVar(S_table env, int depth, A_var v)
{
	switch (v->kind) {
		case A_simpleVar: {
			E_enventry x = S_look(env, get_simplevar_sym(v));
			if (x && depth > x->u.esc.depth) {
				*(x->u.esc.escape) = TRUE;
			}
			break;
		}
		case A_fieldVar: {
			traverseVar(env, depth, get_fieldvar_var(v));
			break;
		}
		case A_subscriptVar: {
			traverseVar(env, depth, get_subvar_var(v));
			traverseExp(env, depth, get_subvar_exp(v));
			break;
		}
	}
}

void Esc_findEscape(A_exp exp) {
	S_table env = S_empty();
	traverseExp(env, 0, exp);	
}
