/*
 * flowgraph.h - Function prototypes to represent control flow graphs.
 */

#ifndef FLOWGRAPH_H
#define FLOWGRAPH_H

Temp_tempList FG_def(G_node n); //结点n对应指令中的目标寄存器
Temp_tempList FG_use(G_node n); //结点n对应指令中源寄存器
bool FG_isMove(G_node n);  //n对应的指令是不是MOVE指令，如果是，当def和use相同时可以删除这条指令
G_graph FG_AssemFlowGraph(AS_instrList il);

#endif
