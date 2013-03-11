deps_config := \
	/home/mkreider/hdlprojects/bel_projects/ip_cores/wrpc-sw/Kconfig

include/config/auto.conf: \
	$(deps_config)


$(deps_config): ;
