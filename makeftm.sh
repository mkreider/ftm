cd ~/hdlprojects/wr-cores/ip_cores/etherbone-core/api
make clean
make
cp ./etherbone.h ~/hdlprojects/ftm
cp ./etherbone.a ~/hdlprojects/ftm
cd ~/hdlprojects/ftm
make clean
make
lm32-ctl load wrc.elf



