#ifndef __EEPROM_H
#define __EEPROM_H

#define SFP_SECTION_PATTERN 0xdeadbeef
#define SFPS_MAX 4
#define SFP_PN_LEN 16
// I2C
#define EE_BASE_CAL 4*1024
#define EE_BASE_SFP 4*1024+4
#define EE_BASE_INIT 4*1024+SFPS_MAX*29

#define EE_RET_I2CERR -1
#define EE_RET_DBFULL -2
#define EE_RET_CORRPT -3
#define EE_RET_POSERR -4
// oneWire
#define OW_ROM_BASE_CAL 0x0200
#define OW_ROM_SCRIPT   0x0400
//#define EEPROM_MAC_PAGE 0x0
#define OW_PAGE_SIZE    0x20
#define NUM_OF_SFP      4
#define OW_BUF          32

#define OW_ERR        -5
#define OW_DBFULL     -6
#define OW_POSERR -7

extern int32_t sfp_alpha;
extern int32_t sfp_deltaTx;
extern int32_t sfp_deltaRx;
extern uint32_t cal_phase_transition;
extern uint8_t has_eeprom;
extern uint8_t has_ow_eeprom;

struct s_sfpinfo {
   char pn[SFP_PN_LEN];
   int32_t alpha;
   int32_t dTx;
   int32_t dRx;
   uint8_t chksum;
} __attribute__ ((__packed__));

uint8_t eeprom_present(uint8_t i2cif, uint8_t i2c_addr);
int     eeprom_read(uint8_t i2cif, uint8_t i2c_addr, uint32_t offset, uint8_t * buf, size_t size);
int     eeprom_write(uint8_t i2cif, uint8_t i2c_addr, uint32_t offset,uint8_t * buf, size_t size);

int32_t eeprom_sfpdb_erase(uint8_t i2cif, uint8_t i2c_addr);
int32_t eeprom_sfp_section(uint8_t i2cif, uint8_t i2c_addr, size_t size, uint16_t * section_sz);
int8_t  eeprom_match_sfp(uint8_t i2cif, uint8_t i2c_addr, struct s_sfpinfo *sfp);
int32_t eeprom_get_sfp(uint8_t i2cif, uint8_t i2c_addr, struct s_sfpinfo * sfp, uint8_t add, uint8_t pos);

int8_t  eeprom_phtrans(uint8_t i2cif, uint8_t i2c_addr, uint32_t * val, uint8_t write);

int8_t  eeprom_init_erase(uint8_t i2cif, uint8_t i2c_addr);
int8_t  eeprom_init_add(uint8_t i2cif, uint8_t i2c_addr, const char *args[]);
int32_t eeprom_init_show(uint8_t i2cif, uint8_t i2c_addr);
int8_t  eeprom_init_readcmd(uint8_t i2cif, uint8_t i2c_addr, uint8_t *buf, uint8_t bufsize, uint8_t next);
int8_t  eeprom_init_purge(uint8_t i2cif, uint8_t i2c_addr);

uint8_t ow_eeprom_present(uint8_t ow_portnum, uint32_t family);
int8_t  ow_eeprom_read(uint8_t ow_portnum, int32_t ow_addr, uint8_t *buf);
int8_t  ow_eeprom_write(uint8_t ow_portnum, int32_t ow_addr, uint8_t *buf);

int8_t  rom_read(uint8_t ow_portnum, int32_t ow_addr, uint8_t *buf);
int8_t  rom_write(uint8_t ow_portnum, int32_t ow_addr, uint8_t *buf);

int32_t ow_eeprom_sfpdb_erase(uint8_t ow_portnum, uint32_t ow_addr);
uint8_t ow_eeprom_match_sfp(uint8_t ow_portnum, uint32_t ow_addr, struct s_sfpinfo* sfp);
int8_t  ow_eeprom_get_sfp(uint8_t ow_portnum, uint32_t ow_addr, struct s_sfpinfo* sfp, uint8_t add, uint8_t pos);
uint8_t ow_eeprom_phtrans(uint8_t ow_portnum, uint32_t ow_addr, uint32_t *val, uint8_t write);

int8_t  ow_eeprom_init_erase(uint8_t ow_portnum, uint32_t ow_addr);
int8_t  ow_eeprom_init_add(uint8_t ow_portnum, uint32_t ow_addr, const char *args[]);
int8_t  ow_eeprom_init_show(uint8_t ow_portnum, uint32_t ow_addr);
int32_t ow_eeprom_init_readcmd(uint8_t ow_portnum, uint32_t ow_addr, uint8_t *buf, uint8_t next);
//int8_t eeprom_init_purge(uint8_t ow_portnum, uint32_t ow_addr);

#endif
