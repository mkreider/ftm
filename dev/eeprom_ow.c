/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Cesar Prados <c.prados@gsi.de>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <string.h>
#include <stdlib.h>
#include <wrc.h>

#include "types.h"
#include "onewire.h"
#include "eeprom.h"
#include "board.h"
#include "syscon.h"
#include "../sockitowm/ownet.h"
#include "../sockitowm/findtype.h"
#include "../sockitowm/eep43.h"
/*
 * The SFP section is placed somewhere inside FMC EEPROM and it really does not
 * matter where (can be a binary data inside the Board Info section but can be
 * placed also outside the FMC standardized EEPROM structure.
 * The only requirement is that it starts with 0xdeadbeef pattern. The structure of SFP section is:
 *
 * ----------------------------------------------
 * | cal_ph_trans (4B) | SFP count (1B) |
 * --------------------------------------------------------------------------------------------
 * |   SFP(1) part number (16B)       | alpha (4B) | deltaTx (4B) | deltaRx (4B) | chksum(1B) |
 * --------------------------------------------------------------------------------------------
 * |   SFP(2) part number (16B)       | alpha (4B) | deltaTx (4B) | deltaRx (4B) | chksum(1B) |
 * --------------------------------------------------------------------------------------------
 * | (....)                           | (....)     | (....)       | (....)       | (...)      |
 * --------------------------------------------------------------------------------------------
 * |   SFP(count) part number (16B)   | alpha (4B) | deltaTx (4B) | deltaRx (4B) | chksum(1B) |
 * --------------------------------------------------------------------------------------------
 *
 * Fields description:
 * cal_ph_trans       - t2/t4 phase transition value (got from measure_t24p() ), contains
 *                      _valid_ bit (MSB) and 31 bits of cal_phase_transition value
 * count              - how many SFPs are described in the list (binary)
 * SFP(n) part number - SFP PN as read from SFP's EEPROM (e.g. AXGE-1254-0531)
 *                      (16 ascii chars)
 * checksum           - low order 8 bits of the sum of all bytes for the SFP(PN,alpha,dTx,dRx)
 *
 * The init script area consist of 2-byte size field and a set of shell commands
 * separated with '\n' character.
 *
 * -------------------------------------------------
 * | number of row (32 bytes) used in the room (1B) |
 * -------------------------------------------------
 * | shell commands ended with '\n'                 |
 * |                                                |
 * |                                                |
 * -------------------------------------------------
 */
uint8_t has_ow_eeprom = 0;
uint8_t ow_eeprom_present(uint8_t ow_portnum, uint32_t family)
{
   has_ow_eeprom=0;

   if(FindDevices(ow_portnum, &gsi_rom, family, MAX_DEV1WIRE)) /* EEPROM */
      has_ow_eeprom = 1;

   return 0;
}
int8_t ow_eeprom_read(uint8_t ow_portnum, int32_t ow_addr, uint8_t* buf)
{
   owLevel(ow_portnum, MODE_NORMAL);
   if(!ReadMem43(ow_portnum, gsi_rom, ow_addr, buf))
      return OW_ERR;
   return 0;
}
int8_t ow_eeprom_write(uint8_t ow_portnum, int32_t ow_addr, uint8_t* buf)
{
   uint8_t buf_w[OW_BUF];
   memcpy(buf_w,buf,sizeof(buf_w));
   owLevel(ow_portnum, MODE_NORMAL);
   if (!Write43(ow_portnum, gsi_rom, ow_addr, buf_w))
      return OW_ERR;
   return 0;
}

int32_t ow_eeprom_sfpdb_erase(uint8_t ow_portnum, uint32_t ow_addr)
{
   uint8_t buf[OW_BUF];
   if(ow_eeprom_read(ow_portnum, ow_addr, buf))
      return OW_ERR;
   mprintf("Number of SFP in DB %d \n", buf[NUM_OF_SFP]);
   buf[NUM_OF_SFP] &= 0x0; // | cal_ph_trans (4B) | SFP count (1B) |
   if(ow_eeprom_write(ow_portnum, ow_addr, buf))
      return OW_ERR;
   mprintf("Erase..Number of SFP in DB %d \n", buf[NUM_OF_SFP]);
   return 0;
}

int8_t ow_eeprom_get_sfp(uint8_t ow_portnum, uint32_t ow_addr,struct s_sfpinfo* sfp, uint8_t add, uint8_t pos)
{
   uint8_t sfpc=0;
   uint8_t i, chksum=0;
   uint8_t* ptr;
   uint8_t buf[OW_BUF];
   if( pos >= SFPS_MAX)
      return OW_POSERR;
   //read how many SFPs are in the database when pos=0
   if(ow_eeprom_read(ow_portnum, ow_addr, buf))
      return OW_ERR;
   sfpc = buf[NUM_OF_SFP];
   mprintf("Number of SFP in data base %d \n",sfpc);
   if(pos >= SFPS_MAX)   // position out of range
      return OW_POSERR;
   if(add && (sfpc == SFPS_MAX)) //no more space in database for new SFPs
      return OW_DBFULL;
   else if(!pos && !add && (sfpc==0)) // there are no SFPs in the database
      return sfpc;

   if(!add)  // showing SFPs in the DB
   {
      ptr = (uint8_t *) sfp;
      if(ow_eeprom_read(ow_portnum,ow_addr+((pos+1)*OW_PAGE_SIZE*2),ptr))
         return OW_ERR;

      for(i=0; i<4; ++i) //'-1' because we do not include chksum in computation
         chksum = (uint8_t) ((uint16_t)chksum + *(ptr++)) & 0xff;
   }
   else      // Adding a new SFP in the DB
   {

      //for(i=0; i<4;++i) //'-1' because  we do not include chksum in computation
      //   chksum = (uint8_t) ((uint16_t)chksum + *(ptr++)) & 0xff;
      //ptr[5] = chksum;

      //add SFP at the end of the DB
      if(ow_eeprom_write(ow_portnum, ow_addr +((sfpc+1)*OW_PAGE_SIZE*2), (uint8_t *)sfp))
         return OW_ERR;
      sfpc++;
      buf[4] = sfpc;
      // writing the new number of SFPs in DB
      if(ow_eeprom_write(ow_portnum, ow_addr, buf))
         return OW_ERR;
   }
   return sfpc;
}
uint8_t ow_eeprom_match_sfp(uint8_t ow_portnum, uint32_t ow_addr, struct s_sfpinfo* sfp)
{
   uint8_t sfpcount = 1;
   int8_t i, temp;
   struct s_sfpinfo dbsfp;
   for(i=0; i<sfpcount; ++i)
   {
      temp = ow_eeprom_get_sfp(ONEWIRE_PORT, OW_ROM_BASE_CAL, &dbsfp, 0, i);
      if(!i)
      {
         sfpcount=temp;
         //only in first round valid sfpcount is returned from eeprom_get_sfp
         if(sfpcount == 0 || sfpcount == 0xFF)
            return 0;
         else if(sfpcount<0)
            return sfpcount;
      }
       if (!strncmp(dbsfp.pn, sfp->pn, 16)) {
         sfp->dTx = dbsfp.dTx;
         sfp->dRx = dbsfp.dRx;
         sfp->alpha = dbsfp.alpha;
         return 1;
      }
   }
   return 0;
}
uint8_t ow_eeprom_phtrans(uint8_t ow_portnum, uint32_t ow_addr, uint32_t *val, uint8_t write)
{
   uint8_t buf[OW_BUF];
   if(write)
   {
      *val |= (1<<31);
      if(ow_eeprom_read(ow_portnum, ow_addr, buf))
         return OW_ERR;
      memcpy(buf,val,4);
      mprintf("writing calibration... \n");
      if(ow_eeprom_write(ow_portnum, ow_addr, buf))
         return OW_ERR;
      else
         return 1;
   }
   else
   {
      if(ow_eeprom_read(ow_portnum, ow_addr, buf))
         return OW_ERR;
      memcpy(val,buf,4);
      if(!(*val&(1<<31)))
         return 0;
      *val &= 0x7fffffff;  //return ph_trans value without validity bit
      return 1;
   }
   return 0;
}
int8_t ow_eeprom_init_erase(uint8_t ow_portnum, uint32_t ow_addr)
{
   uint8_t     buf[OW_BUF];
   buf[0]=0xff;
   if(ow_eeprom_write(ow_portnum,ow_addr, buf))
      return OW_ERR;
   return 0;
}
int8_t ow_eeprom_init_add(uint8_t ow_portnum, uint32_t ow_addr, const char *args[])
{
   uint8_t i=1;
   uint8_t buf[OW_BUF]={"\0"};
   uint8_t num_cmd[OW_BUF]={"\0"};
   uint16_t used;
   if(ow_eeprom_read(ow_portnum, ow_addr, num_cmd))
      return OW_ERR;
   used = num_cmd[0];
   if( used==0xff ) used=0;  //this means the memory is blank
   while(args[i] != '\0')
   {
      if(strlen((char *)buf)+strlen((char *)args[i]) > OW_BUF)
      {
         mprintf("Sorry but the command is too large \n");
         return OW_ERR;
      }
      strcat((char *)buf,(char *)args[i]);
      strcat((char *)buf," ");
      i++;
   }
   buf[strlen((char*)buf)-1]='\n'; // replace last separator with \n

   if(ow_eeprom_write(ow_portnum, ow_addr+(OW_PAGE_SIZE*(used+1)), buf))
      return OW_ERR;
   used++;
   mprintf("%d commands in init script \n",used);
   num_cmd[0] = used; // update the number of scripts
   if( ow_eeprom_write(ow_portnum, ow_addr, num_cmd) )
      return OW_ERR;
   return 0;
}
int8_t ow_eeprom_init_show(uint8_t ow_portnum, uint32_t ow_addr)
{
   uint8_t     buf[OW_BUF]={"\0"};
   uint16_t    used,i,j;
   if(ow_eeprom_read(ow_portnum, ow_addr, buf))
      return OW_ERR;
   used = (uint16_t)buf[0];
   if(used == 0xff)
   {
      used = 0;
      mprintf("Empty init script.. \n");
   }
   for(i=1; i<=used;i++)
   {
      if(ow_eeprom_read(ow_portnum, ow_addr+(i*OW_PAGE_SIZE), buf))
         return OW_ERR;
      mprintf("Command %d ",i);
      for(j=0;j<strlen((char *)buf);++j)
         mprintf("%c",buf[j]);
   }
   return 0;
}

int32_t ow_eeprom_init_readcmd(uint8_t ow_portnum, uint32_t ow_addr, uint8_t *buf, uint8_t next)
{
   static uint16_t read_cmd;
   static uint16_t num_cmd;
   uint8_t tmp_buf[32]={"\0"};

   if (next == 0) {
      if (ow_eeprom_read(ow_portnum, ow_addr, tmp_buf))
         return OW_ERR;
      num_cmd = (uint16_t)tmp_buf[0];
      read_cmd = 0;
   }

   if(num_cmd == 0 || (num_cmd==read_cmd))
      return 0;

   read_cmd++;

   if (ow_eeprom_read(ow_portnum, ow_addr+(read_cmd*OW_PAGE_SIZE), buf))
      return OW_ERR;

   return (int32_t)strlen((char *)buf);
}
