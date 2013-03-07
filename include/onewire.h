#ifndef PERSISTENT_MAC_H
#define PERSISTENT_MAC_H

#define EEPROM_MAC_PAGE 0x0
#define MAX_DEV1WIRE 8

#define FOUND_DS18B20   0x01
#define FOUND_DS28AE    0x02

#define TEMP_28     0x28
#define TEMP_42     0x42
#define ROM_43      0x43

extern uint8_t gsi_rom[8];

void own_scanbus(uint8_t portnum);
int16_t own_readtemp(uint8_t portnum, int16_t * temp, int16_t * t_frac);
/* 0 = success, -1 = error */
int8_t get_persistent_mac(uint8_t portnum, uint8_t * mac);
int8_t set_persistent_mac(uint8_t portnum, uint8_t * mac);

#endif
