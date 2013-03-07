/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Cesar Prados <c.prados@gsi.de>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 *
 * Command: calib
 * Arguments: subcommand [subcommand-specific args]
 * Description: executes the WR calibration
 *
 * Subcomands:
 * none  - shows the calibration, if there is not previous calibration,
 *         does the calibration
 * force - forces to make a new calibration
*/

#include <string.h>
#include <stdlib.h>
#include <wrc.h>

#include "shell.h"
#include "eeprom.h"
#include "syscon.h"
#include "onewire.h"

extern int measure_t24p(uint32_t *value);

int cmd_calib(const char *args[])
{
   uint32_t trans;

   if(args[0] && !strcasecmp(args[0], "force"))
   {
      if( measure_t24p(&trans)<0 )
         return -1;
      return ow_eeprom_phtrans(ONEWIRE_PORT, OW_ROM_BASE_CAL, &trans, 1);
   }
   else if( !args[0] )
   {
      if( ow_eeprom_phtrans(ONEWIRE_PORT, OW_ROM_BASE_CAL, &trans, 0) > 0)
      {
         mprintf("Found phase transition in EEPROM: %dps\n", trans);
         cal_phase_transition = trans;
         return 0;
      }
      else
      {
         mprintf("Measuring t2/t4 phase transition...\n");
         if( measure_t24p(&trans)<0 )
            return -1;
         cal_phase_transition = trans;
         return ow_eeprom_phtrans(ONEWIRE_PORT, OW_ROM_BASE_CAL, &trans, 1);
      }
   }
   return 0;
}
