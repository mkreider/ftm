#include "fesa_if.h"

//base address
volatile unsigned int* fesa_if = (unsigned int*)0x200000;

//control & status registers
const unsigned int REG_CMD			= 0x00000000;	//cmd register for this cycle
const unsigned int REG_RST			= 0x00000004;   //writing to this reg will restart the program/cycle
const unsigned int REG_STAT			= 0x00000008;	//Shows status of all msg channels

//from here on we have a two page layout mem
const unsigned int ADR_OFFS_PAGE_B		= 0x00001000;	//mem page B offset 

//cyc registers
const unsigned int REG_CYC_CNT			= 0x0000000C;	//counter how often the cycle was executed already
const unsigned int REG_NUM_MSGS			= 0x00000010;	//Number of msgs used in this cycle

const unsigned int REG_CYC_START_TIME_HI	= 0x00000014;	//Time to begin cycle
const unsigned int REG_CYC_START_TIME_LO	= 0x00000018;	//
const unsigned int REG_CYC_PERIOD_HI		= 0x0000001C;	//cycle period
const unsigned int REG_CYC_PERIOD_LO		= 0x00000020;	// 
const unsigned int REG_CYC_REP			= 0x00000024;	//Number of times to execute cycle, -1 -> infinite
const unsigned int REG_CYC_EXEC_TIME_HI		= 0x00000010;	//
const unsigned int REG_CYC_EXEC_TIME_LO		= 0x00000014;	//


const unsigned int ADR_BASE_MSGS		= 0x00000040;	//msg feed base address. 
const unsigned int ADR_OFFS_NMSG		= 0x00000040;	//address FEEDet for n-th feed 

//msg registers
const unsigned int REG_MSG_CMD			= 0x00000000;	//command register
const unsigned int REG_MSG_CNT			= 0x00000004;	//counter how often this was executed already
const unsigned int REG_MSG_OFFS_EXEC_TIME_HI	= 0x00000008;	//Time offset to the beginning of the cycle
const unsigned int REG_MSG_OFFS_EXEC_TIME_LO	= 0x0000000C;	//

const unsigned int REG_MSG_EXEC_TIME_HI		= 0x00000010;	//Time offset to the beginning of the cycle
const unsigned int REG_MSG_EXEC_TIME_LO		= 0x00000014;	//
const unsigned int REG_MSG_ID_HI		= 0x00000018;	//message ID
const unsigned int REG_MSG_ID_LO		= 0x0000001C;	//
const unsigned int REG_MSG_PARAM		= 0x00000020;	//Parameter for ECA




//masks & constants
const unsigned int CMD_CYC_START		= 0x00000001;	//Start timing msg program
const unsigned int CMD_CYC_STOP			= 0x00000002;	//Stop timing msg program safely
const unsigned int CMD_CYC_STOP_I		= 0x00000004;	//Stop timing msg program immediately
const unsigned int CMD_CYC_RST			= 0x00000008;	//Rst this CPU
const unsigned int CMD_CYC_PAGESWAP		= 0x00000010;	//Use mempage A/B

const unsigned int CMD_MSG_USE			= 0x00000001;	//Start this msg feed
const unsigned int CMD_MSG_CLR			= 0x00000002;	//Stop this msg feed
const unsigned int CMD_MSG_CLR_CNT		= 0x00000004;	//rst msg counter

const unsigned int STAT_CYC_ACTIVE		= 0x10000000;	//shows cycle is active/inactive
const unsigned int STAT_CYC_MEMPAGE_B		= 0x20000000;	//using mem page A when 0, B when 1
const unsigned int STAT_CYC_RUNNING		= 0x40000000;	//shows if cycle is currently running/idle
const unsigned int STAT_CYC_WAITING		= 0x80000000;	//shows if cycle is waiting for condition
const unsigned int STAT_MSGS			= 0x00FFFFFF;	//shows use status of all msgs  





unsigned int fesa_get(unsigned int offset)
{
	return *(fesa_if + (offset>>2));
}

void fesa_set(unsigned int offset, unsigned int value)
{
	*(fesa_if + (offset>>2)) = value;
}

void fesa_clear_bit(unsigned int offset, unsigned int mask)
{
	*(fesa_if + (offset>>2)) &= ~mask;
}

void fesa_set_bit(unsigned int offset, unsigned int mask)
{
	*(fesa_if + (offset>>2)) |= mask;
}
