#include <stdio.h>
#include "util.h"
#include "table.h"
#include "symbol.h"
#include "absyn.h"
#include "temp.h"
#include "tree.h"
#include "printtree.h"
#include "frame.h"
#include "translate.h"
#include "string.h"

//LAB5: you can modify anything you want.

typedef struct patchList_ *patchList;
struct patchList_ 
{
	Temp_label *head; 
	patchList tail;
};

struct Cx 
{
	patchList trues;  //真值符号回填表
	patchList falses; //假值符号回填表
	T_stm stm;
};

struct Tr_exp_ {
	enum {Tr_ex, Tr_nx, Tr_cx} kind;
	union {T_exp ex; T_stm nx; struct Cx cx; } u;
};

static patchList PatchList(Temp_label *head, patchList tail)
{
	patchList list;

	list = (patchList)checked_malloc(sizeof(struct patchList_));
	list->head = head;
	list->tail = tail;
	return list;
}

void doPatch(patchList tList, Temp_label label)
{
	for(; tList; tList = tList->tail)
		*(tList->head) = label;
}

patchList joinPatch(patchList first, patchList second)
{
	if(!first) return second;
	for(; first->tail; first = first->tail);
	first->tail = second;
	return first;
}

Tr_access Tr_Access(Tr_level l, F_access a)
{
	Tr_access ta = checked_malloc(sizeof(*ta));
	ta->level = l;
	ta->access = a;
	return ta;
}

Tr_accessList Tr_AccessList(Tr_access head, Tr_accessList tail)
{
	Tr_accessList a = checked_malloc(sizeof(*a));
	a->head = head;
	a->tail = tail;
	return a;
}

static Tr_level outermost = NULL;
Tr_level Tr_outermost(void)
{
	if (outermost) {
		return outermost;
	}
	else{
		outermost = Tr_newLevel(NULL, Temp_newlabel(), NULL);
	}
	return outermost;
}

static Tr_accessList makeFormalAccessList(Tr_level l)
{
	Tr_accessList head = NULL;
	Tr_accessList tail = NULL;
	F_accessList f = F_formals(l->frame)->tail; 

	for(; f; f=f->tail) {
		Tr_access a = Tr_Access(l, f->head);
		if (head == NULL) {
			head = tail = Tr_AccessList(a, NULL);
		} else {
			tail->tail = Tr_AccessList(a, NULL);
			tail = tail->tail;
		}
	}
	return head;
}

Tr_level Tr_newLevel(Tr_level parent, Temp_label name, U_boolList formals)
{
	Tr_level l = checked_malloc(sizeof(*l));
	l->parent = parent;
	l->frame = F_newFrame(name, U_BoolList(TRUE, formals));
	l->fmls = makeFormalAccessList(l);
	l->name = name;
	return l;
}

Tr_accessList Tr_formals(Tr_level level)
{
	return level->fmls;
}

Tr_access Tr_allocLocal(Tr_level level, bool escape)
{
	return Tr_Access(level, F_allocLocal(level->frame, escape));
}

Tr_expList Tr_ExpList(Tr_exp head, Tr_expList tail)
{
	Tr_expList l = checked_malloc(sizeof(*l));
	l->head = head;
	l->tail = tail;
	return l;
}

static Tr_exp Tr_Ex(T_exp ex);
static Tr_exp Tr_Nx(T_stm nx);
static Tr_exp Tr_Cx(patchList trues, patchList falses, T_stm stm);

static T_exp unEx(Tr_exp e);
static T_stm unNx(Tr_exp e);
static struct Cx unCx(Tr_exp e);

static Tr_exp Tr_Ex(T_exp ex)  //表达式
{
	Tr_exp e = checked_malloc(sizeof(*e));
	e->kind = Tr_ex;
	e->u.ex = ex;
	return e;
}

static Tr_exp Tr_Nx(T_stm nx)  //无结果语句
{
	Tr_exp e = checked_malloc(sizeof(*e));
	e->kind = Tr_nx;
	e->u.nx = nx;
	return e;
}

static Tr_exp Tr_Cx(patchList trues, patchList falses, T_stm stm)  //条件语句
{
	Tr_exp e = checked_malloc(sizeof(*e));
	e->kind = Tr_cx;
	e->u.cx.trues = trues;
	e->u.cx.falses = falses;
	e->u.cx.stm = stm;
	return e;
}


static T_exp unEx(Tr_exp e) 
{
	switch(e->kind) {
		case Tr_cx: {
			Temp_temp r = Temp_newtemp();
			Temp_label t = Temp_newlabel(), f = Temp_newlabel();
			doPatch(e->u.cx.trues, t);
			doPatch(e->u.cx.falses, f);
			return T_Eseq(T_Move(T_Temp(r), T_Const(1)),T_Eseq(e->u.cx.stm,T_Eseq(T_Label(f),T_Eseq(T_Move(T_Temp(r), T_Const(0)),T_Eseq(T_Label(t), T_Temp(r))))));
		}
		case Tr_ex:
			return e->u.ex;
		case Tr_nx:
			return T_Eseq(e->u.nx, T_Const(0));
	}
	assert(0);
}

static T_stm unNx(Tr_exp e)
{
	switch (e->kind) {
		case Tr_cx: {
			Temp_temp r = Temp_newtemp();
			Temp_label t = Temp_newlabel(), f = Temp_newlabel();
			doPatch(e->u.cx.trues, t);
			doPatch(e->u.cx.falses, f);
			return T_Exp(T_Eseq(T_Move(T_Temp(r), T_Const(1)),T_Eseq(e->u.cx.stm,T_Eseq(T_Label(f),T_Eseq(T_Move(T_Temp(r), T_Const(0)),T_Eseq(T_Label(t), T_Temp(r)))))));
		}
		case Tr_ex:
			return T_Exp(e->u.ex);
		case Tr_nx:
			return e->u.nx;
	}
	assert(0);
}

static struct Cx unCx(Tr_exp e)  
{
	switch(e->kind) {
		case Tr_cx:
			return e->u.cx;
		case Tr_ex: {
			struct Cx trCx;
			trCx.stm = T_Cjump(T_ne, unEx(e), T_Const(0), NULL, NULL);
			trCx.trues = PatchList(&(trCx.stm->u.CJUMP.true), NULL);
			trCx.falses = PatchList(&(trCx.stm->u.CJUMP.false), NULL);
			return trCx;
		}
		case Tr_nx: {
			assert(0);
		}
	}
	assert(0);
}

static F_fragList fragList = NULL;
void Tr_procEntryExit(Tr_level level, Tr_exp body, Tr_accessList formals)
{
	Temp_temp t = Temp_newtemp();
	T_stm stm = F_procEntryExit1(level->frame, T_Seq(T_Move(T_Temp(t), unEx(body)), T_Move(T_Temp(F_RV()), T_Temp(t))));
	F_frag f = F_ProcFrag(stm, level->frame);
	fragList = F_FragList(f, fragList);
}

F_fragList Tr_getResult(void)
{
	return fragList;
}

Tr_exp Tr_simpleVar(Tr_access a, Tr_level l)
{
	T_exp fp = T_Temp(F_FP());
	while (l && l != a->level) {
		F_access staticLink = F_formals(l->frame)->head;
		fp = F_Exp(staticLink, fp);
		l = l->parent;
	}
	return Tr_Ex(F_Exp(a->access, fp));
}

Tr_exp Tr_fieldVar(Tr_exp b, int off)
{
	return Tr_Ex(T_Mem(T_Binop(T_plus, unEx(b), T_Binop(T_mul, T_Const(off), T_Const(F_wordSize)))));
}

Tr_exp Tr_subscriptVar(Tr_exp b, Tr_exp i)
{
	return Tr_Ex(T_Mem(T_Binop(T_plus, unEx(b), T_Binop(T_mul, unEx(i), T_Const(F_wordSize)))));
}

Tr_exp Tr_nilExp()
{
	return Tr_Nx(T_Exp(T_Const(0)));
}

Tr_exp Tr_intExp(int i)
{
	return Tr_Ex(T_Const(i));
}

Tr_exp Tr_stringExp(string s)
{
	string str;
	int len;
	for (F_fragList fl=fragList; fl; fl=fl->tail) {
		F_frag f = fl->head;
		if (f->kind == F_stringFrag) {
			str = f->u.stringg.str;
	        int len;
			if (*(int *)str < strlen(s)){
				len = *(int *)str;
			}
			else
			{
				len = strlen(s);
			}
			if (strncmp(s, str+4, len) == 0) {
				return Tr_Ex(T_Name(f->u.stringg.label));
			}
		}
	}
	Temp_label l = Temp_newlabel();
	char *buf = checked_malloc(strlen(s) + sizeof(int));
	(*(int *)buf) = strlen(s);
	strncpy(buf + 4, s, strlen(s));
	F_frag f = F_StringFrag(l, buf);
	fragList = F_FragList(f, fragList);
	return Tr_Ex(T_Name(l));
}

static T_exp staticLink(Tr_level level, Tr_level target) {
	T_exp ex = T_Temp(F_FP());
	Tr_level nowLevel = level;
	while (nowLevel && nowLevel != target) {
		ex = F_Exp(F_formals(nowLevel->frame)->head, ex);
		nowLevel = nowLevel->parent;
	}
	return ex;
}

T_expList makeCallParams(Tr_expList params) {
	T_expList exps = NULL;
	T_expList tail = NULL;
	Tr_expList trExps = params;
	while (trExps) {
		exps = T_ExpList(unEx(trExps->head), exps);
		trExps = trExps->tail;
	}
	return exps;
}

Tr_exp Tr_callExp(Temp_label label, Tr_level callee, Tr_level caller, Tr_expList args)
{
	T_expList arglist = NULL;
	T_expList tail = NULL;
	while(args){
		T_exp t = unEx(args->head);
		if (arglist == NULL) {
			arglist = tail = T_ExpList(t, NULL);
		} 
		else {
			tail->tail = T_ExpList(t, NULL);
			tail = tail->tail;
		}
		args = args->tail;
	}

	if (Temp_labelIn(F_preDefineFuncs(), label)) {
		return Tr_Ex(F_externalCall(Temp_labelstring(label), arglist));
	}
	T_exp ex = T_Call(T_Name(label), T_ExpList(staticLink(caller, callee->parent), arglist));
	return Tr_Ex(ex);
}

Tr_exp Tr_binExp(A_oper op, Tr_exp left, Tr_exp right)
{
	T_exp ex;
	switch (op) {
		case A_plusOp: ex = T_Binop(T_plus, unEx(left), unEx(right)); break;
		case A_minusOp: ex = T_Binop(T_minus, unEx(left), unEx(right));break;
		case A_timesOp : ex = T_Binop(T_mul, unEx(left), unEx(right));break;
		case A_divideOp: ex = T_Binop(T_div, unEx(left), unEx(right));break;
	}
	return Tr_Ex(ex);
}

Tr_exp Tr_relExp(A_oper op, Tr_exp left, Tr_exp right)
{
	T_relOp opp;
	switch (op) {
		case A_eqOp: opp = T_eq; break;
		case A_neqOp: opp = T_ne; break;
		case A_leOp: opp = T_le; break;
		case A_ltOp: opp = T_lt; break;
		case A_geOp: opp = T_ge; break;
		case A_gtOp: opp = T_gt; break;
	}
	T_stm stm = T_Cjump(opp, unEx(left), unEx(right), NULL, NULL);
	patchList trues = PatchList(&(stm->u.CJUMP.true), NULL);
	patchList falses = PatchList(&(stm->u.CJUMP.false), NULL);
	return Tr_Cx(trues, falses, stm);
}

Tr_exp Tr_recordExp(int n, Tr_expList fields)
{
    Temp_temp r = Temp_newtemp();

	T_stm alloc = T_Move(T_Temp(r), F_externalCall("malloc", T_ExpList(T_Const(n * F_wordSize), NULL)));
	Tr_expList l = fields;
	T_stm seq = T_Move(T_Mem(T_Binop(T_plus, T_Temp(r), T_Const(0))), unEx(l->head));
	l = l->tail;
	for (int i = 1; l; l=l->tail, i++) {
		seq = T_Seq(seq, T_Move(T_Mem(T_Binop(T_plus, T_Temp(r), T_Const(i * F_wordSize))), unEx(l->head))); 
	}
	return Tr_Ex(T_Eseq(T_Seq(alloc, seq), T_Temp(r)));
}

Tr_exp Tr_seqExp(Tr_expList exps)
{
	T_exp l = unEx(exps->head);
	exps = exps->tail;
	while(exps){
		l = T_Eseq(unNx(Tr_Ex(l)), unEx(exps->head));
		exps=exps->tail;
	}
	return Tr_Ex(l);
}

Tr_exp Tr_assignExp(Tr_exp lval, Tr_exp v)
{
	return Tr_Nx(T_Move(unEx(lval), unEx(v)));
}

Tr_exp Tr_ifExp(Tr_exp test, Tr_exp then, Tr_exp elsee)
{
	struct Cx c = unCx(test);
	Temp_label t = Temp_newlabel(),f = Temp_newlabel(),done = Temp_newlabel();
	Temp_temp r = Temp_newtemp();
	
	doPatch(c.trues, t);
	doPatch(c.falses, f);

	if (elsee == NULL) {
		return Tr_Nx(T_Seq(c.stm,
						T_Seq(T_Label(t),
							T_Seq(unNx(then), T_Label(f)))));
	} 
	else {
		return Tr_Ex(T_Eseq(T_Seq(c.stm,
								T_Seq(T_Label(t),
									T_Seq(T_Move(T_Temp(r), unEx(then)),
										T_Seq(T_Jump(T_Name(done), Temp_LabelList(done, NULL)),
											T_Seq(T_Label(f),
												T_Seq(T_Move(T_Temp(r), unEx(elsee)),
													T_Seq(T_Jump(T_Name(done), Temp_LabelList(done, NULL)), T_Label(done)))))))) , T_Temp(r)));
	}
}

Tr_exp Tr_whileExp(Tr_exp test, Tr_exp body, Temp_label done)
{
	Temp_label t = Temp_newlabel(), b = Temp_newlabel();
	struct Cx c = unCx(test);
	doPatch(c.trues, b);
	doPatch(c.falses, done);
	return Tr_Nx(T_Seq(T_Label(t),T_Seq(c.stm,T_Seq(T_Label(b),T_Seq(unNx(body),T_Seq(T_Jump(T_Name(t), Temp_LabelList(t, NULL)), T_Label(done)))))));

}

Tr_exp Tr_forExp(Tr_access access, Tr_exp lo, Tr_exp hi, Tr_exp body, Temp_label done)
{
	Temp_label t = Temp_newlabel();
	Temp_label b = Temp_newlabel();
	T_exp iexp = F_Exp(access->access, T_Temp(F_FP())); 
	return Tr_Nx(T_Seq(T_Move(iexp, unEx(lo)),
					T_Seq(T_Label(t),
						T_Seq(T_Cjump(T_le, iexp, unEx(hi), b, done),
							T_Seq(T_Label(b),
								T_Seq(unNx(body),
									T_Seq(T_Move(iexp, unEx(Tr_binExp(A_plusOp, Tr_Ex(iexp), Tr_Ex(T_Const(1))))),
										T_Seq(T_Jump(T_Name(t), Temp_LabelList(t, NULL)), T_Label(done)))))))));

}

Tr_exp Tr_breakExp(Temp_label done)
{
	return Tr_Nx(T_Jump(T_Name(done), Temp_LabelList(done, NULL)));
}

Tr_exp Tr_arrayExp(Tr_exp size, Tr_exp init)
{
	return Tr_Ex(F_externalCall("initArray", T_ExpList(unEx(size), T_ExpList(unEx(init), NULL))));
}

Tr_exp Tr_noExp()
{
	return Tr_Nx(T_Exp(T_Const(0))); 
}

Tr_exp Tr_stringEqualExp(Tr_exp left, Tr_exp right)
{
	return Tr_Ex(F_externalCall("stringEqual", T_ExpList(unEx(left), T_ExpList(unEx(right), NULL))));
}

void Tr_print(Tr_exp e)
{
	printStmList(stdout, T_StmList(unNx(e), NULL));
}