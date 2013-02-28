#include "fesa_if.h"
#include "display.h"
#include <stdint.h>

#define DEBUG_RUN 1

volatile unsigned int* leds = (unsigned int*)0x100400;
volatile unsigned int* display = (unsigned int*)0x140000;

volatile unsigned int* tx_ctrl 	= (unsigned int*)0x100400;
const unsigned int REG_P_TX_WR 	= 0x00000000;
const unsigned int REG_P_TX_RD 	= 0x00000004;
const unsigned int P_TX_MSK 	= 0x000003FF;
const unsigned int P_TX_WRAP 	= 0x00000400;

const unsigned char BUF_OK	= 1;
const unsigned char BUF_FULL	= 0;


volatile unsigned int* tx_buffer = (unsigned int*)0x100400;



void _irq_entry(void) {
  /* Currently only triggered by DMA completion */
}


const char mytext[] = "I am alive!...\n\n";

void main(void) {
  unsigned int i, j;




const uintptr_t P_TX_MSK_W_WRAP = 0x0000001F;
const uintptr_t P_TX_MSK 	= 0x0000000F;
const uintptr_t P_TX_WRAP 	= 0x00000010;

const uintptr_t BUF_CAPACITY 	= P_TX_MSK;

const uint8_t BUF_OK	= 1;
const uint8_t BUF_FULL	= 0;



uint8_t tx_write(void* dest, const void* toSend, uintptr_t* wr_offs, const uintptr_t* rd_offs, size_t length)
{
	
	uint8_t wraparound, buffer_status;
	size_t space_b4_end;

	uintptr_t end_offs = (*wr_offs + length) & P_TX_MSK_W_WRAP;

	//check if there is eneough space in the TX buffer
	
	if((end_offs & P_TX_WRAP) ^ (*rd_offs & P_TX_WRAP)) 	wraparound = 1;
	else 							wraparound = 0;
	
	buffer_status = BUF_FULL;
	if(wraparound) 	{if( (end_offs & P_TX_MSK) < (*rd_offs & P_TX_MSK) ) buffer_status = BUF_OK;}
	else	   	{if( (end_offs & P_TX_MSK) > (*rd_offs & P_TX_MSK) ) buffer_status = BUF_OK;}

	if(buffer_status == BUF_OK)
	{
		if(wraparound) // does the transfer wrap around ? 
		{
			//yes, do it in two steps			
			space_b4_end = buf_end - (*wr_offs & P_TX_MSK); 
		
			memcpy( dest + (*wr_offs &  P_TX_MSK), (void*)toSend, space_b4_end);
			memcpy( dest, (void*)(toSend+space_b4_end), length - space_b4_end);
		}		
		else // no, do it in one
		{
			memcpy( dest + (*wr_offs &  P_TX_MSK), (void*)toSend, length);
		}
		*wr_offs = end_offs & P_TX_MSK_W_WRAP;
		
		return length;	
	} else  return 0;
}



int main(void)
{
  
  uint64_t t_now, t_due, t_overdue, t_cyc_exec, t_msg_offs, t_period;

#if DEBUG_RUN

	//cyc registers
	fesa_set(REG_CYC_T_TRANSMIT, 	    200000); //200us transmit time
	fesa_set(REG_CYC_T_MARGIN, 	      5000); //5us margin
	fesa_set(REG_CYC_CNT, 		0x00000000); 
	fesa_set(REG_NUM_MSGS, 		0x00000003); // 5 msgs
	fesa_set(REG_CYC_MSG_SENT, 	0x00000000); 

	fesa_set(REG_CYC_START_TIME_HI,		 0); //
	fesa_set(REG_CYC_START_TIME_LO,	1000000000); // start @ 1s 
	fesa_set(REG_CYC_PERIOD_HI,		 0); // 
	fesa_set(REG_CYC_PERIOD_LO,	 500000000); // period 0.5s 
	fesa_set(REG_CYC_REP,			20); // 20x
	fesa_set(REG_CYC_EXEC_TIME_HI,		 0); //
	fesa_set(REG_CYC_EXEC_TIME_LO,	1000000000); // exec @ 1s

	//msg 1
	fesa_set(ADR_BASE_MSGS + 0*ADR_OFFS_NMSG + REG_MSG_CMD,		  	0x00000000;	
	fesa_set(ADR_BASE_MSGS + 0*ADR_OFFS_NMSG + REG_MSG_CNT,		   	0x00000000;	
	fesa_set(ADR_BASE_MSGS + 0*ADR_OFFS_NMSG + REG_MSG_OFFS_EXEC_TIME_HI,	         0;	
	fesa_set(ADR_BASE_MSGS + 0*ADR_OFFS_NMSG + REG_MSG_OFFS_EXEC_TIME_LO,           80;	// 80ns

	fesa_set(ADR_BASE_MSGS + 0*ADR_OFFS_NMSG + REG_MSG_EXEC_TIME_HI,	0x00000000;	
	fesa_set(ADR_BASE_MSGS + 0*ADR_OFFS_NMSG + REG_MSG_EXEC_TIME_LO,   	0x00000000;	
	fesa_set(ADR_BASE_MSGS + 0*ADR_OFFS_NMSG + REG_MSG_ID_HI, 	 	0xCAFE0123;	
	fesa_set(ADR_BASE_MSGS + 0*ADR_OFFS_NMSG + REG_MSG_ID_LO, 	 	0x45670001;	
	fesa_set(ADR_BASE_MSGS + 0*ADR_OFFS_NMSG + REG_MSG_PARAM, 	 	0xDEADBEE1;	

	//msg 2
	fesa_set(ADR_BASE_MSGS + 1*ADR_OFFS_NMSG + REG_MSG_CMD,		   	0x00000000;	
	fesa_set(ADR_BASE_MSGS + 1*ADR_OFFS_NMSG + REG_MSG_CNT,		   	0x00000000;	
	fesa_set(ADR_BASE_MSGS + 1*ADR_OFFS_NMSG + REG_MSG_OFFS_EXEC_TIME_HI,	         0;	
	fesa_set(ADR_BASE_MSGS + 1*ADR_OFFS_NMSG + REG_MSG_OFFS_EXEC_TIME_LO,         3000;	// 3us

	fesa_set(ADR_BASE_MSGS + 1*ADR_OFFS_NMSG + REG_MSG_EXEC_TIME_HI,	0x00000000;	
	fesa_set(ADR_BASE_MSGS + 1*ADR_OFFS_NMSG + REG_MSG_EXEC_TIME_LO,   	0x00000000;	
	fesa_set(ADR_BASE_MSGS + 1*ADR_OFFS_NMSG + REG_MSG_ID_HI, 	 	0xBABE0123;	
	fesa_set(ADR_BASE_MSGS + 1*ADR_OFFS_NMSG + REG_MSG_ID_LO, 	 	0x45670002;	
	fesa_set(ADR_BASE_MSGS + 1*ADR_OFFS_NMSG + REG_MSG_PARAM, 	 	0xDEADBEE2;	

	//msg 3
	fesa_set(ADR_BASE_MSGS + 2*ADR_OFFS_NMSG + REG_MSG_CMD,		   	0x00000000;	
	fesa_set(ADR_BASE_MSGS + 2*ADR_OFFS_NMSG + REG_MSG_CNT,		   	0x00000000;	
	fesa_set(ADR_BASE_MSGS + 2*ADR_OFFS_NMSG + REG_MSG_OFFS_EXEC_TIME_HI,	         0;	
	fesa_set(ADR_BASE_MSGS + 2*ADR_OFFS_NMSG + REG_MSG_OFFS_EXEC_TIME_LO,    100000000;	// 100ms

	fesa_set(ADR_BASE_MSGS + 2*ADR_OFFS_NMSG + REG_MSG_EXEC_TIME_HI,	0x00000000;	
	fesa_set(ADR_BASE_MSGS + 2*ADR_OFFS_NMSG + REG_MSG_EXEC_TIME_LO,   	0x00000000;	
	fesa_set(ADR_BASE_MSGS + 2*ADR_OFFS_NMSG + REG_MSG_ID_HI, 	 	0xC01A0123;	
	fesa_set(ADR_BASE_MSGS + 2*ADR_OFFS_NMSG + REG_MSG_ID_LO, 	 	0x45670003;	
	fesa_set(ADR_BASE_MSGS + 2*ADR_OFFS_NMSG + REG_MSG_PARAM, 	 	0xDEADBEE2;	

	fesa_set_bit(REG_CMD, CMD_CYC_START);
#endif




  disp_reset();	
  disp_put_c('\f');
  disp_put_disp_put_str(mytext);	
	
  while (1) {

	// read the command register
	cmd = fesa_get(REG_CMD);
	conditional_not_met = false;
	
	//start/stop the cycle
	if(cmd & (CMD_CYC_STOP | CMD_CYC_STOP_I) 	fesa_clear_bit(REG_STAT, STAT_CYC_ACTIVE); 
	else if(cmd & (CMD_CYC_START)			fesa_set_bit(REG_STAT, STAT_CYC_ACTIVE);	

	//do pageswap on request...
	if(cmd & CMD_CYC_PAGESWAP) {p_offs ^= ADR_OFFS_PAGE_B;
	//...and update status	
	if(p_offs) 			fesa_set_bit(REG_STAT, STAT_CYC_MEMPAGE_B);
	else				fesa_clr_bit(REG_STAT, STAT_CYC_MEMPAGE_B);
		
	msg_base_adr 	= p_offs + ADR_BASE_MSGS; 
	t_period	= ((fesa_get(p_offs+REG_CYC_PERIOD_HI) << 32) 		| fesa_get(p_offs+REG_CYC_PERIOD_LO));	
	
	if(fesa_get(REG_STAT) & STAT_CYC_ACTIVE)
	{
		//process cycle if there are repetitions left or should run infinitely (reps = 0)
		while( (fesa_get(p_offs+REG_CYC_CNT) < fesa_get(p_offs+REG_CYC_REP)) || ~fesa_get(p_offs+REG_CYC_REP))			
		{	
			//abort command detected ?
			if(fesa_get(REG_CMD) & (CMD_CYC_STOP | CMD_CYC_STOP_I)) fesa_clear_bit(REG_STAT, STAT_CYC_ACTIVE); 
			else 							fesa_set_bit(REG_STAT, STAT_CYC_ACTIVE);
		
			error = false;
			//clr sent message flags			
			fesa_set(p_offs+REG_CYC_MSG_SENT, 0x00000000);

			while( ( (fesa_get(p_offs+REG_STAT) & STAT_MSGS) ^ fesa_get(p_offs+REG_CYC_MSG_SENT)) && ~error && ~fesa_get(REG_STAT, STAT_CYC_ACTIVE) ) 
			{
				//get next cycle execution time			
				t_cyc_exec	= ((fesa_get(p_offs+REG_CYC_EXEC_TIME_HI) << 32) 	| fesa_get(p_offs+REG_CYC_EXEC_TIME_HI));			
			
				for(i=0; i<fesa_get(p_offs+REG_NUM_MSGS); i++)
				{	
					if( ~(fesa_get(p_offs+REG_CYC_MSG_SENT) & (1<<i)) ) //if not already sent
					{
						//abort immediately command detected ?				
						if(fesa_get(REG_CMD) & CMD_CYC_STOP_I) 	
						{
							fesa_clear_bit(REG_STAT, STAT_CYC_ACTIVE);
							break;
						}	
						else	fesa_set_bit(REG_STAT, STAT_CYC_ACTIVE);				
					
						//
						//               t_now
						//---------------|--------------------------------------------------------
						//----------|-------------|------------------------|-----------------|----
						//          t_margin  <-  t_transmit     <-        t_cyc_exec    <-  t_msg_offs
						//          |             |		
						//          t_due         t_overdue  	 					

					
						// Calculate all auxiliary times
						t_msg_offs = 	((fesa_get(msg_base_adr + i*ADR_OFFS_NMSG + REG_MSG_OFFS_EXEC_TIME_HI) << 32) 
							      |   fesa_get(msg_base_adr + i*ADR_OFFS_NMSG + REG_MSG_OFFS_EXEC_TIME_LO));				
						t_msg_exec	= t_cyc_exec + t_msg_offs;						
						t_overdue 	= t_msg_exec - fesa_get(p_offs+REG_CYC_T_TRANSMIT);  			
						t_due 		= t_overdue - fesa_get(p_offs+REG_CYC_T_MARGIN);
						t_now 		= get_current_time();	 
							     		
						if(t_now >= t_due) //time to act yet?
						{ 
				 			if(t_now < t_overdue)	// still enough time ?	
							{
								if(conditional_not_met) {}
								else 
								{
									fesa_set(msg_base_adr + REG_MSG_EXEC_TIME_HI, (t_msg_exec)>>32);
									fesa_set(msg_base_adr + REG_MSG_EXEC_TIME_LO, (0x00000000FFFFFFFF & t_msg_exec));			
									//write to tx buffer
									if(tx_write(ADR_MSG_START, 20))									
									{	
										//mark as sent						                
										fesa_set_bit((p_offs+REG_CYC_MSG_SENT), (1<<i)); 
										//update msg count						
										fesa_inc(msg_base_adr + i*ADR_OFFS_NMSG + REG_MSG_CNT);
									}	
								}
										
									
							}					
							else error = true; //error, we're too late
						}
					}
				}		
			}
		
			//update next cycle execution time		
			t_cyc_exec += t_period;
			fesa_set(p_offs+REG_CYC_EXEC_TIME_HI, (t_cyc_exec)>>32);
			fesa_set(p_offs+REG_CYC_EXEC_TIME_LO, (0x00000000FFFFFFFF & t_cyc_exec));
			//update cycle count
			fesa_inc(p_offs+REG_CYC_CNT);
			
		}		
			
	}		

	fesa_set(REG_CMD, 0x00000000);	//make command register write only
		

		

    /* Rotate the LEDs */
    for (i = 0; i < 8; ++i) {
      *leds = 1 << i;
	
	
	

      /* Each loop iteration takes 4 cycles.
       * It runs at 125MHz.
       * Sleep 0.2 second.
       */
      for (j = 0; j < 125000000/160; ++j) {
        asm("# noop"); /* no-op the compiler can't optimize away */
      }

	if(time++ > 500) {time = 0; color = ~color; }
	
    }
  }
}
