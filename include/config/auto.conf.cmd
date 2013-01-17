deps_config := \
	/home/mkreider/hdlprojects/wrpc-sw/Kconfig

include/config/auto.conf: \
	$(deps_config)


$(deps_config): ;
