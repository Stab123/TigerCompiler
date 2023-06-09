/*
 * color.h - Data structures and function prototypes for coloring algorithm
 *             to determine register allocation.
 */

#ifndef COLOR_H
#define COLOR_H

struct COL_result {Temp_map coloring; Temp_tempList spills;};   //寄存器分配表和溢出表
struct COL_result COL_color(G_graph ig, Temp_map initial, Temp_tempList regs, Live_moveList moves, G_table temp_to_moves, G_table cost);

#endif
