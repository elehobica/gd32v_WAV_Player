#include <stdlib.h>
#include <string.h>
#include "cfifo.h"

// Create FIFO
cfifo_t *cfifo_create(unsigned int size)
{
    cfifo_t *inst;
    inst = (cfifo_t *) malloc(sizeof(cfifo_t));
    if (inst == NULL) return NULL;
    memset(inst, 0, sizeof(cfifo_t));
    inst->buf = (cfifo_data_t *) malloc(sizeof(cfifo_data_t) * (size + 1));
    if (inst->buf == NULL) {
        free(inst);
        return NULL;
    }
    inst->size = size + 1;
    return inst;
}

// Delete FIFO
void cfifo_delete(cfifo_t *inst)
{
    free(inst->buf);
    free(inst);
}

// Get current FIFO count
unsigned int cfifo_get_count(cfifo_t *inst)
{
    unsigned int val;
    val = (inst->size + inst->write - inst->read) % inst->size;
    return val;
}

// Get current FIFO rest of count to write
unsigned int cfifo_get_rest_count(cfifo_t *inst)
{
    unsigned int val;
    val = (inst->size + inst->read - inst->write - 1) % inst->size;
    return val;
}

// Get FIFO is empty
int cfifo_is_empty(cfifo_t *inst)
{
    return (cfifo_get_count(inst) == 0);
}

// Get FIFO is full
int cfifo_is_full(cfifo_t *inst)
{
    return (cfifo_get_rest_count(inst) == 0);
}

// Write Single Data
int cfifo_write(cfifo_t *inst, char *ptr)
{
    unsigned int next = (inst->write + 1) % inst->size;
    if (next == inst->read) return 0;
    memcpy(inst->buf + inst->write, ptr, sizeof(cfifo_data_t));
    inst->write = next;
    return 1;
}

// Read Data
int cfifo_read(cfifo_t *inst, char *ptr)
{
    if (cfifo_is_empty(inst)) return 0;
    memcpy(ptr, inst->buf + inst->read, sizeof(cfifo_data_t));
    inst->read = (inst->read + 1) % inst->size;
    return 1;
}
