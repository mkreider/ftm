#include "inttypes.h"
#include <string.h>
#include "fesa_if.h"


#define DEBUG_RUN 1

volatile unsigned int* fesa_if = (unsigned int*)0x200000; 
volatile uint32_t* tx_ctrl 	= (uint32_t*)0x100400;
volatile uint32_t* tx_buffer = (uint32_t*)0x100400;

#define REG_P_TX_WR 	   0
#define REG_P_TX_RD 	   (REG_P_TX_WR+4)

#define BUF_SIZE        0x000000400
#define P_TX_MSK 	      (BUF_SIZE-1)
#define P_TX_WRAP 	   BUF_SIZE
#define P_TX_MSK_WRAP 	(P_TX_MSK | P_TX_WRAP)

const uint8_t BUF_OK	= 1;
const uint8_t BUF_FULL	= 0;

typedef uintptr_t size_t; 





void _irq_entry(void) {
  // Currently only triggered by DMA completion
}



uint64_t get_current_time()
{
   uint32_t nsec;	
   uint64_t sec, nTimeStamp;

   //pps_gen_get_time(&sec, &nsec);
   
   nTimeStamp 	= sec & 0xFFFFFFFFFFULL;
   nTimeStamp  *= (1000000000/8);
   nTimeStamp  += (nsec/8);

   return nTimeStamp;
}

uint8_t tx_write(void* dest, const void* src, volatile uintptr_t* wr_offs, volatile uintptr_t* rd_offs, size_t length)
{
	
	uint8_t wraparound, buffer_status;
	size_t space_b4_end;
	uintptr_t end_offs = (*wr_offs + length) & (uintptr_t)P_TX_MSK_WRAP;

	//check if there is eneough space in the TX buffer
	
	if((end_offs & (uintptr_t)P_TX_WRAP) ^ (*rd_offs & (uintptr_t)P_TX_WRAP)) 	wraparound = 1;
	else 							wraparound = 0;
	

    
	buffer_status = BUF_FULL;
	if(wraparound) 	{if( (end_offs & (uintptr_t)P_TX_MSK) < (*rd_offs & (uintptr_t)P_TX_MSK) ) buffer_status = BUF_OK;}
	else	   	{if( (end_offs & (uintptr_t)P_TX_MSK) > (*rd_offs & (uintptr_t)P_TX_MSK) ) buffer_status = BUF_OK;}

	if(buffer_status == BUF_OK)
	{
		
      if(wraparound) // does the transfer wrap around ? 
		{
		
         //yes, do it in two steps			
			space_b4_end = (uintptr_t)BUF_SIZE - (*wr_offs & (uintptr_t)P_TX_MSK); 
	      memcpy( dest + (*wr_offs &  (uintptr_t)P_TX_MSK), (void*)src, space_b4_end);
			memcpy( dest, (void*)(src+space_b4_end), length - space_b4_end);
		}		
		else // no, do it in one
		{

			memcpy( dest + (*wr_offs &  (uintptr_t)P_TX_MSK), (void*)src, length);
		}
		*wr_offs = end_offs & (uintptr_t)P_TX_MSK_WRAP;

		return length;	
	} else
   {
       
      return 0;

   }
}

uint8_t acc_cycle( uintptr_t* tx_ctrl, uint32_t* tx_buffer)
{   
   uint64_t t_now, t_due, t_overdue, t_cyc_exec, t_msg_exec, t_msg_offs, t_period;	
   uint32_t cmd, tmp;
   uint8_t conditional_not_met, error;
   static uintptr_t p_offs, msg_base_adr;

   volatile uintptr_t* wr_offs;
   volatile uintptr_t* rd_offs;
   void* src;
   void* dest;
   
   uint8_t i, j, msgs_sent_this_time;

   wr_offs = ((uintptr_t*)tx_ctrl + ((uintptr_t) REG_P_TX_WR>>2));
   rd_offs = ((uintptr_t*)tx_ctrl + ((uintptr_t) REG_P_TX_RD>>2));
   dest    = (void*)(tx_buffer); 

   // read the command register
	cmd = fesa_get(REG_CMD);
	conditional_not_met = 0;
	error = 0;
   msgs_sent_this_time = 0;

 
	//start/stop the cycle
	if(cmd & (CMD_CYC_STOP | CMD_CYC_STOP_I)) 	{fesa_clr_bit(p_offs + REG_STAT, STAT_CYC_ACTIVE);}
	else if(cmd & (CMD_CYC_START))			      {fesa_set_bit(p_offs + REG_STAT, STAT_CYC_ACTIVE);}

	//do pageswap on request...

   p_offs = fesa_get(REG_MEMPAGE);
   if(cmd & CMD_CYC_PAGESWAP) {p_offs ^= ADR_OFFS_PAGE_B; fesa_set(REG_MEMPAGE, p_offs);}
   
   if(cmd & CMD_MSG_USE)        {fesa_set_bit(p_offs + REG_STAT, cmd & STAT_MSGS);}    
   else if (cmd & CMD_MSG_CLR)  {fesa_clr_bit(p_offs + REG_STAT, cmd & STAT_MSGS);}          

	msg_base_adr 	= p_offs + ADR_BASE_MSGS; 
	t_period	= (((uint64_t)(fesa_get(p_offs+REG_CYC_PERIOD_HI)) << 32) 		| (uint64_t)fesa_get(p_offs+REG_CYC_PERIOD_LO));	
	
   if(fesa_get(p_offs+REG_STAT) & STAT_CYC_ACTIVE)
	{
		//process cycle if there are repetitions left or should run infinitely (reps = 0)
		while( (fesa_get(p_offs+REG_CYC_CNT) < fesa_get(p_offs+REG_CYC_REP)) || fesa_get(p_offs+REG_CYC_REP)==-1)			
		{	
         //abort command detected ?
			if(fesa_get(REG_CMD) & (CMD_CYC_STOP | CMD_CYC_STOP_I)) fesa_clr_bit(p_offs+REG_STAT, STAT_CYC_ACTIVE); 
			else 							fesa_set_bit(p_offs + REG_STAT, STAT_CYC_ACTIVE);
		
			error = 0;
			//clr sent message flags			
			fesa_set(p_offs+REG_CYC_MSG_SENT, 0);

        

         while( ( (fesa_get(p_offs+REG_STAT) & STAT_MSGS) ^ fesa_get(p_offs+REG_CYC_MSG_SENT)) && !error && (fesa_get(p_offs + REG_STAT) & STAT_CYC_ACTIVE) ) 
			{
				//get next cycle execution time			
				t_cyc_exec	= (((uint64_t)(fesa_get(p_offs+REG_CYC_EXEC_TIME_HI)) << 32) 	| fesa_get(p_offs+REG_CYC_EXEC_TIME_LO));			
			   
            for(i=0; fesa_get(p_offs + REG_STAT)& STAT_MSGS & (1<<i) ; i++)
				{	
              if( !((fesa_get(p_offs+REG_CYC_MSG_SENT) & STAT_MSGS) & (1<<i)) ) //if not already sent
					{
						//abort immediately command detected ?				
						if(fesa_get(REG_CMD) & CMD_CYC_STOP_I) 	
						{
							fesa_clr_bit(p_offs + REG_STAT, STAT_CYC_ACTIVE);
							break;
						}	
						else	fesa_set_bit(p_offs + REG_STAT, STAT_CYC_ACTIVE);				
					
						//
						//               t_now
						//---------------|--------------------------------------------------------
						//----------|-------------|------------------------|-----------------|----
						//          t_margin  <-  t_transmit     <-        t_cyc_exec    <-  t_msg_offs
						//          |             |		
						//          t_due         t_overdue  	 					

					   // Calculate all auxiliary times
						t_msg_offs = ((((uint64_t)fesa_get(msg_base_adr + i*ADR_OFFS_NMSG + REG_MSG_OFFS_TIME_HI)) << 32) 
							      |   fesa_get(msg_base_adr + i*ADR_OFFS_NMSG + REG_MSG_OFFS_TIME_LO));				
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
									fesa_set(msg_base_adr + + i*ADR_OFFS_NMSG + REG_MSG_EXEC_TIME_HI, (t_msg_exec)>>32);
									fesa_set(msg_base_adr + + i*ADR_OFFS_NMSG + REG_MSG_EXEC_TIME_LO, (0x00000000FFFFFFFF & t_msg_exec));			
									//write to tx buffer
									
                           src = (void*)(fesa_if + ((uintptr_t)(msg_base_adr + i*ADR_OFFS_NMSG + START_MSG)>>2));
                           
                           if(tx_write(dest, (const void*)src, wr_offs, rd_offs, 20)
                              )									
									{	
                              
                              //mark as sent						                
										fesa_set_bit((p_offs+REG_CYC_MSG_SENT), (1<<i)); 
										//update msg count						
										fesa_inc(msg_base_adr + i*ADR_OFFS_NMSG + REG_MSG_CNT);
                              msgs_sent_this_time++;
                           }	
								}
							}					
							else{ error = 1;}//error, we're too late
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
   


	fesa_set(REG_CMD, cmd ^ fesa_get(REG_CMD));	//leave only new changes in the command register

  return msgs_sent_this_time; 

}


void init_fesa()
{

   //cyc registers
	fesa_set(REG_CYC_T_TRANSMIT, 	    200000); //200us transmit time
	fesa_set(REG_CYC_T_MARGIN, 	      5000); //5us margin
	fesa_set(REG_CYC_CNT, 		0x00000000); 
	fesa_set(REG_NUM_MSGS, 		0x00000001); // 5 msgs
	fesa_set(REG_CYC_MSG_SENT, 	0x00000000); 

	fesa_set(REG_CYC_EXEC_TIME_HI,		 0); //
	fesa_set(REG_CYC_EXEC_TIME_LO,	1000000000); // start @ 1s 
	fesa_set(REG_CYC_PERIOD_HI,		 0); // 
	fesa_set(REG_CYC_PERIOD_LO,	 500000000); // period 0.5s 
	fesa_set(REG_CYC_REP,			3); // 20x
	fesa_set(REG_CYC_EXEC_TIME_HI,		 0); //
	fesa_set(REG_CYC_EXEC_TIME_LO,	1000000000); // exec @ 1s

	//msg 1
	fesa_set(ADR_BASE_MSGS + 0*ADR_OFFS_NMSG + REG_MSG_CMD,		  	0);	
	fesa_set(ADR_BASE_MSGS + 0*ADR_OFFS_NMSG + REG_MSG_CNT,		   	0x00000000);	
	fesa_set(ADR_BASE_MSGS + 0*ADR_OFFS_NMSG + REG_MSG_OFFS_TIME_HI,	         0);	
	fesa_set(ADR_BASE_MSGS + 0*ADR_OFFS_NMSG + REG_MSG_OFFS_TIME_LO,           80);	// 80ns

	fesa_set(ADR_BASE_MSGS + 0*ADR_OFFS_NMSG + REG_MSG_EXEC_TIME_HI,	0x00000000);	
	fesa_set(ADR_BASE_MSGS + 0*ADR_OFFS_NMSG + REG_MSG_EXEC_TIME_LO,   	0x00000000);	
	fesa_set(ADR_BASE_MSGS + 0*ADR_OFFS_NMSG + REG_MSG_ID_HI, 	 	0x11111111);	
	fesa_set(ADR_BASE_MSGS + 0*ADR_OFFS_NMSG + REG_MSG_ID_LO, 	 	0x45670001);	
	fesa_set(ADR_BASE_MSGS + 0*ADR_OFFS_NMSG + REG_MSG_PARAM, 	 	0xDEADBEE1);	

	//msg 2
	fesa_set(ADR_BASE_MSGS + 1*ADR_OFFS_NMSG + REG_MSG_CMD,		   	0);	
	fesa_set(ADR_BASE_MSGS + 1*ADR_OFFS_NMSG + REG_MSG_CNT,		   	0x00000000);	
	fesa_set(ADR_BASE_MSGS + 1*ADR_OFFS_NMSG + REG_MSG_OFFS_TIME_HI,	         0);	
	fesa_set(ADR_BASE_MSGS + 1*ADR_OFFS_NMSG + REG_MSG_OFFS_TIME_LO,         3000);	// 3us

	fesa_set(ADR_BASE_MSGS + 1*ADR_OFFS_NMSG + REG_MSG_EXEC_TIME_HI,	0x00000000);	
	fesa_set(ADR_BASE_MSGS + 1*ADR_OFFS_NMSG + REG_MSG_EXEC_TIME_LO,   	0x00000000);	
	fesa_set(ADR_BASE_MSGS + 1*ADR_OFFS_NMSG + REG_MSG_ID_HI, 	 	0x22222222);	
	fesa_set(ADR_BASE_MSGS + 1*ADR_OFFS_NMSG + REG_MSG_ID_LO, 	 	0x45670002);	
	fesa_set(ADR_BASE_MSGS + 1*ADR_OFFS_NMSG + REG_MSG_PARAM, 	 	0xDEADBEE2);	

	//msg 3
	fesa_set(ADR_BASE_MSGS + 2*ADR_OFFS_NMSG + REG_MSG_CMD,		   	0);	
	fesa_set(ADR_BASE_MSGS + 2*ADR_OFFS_NMSG + REG_MSG_CNT,		   	   0x00000000);	
	fesa_set(ADR_BASE_MSGS + 2*ADR_OFFS_NMSG + REG_MSG_OFFS_TIME_HI,	         0);	
	fesa_set(ADR_BASE_MSGS + 2*ADR_OFFS_NMSG + REG_MSG_OFFS_TIME_LO,     100000000);	// 100ms

	fesa_set(ADR_BASE_MSGS + 2*ADR_OFFS_NMSG + REG_MSG_EXEC_TIME_HI,	   0x00000000);	
	fesa_set(ADR_BASE_MSGS + 2*ADR_OFFS_NMSG + REG_MSG_EXEC_TIME_LO,   	0x00000000);	
	fesa_set(ADR_BASE_MSGS + 2*ADR_OFFS_NMSG + REG_MSG_ID_HI, 	 	      0x33333333);	
	fesa_set(ADR_BASE_MSGS + 2*ADR_OFFS_NMSG + REG_MSG_ID_LO, 	 	      0x45670003);	
	fesa_set(ADR_BASE_MSGS + 2*ADR_OFFS_NMSG + REG_MSG_PARAM, 	 	      0xDEADBEE2);	
   
   fesa_set(REG_STAT, STAT_CYC_ACTIVE | 0x07); //first 3 msg slots
   
//page B

      //cyc registers
	fesa_set(ADR_OFFS_PAGE_B + REG_CYC_T_TRANSMIT, 	    200000); //200us transmit time
	fesa_set(ADR_OFFS_PAGE_B + REG_CYC_T_MARGIN, 	      5000); //5us margin
	fesa_set(ADR_OFFS_PAGE_B + REG_CYC_CNT, 		0x00000000); 
	fesa_set(ADR_OFFS_PAGE_B + REG_NUM_MSGS, 		0x00000001); // 5 msgs
	fesa_set(ADR_OFFS_PAGE_B + REG_CYC_MSG_SENT, 	0x00000000); 

	fesa_set(ADR_OFFS_PAGE_B + REG_CYC_EXEC_TIME_HI,		 0); //
	fesa_set(ADR_OFFS_PAGE_B + REG_CYC_EXEC_TIME_LO,	50); // start @ 50ns 
	fesa_set(ADR_OFFS_PAGE_B + REG_CYC_PERIOD_HI,		 0); // 
	fesa_set(ADR_OFFS_PAGE_B + REG_CYC_PERIOD_LO,	 100000); // period 100us 
	fesa_set(ADR_OFFS_PAGE_B + REG_CYC_REP,			2); // 20x
	fesa_set(ADR_OFFS_PAGE_B + REG_CYC_EXEC_TIME_HI,		 0); //
	fesa_set(ADR_OFFS_PAGE_B + REG_CYC_EXEC_TIME_LO,	1500000000); // exec @ 2s

	//msg 1
	fesa_set(ADR_OFFS_PAGE_B + ADR_BASE_MSGS + 0*ADR_OFFS_NMSG + REG_MSG_CMD,		  	0);	
	fesa_set(ADR_OFFS_PAGE_B + ADR_BASE_MSGS + 0*ADR_OFFS_NMSG + REG_MSG_CNT,		   	0x00000000);	
	fesa_set(ADR_OFFS_PAGE_B + ADR_BASE_MSGS + 0*ADR_OFFS_NMSG + REG_MSG_OFFS_TIME_HI,	         0);	
	fesa_set(ADR_OFFS_PAGE_B + ADR_BASE_MSGS + 0*ADR_OFFS_NMSG + REG_MSG_OFFS_TIME_LO,           300);	// 80ns

	fesa_set(ADR_OFFS_PAGE_B + ADR_BASE_MSGS + 0*ADR_OFFS_NMSG + REG_MSG_EXEC_TIME_HI,	0x00000000);	
	fesa_set(ADR_OFFS_PAGE_B + ADR_BASE_MSGS + 0*ADR_OFFS_NMSG + REG_MSG_EXEC_TIME_LO,   	0x00000000);	
	fesa_set(ADR_OFFS_PAGE_B + ADR_BASE_MSGS + 0*ADR_OFFS_NMSG + REG_MSG_ID_HI, 	 	0xAAAAAAAA);	
	fesa_set(ADR_OFFS_PAGE_B + ADR_BASE_MSGS + 0*ADR_OFFS_NMSG + REG_MSG_ID_LO, 	 	0x0000BBBB);	
	fesa_set(ADR_OFFS_PAGE_B + ADR_BASE_MSGS + 0*ADR_OFFS_NMSG + REG_MSG_PARAM, 	 	0xDEAD0000);	



   fesa_set(ADR_OFFS_PAGE_B + REG_STAT, STAT_CYC_ACTIVE | 0x01); //first msg slot

}




int main(void)
{
  
 init_fesa();


  while (1) {

   acc_cycle(tx_ctrl, tx_buffer);

   }
}
