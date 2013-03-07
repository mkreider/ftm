/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Cesar Prados <c.prados@gsi.de>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 *
 * Command: sfp
 * Arguments: subcommand [subcommand-specific args]
 * Description: SFP detection/database manipulation.
 *
 * Subcommands:
 * add vendor_type delta_tx delta_rx alpha - adds an SFP to the database, with given alpha/delta_rx/delta_rx values
 * show  - shows the SFP database
 * match - tries to get calibration parameters from DB for a detected SFP
 * erase - cleans the SFP database
 * detect- detects the transceiver type
*/
#include <string.h>
#include <stdlib.h>
#include <wrc.h>

#include "shell.h"
#include "eeprom.h"
#include "onewire.h"
#include "syscon.h"
#include "sfp.h"

int cmd_sfp(const char *args[])
{
   struct s_sfpinfo sfp;
   static char pn[SFP_PN_LEN+1] = "\0";
   int16_t sfpcount=1, i, j, temp=0;

   if(args[0] && !strcasecmp(args[0], "detect"))
   {
      if(!sfp_present())
         mprintf("No SFP.\n");
      else
         sfp_read_part_id(pn);
      pn[16]=0;
      mprintf("%s\n",pn);
      return 0;
   }
   else if (!strcasecmp(args[0], "erase"))
   {
      if( ow_eeprom_sfpdb_erase(ONEWIRE_PORT, OW_ROM_BASE_CAL) == OW_ERR)
         mprintf("Could not erase DB\n");
   }
   else if (args[0] && !strcasecmp(args[0], "add"))
   {
      if(strlen( args[1] )>16) temp=16;
      else temp=strlen( args[1] );
      for(i=0; i<temp; ++i)
         sfp.pn[i]=args[1][i];
      while(i<16) sfp.pn[i++]=' ';   //padding
      sfp.dTx = atoi(args[2]);
      sfp.dRx = atoi(args[3]);
      sfp.alpha = atoi(args[4]);

      temp = ow_eeprom_get_sfp(ONEWIRE_PORT, OW_ROM_BASE_CAL, &sfp, 1, 0);
      if(temp == OW_DBFULL)
         mprintf("SFP DB is full\n");
      else if(temp == OW_ERR)
         mprintf("oneWire error\n");
      else
         mprintf("%d SFPs in DB\n", temp);
   }
   else if (args[0] && !strcasecmp(args[0], "show"))
   {

      for(i=0; i<sfpcount; ++i)
      {
         memset(&sfp,0,32);
         temp = ow_eeprom_get_sfp(ONEWIRE_PORT, OW_ROM_BASE_CAL, &sfp, 0, i);

         if(i==0)
         {
            sfpcount=temp; //only in first round valid sfpcount is returned from eeprom_get_sfp
            if(sfpcount == 0 || sfpcount == 0xFF)
            {
               mprintf("SFP database empty...\n");
               return 0;
            }
            else if(sfpcount == -1)
            {
               mprintf("SFP database corrupted...\n");
               return 0;
            }
         }

         if(temp<0)
         {
            mprintf("Error reading the EEPROM \n");
         }
         else
         {
            mprintf("%d: PN:", i+1);
            for(j=0; j<16; ++j)
               mprintf("%c", sfp.pn[j]);
            mprintf(" dTx: %d, dRx: %d, alpha: %d\n", sfp.dTx, sfp.dRx, sfp.alpha);
         }
      }
   }
   else if (args[0] && !strcasecmp(args[0], "match"))
   {
      if(pn[0]=='\0')
      {
         mprintf("Run sfp detect first\n");
         return 0;
      }
      strncpy(sfp.pn, pn, SFP_PN_LEN);
      if(ow_eeprom_match_sfp(ONEWIRE_PORT, OW_ROM_BASE_CAL, &sfp) > 0)
      {
         mprintf("SFP matched, dTx=%d, dRx=%d, alpha=%d\n", sfp.dTx, sfp.dRx, sfp.alpha);
         sfp_deltaTx = sfp.dTx;
         sfp_deltaRx = sfp.dRx;
         sfp_alpha = sfp.alpha;
      }
      else
         mprintf("Could not match to DB\n");
      return 0;
   }
   // Debug commnads
   // sfp read #page -> read a page that should have sfp information
   /*else if (args[0] && !strcasecmp(args[0], "read"))
   {
      mprintf("Reading ROM \n");

      temp = ow_eeprom_read(ONEWIRE_PORT,0x200+(0x20*(atoi(args[1]))),(uint8_t*)&sfp);

      if(temp<0)
         mprintf("Error Reading \n");
      else
      {
         mprintf("PN:");
         for(j=0; j<16; ++j)
            mprintf("%c", sfp.pn[j]);

         mprintf(" dTx: %d, dRx: %d, alpha: %d\n", sfp.dTx, sfp.dRx, sfp.alpha);
      }
   }*/
   // sfp write #page #sfp# tx rx alpha -> write a page that should have sfp information
   /*else if (args[0] && !strcasecmp(args[0], "write"))
   {
      mprintf("Writing ROM \n");

      if(strlen( args[2] )>16) temp=16;
      else temp=strlen( args[2] );
      for(i=0; i<temp; ++i)
         sfp.pn[i]=args[2][i];

      while(i<16) sfp.pn[i++]=' ';   //padding
      sfp.dTx = atoi(args[3]);
      sfp.dRx = atoi(args[4]);
      sfp.alpha = atoi(args[5]);

      temp = ow_eeprom_write(ONEWIRE_PORT,0x200+(0x20*(atoi(args[1]))),(uint8_t*)&sfp);

      if(temp<0)
         mprintf("Error Writing \n");
   }*/


   return 0;
}
