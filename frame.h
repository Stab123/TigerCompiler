
/*Lab5: This header file is not complete. Please finish it with more definition.*/

#ifndef FRAME_H
#define FRAME_H

#include "tree.h"
#include "assem.h"

extern const int F_wordSize;
extern const int F_formalRegNum;
extern const int F_regNum;

typedef struct F_accessList_ *F_accessList;

typedef struct F_frame_ *F_frame;
struct F_frame_ {
	T_stm shift;  //视角移位
	int size;
	F_accessList fmls;  //formals
	Temp_label name;
};

typedef struct F_access_ *F_access;   //存放在栈中或寄存器中的形式参数和局部变量
struct F_access_ {
	enum {inFrame, inReg} kind;
	union {
		int offset; //inFrame
		Temp_temp reg; //inReg
	} u;
};

struct F_accessList_ {F_access head; F_accessList tail;};

F_accessList F_AccessList(F_access head, F_accessList tail);

F_frame F_newFrame(Temp_label name, U_boolList formals);
Temp_label F_name(F_frame f);
F_accessList F_formals(F_frame f);
F_access F_allocLocal(F_frame f, bool escape);  //在f中分配一个新的局部变量，逃逸变量必须分配在frame中

T_exp F_Exp(F_access acc, T_exp framePtr);  //帧指针寄存器

T_exp F_externalCall(string s, T_expList args);

typedef struct F_frag_ *F_frag;  //片段
struct F_frag_ {enum {F_stringFrag, F_procFrag} kind;
			union {
				struct {Temp_label label; string str;} stringg;
				struct {T_stm body; F_frame frame;} proc;
			} u;
};

F_frag F_StringFrag(Temp_label label, string str);
F_frag F_ProcFrag(T_stm body, F_frame frame);

typedef struct F_fragList_ *F_fragList;
struct F_fragList_ 
{
	F_frag head; 
	F_fragList tail;
};

F_fragList F_FragList(F_frag head, F_fragList tail);

Temp_temp F_FP(void); 
Temp_temp F_SP(void); //栈指针
Temp_temp F_ZERO(void);
Temp_temp F_RA(void);
Temp_temp F_RV(void);

Temp_map F_tempMap;  //预着色临时变量
Temp_tempList F_registers(void);
Temp_tempList F_calleeSavedReg(void);
Temp_tempList F_callerSavedReg(void);
Temp_tempList F_paramReg(void);
Temp_map F_regTempMap();
Temp_tempList F_X86MUL(void);
Temp_tempList F_X86DIV(void);
T_stm F_procEntryExit1(F_frame frame, T_stm stm); //将每一个传入的寄存器参数存放到函数内来看它的位置
AS_instrList F_procEntryExit2(AS_instrList body);
AS_proc F_procEntryExit3(F_frame frame, AS_instrList body); //生成过程入口处理和出口处理的汇编语言代码

Temp_labelList F_preDefineFuncs();

#endif
