
#include <stdio.h>

#include "lib/etherbone.h"
#include <inttypes.h>

#include <stdarg.h>

#include <wrc.h>
#include "syscon.h"
#include "uart.h"
#include "endpoint.h"
#include "minic.h"
#include "pps_gen.h"
#include "ptpd.h"
#include "ptpd_netif.h"
#include "i2c.h"
//#include "eeprom.h"
#include "softpll_ng.h"
#include "onewire.h"
#include "pps_gen.h"
#include "sockitowm.h"
#include "shell.h"
#include "lib/ipv4.h"

#include "wrc_ptp.h"
#include "lib/fesa_if.h"
#include "ptpd_exports.h"

#include "ptpd.h"

extern ptpdexp_sync_state_t cur_servo_state;


extern unsigned int* _startshared[];
extern unsigned int* _endshared[];
uintptr_t page_offs; 
volatile unsigned int* fesa_if = _startshared;


volatile uint32_t* tx_ctrl 	= (uint32_t*)0x100400;
volatile uint32_t* tx_buffer = (uint32_t*)0x100400;


  const char c_running[4] = {'|', '/', '-', '\\'};

   void progress_wheel()
   {
      static uint8_t index = 0;

      TRACE_DEV("\r%c", c_running[index & 0x03]);
      index++;

   }


void crappy_printu64(uint64_t output)
{
   
   uint64_t div = 1000000000000000000;
   uint32_t res;
   uint8_t leadingzeros = 1;

   while(div > 0) 
   {
      
      
      res = (uint32_t)(output / div);

      output %= div;
      div /= 10;
      if(res) leadingzeros = 0;        
      if(!leadingzeros) TRACE_DEV("%d", res);

   }
}


#define SECOND          UINT64_C(1000000000)


#define REG_P_TX_WR 	   0
#define REG_P_TX_RD 	   (REG_P_TX_WR+4)

#define BUF_SIZE        0x000000400
#define P_TX_MSK 	      (BUF_SIZE-1)
#define P_TX_WRAP 	   BUF_SIZE
#define P_TX_MSK_WRAP 	(P_TX_MSK | P_TX_WRAP)

const uint8_t BUF_OK	= 1;
const uint8_t BUF_FULL	= 0;

typedef uintptr_t size_t; 


int wrc_ui_mode = UI_SHELL_MODE;

uint8_t gerror;
uint32_t cmp1, cmp2; 

///////////////////////////////////
//Calibration data (from EEPROM if available)
int32_t sfp_alpha = 73622176;	//default values if could not read EEPROM
int32_t sfp_deltaTx = 46407;
int32_t sfp_deltaRx = 167843;
uint32_t cal_phase_transition = 2389;

const char netaddress[] = "hw/ff:ff:ff:ff:ff:ff/udp/192.168.191.255/port/60368";
 
eb_socket_t socket;
eb_status_t status;
eb_device_t device;

uint8_t showexectime = 0;


static void wrc_initialize()
{
	uint8_t mac_addr[6];

	sdb_find_devices();
	uart_init();

	mprintf("WR Core: starting up...\n");

	timer_init(1);
	owInit();

	mac_addr[0] = 0x08;	//
	mac_addr[1] = 0x00;	// CERN OUI
	mac_addr[2] = 0x30;	//

	own_scanbus(ONEWIRE_PORT);
	if (get_persistent_mac(ONEWIRE_PORT, mac_addr) == -1) {
		mprintf("Unable to determine MAC address\n");
		mac_addr[0] = 0x11;	//
		mac_addr[1] = 0x22;	//
		mac_addr[2] = 0x33;	// fallback MAC if get_persistent_mac fails
		mac_addr[3] = 0x44;	//
		mac_addr[4] = 0x55;	//
		mac_addr[5] = 0x66;	//
	}

	TRACE_DEV("Local MAC address: %x:%x:%x:%x:%x:%x\n", mac_addr[0],
		  mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4],
		  mac_addr[5]);

	ep_init(mac_addr);
	ep_enable(1, 1);

	minic_init();
	shw_pps_gen_init();
	wrc_ptp_init();

#ifdef CONFIG_ETHERBONE
	ipv4_init("wru1");
	arp_init("wru1");
#endif



TRACE_DEV("Setting up ebsocket...\n");

	if ((status = eb_socket_open(EB_ABI_CODE, "wru1", EB_ADDR32|EB_DATA32, &socket)) != EB_OK) {
    		TRACE_DEV("failed to open Etherbone socket: \n");}
	else{
	   if ((status = eb_device_open(socket, netaddress, EB_ADDR32|EB_DATA32, 0, &device)) != EB_OK) {
     			TRACE_DEV("failed to open Etherbone device");
   	}	
   }

}

#define LINK_WENT_UP 1
#define LINK_WENT_DOWN 2
#define LINK_UP 3
#define LINK_DOWN 4

static int wrc_check_link()
{
	static int prev_link_state = -1;
	int link_state = ep_link_up(NULL);
	int rv = 0;

	if (!prev_link_state && link_state) {
		TRACE_DEV("Link up.\n");
		gpio_out(GPIO_LED_LINK, 1);
		rv = LINK_WENT_UP;
	} else if (prev_link_state && !link_state) {
		TRACE_DEV("Link down.\n");
		gpio_out(GPIO_LED_LINK, 0);
		rv = LINK_WENT_DOWN;
	} else
		rv = (link_state ? LINK_UP : LINK_DOWN);
	prev_link_state = link_state;

	return rv;
}

int wrc_extra_debug = 0;

void wrc_debug_printf(int subsys, const char *fmt, ...)
{
	va_list ap;

	if (wrc_ui_mode)
		return;

	va_start(ap, fmt);

	if (wrc_extra_debug || (!wrc_extra_debug && (subsys & TRACE_SERVO)))
		vprintf(fmt, ap);

	va_end(ap);
}


inline uint32_t get_cycle_count()
{
   uint32_t result;

   asm volatile ("rcsr %0, cc": "=r"(result));

   return result;

}


uint64_t get_current_time()
{
   uint32_t nsec;	
   uint64_t sec, nTimeStamp;

   shw_pps_gen_get_time(&sec, &nsec);
   
   nTimeStamp 	= sec & 0xFFFFFFFFFFULL;
   nTimeStamp  *= SECOND;
   nTimeStamp  += (nsec);

   return nTimeStamp;
}

int wrc_man_phase = 0;

static void ui_update()
{

	if (wrc_ui_mode == UI_GUI_MODE) {
		wrc_mon_gui();
		if (uart_read_byte() == 27) {
			shell_init();
			wrc_ui_mode = UI_SHELL_MODE;
		}
	} else if (wrc_ui_mode == UI_STAT_MODE) {
		wrc_log_stats(0);
		if (uart_read_byte() == 27) {
			shell_init();
			wrc_ui_mode = UI_SHELL_MODE;
		}
	} else
		shell_interactive();

}

extern uint32_t _endram;
#define ENDRAM_MAGIC 0xbadc0ffe

static void check_stack(void)
{
	while (_endram != ENDRAM_MAGIC) {
		mprintf("Stack overflow!\n");
		timer_delay(1000);
	}
}


	

int stop()
{
	//TRACE_DEV("Callback STOP\n");
	return 1;
}

void set_stop()
{
	//TRACE_DEV("Callback SETSTOP\n");
}

static void send_EB_Demo_packet()
{
   eb_data_t mask;
   eb_width_t line_width;
   eb_format_t line_widths;
   eb_format_t device_support;
   eb_format_t write_sizes;
   eb_format_t format;
   eb_cycle_t cycle;

   eb_data_t data;
   eb_address_t address = 0x100c00;
   uint32_t nsec;	
   static uint64_t last_sec;
   static uint32_t param=0;
   uint64_t sec, nTimeStamp, nTimeOffset, ID;
   shw_pps_gen_get_time(&sec, &nsec);
   //if( last_sec == sec) return;
   last_sec = sec;

   TRACE_DEV("Seconds:\t %x \n nSeconds: %x\n", (uint32_t)sec, nsec);

   nTimeOffset = 1000000;	
   nTimeStamp 	= sec & 0xFFFFFFFFFFULL;
   nTimeStamp  *= (SECOND/8);
   nTimeStamp  += (nsec/8);
   nTimeStamp  += nTimeOffset;
   ID		= 0xD15EA5EDBABECAFE;
   param++;

   TRACE_DEV("TS: %x %x\n", (uint32_t)(nTimeStamp>>32), (uint32_t)(nTimeStamp)); 

   /* Begin the cycle */
   if ((status = eb_cycle_open(device, 0, &set_stop, &cycle)) != EB_OK) 
   {
      TRACE_DEV(" failed to create cycle, status: %d\n", status);
      return;
   }
    
   eb_cycle_write(cycle, address, EB_DATA32|EB_BIG_ENDIAN, (uint32_t)(ID>>32));
   eb_cycle_write(cycle, address, EB_DATA32|EB_BIG_ENDIAN, (uint32_t)(ID & 0x00000000ffffffff));
   eb_cycle_write(cycle, address, EB_DATA32|EB_BIG_ENDIAN, (uint32_t)(nTimeStamp>>32));
   eb_cycle_write(cycle, address, EB_DATA32|EB_BIG_ENDIAN, (uint32_t)(nTimeStamp & 0x00000000ffffffff));	  
   eb_cycle_write(cycle, address, EB_DATA32|EB_BIG_ENDIAN, param);

   eb_cycle_close_silently(cycle);

/* Begin the cycle */
   if ((status = eb_cycle_open(device, 0, &set_stop, &cycle)) != EB_OK) 
   {
      TRACE_DEV(" failed to create cycle, status: %d\n", status);
      return;
   }
 eb_cycle_write(cycle, address, EB_DATA32|EB_BIG_ENDIAN, (uint32_t)(ID>>32));
   eb_cycle_write(cycle, address, EB_DATA32|EB_BIG_ENDIAN, (uint32_t)(ID & 0x00000000ffffffff));
   eb_cycle_write(cycle, address, EB_DATA32|EB_BIG_ENDIAN, (uint32_t)(nTimeStamp>>32));
   eb_cycle_write(cycle, address, EB_DATA32|EB_BIG_ENDIAN, (uint32_t)(nTimeStamp & 0x00000000ffffffff));	  
   eb_cycle_write(cycle, address, EB_DATA32|EB_BIG_ENDIAN, param);

   eb_cycle_close_silently(cycle);

   //TRACE_DEV("before flush ");
   eb_device_flush(device);
   //TRACE_DEV("after flush ");	
}

void create_eb_msg(const uint32_t* src)
{
   uint32_t cmp1, cmp2, cmp3, cmp4, cmp5;   
cmp1 = get_cycle_count();

 cmp2 = get_cycle_count();     
   eb_cycle_t cycle;

   eb_data_t data;
   eb_address_t address = 0x100c00;
   
   uintptr_t offs;
   //TRACE_DEV("entering eb create msg\n");

   /* Begin the cycle */
   if ((status = eb_cycle_open(device, 0, &set_stop, &cycle)) != EB_OK) 
   {
      TRACE_DEV(" failed to create cycle, status: %d\n", status);
      return;
   }
   cmp3 = get_cycle_count();
   for(offs=0;offs<5;offs++) eb_cycle_write(cycle, address, EB_DATA32|EB_BIG_ENDIAN, *(src+offs) );
   cmp4 = get_cycle_count();   
eb_cycle_close_silently(cycle);

   cmp5 = get_cycle_count();
   //TRACE_DEV("dgb %d prep %d 5xwr %d cycclos %d all %d\n", cmp2-cmp1, cmp3-cmp2, cmp4-cmp3, cmp5-cmp4, cmp5-cmp1);
   //TRACE_DEV("leave msg create \n");
  
}

void send_eb_msg()
{
   //TRACE_DEV("entering eb send msg\n");  
   eb_device_flush(device);
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

uint32_t acc_cycle_layout_valid()
{
   uint32_t msgs, i, ret;   
   uint64_t t_period, t_max_msg_offs, t_min_msg_offs, t_this_msg_offs, t_trn_margin;
   uintptr_t p_offs, msg_base_adr;

  
   p_offs         = fesa_get(REG_MEMPAGE_ACTIVE);
   msgs           = fesa_get(p_offs+REG_NUM_MSGS);   
   msg_base_adr 	= p_offs + ADR_BASE_MSGS; 
   t_period	      = (((uint64_t)(fesa_get(p_offs+REG_CYC_PERIOD_HI)) << 32) | (uint64_t)fesa_get(p_offs+REG_CYC_PERIOD_LO));	  
   t_trn_margin   = fesa_get(p_offs+REG_CYC_T_TRANSMIT) + fesa_get(p_offs+REG_CYC_T_MARGIN);
   t_min_msg_offs = ((((uint64_t)fesa_get(msg_base_adr + REG_MSG_OFFS_TIME_HI)) << 32) | fesa_get(msg_base_adr + REG_MSG_OFFS_TIME_LO));   
   t_max_msg_offs = t_min_msg_offs;   
   ret            = 1;

   for(i=0; i<msgs; i++)
   {
             
     t_this_msg_offs = ((((uint64_t)fesa_get(msg_base_adr + i*ADR_OFFS_NMSG + REG_MSG_OFFS_TIME_HI)) << 32) 
					         |   fesa_get(msg_base_adr + i*ADR_OFFS_NMSG + REG_MSG_OFFS_TIME_LO));		
      
      TRACE_DEV("i=%d msgs=%d\n", i, msgs); 
      TRACE_DEV("T %x%x\n", 	(uint32_t)(t_this_msg_offs>>32), (uint32_t)t_this_msg_offs );  
      TRACE_DEV("T"); crappy_printu64(t_this_msg_offs); TRACE_DEV("\n"); 
      TRACE_DEV("P"); crappy_printu64(t_period); TRACE_DEV("\n"); 
      TRACE_DEV("\n");      

   
      if(t_this_msg_offs >= t_period) 
      {TRACE_DEV("Message %d's offset is bigger than cycle period!\n", i);  ret = 0;}     //must be within cycle period length
      
      if(t_max_msg_offs <= t_this_msg_offs) t_max_msg_offs = t_this_msg_offs; //must be monotonous
      else {TRACE_DEV("Message %d's offset is smaller than the previous!\n", i); ret = 0;}
   }

   if(t_min_msg_offs + (t_period - t_max_msg_offs) < t_trn_margin) 
   {ret = 0; TRACE_DEV("Not enough time between last and first message of cycle!\n");} //must leave enough time to process next cycle
   
   if(ret == 0) 
   {
      if(p_offs)  TRACE_DEV("Error in AccCycle on Mempage B\n");
      else        TRACE_DEV("Error in AccCycle on Mempage A\n");
   }

   return ret;
}


extern int once;
uint8_t acc_cycle( uintptr_t* tx_ctrl, uint32_t* tx_buffer)
{   
  uint64_t t_last, t_due, t_overdue, t_cyc_exec, cyc_inc, t_msg_exec, t_msg_offs, t_period, t_cmp;
  static uint64_t  t_delta, t_enter, t_psetup, t_now;	
  uint32_t cmd, tmp;
  static uint8_t conditional_not_met, error;
  static uintptr_t p_offs, msg_base_adr;

  volatile uintptr_t* wr_offs;
  volatile uintptr_t* rd_offs;
  void* src;
  void* dest;

  uint32_t i, j, msgs_sent_this_time, cmp1, cmp2, cmp3, cmp4, cmp5, cmp6;

  cmp1 = get_cycle_count();

  //FIXME workaround for test implementation
  wr_offs = ((uintptr_t*)tx_ctrl + ((uintptr_t) REG_P_TX_WR>>2));
  rd_offs = ((uintptr_t*)tx_ctrl + ((uintptr_t) REG_P_TX_RD>>2));
  dest    = (void*)(tx_buffer); 
  conditional_not_met = 0;

  msgs_sent_this_time = 0;

  // read the command register
  cmd = fesa_get(REG_CMD);
  if(cmd)
  {

      
    //do pageswap on request...
    if(cmd & CMD_CYC_PAGESWAP) 
    {
       tmp = fesa_get(REG_MEMPAGE_INACTIVE);
       fesa_set(REG_MEMPAGE_INACTIVE, fesa_get(REG_MEMPAGE_ACTIVE));
       fesa_set(REG_MEMPAGE_ACTIVE, tmp);
       p_offs = fesa_get(REG_MEMPAGE_ACTIVE);  
       TRACE_DEV("ACTIVE:   %x, Cyc Exec %x%x\n", fesa_get(REG_MEMPAGE_ACTIVE),	fesa_get(p_offs + REG_CYC_EXEC_TIME_HI), fesa_get(p_offs + REG_CYC_EXEC_TIME_LO));
       TRACE_DEV("inactive: %x, Cyc Exec %x%x\n", fesa_get(REG_MEMPAGE_INACTIVE), fesa_get(fesa_get(REG_MEMPAGE_INACTIVE) + REG_CYC_EXEC_TIME_HI), fesa_get(fesa_get(REG_MEMPAGE_INACTIVE) + REG_CYC_EXEC_TIME_LO));
    }
    p_offs = fesa_get(REG_MEMPAGE_ACTIVE);        

    //start/stop the cycle
	  if(cmd & (CMD_CYC_STOP | CMD_CYC_STOP_I)) 	{fesa_clr_bit(p_offs + REG_STAT, STAT_CYC_ACTIVE); TRACE_DEV("\nStopped\n");}
	  else if(cmd & (CMD_CYC_START))
    {
      if(acc_cycle_layout_valid())
      {gerror = 0; error = 0; fesa_set_bit(p_offs + REG_STAT, STAT_CYC_ACTIVE); TRACE_DEV("\nStarted\n");}
      else {TRACE_DEV("\nStopped\n");} 
    }

    if(cmd & CMD_DBG)
    {
       if(acc_cycle_layout_valid())
       {gerror = 0; error = 0; fesa_set_bit(p_offs + REG_STAT, STAT_DBG); fesa_set_bit(p_offs + REG_STAT, STAT_CYC_ACTIVE);TRACE_DEV("\nDBG started\n");}
       else {TRACE_DEV("\nStopped\n");}          

    }
    fesa_set(REG_CMD, 0);	//leave only new changes in the command register
                 
  }

  p_offs = fesa_get(REG_MEMPAGE_ACTIVE);
  msg_base_adr 	= p_offs + ADR_BASE_MSGS; 
		
	if(fesa_get(p_offs+REG_STAT) & STAT_CYC_ACTIVE)
	{
    //process cycle if there are repetitions left or should run infinitely (reps -1)
		if( (fesa_get(p_offs+REG_CYC_CNT) < fesa_get(p_offs+REG_CYC_REP)) || fesa_get(p_offs+REG_CYC_REP)==-1)			
		{	
      if(!error)         
      {
        //let i be the number of the next due message ...
        if( fesa_get(p_offs+REG_CYC_MSG_SENT) < fesa_get(p_offs+REG_NUM_MSGS)) 
        {
          //get cycle execution time			
          t_cyc_exec	= (((uint64_t)(fesa_get(p_offs+REG_CYC_EXEC_TIME_HI)) << 32) 	| fesa_get(p_offs+REG_CYC_EXEC_TIME_LO));		             
         
          t_last      = t_now;				   
          t_now 		  = get_current_time();				
          
          if(!t_cyc_exec)
          {
            t_cyc_exec = t_now + fesa_get(p_offs+REG_CYC_T_TRANSMIT) + fesa_get(p_offs+REG_CYC_T_MARGIN);
            fesa_set(p_offs+REG_CYC_EXEC_TIME_HI, (uint32_t)(t_cyc_exec>>32));
            fesa_set(p_offs+REG_CYC_EXEC_TIME_LO, (uint32_t)t_cyc_exec);
          }                  
  			    
          
  
          if(fesa_get(p_offs+REG_STAT) & STAT_DBG)
          {
            if(t_now > (t_cyc_exec - fesa_get(p_offs+REG_CYC_T_TRANSMIT) + (((uint64_t)(fesa_get(p_offs+REG_CYC_PERIOD_HI)) << 32) 
            + (uint64_t)fesa_get(p_offs+REG_CYC_PERIOD_LO))	) )
            {
               t_cyc_exec = t_now + fesa_get(p_offs+REG_CYC_T_TRANSMIT) + fesa_get(p_offs+REG_CYC_T_MARGIN);
               fesa_set(p_offs+REG_CYC_EXEC_TIME_HI, (uint32_t)(t_cyc_exec>>32));
               fesa_set(p_offs+REG_CYC_EXEC_TIME_LO, (uint32_t)t_cyc_exec);

               //TRACE_DEV("DBG: Had to change cycle start time to %d from now\n", fesa_get(p_offs+REG_CYC_T_TRANSMIT) + fesa_get(p_offs+REG_CYC_T_MARGIN) );
               //TRACE_DEV("cyc_exec is now %x %x\n\n", 	fesa_get(p_offs + REG_CYC_EXEC_TIME_HI), fesa_get(p_offs + REG_CYC_EXEC_TIME_LO)); 
            }   
          }  
            
				   //               t_now
				   //---------------|--------------------------------------------------------
				   //----------|-------------|------------------------|-----------------|----
				   //          t_margin  <-  t_transmit     <-        t_cyc_exec    <-  t_msg_offs
				   //          |             |		
				   //          t_due         t_overdue  	 					

          // Calculate all auxiliary times
          t_msg_offs = ((((uint64_t)fesa_get(msg_base_adr + fesa_get(p_offs+REG_CYC_MSG_SENT)*ADR_OFFS_NMSG + REG_MSG_OFFS_TIME_HI)) << 32) 
                 |   fesa_get(msg_base_adr + fesa_get(p_offs+REG_CYC_MSG_SENT)*ADR_OFFS_NMSG + REG_MSG_OFFS_TIME_LO));				
          t_msg_exec	= t_cyc_exec + t_msg_offs;						
          t_overdue 	= t_msg_exec - fesa_get(p_offs+REG_CYC_T_TRANSMIT);  			
          t_due 		= t_overdue - fesa_get(p_offs+REG_CYC_T_MARGIN);
                
			    cmp2 = get_cycle_count();

          if(t_now >= t_due) //time to act yet?
				  { 
            if(t_now < t_overdue)	// still enough time ?	
					  {
              ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////              
              //group all messages from this transmit window into one eb packet              
              for(j=fesa_get(p_offs+REG_CYC_MSG_SENT); j < fesa_get(p_offs+REG_NUM_MSGS) ; j++)
		          {
                cmp3 = get_cycle_count();                       
                //we start with j = i because we then need no corner case for sending i                 
                        
                t_cmp = ((((uint64_t)fesa_get(msg_base_adr + j*ADR_OFFS_NMSG + REG_MSG_OFFS_TIME_HI)) << 32) 
					            |   fesa_get(msg_base_adr + j*ADR_OFFS_NMSG + REG_MSG_OFFS_TIME_LO));

                if( (t_cmp - t_msg_offs) < (fesa_get(p_offs+REG_CYC_T_TRANSMIT) + fesa_get(p_offs+REG_CYC_T_MARGIN)) )
                {
                  t_msg_exec  = t_cyc_exec  + t_cmp;	
                  t_overdue 	= t_msg_exec  - fesa_get(p_offs+REG_CYC_T_TRANSMIT);  			
                  t_due 		  = t_overdue   - fesa_get(p_offs+REG_CYC_T_MARGIN);                              

                  fesa_set(msg_base_adr + j*ADR_OFFS_NMSG + REG_MSG_EXEC_TIME_HI, (uint32_t)((t_msg_exec/8)>>32));
                  fesa_set(msg_base_adr + j*ADR_OFFS_NMSG + REG_MSG_EXEC_TIME_LO, (uint32_t)(t_msg_exec/8));                     
                  src = (void*)(fesa_if + ((uintptr_t)(msg_base_adr + j*ADR_OFFS_NMSG + START_MSG)>>2));
                  msgs_sent_this_time++;
                  
                  create_eb_msg((const uint32_t*)src);
                } 
                else {   //TRACE_DEV("now %x %x, diff %d\n", (uint32_t)(t_now >> 32), (uint32_t)(t_now), (uint32_t)(get_current_time()-t_cyc_exec)); 
                            once = 1; 
                            break;
                     }
                cmp4 = get_cycle_count();
              }
                 
              cmp5 = get_cycle_count();
              send_eb_msg();
              //TRACE_DEV("now is       %x %x\n", 	(uint32_t)(t_cyc_exec>>32),	(uint32_t)t_cyc_exec); 
              cmp6 = get_cycle_count(); 
              //TRACE_DEV("prep %d loop1 %d allloop %d send %d \n", cmp2-cmp1, cmp4-cmp3, cmp4-cmp2,cmp6-cmp5);
              fesa_set((p_offs+REG_CYC_MSG_SENT), j);                  
              ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////   	
            }
            else
            { 
              gerror = 1; error = 1; 
              TRACE_DEV("m %d n %d j %d prep %d loop1 %d allloop %d send %d \n", 
               fesa_get(p_offs+REG_CYC_CNT), fesa_get(p_offs+REG_CYC_MSG_SENT), j, cmp2-cmp1, cmp4-cmp3, cmp4-cmp2,cmp6-cmp5);   }//error, we're too late
            }
			    }
          else
          {
            // TRACE_DEV("cycle complete, Old Cyc %d", fesa_get(p_offs+REG_CYC_EXEC_TIME_LO));                
            //update next cycle execution time
            t_cyc_exec	= (((uint64_t)(fesa_get(p_offs+REG_CYC_EXEC_TIME_HI)) << 32) 	| fesa_get(p_offs+REG_CYC_EXEC_TIME_LO));	
            cyc_inc     = (((uint64_t)(fesa_get(p_offs+REG_CYC_PERIOD_HI)) << 32) 		| (uint64_t)fesa_get(p_offs+REG_CYC_PERIOD_LO));		
            t_cyc_exec += cyc_inc;
            fesa_set(p_offs+REG_CYC_EXEC_TIME_HI, (uint32_t)(t_cyc_exec>>32));
            fesa_set(p_offs+REG_CYC_EXEC_TIME_LO, (uint32_t)t_cyc_exec);
            //update cycle count
            fesa_inc(p_offs+REG_CYC_CNT);
            //clr sent message flags			
            fesa_set(p_offs+REG_CYC_MSG_SENT, 0);
            progress_wheel();
          }
		    } 		
	    }
    } 
			
	

   return msgs_sent_this_time; 

}

void init_ftm()
{
  page_offs = ((((uintptr_t)((void*)_endshared))-((uintptr_t)((void*)_startshared))+4)/2);   
  fesa_set(REG_MEMPAGE_ACTIVE, 0);
  fesa_set(REG_MEMPAGE_INACTIVE, page_offs);
  TRACE_DEV("FTM fesaIFf @ %x\n, pOffs %x", fesa_if, page_offs);
   
}


void init_fesa()
{
  //show base address for fesa registers on console  
  

  

  //cyc registers
  fesa_set(REG_CYC_T_TRANSMIT, 	    200000); //200us transmit time
  fesa_set(REG_CYC_T_MARGIN, 	     5000000); //5us margin
  fesa_set(REG_CYC_CNT, 		    0x00000000); 
  fesa_set(REG_NUM_MSGS, 		    0x00000004); // 4 msgs
  fesa_set(REG_CYC_MSG_SENT, 	  0x00000000); 


  uint64_t start, tmp1, tmp2, tmp3;
  start = get_current_time();

    
  //set cycle start to whole seconds (PPS) plus five seconds time
  tmp1 = start % SECOND;
  tmp2 = SECOND - tmp1;
  tmp3 = start + tmp2;  
  start = tmp3 + 5*SECOND;



 // fesa_set(REG_CYC_EXEC_TIME_HI,		  (uint32_t)(start>>32)); //
 // fesa_set(REG_CYC_EXEC_TIME_LO,	          (uint32_t)start); 
 
 fesa_set(REG_CYC_EXEC_TIME_HI,		  0); //
 fesa_set(REG_CYC_EXEC_TIME_LO,	    0); 
 fesa_set(REG_CYC_PERIOD_HI,		                          0); // 
  fesa_set(REG_CYC_PERIOD_LO,	                       SECOND); 
  fesa_set(REG_CYC_REP,			                             -1); // 

  //msg 1
  fesa_set(ADR_BASE_MSGS + 0*ADR_OFFS_NMSG + REG_MSG_CMD,		  	               0);	
  fesa_set(ADR_BASE_MSGS + 0*ADR_OFFS_NMSG + REG_MSG_CNT,		   	      0x00000000);	
  fesa_set(ADR_BASE_MSGS + 0*ADR_OFFS_NMSG + REG_MSG_OFFS_TIME_HI,	           0);	
  fesa_set(ADR_BASE_MSGS + 0*ADR_OFFS_NMSG + REG_MSG_OFFS_TIME_LO,             0);	

  fesa_set(ADR_BASE_MSGS + 0*ADR_OFFS_NMSG + REG_MSG_EXEC_TIME_HI,	  0x00000000);	
  fesa_set(ADR_BASE_MSGS + 0*ADR_OFFS_NMSG + REG_MSG_EXEC_TIME_LO,   	0x00000000);	
  fesa_set(ADR_BASE_MSGS + 0*ADR_OFFS_NMSG + REG_MSG_ID_HI, 	 	      0x11111111);	
  fesa_set(ADR_BASE_MSGS + 0*ADR_OFFS_NMSG + REG_MSG_ID_LO, 	 	      0x45670001);	
  fesa_set(ADR_BASE_MSGS + 0*ADR_OFFS_NMSG + REG_MSG_PARAM, 	 	      0xDEADBEE1);	

  //msg 2
  fesa_set(ADR_BASE_MSGS + 1*ADR_OFFS_NMSG + REG_MSG_CMD,		   	               0);	
  fesa_set(ADR_BASE_MSGS + 1*ADR_OFFS_NMSG + REG_MSG_CNT,		   	      0x00000000);	
  fesa_set(ADR_BASE_MSGS + 1*ADR_OFFS_NMSG + REG_MSG_OFFS_TIME_HI,	           0);	
  fesa_set(ADR_BASE_MSGS + 1*ADR_OFFS_NMSG + REG_MSG_OFFS_TIME_LO,      SECOND/4);	

  fesa_set(ADR_BASE_MSGS + 1*ADR_OFFS_NMSG + REG_MSG_EXEC_TIME_HI,	  0x00000000);	
  fesa_set(ADR_BASE_MSGS + 1*ADR_OFFS_NMSG + REG_MSG_EXEC_TIME_LO,   	0x00000000);	
  fesa_set(ADR_BASE_MSGS + 1*ADR_OFFS_NMSG + REG_MSG_ID_HI, 	 	      0x22222222);	
  fesa_set(ADR_BASE_MSGS + 1*ADR_OFFS_NMSG + REG_MSG_ID_LO, 	 	      0x45670002);	
  fesa_set(ADR_BASE_MSGS + 1*ADR_OFFS_NMSG + REG_MSG_PARAM, 	 	      0xDEADBEE2);	

  //msg 3
  fesa_set(ADR_BASE_MSGS + 2*ADR_OFFS_NMSG + REG_MSG_CMD,		   	               0);	
  fesa_set(ADR_BASE_MSGS + 2*ADR_OFFS_NMSG + REG_MSG_CNT,		   	      0x00000000);	
  fesa_set(ADR_BASE_MSGS + 2*ADR_OFFS_NMSG + REG_MSG_OFFS_TIME_HI,	           0);	
  fesa_set(ADR_BASE_MSGS + 2*ADR_OFFS_NMSG + REG_MSG_OFFS_TIME_LO,   SECOND/4+32);	

  fesa_set(ADR_BASE_MSGS + 2*ADR_OFFS_NMSG + REG_MSG_EXEC_TIME_HI,	  0x00000000);	
  fesa_set(ADR_BASE_MSGS + 2*ADR_OFFS_NMSG + REG_MSG_EXEC_TIME_LO,   	0x00000000);	
  fesa_set(ADR_BASE_MSGS + 2*ADR_OFFS_NMSG + REG_MSG_ID_HI, 	 	      0x33333333);	
  fesa_set(ADR_BASE_MSGS + 2*ADR_OFFS_NMSG + REG_MSG_ID_LO, 	 	      0x45670003);	
  fesa_set(ADR_BASE_MSGS + 2*ADR_OFFS_NMSG + REG_MSG_PARAM, 	 	      0xDEADBEE2);

  //msg 4
  fesa_set(ADR_BASE_MSGS + 3*ADR_OFFS_NMSG + REG_MSG_CMD,		   	               0);	
  fesa_set(ADR_BASE_MSGS + 3*ADR_OFFS_NMSG + REG_MSG_CNT,		   	      0x00000000);	
  fesa_set(ADR_BASE_MSGS + 3*ADR_OFFS_NMSG + REG_MSG_OFFS_TIME_HI,	           0);	
  fesa_set(ADR_BASE_MSGS + 3*ADR_OFFS_NMSG + REG_MSG_OFFS_TIME_LO,      SECOND/3);	

  fesa_set(ADR_BASE_MSGS + 3*ADR_OFFS_NMSG + REG_MSG_EXEC_TIME_HI,	  0x00000000);	
  fesa_set(ADR_BASE_MSGS + 3*ADR_OFFS_NMSG + REG_MSG_EXEC_TIME_LO,   	0x00000000);	
  fesa_set(ADR_BASE_MSGS + 3*ADR_OFFS_NMSG + REG_MSG_ID_HI, 	 	      0x44444444);	
  fesa_set(ADR_BASE_MSGS + 3*ADR_OFFS_NMSG + REG_MSG_ID_LO, 	 	      0x45670003);	
  fesa_set(ADR_BASE_MSGS + 3*ADR_OFFS_NMSG + REG_MSG_PARAM, 	 	      0xDEADBEE2);		

fesa_set(REG_STAT, 0); //first 4 msg slots

  //page B

    //cyc registers
  fesa_set(page_offs +REG_CYC_T_TRANSMIT, 	    200000); //200us transmit time
  fesa_set(page_offs +REG_CYC_T_MARGIN, 	     5000000); //5ms margin
  fesa_set(page_offs +REG_CYC_CNT, 		      0x00000000); 
  fesa_set(page_offs +REG_NUM_MSGS, 		    0x00000001); // 5 msgs
  fesa_set(page_offs +REG_CYC_MSG_SENT, 	  0x00000000); 


  fesa_set(page_offs +REG_CYC_PERIOD_HI,		         0);  
  fesa_set(page_offs +REG_CYC_PERIOD_LO,	    SECOND/2); 
  fesa_set(page_offs +REG_CYC_REP,			            -1); 
  fesa_set(page_offs +REG_CYC_EXEC_TIME_HI,		       0); 
  fesa_set(page_offs +REG_CYC_EXEC_TIME_LO,	0); 

  //msg 1
  fesa_set(page_offs +ADR_BASE_MSGS + 0*ADR_OFFS_NMSG + REG_MSG_CMD,		  	           0);	
  fesa_set(page_offs +ADR_BASE_MSGS + 0*ADR_OFFS_NMSG + REG_MSG_CNT,		   	  0x00000000);	
  fesa_set(page_offs +ADR_BASE_MSGS + 0*ADR_OFFS_NMSG + REG_MSG_OFFS_TIME_HI,	         0);	
  fesa_set(page_offs +ADR_BASE_MSGS + 0*ADR_OFFS_NMSG + REG_MSG_OFFS_TIME_LO,      300000);	

  fesa_set(page_offs +ADR_BASE_MSGS + 0*ADR_OFFS_NMSG + REG_MSG_EXEC_TIME_HI,	0x00000000);	
  fesa_set(page_offs +ADR_BASE_MSGS + 0*ADR_OFFS_NMSG + REG_MSG_EXEC_TIME_LO, 0x00000000);	
  fesa_set(page_offs +ADR_BASE_MSGS + 0*ADR_OFFS_NMSG + REG_MSG_ID_HI, 	 	    0xAAAAAAAA);	
  fesa_set(page_offs +ADR_BASE_MSGS + 0*ADR_OFFS_NMSG + REG_MSG_ID_LO, 	 	    0x0000BBBB);	
  fesa_set(page_offs +ADR_BASE_MSGS + 0*ADR_OFFS_NMSG + REG_MSG_PARAM, 	 	    0xDEAD0000);	



  fesa_set(page_offs +REG_STAT, 0); //first msg slot


}



   

int main(void)
{
  // uint32_t fesa_buf[200];
   uint32_t tx_buf[2];	
   uint32_t tx_c[2];
   uint8_t finit = 1;	
	uint8_t cycle = 0;

   tx_buffer   = &tx_buf[0];
   tx_ctrl   = &tx_c[0];
   // fesa_if = &fesa_buf[0];
   
   uint64_t t_past, t_now; 

   

   wrc_extra_debug = 1;
	wrc_ui_mode = UI_SHELL_MODE;
	_endram = ENDRAM_MAGIC;

	wrc_initialize();
	shell_init();

	wrc_ptp_set_mode(WRC_MODE_SLAVE);
	wrc_ptp_start();

	//try to read and execute init script from EEPROM
	shell_boot_script();

    cmp1 = get_cycle_count();
    cmp2 = get_cycle_count();

         
   

	for (;;) {
		int l_status = wrc_check_link();

		switch (l_status) {
#ifdef CONFIG_ETHERBONE
		case LINK_WENT_UP:
			needIP = 1;
			break;
#endif

		case LINK_UP:
			update_rx_queues();
#ifdef CONFIG_ETHERBONE
			ipv4_poll();
			arp_poll();
#endif
			break;

		case LINK_WENT_DOWN:
			if (wrc_ptp_get_mode() == WRC_MODE_SLAVE)
				spll_init(SPLL_MODE_FREE_RUNNING_MASTER, 0, 1);
			break;
		}
       
      ui_update();	
		wrc_ptp_update();
		spll_update_aux_clocks();
		check_stack();

      if(!needIP) 
      {
     


 

            
         if(finit && (strcmp ("TRACK_PHASE", cur_servo_state.slave_servo_state) == 0) ) {   
                  init_ftm();                        
                  init_fesa();
                   
                  finit=0; 
                  
         } 

      
         if(!finit) acc_cycle(tx_ctrl, tx_buffer);
          
      }



	}
}
