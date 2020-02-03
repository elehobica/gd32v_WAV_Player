#ifndef _FIFO_H_
#define _FIFO_H_

// FIFO data type
typedef unsigned long fifo_data_t;

// FIFO instance struct
typedef struct {
    fifo_data_t *buf;
    unsigned int size;
    unsigned int read;
    unsigned int write;
} fifo_t;

// Function Prototype
fifo_t *fifo_create(unsigned int size);
void fifo_delete(fifo_t *inst);
int fifo_is_empty(fifo_t *inst);
int fifo_is_full(fifo_t *inst);
int fifo_write(fifo_t *inst, fifo_data_t  d);
int fifo_read(fifo_t *inst, fifo_data_t *ptr);

unsigned int fifo_get_count(fifo_t *inst);
unsigned int fifo_get_rest_count(fifo_t *inst);
int fifo_add_block(fifo_t *inst, fifo_data_t *ptr, unsigned int size);
int fifo_get_block(fifo_t *inst, fifo_data_t *ptr, unsigned int size);

#endif /* _FIFO_H_ */