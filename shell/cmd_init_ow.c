/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Cesar Prados <c.prados@gsi.de>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 * Command: init
 * Arguments: subcommand [subcommand-specific args]
 * Description: creates a script with commands
 *
 * Subcomands:
 * add   - adds a new command to the script
 * erase - erases the script
 * show  - shows the commands in the script
 * boot  - executes the script
 * purge - to be implemented
*/


#include <string.h>
#include <wrc.h>
#include "shell.h"
#include "eeprom.h"
#include "syscon.h"
#include "i2c.h"
#include "onewire.h"

int cmd_init(const char *args[])
{
   ow_eeprom_present(ONEWIRE_PORT, GSI_EEPROM);

   if(!has_ow_eeprom)
   {
      mprintf("EEPROM not found..\n");
      return -1;
   }

   if(args[0] && !strcasecmp(args[0], "erase"))
   {
      if(ow_eeprom_init_erase(ONEWIRE_PORT, OW_ROM_SCRIPT))
         mprintf("Could not erase init script\n");
   }
   else if(args[0] && !strcasecmp(args[0], "purge"))
   {
      //ow_eeprom_init_purge(WRPC_FMC_I2C, FMC_EEPROM_ADR);
   }
   else if(args[1] && !strcasecmp(args[0], "add"))
   {
      if(ow_eeprom_init_add(ONEWIRE_PORT, OW_ROM_SCRIPT, args))
         mprintf("Could not add the command\n");
      else
         mprintf("OK.\n");
   }
   else if(args[0] && !strcasecmp(args[0], "show"))
   {
      ow_eeprom_init_show(ONEWIRE_PORT, OW_ROM_SCRIPT);
   }
   else if(args[0] && !strcasecmp(args[0], "boot"))
   {
      shell_boot_script();
   }

   return 0;
}

