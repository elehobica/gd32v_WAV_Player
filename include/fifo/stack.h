#ifndef _STACK_H_
#define _STACK_H_

#include <stdint.h>
#define SIZE 5

typedef struct {
    uint16_t head;
    uint16_t column;
} stack_data_t;

typedef struct {
	stack_data_t data[SIZE];
	int tail;
} stack_t;

stack_t* stack_init();
int stack_push(stack_t *pStack, stack_data_t *item);
int stack_pop(stack_t *pStack, stack_data_t *item);
void stack_delete(stack_t *pStack);

#endif /* _STACK_H_ */