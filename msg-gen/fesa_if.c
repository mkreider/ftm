#include "fesa_if.h"

unsigned int fesa_get(unsigned int offset)
{
	return *(fesa_if + (offset>>2));
}

void fesa_set(unsigned int offset, unsigned int value)
{
	*(fesa_if + (offset>>2)) = value;
}

void fesa_clr_bit(unsigned int offset, unsigned int mask)
{
	*(fesa_if + (offset>>2)) &= ~mask;
}

void fesa_set_bit(unsigned int offset, unsigned int mask)
{
	*(fesa_if + (offset>>2)) |= mask;
}

void fesa_inc(unsigned int offset)
{
	*(fesa_if + (offset>>2)) = *(fesa_if + (offset>>2))+1;
}
