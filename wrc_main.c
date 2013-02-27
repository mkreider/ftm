#include <stdio.h>

#include "etherbone.h"
#include <inttypes.h>

#include <stdarg.h>

#include "syscon.h"
#include "uart.h"
#include "endpoint.h"
#include "minic.h"
#include "pps_gen.h"
#include "ptpd.h"
#include "ptpd_netif.h"
#include "i2c.h"

#include "softpll_ng.h"
#include "onewire.h"
#include "shell.h"
#include "lib/ipv4.h"

#include "wrc_ptp.h"

int wrc_ui_mode = UI_SHELL_MODE;

///////////////////////////////////
//Calibration data (from EEPROM if available)
int32_t sfp_alpha = -73622176;  //default values if could not read EEPROM
int32_t sfp_deltaTx = 46407;
int32_t sfp_deltaRx = 167843;
uint32_t cal_phase_transition = 2389;

#if FAIR_DATA_MASTER
const char netaddress[] = "hw/ff:ff:ff:ff:ff:ff/udp/192.168.191.7/port/60368";

 
  eb_socket_t socket;
  eb_status_t status;
  eb_device_t device;
  /*
  bool tx_timing_msg_waiting( const uintptr_t* wr_offs, const uintptr_t* rd_offs)
  {
	if( *wr_offs ^ *rd_offs ) return true; //if the pointer offsets differ, there is a message to be sent
	else return false;	

  }	

  void create_timing_msg_eb_cyc(eb_address_t dest, const void* src, uintptr_t* wr_offs, const uintptr_t* rd_offs, size_t length)
{
	
	TRACE_DEV("entering create_timing_msg_eb_cyc\n");		
	uint8_t wraparound;
	size_t space_b4_end;

	uint32_t* src, dest;

	uintptr_t end_offs, masked_end_offs, masked_wr_offs, masked_rd_offs, offs;
	
	end_offs 	= (*rd_offs + length) & P_TX_MSK_W_WRAP; //mask it so offset has twice the range of the buffer
	masked_end_offs = end_offs & P_TX_MSK; 	//
	masked_wr_offs 	= *wr_offs & P_TX_MSK;	//
	masked_rd_offs 	= *rd_offs & P_TX_MSK;	//mask to match buffer offset range

	if((end_offs & P_TX_WRAP) ^ (*rd_offs & P_TX_WRAP)) 	wraparound = 1; //if the overflow bits differ, the buffer is wrapped around
	else 							wraparound = 0;
	
	//is enough space available?
	if( (wraparound && (masked_end_offs < masked_wr_offs)) | (~wraparound && (masked_end_offs > masked_wr_offs)) )
	{	
		if(wraparound) // does the transfer wrap around ? 
		{
			//yes, do it in two steps			
			space_b4_end = buf_end - masked_rd_offs; 
		
			//call etherbone function with dest and number of words
			for(offs=0;offs<space_b4_end);offs+=4) 
			{
				eb_cycle_write(cycle, (wb_dest + offs), EB_DATA32|EB_BIG_ENDIAN, *((uint32*)(src + masked_rd_offs + offs)) );
			}
			for(offs=0;offs<(length-space_b4_end));offs+=4) 
			{
				eb_cycle_write(cycle, (wb_dest + offs + space_b4_end), EB_DATA32|EB_BIG_ENDIAN, *((uint32*)(src + offs)) );
			}
		}		
		else // no, do it in one
		{
			//call etherbone function
			for(offs=0;offs<length);offs+=4) 
			{
				eb_cycle_write(cycle, (wb_dest + offs), EB_DATA32|EB_BIG_ENDIAN, *((uint32*)(src + masked_rd_offs + offs)) );
			}
		}
		*rd_offs = end_offs;
		
	} 
	
	
	TRACE_DEV("leaving create_timing_msg_eb_cyc\n");

}
*/	

#endif

void wrc_initialize()
{
  int ret, i;
          uint8_t mac_addr[6];

 sdb_find_devices();
uart_init();
 mprintf("WR Core: starting up...\n");
timer_init(1);
owInit();
mac_addr[0] = 0x08;        //
mac_addr[1] = 0x00;        // CERN OUI
mac_addr[2] = 0x30;        //

own_scanbus(ONEWIRE_PORT);

if (get_persistent_mac(ONEWIRE_PORT, mac_addr) == -1) {

mprintf("Unable to determine MAC address\n");

mac_addr[0] = 0x11;        //

mac_addr[1] = 0x22;        //

 mac_addr[2] = 0x33;        // fallback MAC if get_persistent_mac fails
mac_addr[3] = 0x44;        //
 mac_addr[4] = 0x55;        //
mac_addr[5] = 0x66;        //

        }
TRACE_DEV("Local MAC address: %x:%x:%x:%x:%x:%x\n", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);

  /*
  ip[0] = 10;   
  ip[1] = 0;   
  ip[2] = 0;    
  ip[3] = 100; 
 */ 


 

  ep_init(mac_addr);
  ep_enable(1, 1);

  minic_init();
  pps_gen_init();
  wrc_ptp_init();
  
#if WITH_ETHERBONE
  ipv4_init("wru1");
  //setIP(ip);
  arp_init("wru1");
#endif

#if FAIR_DATA_MASTER
TRACE_DEV("Setting up ebsocket...\n");

	if ((status = eb_socket_open(EB_ABI_CODE, "wru1", EB_ADDR32|EB_DATA32, &socket)) != EB_OK) {
    		TRACE_DEV("failed to open Etherbone socket: \n");}
		else{
		if ((status = eb_device_open(socket, netaddress, EB_ADDR32|EB_DATA32, 0, &device)) != EB_OK) {
    			TRACE_DEV("failed to open Etherbone device");
  		}	
   	}
	

#endif
}

#define LINK_WENT_UP 1
#define LINK_WENT_DOWN 2
#define LINK_UP 3
#define LINK_DOWN 4

int wrc_check_link()
{
	static int prev_link_state = -1;
	int link_state = ep_link_up(NULL);
	int rv = 0;

  if(!prev_link_state && link_state)
  {
    TRACE_DEV("Link up.\n");
    gpio_out(GPIO_LED_LINK, 1);
    rv = LINK_WENT_UP;
  } 
  else if(prev_link_state && !link_state)
  {
    TRACE_DEV("Link down.\n");
    gpio_out(GPIO_LED_LINK, 0);
    rv = LINK_WENT_DOWN;
  }  else rv = (link_state ? LINK_UP : LINK_DOWN);
  prev_link_state = link_state;

  return rv;
}

int wrc_extra_debug = 0;

void wrc_debug_printf(int subsys, const char *fmt, ...)
{
	va_list ap;
	
	if(wrc_ui_mode) return;
	
	va_start(ap, fmt);
	
	if(wrc_extra_debug || (!wrc_extra_debug && (subsys & TRACE_SERVO)))
	 	vprintf(fmt, ap);
	
	va_end(ap);
}

static int wrc_enable_tracking = 1;
static int ptp_enabled = 1;
int wrc_man_phase = 0;

static void ui_update()
{

		if(wrc_ui_mode == UI_GUI_MODE)
		{
			wrc_mon_gui();
			if(uart_read_byte() == 27)
			{
				shell_init();
				wrc_ui_mode = UI_SHELL_MODE;
			}	
		}
    else if(wrc_ui_mode == UI_STAT_MODE)
    {
      wrc_log_stats(0);
			if(uart_read_byte() == 27)
			{
				shell_init();
				wrc_ui_mode = UI_SHELL_MODE;
			}	
    }
		else
			shell_interactive();

}

#if FAIR_DATA_MASTER
int stop()
{
	TRACE_DEV("Callback STOP\n");
	return 1;
}

void set_stop()
{
	TRACE_DEV("Callback SETSTOP\n");
}


/*
static void send_timing_msgs()
{
	uint8_t i;   

    	eb_cycle_t cycle;
	eb_address_t address = 0x00000000;

	
	TRACE_DEV("Checking for pending timing msgs...\n");

	
	//something in the timing msg buffer to send?
	if(tx_timing_msg_waiting)
	{
		// create a new eb cycle
		if ((status = eb_cycle_open(device, 0, &set_stop, &cycle)) != EB_OK) 
		{
		    TRACE_DEV(" failed to create cycle, status: %d\n", status);
		    return;
		}
		//while there are messages, create new records
	 	while(tx_timing_msg_waiting(wr_offs, rd_offs)) 
		{
					create_timing_msg_eb_cyc(address, src, wr_offs, rd_offs, TIMING_MSG_LEN); 
					TRACE_DEV("Adding msg %d to cycle\n", i);					
					i++;

		}
		//close and send the cycle
		TRACE_DEV("Sending eb packet\n");		
		eb_cycle_close_silently(cycle);
		eb_device_flush(device);
	}
}
*/

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
	
    uint32_t  nsec;
    static uint32_t param=0;
    uint64_t sec, nTimeStamp, nTimeOffset, ID;

pps_gen_get_time(&sec, &nsec);
TRACE_DEV("Seconds:\t %x \n nSeconds: %x\n", (uint32_t)sec, nsec);

    nTimeOffset = 1000000;	
    nTimeStamp 	= (sec * 1000000000) + nsec + nTimeOffset;
    ID		= 0xD15EA5EDBABECAFE;
    param++;
 
/* Begin the cycle */
  if ((status = eb_cycle_open(device, 0, &set_stop, &cycle)) != EB_OK) 
  {
    TRACE_DEV(" failed to create cycle, status: %d\n", status);
    return;
  }
    
  
  

  eb_cycle_write(cycle, address+0x00, EB_DATA32|EB_BIG_ENDIAN, (uint32_t)(nTimeStamp>>32));
  eb_cycle_write(cycle, address+0x04, EB_DATA32|EB_BIG_ENDIAN, (uint32_t)(nTimeStamp & 0x00000000ffffffff));
  eb_cycle_write(cycle, address+0x08, EB_DATA32|EB_BIG_ENDIAN, (uint32_t)(ID>>32));
  eb_cycle_write(cycle, address+0x0C, EB_DATA32|EB_BIG_ENDIAN, (uint32_t)(ID & 0x00000000ffffffff));
  eb_cycle_write(cycle, address+0x10, EB_DATA32|EB_BIG_ENDIAN, param);

  eb_cycle_close_silently(cycle);

  //TRACE_DEV("before flush ");
  eb_device_flush(device);
  //TRACE_DEV("after flush ");	
}
#endif

int main(void)
{
  wrc_extra_debug = 1;
 
wrc_ui_mode = UI_SHELL_MODE;

  wrc_initialize();

  shell_init();

  wrc_ptp_set_mode(WRC_MODE_SLAVE);
  wrc_ptp_start();

  //try to read and execute init script from EEPROM
  shell_boot_script();
  uint8_t send = 1;

  for (;;)
  {

  int l_status = wrc_check_link();

    switch (l_status)
    {
#if WITH_ETHERBONE
      case LINK_WENT_UP:
        needIP = 1;
	 break;
#endif
        
      case LINK_UP:
        update_rx_queues();
#if WITH_ETHERBONE
        ipv4_poll();
        arp_poll();
	
#endif
        break;

      case LINK_WENT_DOWN:
        if( wrc_ptp_get_mode() == WRC_MODE_SLAVE )
          spll_init(SPLL_MODE_FREE_RUNNING_MASTER, 0, 1);
        break;
    }

    ui_update();
    wrc_ptp_update();
    spll_update_aux_clocks();

#if FAIR_DATA_MASTER
		
		if(send && !needIP) 
		{
					
			TRACE_DEV("Sending EB packet on WRU1...\n");

			send_EB_Demo_packet();
			send = 0;
			TRACE_DEV("...done\n");
			
		}
		

#endif

  }
}
