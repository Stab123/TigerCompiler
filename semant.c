#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "util.h"
#include "errormsg.h"
#include "symbol.h"
#include "absyn.h"
#include "prabsyn.h"
#include "types.h"
#include "env.h"
#include "semant.h"
#include "helper.h"
#include "translate.h"

/*Lab5: Your implementation of lab5.*/

struct expty 
{
	Tr_exp exp; 
	Ty_ty ty;
};

//In Lab4, the first argument exp should always be **NULL**.
struct expty expTy(Tr_exp exp, Ty_ty ty)    //转换后的表达式和语言类型
{
	struct expty e;

	e.exp = exp;
	e.ty = ty;

	return e;
}

Ty_ty actual_ty(Ty_ty ty)
{
	if (ty == NULL) return Ty_Void();
	if (ty->kind == Ty_name) {
		return actual_ty(ty->u.name.ty);
	} else {
		return ty;
	}
}

Ty_tyList makeFormalTyList(S_table tenv, A_fieldList fieldList)
{
  A_fieldList fields;
  Ty_tyList tyList = NULL;

  for(fields = fieldList; fields; fields = fields->tail) {
    Ty_ty ty = actual_ty(S_look(tenv, fields->head->typ));
    tyList = Ty_TyList(ty, tyList);
  }

  Ty_tyList rlist = NULL;
  while (tyList){
    rlist = Ty_TyList(tyList->head, rlist);
    tyList = tyList->tail;
  }

  return rlist;
}

bool isSameType(Ty_ty t1, Ty_ty t2)
{
	Ty_ty left = actual_ty(t1);
	Ty_ty right = actual_ty(t2);
    if (left->kind == Ty_array) {
        if (left == right) {
        	return TRUE;
        }
    } else if (left->kind == Ty_record) {
        if (left == right || right->kind == Ty_nil) {
        	return TRUE;
        }
    } else if (right->kind == Ty_record) {
        if (left == right || left->kind == Ty_nil) {
        	return TRUE;
        }
    } else {
        if (left->kind == right->kind) {
        	return TRUE;
        }
    }
    return FALSE;
}

U_boolList makeFormalBoolList(A_fieldList fieldList)
{
	if (fieldList) {
		return U_BoolList(fieldList->head->escape, makeFormalBoolList(fieldList->tail));
	} else {
		return NULL;
	}
}

struct expty transVar(S_table venv, S_table tenv, A_var v, Tr_level l, Temp_label breakLabel)
{
	switch (v->kind) {
		case A_simpleVar: 
		{ //变量
			E_enventry x = S_look(venv, get_simplevar_sym(v));
			if (x && x->kind == E_varEntry) {
				return expTy(Tr_simpleVar(get_var_access(x), l), actual_ty(get_varentry_type(x)));
			} 
			else {
				EM_error(v->pos, "undefined variable %s", S_name(get_simplevar_sym(v)));
				return expTy(Tr_noExp(), Ty_Int());
			}
		}
		case A_fieldVar: 
		{  //域
			struct expty var = transVar(venv, tenv, v->u.field.var,l,breakLabel);
            if(var.ty->kind != Ty_record){
               EM_error(v->pos, "not a record type");
               return expTy(Tr_noExp(), Ty_Void());
            }
			int i = 0;
            for(Ty_fieldList fields = var.ty->u.record; fields; fields = fields->tail,i++){
               if(fields->head->name == v->u.field.sym){
                   return expTy(Tr_fieldVar(var.exp,i), actual_ty(fields->head->ty));
               }  
            }
            EM_error(v->pos, "field %s doesn't exist", S_name(v->u.field.sym));
            return expTy(Tr_noExp(), Ty_Void());
		}
		case A_subscriptVar: 
		{  //下标
			struct expty exp = transExp(venv, tenv, get_subvar_exp(v), l, breakLabel);
			struct expty var = transVar(venv, tenv, get_subvar_var(v), l, breakLabel);
			if (get_expty_kind(exp) != Ty_int) {
				EM_error(get_subvar_exp(v)->pos, "interger required");
				return expTy(Tr_noExp(), Ty_Int());
			}
			if (get_expty_kind(var) == Ty_array) {
				return expTy(Tr_subscriptVar(var.exp, exp.exp), actual_ty(get_array(var)));
			} 
			else {
				EM_error(get_subvar_var(v)->pos, "array type required");
				return expTy(Tr_noExp(), Ty_Int());
			}
		}
	}
	assert(0);
}

struct expty transExp(S_table venv, S_table tenv, A_exp a, Tr_level l, Temp_label breakLabel)  //表达式的类型检查
{
	switch (a->kind) {
		case A_opExp: {
			A_oper oper = a->u.op.oper;
			struct expty left = transExp(venv, tenv, get_opexp_left(a), l, breakLabel);
			struct expty right = transExp(venv, tenv, get_opexp_right(a), l, breakLabel);
			if (oper == A_plusOp || oper == A_minusOp || oper == A_timesOp || oper == A_divideOp) {
				if (left.ty->kind != Ty_int) {
					EM_error(get_opexp_leftpos(a), "integer required");
				}
				if (right.ty->kind != Ty_int) {
					EM_error(get_opexp_rightpos(a), "integer required");
				}
				return expTy(Tr_binExp(oper, left.exp, right.exp), Ty_Int());
			}
			if (oper == A_eqOp || oper == A_neqOp || oper == A_ltOp || oper == A_leOp || oper == A_gtOp || oper == A_geOp) {
				if (!isSameType(left.ty,right.ty)) {
					EM_error(get_opexp_leftpos(a), "same type required");
				}
				if (left.ty == Ty_String() && right.ty == Ty_String()) {
					return expTy(Tr_stringEqualExp(left.exp, right.exp), Ty_Int());
				}
				return expTy(Tr_relExp(oper, left.exp, right.exp), Ty_Int());
			}
		}
		case A_varExp: {
			return transVar(venv, tenv, a->u.var, l, breakLabel);
		}
		case A_nilExp: {
			return expTy(Tr_nilExp(), Ty_Nil());
		}
		case A_intExp: {
			return expTy(Tr_intExp(a->u.intt), Ty_Int());
		}
		case A_stringExp: {
			return expTy(Tr_stringExp(a->u.stringg), Ty_String());
		}
		case A_breakExp: {
			if (!breakLabel) {
				return expTy(Tr_noExp(), Ty_Void());
			}
			return expTy(Tr_breakExp(breakLabel), Ty_Void());
		}
		case A_letExp: {
			struct expty e;
			A_decList d;
			Tr_expList exps = NULL;
			Tr_expList tail = NULL;
			S_beginScope(venv);
			S_beginScope(tenv);
			for (d = get_letexp_decs(a); d; d=d->tail) {
				Tr_exp exp = transDec(venv, tenv, d->head, l, breakLabel);
				if (exps == NULL) {
					exps = tail = Tr_ExpList(exp, NULL);
				} else {
					tail->tail = Tr_ExpList(exp, NULL);
					tail = tail->tail;
				}
			}
			e = transExp(venv, tenv, get_letexp_body(a), l, breakLabel);
			tail->tail = Tr_ExpList(e.exp, NULL);
			S_endScope(tenv);
			S_endScope(venv);
			return expTy(Tr_seqExp(exps), e.ty);
		}
		case A_callExp: {
			E_enventry x = S_look(venv, get_callexp_func(a));
			if (!(x && x->kind == E_funEntry)) {
				EM_error(a->pos, "undefined function %s", S_name(get_callexp_func(a)));
				return expTy(Tr_noExp(), Ty_Void());
			}
			Ty_tyList fTys;
			A_expList paras;
			Tr_expList args = NULL;
			Tr_expList tail = NULL;
			for (fTys = get_func_tylist(x), paras = get_callexp_args(a); fTys; fTys=fTys->tail, paras=paras->tail) {
				if (paras == NULL) {
					EM_error(a->pos, "para type mismatch");
					return expTy(Tr_noExp(), Ty_Void());
				}
				struct expty e = transExp(venv, tenv, paras->head, l, breakLabel);
				if (!isSameType(e.ty,fTys->head)) {
					EM_error(a->pos, "para type mismatch");
					return expTy(Tr_noExp(), Ty_Void());
				}
				if (args == NULL) {
					args = tail = Tr_ExpList(e.exp, NULL);
				} else {
					tail->tail = Tr_ExpList(e.exp, NULL);
					tail = tail->tail;
				}
			}
			if (paras != NULL) {
				EM_error(a->pos, "too many params in function %s", S_name(get_callexp_func(a)));
				return expTy(Tr_noExp(), Ty_Void());
			}
			return expTy(Tr_callExp(get_func_label(x), get_func_level(x), l, args), actual_ty(get_func_res(x)));
		}
		case A_arrayExp: {
			Ty_ty t = actual_ty(S_look(tenv, get_arrayexp_typ(a)));
			if (!t) {
                EM_error(a->pos, "undefined type %s", S_name(a->u.array.typ));
                return expTy(NULL, Ty_Int());
            }
			struct expty size = transExp(venv, tenv, get_arrayexp_size(a), l, breakLabel);
			struct expty init = transExp(venv, tenv, get_arrayexp_init(a), l, breakLabel);
			if (!isSameType(init.ty, actual_ty(t)->u.array)) {
				EM_error(a->pos, "type mismatch");
				return expTy(Tr_noExp(), Ty_Int());
			}
			return expTy(Tr_arrayExp(size.exp, init.exp), actual_ty(t));
		}
		case A_recordExp: {
			Ty_ty x = actual_ty(S_look(tenv, get_recordexp_typ(a)));
			if (x == NULL) {
				EM_error(a->pos, "undefined type %s", S_name(get_recordexp_typ(a)));
				return expTy(Tr_noExp(), Ty_Record(NULL));
			}
			Tr_expList fields = NULL,tail = NULL;
			A_efieldList el;
			int n = 0;
			for (el = get_recordexp_fields(a); el; el=el->tail) {
				struct expty e = transExp(venv, tenv, el->head->exp, l, breakLabel);
				if (fields == NULL) {
					fields = tail = Tr_ExpList(e.exp, NULL);
				} else {
					tail->tail = Tr_ExpList(e.exp, NULL);
					tail = tail->tail;
				}
				n++;
			}
			return expTy(Tr_recordExp(n, fields), x);
		}
		case A_seqExp: {
			A_expList expList = a->u.seq;
			struct expty e;
			Tr_expList seq = NULL;
			Tr_expList tail = NULL;
			if (expList == NULL) {
				return expTy(Tr_noExp(), Ty_Void());
			}
			for (; expList; expList=expList->tail) {
				e = transExp(venv, tenv, expList->head, l, breakLabel);
				if (seq == NULL) {
					seq = tail = Tr_ExpList(e.exp, NULL);
				} else {
					tail->tail = Tr_ExpList(e.exp, NULL);
					tail = tail->tail;
				}
			}
			return expTy(Tr_seqExp(seq), e.ty);
		}
		case A_assignExp: {
			struct expty ee = transExp(venv, tenv, get_assexp_exp(a), l, breakLabel);
			struct expty ev = transVar(venv, tenv, get_assexp_var(a), l, breakLabel);
			if (!isSameType(ee.ty, ev.ty)) {
				EM_error(a->pos, "unmatched assign exp");
			}
			if (get_assexp_var(a)->kind == A_simpleVar) {
				E_enventry x = S_look(venv, get_simplevar_sym(get_assexp_var(a)));
				if (x && x->kind == E_varEntry) {
					if (x->readonly == 1) {
						EM_error(a->pos, "loop variable can't be assigned");
					}
				}
			}
			return expTy(Tr_assignExp(ev.exp, ee.exp), Ty_Void());
		}
		case A_ifExp: {
			struct expty ei = transExp(venv, tenv, get_ifexp_test(a), l, breakLabel);
			struct expty et = transExp(venv, tenv, get_ifexp_then(a), l, breakLabel);
			if (get_ifexp_else(a) == NULL) {
				if (get_expty_kind(et) != Ty_void) {
					EM_error(a->pos, "if-then exp's body must produce no value");
				}
				return expTy(Tr_ifExp(ei.exp, et.exp, NULL), Ty_Void());
			} else {
				struct expty ee = transExp(venv, tenv, get_ifexp_else(a), l, breakLabel);
				if (!isSameType(ee.ty, et.ty)) {
					EM_error(a->pos, "then exp and else exp type mismatch");
				}
				return expTy(Tr_ifExp(ei.exp, et.exp, ee.exp), et.ty);
			}
		}
		case A_whileExp: {
			struct expty test = transExp(venv, tenv, get_whileexp_test(a), l, breakLabel);
			Temp_label done = Temp_newlabel();
			struct expty body = transExp(venv, tenv, get_whileexp_body(a), l, done);
			if (get_expty_kind(body) != Ty_void) {
				EM_error(a->pos, "while body must produce no value");
			}
			return expTy(Tr_whileExp(test.exp, body.exp, done), Ty_Void());
		}
		case A_forExp: {
			struct expty lo = transExp(venv, tenv, get_forexp_lo(a), l, breakLabel);
			if (get_expty_kind(lo) != Ty_int) {
				EM_error(get_forexp_lo(a)->pos, "for exp's range type is not integer");
			}
			struct expty hi = transExp(venv, tenv, get_forexp_hi(a), l, breakLabel);
			if (get_expty_kind(hi) != Ty_int) {
				EM_error(get_forexp_hi(a)->pos, "for exp's range type is not integer");
			}
			Tr_access ta = Tr_allocLocal(l, TRUE);
			S_beginScope(venv);
			S_enter(venv, get_forexp_var(a), E_ROVarEntry(ta, Ty_Int()));
			Temp_label done = Temp_newlabel();
			struct expty body = transExp(venv, tenv, get_forexp_body(a), l, done);
			if (get_expty_kind(body) != Ty_void) {
				;
			}
			S_endScope(venv);
			return expTy(Tr_forExp(ta, lo.exp, hi.exp, body.exp, done), Ty_Void());
		}
	}
}

Tr_exp transDec(S_table venv, S_table tenv, A_dec d, Tr_level l, Temp_label breakLabel) //声明的类型检查
{
	switch (d->kind) {
		case A_functionDec: 
		{ //函数声明
			A_fundecList fl, ffl;
			A_fundec f;
			Ty_ty resultTy;
			Tr_level newLevel;
            Temp_label newLabel;
			Ty_tyList formalTys;
			U_boolList formalEscapes;
			E_enventry x;
			for (fl = get_funcdec_list(d); fl; fl=fl->tail) {
				f = fl->head;
				for (ffl = get_funcdec_list(d); ffl != fl; ffl=ffl->tail) {
					if (ffl->head->name == f->name) {
						EM_error(f->pos, "two functions have the same name");
						break;
					}
				}
				if (f->result) {
					resultTy = S_look(tenv, f->result);
				} else {
					resultTy = Ty_Void();
				}
				formalTys = makeFormalTyList(tenv, f->params);
				formalEscapes = makeFormalBoolList(f->params);
				newLabel = Temp_namedlabel(S_name(f->name));
				newLevel = Tr_newLevel(l, newLabel, formalEscapes);
				S_enter(venv, f->name, E_FunEntry(newLevel, newLabel, formalTys, resultTy));
			}
			for (fl = get_funcdec_list(d); fl; fl=fl->tail) {
				f = fl->head;
				x = S_look(venv, f->name);
				S_beginScope(venv);
				A_fieldList l;
				Ty_tyList t;
				Tr_accessList al = Tr_formals(get_func_level(x));
				for (l = f->params, t = get_func_tylist(x); l; l=l->tail, t=t->tail, al=al->tail) {
					S_enter(venv, l->head->name, E_VarEntry(al->head, t->head));
				}
				struct expty e = transExp(venv, tenv, f->body, get_func_level(x), breakLabel);
				if (get_func_res(x)->kind == Ty_void && get_expty_kind(e) != Ty_void) {
					EM_error(f->pos, "procedure returns value");
				}
				Tr_procEntryExit(get_func_level(x), e.exp, al);
				S_endScope(venv);
			}
			return Tr_noExp();
		}
		case A_varDec: 
		{  //变量声明
			struct expty e = transExp(venv, tenv, d->u.var.init, l, breakLabel);
			Tr_access a = Tr_allocLocal(l, d->u.var.escape);
			if (d->u.var.typ) {
				Ty_ty t = S_look(tenv, get_vardec_typ(d));
				if (!t) {
					EM_error(d->pos, "undefined type %s", S_name(get_vardec_typ(d)));
				} else {
					if (!isSameType(t, e.ty)) {
						EM_error(d->pos, "vardec type mismatch");
					}
					S_enter(venv, get_vardec_var(d), E_VarEntry(a, t));
				}
			} else {
				if (actual_ty(e.ty)->kind == Ty_nil) {
					EM_error(d->pos, "init should not be nil without type specified");
					S_enter(venv, get_vardec_var(d), E_VarEntry(a, Ty_Int()));
				} else {
					S_enter(venv, get_vardec_var(d), E_VarEntry(a, e.ty));
				}
			}
			return Tr_assignExp(Tr_simpleVar(a, l), e.exp);
		}
		case A_typeDec: 
		{  //类型声明
			A_nametyList nametys, afterNametys;
			A_namety namety;
			int cycle = 0;
			int count = 0;
			for (nametys = get_typedec_list(d); nametys; nametys = nametys->tail) {
            	namety = nametys->head;
            	for (afterNametys = nametys->tail; afterNametys; 
            		afterNametys = afterNametys->tail) {
            		if (namety->name == afterNametys->head->name) {
            			EM_error(d->pos, "two types have the same name");
            			break;
            		}
            	}
                S_enter(tenv, namety->name, Ty_Name(namety->name, NULL));
            }
			for (nametys = get_typedec_list(d); nametys; nametys = nametys->tail) {
				Ty_ty ty = S_look(tenv, nametys->head->name);
				ty->u.name.ty = transTy(tenv, nametys->head->ty);
			}
			for (nametys= get_typedec_list(d); nametys; nametys = nametys->tail) {
				Ty_ty ty = S_look(tenv, nametys->head->name);
				Ty_ty tmp = ty;
				while (tmp->kind == Ty_name) { 
					count++;
					assert(count < 10000);
					tmp = tmp->u.name.ty;
					if (tmp == ty) {
						EM_error(d->pos, "illegal type cycle");
						cycle = 1;
						break;
					}
				}
				if (cycle) break;
			}
			return Tr_noExp();
		}
	}
}

Ty_ty transTy (S_table tenv, A_ty a)
{
	switch (a->kind) {
		case A_nameTy: 
		{  //内建
			Ty_ty t = S_look(tenv, get_ty_name(a));
			if (t != NULL) {
				return t;
			} else {
				EM_error(a->pos, "undefined type %s", S_name(get_ty_name(a)));
				return Ty_Int();
			}
		}
		case A_recordTy: 
		{  //记录
			Ty_fieldList tf = NULL;
			Ty_fieldList tail = NULL;
			for (A_fieldList f = get_ty_record(a); f; f=f->tail) {
				Ty_ty t = S_look(tenv, f->head->typ);
				if (t == NULL) {
					EM_error(f->head->pos, "undefined type %s", S_name(f->head->typ));
					t = Ty_Int();
				}
				if (tf == NULL) {
					tf = tail = Ty_FieldList(Ty_Field(f->head->name, t), NULL);
				} else {
					tail->tail = Ty_FieldList(Ty_Field(f->head->name, t), NULL);
					tail = tail->tail;
				}
				
			}
			return Ty_Record(tf);
		}
		case A_arrayTy: 
		{ //数组
			return Ty_Array(S_look(tenv, a->u.array));
		}
	}
}

F_fragList SEM_transProg(A_exp exp)
{
	Temp_label main_label = Temp_namedlabel("tigermain");
	Tr_level main_level = Tr_newLevel(Tr_outermost(), main_label, NULL);
	struct expty main = transExp(E_base_venv(), E_base_tenv(), exp, main_level, NULL);
	Tr_procEntryExit(main_level, main.exp, NULL);
	return Tr_getResult();
}

