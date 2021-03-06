#include <stdio.h>
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
//#include "eeprom.h"
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
  }
}
