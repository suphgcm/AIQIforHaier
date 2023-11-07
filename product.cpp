#include "product.h"

ProcessUnit* insertProcessUnit(ProcessUnit* listhead, ProcessUnit* node) {
	ProcessUnit* find = listhead->nextunit;
	while (find != listhead && node->laterncy > find->laterncy) {
		find = find->nextunit;
	}

	find->prevunit->nextunit = node;
	node->nextunit = find;
	node->prevunit = find->prevunit;
	find->prevunit = node;
	return listhead;
}