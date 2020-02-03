#include <stdlib.h>
#include <string.h>
#include "./fifo/fifo.h"

// https://udp-ip.hatenadiary.org/entry/20110302/1299039216

// Create FIFO
fifo_t *fifo_create(unsigned int size)
{
    fifo_t *inst;
    inst = (fifo_t *) malloc(sizeof(fifo_t));
    if (inst == NULL) return NULL;
    memset(inst, 0, sizeof(fifo_t));
    inst->buf = (fifo_data_t *) malloc(sizeof(fifo_data_t) * (size + 1));
    if (inst->buf == NULL) {
        free(inst);
        return NULL;
    }
    inst->size = size + 1;
    return inst;
}

// Delete FIFO
void fifo_delete(fifo_t *inst)
{
    free(inst->buf);
    free(inst);
}

// Get current FIFO count
unsigned int fifo_get_count(fifo_t *inst)
{
    unsigned int val;
    val = (inst->size + inst->write - inst->read) % inst->size;
    return val;
}

// Get current FIFO rest of count to write
unsigned int fifo_get_rest_count(fifo_t *inst)
{
    unsigned int val;
    val = (inst->size + inst->read - inst->write - 1) % inst->size;
    return val;
}

// Get FIFO is empty
int fifo_is_empty(fifo_t *inst)
{
    return (fifo_get_count(inst) == 0);
}

// Get FIFO is full
int fifo_is_full(fifo_t *inst)
{
    return (fifo_get_rest_count(inst) == 0);
}

// Write Single Data
int fifo_write(fifo_t *inst, fifo_data_t d)
{
    unsigned int next = (inst->write + 1) % inst->size;
    if (next == inst->read) return 0;

    *(inst->buf + inst->write) = d;
    inst->write = next;
    return 1;
}

// Write Array Data
int fifo_add_block(fifo_t *inst, fifo_data_t *ptr, unsigned int size)
{
    int n = fifo_get_rest_count(inst);
    int i;

    if (size < n) n = size;
    for (i = 0; i < n; i++) {
        inst->buf[(inst->write + i) % inst->size] = *(ptr + i);
    }
    inst->write = (inst->write + n) % inst->size;
    return n;
}

// Read Array Data
int fifo_get_block(fifo_t *inst, fifo_data_t *ptr, unsigned int size)
{
    int n = fifo_get_count(inst);
    int i;

    if (n > size) n = size;
    for (i = 0; i < n; i++) {
        *(ptr + i) = *(inst->buf + (inst->read + i) % inst->size);
    }
    inst->read = (inst->read + n) % inst->size;
    return n;
}

// Read Data
int fifo_read(fifo_t *inst, fifo_data_t *ptr)
{
    if (fifo_is_empty(inst)) return 0;
    *ptr = *(inst->buf + inst->read);
    inst->read = (inst->read + 1) % inst->size;
    return 1;
}