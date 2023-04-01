#include <stdio.h>
#include <string.h>

#include "util.h"
#include "symbol.h"
#include "temp.h"
#include "tree.h"
#include "absyn.h"
#include "assem.h"
#include "frame.h"
#include "graph.h"
#include "liveness.h"
#include "color.h"
#include "table.h"

static int K;
static Temp_tempList precolored;
static G_graph interferenceGraph;
static G_nodeList simplifyWorklist, freezeWorklist, spillWorklist, spilledNodes, coalescedNodes, coloredNodes, selectStack;
static Live_moveList coalescedMoves, constrainedMoves, frozenMoves, worklistMoves, activeMoves;
static G_table degree, moveList, alias, color, spillCost;

void enterDegreeMap(G_node n, int d) {
	G_enter(degree, n, (void *)d);
}

int lookupDegreeMap(G_node n) {
	return (int)G_look(degree, n);
}

Live_moveList lookupMoveListMap(G_node n) {
	return (Live_moveList)G_look(moveList, n);
}

Live_moveList nodeMoves(G_node n) {
	return Live_moveIntersect(lookupMoveListMap(n), Live_moveUnion(activeMoves, worklistMoves));
}

bool isMoveRelated(G_node n) {
	return nodeMoves(n) != NULL;
}

G_nodeList adjacent(G_node n) {
	G_nodeList in = G_succ(n),notin = G_nodeUnion(selectStack, coalescedNodes),res = NULL;
	for (;in; in=in->tail) {
		if (!G_nodeIn(notin, in->head)) {
			res = G_NodeList(in->head, res);
		}
	}
	return res;
}

G_node getAlias(G_node n) {
	if (G_nodeIn(coalescedNodes, n)) {
		return getAlias((G_node)G_look(alias, n));
	}
	return n;
}

void assignColor(G_node n, Temp_temp t) {
	G_enter(color, n, t);
}

Temp_temp getColor(G_node n) {
	return (Temp_temp)G_look(color, n);
}

bool isPrecolored(G_node n) {
	return Temp_tempIn(precolored, Live_gtemp(n));
}

void init(G_graph ig, Temp_tempList regs, Live_moveList moves, G_table temp_to_moves, G_table cost) {

	K = F_regNum;
	simplifyWorklist = NULL;
	freezeWorklist = NULL;
	spillWorklist = NULL;
	spilledNodes = NULL;
	coalescedNodes = NULL;
	coloredNodes = NULL;
	selectStack = NULL;
	precolored = regs;
	coalescedMoves = NULL;
	constrainedMoves = NULL;
	frozenMoves = NULL;
	worklistMoves = moves;
	activeMoves = NULL;
	degree = G_empty();
	moveList = temp_to_moves;
	alias = G_empty();
	color = G_empty();
	spillCost = cost;
	interferenceGraph = ig;

	G_nodeList nodes = G_nodes(ig);
	for (; nodes; nodes=nodes->tail) {
		G_node n = nodes->head;
		if (!isPrecolored(n)) {
			enterDegreeMap(n, G_degree(n)/2);
		} else {
			enterDegreeMap(n , 10000);
		}
	}
}

void enableMoves(G_nodeList nl) {
	for (; nl; nl=nl->tail) {
		G_node n = nl->head;
		Live_moveList ml = nodeMoves(n);
		for (; ml; ml=ml->tail) {
			Live_move m = ml->head;
			if (Live_moveIn(activeMoves, m)) {
				activeMoves = Live_moveRemove(activeMoves, m);
				worklistMoves = Live_moveAppend(worklistMoves, m);
			}
		}
	}
}

void decrementDegree(G_node m) {
	int d = lookupDegreeMap(m);
	enterDegreeMap(m, d-1);
	if (d == K && !isPrecolored(m)) { 
		enableMoves(G_nodeAppend(adjacent(m), m));
		spillWorklist = G_nodeRemove(spillWorklist, m);
		if (isMoveRelated(m)) {
			freezeWorklist = G_nodeAppend(freezeWorklist, m);
		} else {
			simplifyWorklist = G_nodeAppend(simplifyWorklist, m);
		}
	}
}

void simplify() {
	G_node n = simplifyWorklist->head;
	simplifyWorklist = simplifyWorklist->tail;
	selectStack = G_nodeAppend(selectStack, n);
	G_nodeList nl = adjacent(n);
	for (; nl; nl=nl->tail) {
		G_node m = nl->head;
		decrementDegree(m);
	}
}

void addWorkList(G_node u) {
	if (!isPrecolored(u) && !isMoveRelated(u) && lookupDegreeMap(u) < K) {
		freezeWorklist = G_nodeRemove(freezeWorklist, u);
		simplifyWorklist = G_nodeAppend(simplifyWorklist, u);
	}
}


bool conservative(G_nodeList nl) {
	int k = 0;
	for (; nl; nl=nl->tail) {
		G_node node = nl->head;
		if (lookupDegreeMap(node) >= K) {
			k = k + 1;
		}
	}
	return k < K;
}

void combine(G_node u, G_node v) {
	if (G_nodeIn(freezeWorklist, v)) {
		freezeWorklist = G_nodeRemove(freezeWorklist, v);
	} else {
		spillWorklist = G_nodeRemove(spillWorklist, v);
	}
	coalescedNodes = G_nodeAppend(coalescedNodes, v);
	G_enter(alias, v, u);
	Live_moveList uml = lookupMoveListMap(u);
	Live_moveList vml = lookupMoveListMap(v);
	G_enter(moveList, u, Live_moveUnion(uml, vml));
	enableMoves(G_NodeList(v, NULL));
	G_nodeList nl = adjacent(v);
	for (; nl; nl=nl->tail) {
		G_node t = nl->head;
		if (!G_goesTo(t, u)) {
		G_addEdge(t, u);
		G_addEdge(u, t);
		enterDegreeMap(t, lookupDegreeMap(t) + 1);
		enterDegreeMap(u, lookupDegreeMap(u) + 1);
	}	
		decrementDegree(t);
	}
	if (lookupDegreeMap(u) >= K && G_nodeIn(freezeWorklist, u)) {
		freezeWorklist = G_nodeRemove(freezeWorklist, u);
		spillWorklist = G_nodeAppend(spillWorklist, u);
	}
}

void coalesce() {
	assert(worklistMoves != NULL);
	Live_move m = worklistMoves->head;

	G_node x = getAlias(m->src);
	G_node y = getAlias(m->dst);
	G_node u, v;
	if (isPrecolored(y)) {
		u = y;
		v = x;
	} else {
		u = x;
		v = y;
	}
	worklistMoves = Live_moveRemove(worklistMoves, m);
	if (u == v) {
		coalescedMoves = Live_moveAppend(coalescedMoves, m);
		addWorkList(u);
	} else if (isPrecolored(v) || G_goesTo(u, v)) {
		constrainedMoves = Live_moveAppend(constrainedMoves, m);
		addWorkList(u);
		addWorkList(v);
	} else {
		G_nodeList nl = adjacent(v);
		bool flag = TRUE;
		for (; nl; nl=nl->tail) {
			G_node t = nl->head;
			if (!(lookupDegreeMap(t) < K || isPrecolored(t) || G_goesTo(t, u))) {
				flag = FALSE;
				break;
			}
		}
		if (isPrecolored(u) && flag || !isPrecolored(u) && conservative(G_nodeUnion(adjacent(u), adjacent(v)))) {
			coalescedMoves = Live_moveAppend(coalescedMoves, m);
			combine(u, v);
			addWorkList(u);
		} else {
			activeMoves = Live_moveAppend(activeMoves, m);
		}
	}
}

void freeze() {
	G_node u = freezeWorklist->head;
	freezeWorklist = G_nodeRemove(freezeWorklist, u);
	simplifyWorklist = G_nodeAppend(simplifyWorklist, u);
	Live_moveList ml = nodeMoves(u);
	for (; ml; ml=ml->tail) {
		Live_move m = ml->head;
		G_node x = m->src;
		G_node y = m->dst;
		G_node v;
		if (getAlias(y) == getAlias(u)) {
			v = getAlias(x);
		} else {
			v = getAlias(y);
		}
		activeMoves = Live_moveRemove(activeMoves, m);
		frozenMoves = Live_moveAppend(frozenMoves, m);
		if (!nodeMoves(v) && lookupDegreeMap(v) < K) {
			freezeWorklist = G_nodeRemove(freezeWorklist, v);
			simplifyWorklist = G_nodeAppend(simplifyWorklist, v);
		}
	}
}

G_node findMinSpillCost() {
	double min = 100000;
	G_node minNode = NULL;
	for (G_nodeList nl = spillWorklist; nl; nl=nl->tail) {
		G_node n = nl->head;
		assert(!isPrecolored(n));
		double cost = *(double*)G_look(spillCost, n);
		if (cost < min) {
			min = cost;
			minNode = n;
		}
	}
	return minNode;
}

void selectSpill() {
	G_node n = findMinSpillCost(); 
	spillWorklist = G_nodeRemove(spillWorklist, n);
	simplifyWorklist = G_nodeAppend(simplifyWorklist, n);
	Live_moveList ml = nodeMoves(n);
	for (; ml; ml=ml->tail) {
		Live_move m = ml->head;
		G_node x = m->src;
		G_node y = m->dst;
		G_node v;
		if (getAlias(y) == getAlias(n)) {
			v = getAlias(x);
		} else {
			v = getAlias(y);
		}
		activeMoves = Live_moveRemove(activeMoves, m);
		frozenMoves = Live_moveAppend(frozenMoves, m);
		if (!nodeMoves(v) && lookupDegreeMap(v) < K) {
			freezeWorklist = G_nodeRemove(freezeWorklist, v);
			simplifyWorklist = G_nodeAppend(simplifyWorklist, v);
		}
	}
}

void assignColors() {
	for (G_nodeList nl = G_nodes(interferenceGraph); nl; nl=nl->tail) {
		G_node n = nl->head;
		if (isPrecolored(n)) {
			assignColor(n, Live_gtemp(n));
			coloredNodes = G_nodeAppend(coloredNodes, n);
		}
	}
	while (selectStack) {
		G_node n = selectStack->head;
	    selectStack = selectStack->tail;
		if (G_nodeIn(coloredNodes, n)) continue;
		Temp_tempList okColors = precolored;
		G_nodeList adj = G_adj(n);
		for (; adj; adj=adj->tail) {
			G_node w = adj->head;
			if (G_nodeIn(coloredNodes, getAlias(w)) || isPrecolored(getAlias(w))) {
				okColors = Temp_tempComplement(okColors, Temp_TempList(getColor(getAlias(w)), NULL));
			}
		}
		if (!okColors) {
			spilledNodes = G_nodeAppend(spilledNodes, n);
		} else {
			coloredNodes = G_nodeAppend(coloredNodes, n);
			assignColor(n, okColors->head);
		}
	}
	G_nodeList nl = coalescedNodes;
	for (; nl; nl=nl->tail) {
		G_node n = nl->head;
		assignColor(n, getColor(getAlias(n)));
	}
}

struct COL_result COL_color(G_graph ig, Temp_map initial, Temp_tempList regs, Live_moveList moves, G_table temp_to_moves, G_table cost) {
	struct COL_result ret;
	init(ig, regs, moves, temp_to_moves, cost);
	G_nodeList nodes = G_nodes(interferenceGraph);

	for (; nodes; nodes=nodes->tail) {
		G_node n = nodes->head;
		if (isPrecolored(n)) continue;
		if (lookupDegreeMap(n) >= K) {
			spillWorklist = G_nodeAppend(spillWorklist, n);
		} 
		else if (isMoveRelated(n)) {
			freezeWorklist = G_nodeAppend(freezeWorklist, n);
		} 
		else {
			simplifyWorklist = G_nodeAppend(simplifyWorklist, n);
		}
	}

	do {
		if (simplifyWorklist) 
		{
			simplify();
		} 
		else if (worklistMoves) 
		{
			coalesce();
		} 
		else if (freezeWorklist) 
		{
			freeze();
		} 
		else if (spillWorklist) 
		{
			selectSpill();
		}
	} 
	while (simplifyWorklist || worklistMoves || freezeWorklist || spillWorklist);

	assignColors();

	if (!spilledNodes) {
		Temp_map coloring = Temp_empty();
		G_nodeList nl = G_nodes(interferenceGraph);
		for (; nl; nl=nl->tail) {
			G_node n = nl->head;
			Temp_temp color = getColor(n);
			if (color) {
				Temp_enter(coloring, Live_gtemp(n), Temp_look(initial, color));
			}
		}
		ret.coloring = Temp_layerMap(coloring, initial);
	}

	Temp_tempList spills = NULL;
	for (G_nodeList nl = spilledNodes; nl; nl=nl->tail) {
		G_node n = nl->head;
		spills = Temp_TempList(Live_gtemp(n), spills);
	}
	ret.spills = spills;

	return ret;
}
