#ifndef _CFIFO_H_
#define _CFIFO_H_

// FIFO data type
typedef char cfifo_data_t[256];

// FIFO instance struct
typedef struct {
    cfifo_data_t *buf;
    unsigned int size;
    unsigned int read;
    unsigned int write;
} cfifo_t;

// Function Prototype
cfifo_t *cfifo_create(unsigned int size);
void cfifo_delete(cfifo_t *inst);
int cfifo_is_empty(cfifo_t *inst);
int cfifo_is_full(cfifo_t *inst);
int cfifo_write(cfifo_t *inst, cfifo_data_t ptr);
int cfifo_read(cfifo_t *inst, cfifo_data_t ptr);

unsigned int cfifo_get_count(cfifo_t *inst);
unsigned int cfifo_get_rest_count(cfifo_t *inst);

#endif /* _CFIFO_H_ */