#include <stdlib.h>
#include "doublell.h"
#include <stdio.h>
struct dll_node* create(void *data) {
	struct dll_node* toRet = malloc(sizeof(struct dll_node));
	toRet->next = NULL;
	toRet->prev = NULL;
	toRet->data = data;	
	return toRet;
}

void add(struct dll_node* head, struct dll_node* toAdd) {
	struct dll_node* last;
	last = head;
	while(last->next != NULL) {
		last = last->next;
	}
	last->next = toAdd;
	toAdd->prev = last;
}

struct dll_node* delete(struct dll_node* toDelete) {
	struct dll_node* next = toDelete->next;
	struct dll_node* prev = toDelete->prev;
	if(prev != NULL) {
		prev->next = next;
	}
	if(next != NULL) {
		next->prev = prev;
	}
	free(toDelete);
	return (prev == NULL) ? next : prev;
}


