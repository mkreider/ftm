#ifndef _FESA_IF_H_
#define _FESA_IF_H_

extern volatile unsigned int* fesa_if;


//control & status registers
extern const unsigned int REG_CMD;			//cmd register for this cycle
extern const unsigned int REG_RST;   			//writing to this reg will restart the program/cycle
extern const unsigned int REG_STAT;			//Shows status of all msg channels
extern const unsigned int REG_CYC_CNT;			//counter how often the cycle was executed already
extern const unsigned int REG_NUM_MSG;		//Number of msgs used in this cycle

extern const unsigned int REG_CYC_START_TIME_HI;	//Time to begin cycle
extern const unsigned int REG_CYC_START_TIME_LO;	//
extern const unsigned int REG_CYC_PERIOD_HI;		//cycle period
extern const unsigned int REG_CYC_PERIOD_LO;		// 
extern const          int REG_CYC_REP;			//Number of times to execute cycle, -1 -> infinite

extern const unsigned int ADR_BASE_MSGS;		//msg feed base address. 
extern const unsigned int ADR_OFFS_NMSG;		//address FEEDet for n-th feed 

//msg registers
extern const unsigned int REG_MSG_CMD;			//command register
extern const unsigned int REG_MSG_CNT;			//counter how often this was executed already
extern const unsigned int REG_MSG_ID_HI;		//message ID
extern const unsigned int REG_MSG_ID_LO;		//
extern const unsigned int REG_MSG_OFFS_EXEC_TIME_HI;	//Time FEEDet to begin sequence
extern const unsigned int REG_MSG_OFFS_EXEC_TIME_LO;	//
extern const unsigned int REG_MSG_PARAM;		//Parameter for ECA

//masks & constants
extern const unsigned int CMD_CYC_START;		//Start timing msg program
extern const unsigned int CMD_CYC_STOP;			//Stop timing msg program safely
extern const unsigned int CMD_CYC_STOP_I;		//Stop timing msg program immediately
extern const unsigned int CMD_CYC_RST;			//Rst this CPU

extern const unsigned int CMD_MSG_USE;			//Start this msg feed
extern const unsigned int CMD_MSG_CLR;			//Stop this msg feed
extern const unsigned int CMD_MSG_CLR_CNT;		//rst msg counter

extern const unsigned int STAT_CYC_ACTIVE;		//shows cycle is active/inactive
extern const unsigned int STAT_CYC_RUNNING;		//shows if cycle is currently running/idle
extern const unsigned int STAT_CYC_WAITING;		//shows if cycle is waiting for condition
extern const unsigned int STAT_MSGS;			//shows use status of all msgs  
	

unsigned int 	fesa_get(unsigned int offset);
void 		fesa_set(unsigned int offset, unsigned int value);
void 		fesa_clear_bit(unsigned int offset, unsigned int mask);
void 		fesa_set_bit(unsigned int offset, unsigned int mask);


#endif 

