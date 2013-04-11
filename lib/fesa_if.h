#ifndef _FESA_IF_H_
#define _FESA_IF_H_

extern volatile unsigned int* fesa_if;

//control & status registers

#define ADR_BASE              0	
#define REG_CMD               ADR_BASE                   //cmd register for this cycle
#define REG_MEMPAGE_ACTIVE    (REG_CMD+4)                //writing to this reg will restart the program/cycle
#define REG_MEMPAGE_INACTIVE  (REG_MEMPAGE_ACTIVE+4)     //writing to this reg will restart the program/cycle

//from here on, it depends on the active mempage !!!
#define ADR_PAGE              ADR_BASE+12
#define REG_CYC_STAT			    ADR_PAGE                    //Shows status of all msg channels
#define REG_CYC_CNT           (REG_CYC_STAT+4)                //counter how often the cycle was executed already
#define REG_CYC_MSG_SENT      (REG_CYC_CNT+4)             //

#define REG_NUM_MSGS          (REG_CYC_MSG_SENT+4)        //Number of msgs used in this cycle


#define REG_CYC_T_TRANSMIT    (REG_NUM_MSGS+4) 
#define REG_CYC_T_MARGIN      (REG_CYC_T_TRANSMIT+4)    
#define REG_CYC_EXEC_TIME_HI  (REG_CYC_T_MARGIN+4)        //Time to begin cycle
#define REG_CYC_EXEC_TIME_LO  (REG_CYC_EXEC_TIME_HI+4)    //
#define REG_CYC_PERIOD_HI     (REG_CYC_EXEC_TIME_LO+4)    //cycle period
#define REG_CYC_PERIOD_LO     (REG_CYC_PERIOD_HI+4)       // 
#define REG_CYC_REP           (REG_CYC_PERIOD_LO+4)       //Number of times to execute cycle, -1 -> infinite

#define END_CTRL              REG_CYC_REP

#define ADR_BASE_MSGS         END_CTRL+4                  //msg feed base address. 

//msg registers
#define REG_MSG_CMD           0                           //command register
#define REG_MSG_CNT           (REG_MSG_CMD+4)             //counter how often this was executed already
#define REG_MSG_OFFS_TIME_HI  (REG_MSG_CNT+4)             //Time offset to the beginning of the cycle
#define REG_MSG_OFFS_TIME_LO  (REG_MSG_OFFS_TIME_HI+4)    //

//This is the actual msg block for the dispatcher
#define START_MSG             (REG_MSG_OFFS_TIME_LO+4)
#define REG_MSG_ID_HI         START_MSG                   //message ID
#define REG_MSG_ID_LO         (REG_MSG_ID_HI+4)           //
#define REG_MSG_EXEC_TIME_HI  (REG_MSG_ID_LO+4)           //Time offset to the beginning of the cycle
#define REG_MSG_EXEC_TIME_LO  (REG_MSG_EXEC_TIME_HI+4)    //
#define REG_MSG_PARAM         (REG_MSG_EXEC_TIME_LO+4)    //Parameter for ECA
#define END_MSG               REG_MSG_PARAM

#define ADR_OFFS_NMSG         (END_MSG-REG_MSG_CMD+4)     //address offset for n-th msg 

//masks & constants
#define CMD_RST           		0x10000000	//Reset FTM status and counters
#define CMD_PAGESWAP      		0x20000000	//Use mempage A/B

#define CMD_CYC_START         0x01000000	//Start timing msg program
#define CMD_CYC_DBG           0x02000000	//Run Cycle in  debug mode (start time will be corrected if in the past, no error detection)
#define CMD_CYC_STOP          0x04000000	//Stop timing msg program safely
#define CMD_CYC_STOP_I        0x08000000	//Stop timing msg program immediately


#define STAT_CYC_ERROR      	0x08000000	//error occured during cycle execution
#define STAT_CYC_DBG          0x10000000	//shows cycle debug mode is active/inactive
#define STAT_CYC_ACTIVE       0x20000000	//shows cycle is active/inactive
#define STAT_CYC_MEMPAGE_B    0x40000000	//using mem page A when 0, B when 1
#define STAT_CYC_WAITING      0x80000000	//shows if cycle is waiting for condition






unsigned int 	fesa_get(unsigned int offset);
void  fesa_set(unsigned int offset, unsigned int value);
void 	fesa_clr_bit(unsigned int offset, unsigned int mask);
void 	fesa_set_bit(unsigned int offset, unsigned int mask);
void  fesa_inc(unsigned int offset);

#endif 

