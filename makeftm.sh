cd ~/hdlprojects/wr-cores/ip_cores/etherbone-core/api
make clean
make
cp ./etherbone.h ~/hdlprojects/wrpc-sw
cp ./etherbone.a ~/hdlprojects/wrpc-sw
cd ~/hdlprojects/wrpc-sw
make clean
make



