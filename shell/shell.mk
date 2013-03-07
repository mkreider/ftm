obj-y += \
	shell/shell.o \
	shell/environ.o \
	shell/cmd_version.o \
	shell/cmd_pll.o \
	shell/cmd_stat.o \
	shell/cmd_ptp.o \
	shell/cmd_mode.o \
	shell/cmd_time.o \
	shell/cmd_gui.o \
	shell/cmd_sdb.o \
	shell/cmd_mac.o \

obj-$(CONFIG_ETHERBONE) += shell/cmd_ip.o

obj-$(CONFIG_EEPROM) += shell/cmd_init.o \
						shell/cmd_calib.o \
						shell/cmd_sfp.o
						
obj-$(CONFIG_EEPROM_OW) += shell/cmd_init_ow.o \
						shell/cmd_calib_ow.o \
						shell/cmd_sfp_ow.o 
