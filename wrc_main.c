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
const char netaddress[] = "hw/00:22:19:21:fb:07/udp/10.0.0.1/port/60368";

 
  eb_socket_t socket;
  eb_status_t status;
  eb_device_t device;
  

#endif

void wrc_initialize()
{
  int ret, i;
  uint8_t mac_addr[6], ds18_id[8] = {0,0,0,0,0,0,0,0};
  char sfp_pn[17];
  
  sdb_find_devices();
  uart_init();
  
  mprintf("WR Core: starting up...\n");
  
  timer_init(1);
  owInit();
  
  mac_addr[0] = 0x08;   //
  mac_addr[1] = 0x00;   // CERN OUI
  mac_addr[2] = 0x30;   //  
  mac_addr[3] = 0xDE;   // fallback MAC if get_persistent_mac fails
  mac_addr[4] = 0xAD;
  mac_addr[5] = 0x42;
  
  own_scanbus(ONEWIRE_PORT);
  if (get_persistent_mac(ONEWIRE_PORT, mac_addr) == -1) {
    mprintf("Unable to determine MAC address\n");
  }

  TRACE_DEV("Local MAC address: %x:%x:%x:%x:%x:%x\n", mac_addr[0],mac_addr[1],mac_addr[2],mac_addr[3],mac_addr[4],mac_addr[5]);

  ep_init(mac_addr);
  ep_enable(1, 1);

  minic_init();
  pps_gen_init();
  wrc_ptp_init();
  
#if WITH_ETHERBONE
  ipv4_init("wru1");
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



static void send_EB_packet()
{
    eb_data_t mask;
    eb_width_t line_width;
    eb_format_t line_widths;
    eb_format_t device_support;
    eb_format_t write_sizes;
    eb_format_t format;
    eb_cycle_t cycle;

    eb_data_t data;
    eb_address_t address;
	


/* Begin the cycle */
  if ((status = eb_cycle_open(device, 0, &set_stop, &cycle)) != EB_OK) 
  {
    TRACE_DEV(" failed to create cycle, status: %d\n", status);
    return;
  }
    
  
  eb_cycle_write(cycle, 0x0000000C, EB_DATA32|EB_BIG_ENDIAN, 0xDEADBEEF);	
  eb_cycle_close_silently(cycle);

  TRACE_DEV("before flush ");
  eb_device_flush(device);
  TRACE_DEV("after flush ");	
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
  needIP = 0;
  uint8_t send = 1;

  for (;;)
  {
 
  int l_status = wrc_check_link();

    switch (l_status)
    {
#if WITH_ETHERBONE
      case LINK_WENT_UP:
        needIP = 0;
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
		
		if(send) 
		{
			TRACE_DEV("Sending EB packet on WRU1...\n");

			send_EB_packet();
			send = 0;
			TRACE_DEV("...done\n");
	
		}
		

#endif

  }
}
