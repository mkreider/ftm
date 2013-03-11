/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2011,2012 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 * Author: Grzegorz Daniluk <grzegorz.daniluk@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <stdio.h>

#include "etherbone.h"
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



int wrc_ui_mode = UI_SHELL_MODE;

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
	TRACE_DEV("Callback STOP\n");
	return 1;
}

void set_stop()
{
	TRACE_DEV("Callback SETSTOP\n");
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
   if( last_sec == sec) return;
   last_sec = sec;

   TRACE_DEV("Seconds:\t %x \n nSeconds: %x\n", (uint32_t)sec, nsec);

   nTimeOffset = 1000000;	
   nTimeStamp 	= sec & 0xFFFFFFFFFFULL;
   nTimeStamp  *= (1000000000/8);
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

   //TRACE_DEV("before flush ");
   eb_device_flush(device);
   //TRACE_DEV("after flush ");	
}




int main(void)
{
	wrc_extra_debug = 1;
	wrc_ui_mode = UI_SHELL_MODE;
	_endram = ENDRAM_MAGIC;

	wrc_initialize();
	shell_init();

	wrc_ptp_set_mode(WRC_MODE_SLAVE);
	wrc_ptp_start();

	//try to read and execute init script from EEPROM
	shell_boot_script();

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

      if(!needIP) send_EB_Demo_packet();
	}
}
