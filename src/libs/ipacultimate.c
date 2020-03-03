/*
 Name        : ipacultimate.c
 Author      : Robert Abram, Katie Snow
 Version     :
 Copyright   : Copyright 2014 Robert Abram, Katie Snow
 Description : Ultimarc IPac Ultimate configuration library
 ============================================================================
 */

/* C */
#include <stdio.h>
#include <string.h>

/* Unix */
#include <json.h>
#include <libusb-1.0/libusb.h>

/* Local */
#include "common.h"
#include "ipacultimate.h"
#include "ipacseries.h"
#include "uhid.h"
#include "dbg.h"
#include "ulboard.h"

struct ipacultimate pUltimate;

bool isIPACUltimateConfig(json_object* jobj, ulboard* board)
{
  bool isBoardCfg = false;

  if (board->type == ulboard_type_ultimate)
  {
    isBoardCfg = validateIPacUltimateData(jobj);
  }

  return isBoardCfg;
}

bool validateIPacUltimateData(json_object* jobj)
{
  const char invalidKey = 0x00;
  char data;

  bool valid = true;
  int idx = 0;
  int boardID = 0;
  json_object* tmp = NULL;
  json_object* leds = NULL;
  json_object* led = NULL;
  json_object* pins = NULL;

  pUltimate.allIntensities = false;
  pUltimate.ledMapIntensity = false;
  pUltimate.random = false;
  pUltimate.boardIDUpdate = false;
  pUltimate.pins = false;

  if (checkBoardID(jobj, "board id"))
  {
    /* Figure out what items we have in this file */
    if (json_object_object_get_ex(jobj, "intensity", &leds))
    {
      if (json_object_get_type(leds) == json_type_array)
      {
        for (idx = 0; idx < json_object_array_length(leds); ++ idx)
        {
          led = json_object_array_get_idx(leds, idx);
          /* required entries */
          if (!json_object_object_get_ex(led, "led", &tmp) &&
              !json_object_object_get_ex(led, "intensity", &tmp))
          {
            log_err ("Missing required entries.  Valid entries 'led' and 'intensity'");
            valid = false;
          }

          pUltimate.ledMapIntensity = true;
        }
      }
      else
      {
        log_err ("'intensity' is not defined as an array");
      }
    }
    if (json_object_object_get_ex(jobj, "LED intensity all", &tmp))
    {
      /* can't have the intensity entry with 'LED intensity all' */
      if (pUltimate.ledMapIntensity == true)
      {
        log_err ("'intensity' and 'LED intensity all' are not permitted same configuration file");
        valid = false;
      }
      else if (json_object_get_type(tmp) == json_type_int)
      {
        pUltimate.allIntensities = true;
      }
      else
      {
        log_err ("'LED intensity all' is not defined as an integer");
        valid = false;
      }
    }

    if (json_object_object_get_ex(jobj, "LED states random", &tmp))
    {
      if (json_object_get_type(tmp) == json_type_boolean)
      {
        pUltimate.random = json_object_get_boolean(tmp);
      }
      else
      {
        log_err ("'LED states random' is not defined as a boolean");
        valid = false;
      }
    }

    if (json_object_object_get_ex(jobj, "fade rate", &tmp))
    {
      if (json_object_get_type(tmp) == json_type_int)
      {
        if (json_object_get_int(tmp) >= 0 &&
            json_object_get_int(tmp) <= 255)
        {
          pUltimate.fadeRate = true;
        }
        else
        {
          log_err ("'Fade rate' value is not between 0 and 255");
          valid = false;
        }
      }
      else
      {
        log_err ("'Fade rate' is not defined as an integer");
         valid = false;
      }
    }

    if (json_object_object_get_ex(jobj, "pins", &tmp))
    {
      if (json_object_get_type(tmp) == json_type_array)
      {
        if (json_object_array_length(tmp) == 48)
        {
          for (idx = 0; idx < json_object_array_length(tmp); ++ idx)
          {
            pins = json_object_array_get_idx(tmp, idx);

            json_object_object_foreach(pins, key, pin)
            {
              if (json_object_get_type(pin) == json_type_array)
              {
                if (json_object_get_type(json_object_array_get_idx(pin, 0)) != json_type_string ||
                    json_object_get_type(json_object_array_get_idx(pin, 1)) != json_type_string ||
                    json_object_get_type(json_object_array_get_idx(pin, 2)) != json_type_boolean)
                {
                  log_err ("The pin at index %i is not defined as a string, string, boolean", idx);
                   valid = false;
                }
              }
              else
              {
                log_err ("'pin' object is not defined as an array");
                valid = false;
              }
            }
          }
        }
        else
        {
          log_err ("'pins' array is not the correct size. Size is %i, should be 48", json_object_array_length(tmp));
          valid = false;
        }
      }
      else
      {
        log_err ("'pins' is not defined as an array");
        valid = false;
      }

      /* Macro validation */
      valid = validateIPacUltimarcMacros(jobj, valid);

      if (json_object_object_get_ex(jobj, "x threshold", &tmp))
      {
        if (json_object_get_type(tmp) == json_type_int)
        {
          if (json_object_get_int(tmp) <= 0 ||
              json_object_get_int(tmp) > 128)
          {
            log_err ("'x threshold' value is not between 0 and 127");
            valid = false;
          }
        }
        else
        {
          log_err ("'x threshold' is not defined as an integer");
          valid = false;
        }
      }
      else
      {
        log_err ("'x threshold' is required when pins array is defined");
        valid = false;
      }

      if (json_object_object_get_ex(jobj, "y threshold", &tmp))
      {
        if (json_object_get_type(tmp) == json_type_int)
        {
          if (json_object_get_int(tmp) <= 0 ||
              json_object_get_int(tmp) >= 128)
          {
            log_err ("'y threshold' value is not between 0 and 127");
            valid = false;
          }
        }
        else
        {
          log_err ("'y threshold' is not defined as an integer");
          valid = false;
        }
      }
      else
      {
        log_err ("'y threshold' is required when pins array is defined");
        valid = false;
      }

      pUltimate.pins = true;
    }
  }
  else if (checkBoardID(jobj, "current board id"))
  {
    if (checkBoardID(jobj, "new board id"))
    {
      pUltimate.boardIDUpdate = true;
    }
    else
    {
      valid = false;

      if (!json_object_object_get_ex(jobj, "new board id", &tmp))
      {
        log_err ("'new board id' is not defined in the configuration file");
      }
    }
  }
  else
  {
    valid = false;
    log_err ("IPAC Ultimate configuration file is not configured correctly");
  }

  return valid;
}

/*
 * macros are optional.
 * Validation will be the following:
 *  each macro entry is an array (all boards)
 *  each key entry is type string (all boards)
 *  limit of 85 bytes for the complete macro group (2015 boards)
 */
bool
validateIPacUltimarcMacros (json_object* jobj, bool curResult)
{
  bool result = curResult;

  /* 2015 board variables */
  int macroEntries = 0;
  static const int maxMacroEntries = 85;

  int macroCount2015 = 0;
  static const int maxMacroCount2015 = 30;

  /* General variables */
  json_object* macros = NULL;
  json_object* key    = NULL;

  int keyCount = 0;

  if (json_object_object_get_ex(jobj, "macros", &macros))
  {
    if (!json_object_is_type(macros, json_type_object))
    {
      log_err ("'macros' is not of type object");
      result = false;
    }
    else
    {
      json_object_object_foreach(macros, name, macro)
      {
        macroEntries++;
        macroCount2015++;

        if (!json_object_is_type(macro, json_type_array))
        {
          log_err ("'%s' is not of type array", name);
          result = false;
        }
        else
        {
          macroEntries += json_object_array_length(macro);
          for (; keyCount < json_object_array_length(macro); ++keyCount)
          {
            key = json_object_array_get_idx(macro, keyCount);
            if (!json_object_is_type(key, json_type_string))
            {
              log_err ("Key entry in '%s' needs to be of type string", name);
              result = false;
            }
          }
        }
      }

      if (macroCount2015 > maxMacroCount2015)
      {
        log_err("The number of macros defined is '%i'. 30 macro entries are allowed.", macroCount2015);
        result = false;
      }

      if (macroEntries > maxMacroEntries)
      {
        log_err("The total size of the macros plus control characters is '%i'.  Needs to be a total size of 85 or less.", macroEntries);
        result = false;
      }
    }
  }

  return result;
}

bool
quadAssignmentIPACUltimate (unsigned char* data, unsigned char keyval,
                            int idx1, int idx2, int idx3, int idx4)
{
  bool quadAssigned = true;

  if (data[idx1] == 0x00)
  {
    if (keyval == 0xc0)
    {
      data[idx1] = keyval;
      data[idx2] = 0xc1;
      data[idx3] = 0x01;
      data[idx4] = 0x01;
    }
    else if (keyval == 0xc2)
    {
      data[idx1] = keyval;
      data[idx2] = 0xc3;
      data[idx3] = 0x01;
      data[idx4] = 0x01;
    }
    else if (keyval == 0xc4)
    {
      data[idx1] = keyval;
      data[idx2] = 0xc5;
      data[idx3] = 0x01;
      data[idx4] = 0x01;
    }
    else
    {
      quadAssigned = false;
    }
  }
  else if (data[idx1] != 0xc0 &&
           data[idx1] != 0xc2 &&
           data[idx1] != 0xc4)
  {
    quadAssigned = false;
  }

  return quadAssigned;
}

bool populateIPACUltimateData(json_object* jobj, unsigned char* data)
{
  bool retval = false;

  int idx = 0;
  int keypos = 0;
  int shiftpos = 0;
  int swappos = 0;

  unsigned char keyval;
  char shiftval;
  char swapval;

  json_object *tmp = NULL;

  json_object_object_foreach(jobj, key, pin)
  {
    if (strlen(key) == 0)
    {
      log_err ("\nKey value is empty!  Invalid configuration.\n");
      retval = false;
      continue;
    }

    /* Key value */
    tmp = json_object_array_get_idx(pin, 0);
    keyval = convertIPACSeries(tmp);

    /* Shift key value */
    tmp = json_object_array_get_idx(pin, 1);
    shiftval = convertIPACSeries(tmp);

    /* Is a swap key */
    tmp = json_object_array_get_idx(pin, 2);

    swapval = ((!strcasecmp(json_object_get_string(tmp), "true")) > 0 ? 0x40 : 0x01);

    if (!strcasecmp(key, "1right"))
    {
      data[0] = keyval; data[50] = shiftval; data[100] = swapval;
    }
    if (!strcasecmp(key, "1sw4"))
    {
      data[1] = keyval; data[51] = shiftval; data[101] = swapval;
    }
    if (!strcasecmp(key, "1left"))
    {
      data[2] = keyval; data[52] = shiftval; data[102] = swapval;
    }
    if (!strcasecmp(key, "1sw3"))
    {
      data[3] = keyval; data[53] = shiftval; data[103] = swapval;
    }
    if (!strcasecmp(key, "1up"))
    {
      data[4] = keyval; data[54] = shiftval; data[104] = swapval;
    }
    if (!strcasecmp(key, "1sw2"))
    {
      data[5] = keyval; data[55] = shiftval; data[105] = swapval;
    }
    if (!strcasecmp(key, "1down"))
    {
      data[6] = keyval; data[56] = shiftval; data[106] = swapval;
    }
    if (!strcasecmp(key, "1sw1"))
    {
      data[7] = keyval; data[57] = shiftval; data[107] = swapval;
    }
    if (!strcasecmp(key, "4lf"))
    {
      if (!quadAssignmentIPACUltimate(data, keyval, 9, 8, 109, 108))
      {
        data[8] = keyval; data[58] = shiftval; data[108] = swapval;
      }
    }
    if (!strcasecmp(key, "4rt"))
    {
      if (!quadAssignmentIPACUltimate(data, keyval, 9, 8, 109, 108))
      {
        data[9] = keyval; data[59] = shiftval; data[109] = swapval;
      }
    }
    if (!strcasecmp(key, "2sw8"))
    {
      data[10] = keyval; data[60] = shiftval; data[110] = swapval;
    }
    if (!strcasecmp(key, "2sw5"))
    {
      data[11] = keyval; data[61] = shiftval; data[111] = swapval;
    }
    if (!strcasecmp(key, "1a"))
    {
      data[12] = keyval; data[62] = shiftval; data[112] = swapval;
    }
    if (!strcasecmp(key, "1b"))
    {
      data[14] = keyval; data[64] = shiftval; data[112] = swapval;
    }
    if (!strcasecmp(key, "3up"))
    {
      if (!quadAssignmentIPACUltimate(data, keyval, 16, 18, 114, 116))
      {
        data[16] = keyval; data[66] = shiftval; data[114] = swapval;
      }
    }
    if (!strcasecmp(key, "1sw8"))
    {
      data[17] = keyval; data[67] = shiftval; data[115] = swapval;
    }
    if (!strcasecmp(key, "3dn"))
    {
      if (!quadAssignmentIPACUltimate(data, keyval, 16, 18, 114, 116))
      {
        data[18] = keyval; data[68] = shiftval; data[116] = swapval;
      }
    }
    if (!strcasecmp(key, "1sw7"))
    {
      data[19] = keyval; data[69] = shiftval; data[117] = swapval;
    }
    if (!strcasecmp(key, "3lf"))
    {
      if (!quadAssignmentIPACUltimate(data, keyval, 22, 20, 118, 120))
      {
        data[20] = keyval; data[70] = shiftval; data[118] = swapval;
      }
    }
    if (!strcasecmp(key, "1sw6"))
    {
      data[21] = keyval; data[71] = shiftval; data[119] = swapval;
    }
    if (!strcasecmp(key, "3rt"))
    {
      if (!quadAssignmentIPACUltimate(data, keyval, 22, 20, 118, 120))
      {
        data[22] = keyval; data[72] = shiftval; data[120] = swapval;
      }
    }
    if (!strcasecmp(key, "1sw5"))
    {
      data[23] = keyval; data[73] = shiftval; data[121] = swapval;
    }
    if (!strcasecmp(key, "4s2"))
    {
      data[24] = keyval; data[74] = shiftval; data[122] = swapval;
    }
    if (!strcasecmp(key, "2down"))
    {
      data[25] = keyval; data[75] = shiftval; data[123] = swapval;
    }
    if (!strcasecmp(key, "4s1"))
    {
      data[26] = keyval; data[76] = shiftval; data[124] = swapval;
    }
    if (!strcasecmp(key, "2up"))
    {
      data[27] = keyval; data[77] = shiftval; data[125] = swapval;
    }
    if (!strcasecmp(key, "4dn"))
    {
      data[28] = keyval; data[78] = shiftval; data[126] = swapval;
    }
    if (!strcasecmp(key, "2left"))
    {
      data[29] = keyval; data[79] = shiftval; data[127] = swapval;
    }
    if (!strcasecmp(key, "4up"))
    {
      data[30] = keyval; data[80] = shiftval; data[128] = swapval;
    }
    if (!strcasecmp(key, "2right"))
    {
      data[31] = keyval; data[81] = shiftval; data[128] = swapval;
    }
    if (!strcasecmp(key, "3s3"))
    {
      if (!quadAssignmentIPACUltimate(data, keyval, 32, 34, 130, 132))
      {
        data[32] = keyval; data[82] = shiftval; data[130] = swapval;
      }
    }
    if (!strcasecmp(key, "2coin"))
    {
      data[33] = keyval; data[88] = shiftval; data[131] = swapval;
    }
    if (!strcasecmp(key, "3s4"))
    {
      if (!quadAssignmentIPACUltimate(data, keyval, 32, 34, 130, 132))
      {
        data[34] = keyval; data[84] = shiftval; data[132] = swapval;
      }
    }
    if (!strcasecmp(key, "1coin"))
    {
      data[35] = keyval; data[85] = shiftval; data[133] = swapval;
    }
    if (!strcasecmp(key, "3s1"))
    {
      if (!quadAssignmentIPACUltimate(data, keyval, 36, 38, 134, 136))
      {
        data[36] = keyval; data[86] = shiftval; data[134] = swapval;
      }
    }
    if (!strcasecmp(key, "2start"))
    {
      data[37] = keyval; data[87] = shiftval; data[135] = swapval;
    }
    if (!strcasecmp(key, "3s2"))
    {
      if (!quadAssignmentIPACUltimate(data, keyval, 36, 38, 134, 136))
      {
        data[38] = keyval; data[88] = shiftval; data[136] = swapval;
      }
    }
    if (!strcasecmp(key, "1start"))
    {
      data[39] = keyval; data[89] = shiftval; data[137] = swapval;
    }
    if (!strcasecmp(key, "2a"))
    {
      data[40] = keyval; data[90] = shiftval; data[138] = swapval;
    }
    if (!strcasecmp(key, "2sw4"))
    {
      data[41] = keyval; data[91] = shiftval; data[138] = swapval;
    }
    if (!strcasecmp(key, "2b"))
    {
      data[42] = keyval; data[92] = shiftval; data[140] = swapval;
    }
    if (!strcasecmp(key, "2sw3"))
    {
      data[43] = keyval; data[93] = shiftval; data[141] = swapval;
    }
    if (!strcasecmp(key, "4s4"))
    {
      data[44] = keyval; data[94] = shiftval; data[142] = swapval;
    }
    if (!strcasecmp(key, "2sw2"))
    {
      data[45] = keyval; data[95] = shiftval; data[143] = swapval;
    }
    if (!strcasecmp(key, "4s3"))
    {
      data[46] = keyval; data[96] = shiftval; data[144] = swapval;
    }
    if (!strcasecmp(key, "2sw1"))
    {
      data[47] = keyval; data[97] = shiftval; data[145] = swapval;
    }
    if (!strcasecmp(key, "2sw7"))
    {
      data[48] = keyval; data[98] = shiftval; data[146] = swapval;
    }
    if (!strcasecmp(key, "2sw6"))
    {
      data[49] = keyval; data[99] = shiftval; data[147] = swapval;
    }
  }

  return retval;
}

void populateIPacUltimateMacro(json_object* jobj, unsigned char* barray)
{
  int pos = 0;
  int idx = 168;

  json_object *tmp = NULL;

  json_object_object_foreach(jobj, key, macro)
  {
    barray[idx] = convertIPACSeries(json_object_new_string(key));

    idx++;
    for (pos = 0; pos < json_object_array_length(macro); pos++)
    {
      tmp = json_object_array_get_idx(macro, pos);
      barray[idx+pos] = convertIPACSeries(tmp);
    }

    /* Move the idx to the next available location */
    idx += pos;
  }
}

bool updateBoardIPacUltimate(json_object* jobj)
{
  libusb_context *ctx = NULL;
  libusb_device *device = NULL;

  struct libusb_device_handle *handle = NULL;
  struct libusb_device_descriptor descriptor;

  int ipac_idx = 4;
  int pos = 0;
  int ret = 0;

  int idx = 0;
  int intensity = 0;
  int board = 0;
  int interface = IPACULTIMATE_INTERFACE;
  uint16_t product = IPACULTIMATE_PRODUCT;

  unsigned char data[IPACULTIMATE_DATA_SIZE];
  char header[4] = {0x50, 0xdd, 0x0f, 0x00};
  unsigned char map[IPACSERIES_MESG_LENGTH] = {0x03,0,0,0,0};

  bool result = true;

  json_object *leds = NULL;
  json_object *led  = NULL;
  json_object *tmp  = NULL;
  json_object *pins = NULL;
  json_object *pin  = NULL;
  json_object *macros = NULL;

  if (pUltimate.boardIDUpdate == true)
  {
    json_object_object_get_ex(jobj, "current board id", &tmp);
    board = json_object_get_int(tmp);
    product += (board - 1);
  }
  else
  {
    json_object_object_get_ex(jobj, "board id", &tmp);
    board = json_object_get_int(tmp);
    product += (board - 1);
  }

  handle = openUSB(ctx, IPACULTIMATE_VENDOR, product, -1, 1);

  if (!handle)
  {
    result = false;
    goto error;
  }

  debug ("Determine which interface to use...");
  device = libusb_get_device(handle);

  if (!device)
  {
    result = false;
    goto error;
  }

  libusb_get_device_descriptor (device, &descriptor);
  if ((descriptor.bcdDevice & 0x40) != 0)
  {
    log_info ("No Game Controller board");
    interface = IPACULTIMATE_NGC_INTERFACE;
  }
  else
  {
    log_info ("Game Controller board");
    interface = IPACULTIMATE_INTERFACE;
  } 

  debug ("Claiming interface...");
  if (!claimInterface(handle, interface, true))
  {
    result = false;
    goto error;
  }

  /* Intensity settings */
  if (pUltimate.ledMapIntensity == true)
  {
    json_object_object_get_ex(jobj, "intensity", &leds);
    for (idx = 0; idx < json_object_array_length(leds); ++ idx)
    {
      led = json_object_array_get_idx(leds, idx);
      json_object_object_get_ex(led, "led", &tmp);
      map[1] = convertDecimalToHex(json_object_get_int(tmp));

      json_object_object_get_ex(led, "intensity", &tmp);
      map[2] = convertDecimalToHex (json_object_get_int(tmp));

      /* ship this data off to the USB device */
      libusb_control_transfer(handle,
                              UM_REQUEST_TYPE,
                              UM_REQUEST,
                              IPACULTIMATE_VALUE,
                              interface,
                              map,
                              IPACSERIES_MESG_LENGTH,
                              UM_TIMEOUT);
    }
  }
  else if (pUltimate.allIntensities == true)
  {
    map[1] = 0x80; // 128 decimal
    json_object_object_get_ex(jobj, "LED intensity all", &tmp);
    map[2] = convertDecimalToHex (json_object_get_int(tmp));

    /* ship this data off to the USB device */
    libusb_control_transfer(handle,
                            UM_REQUEST_TYPE,
                            UM_REQUEST,
                            IPACULTIMATE_VALUE,
                            interface,
                            map,
                            IPACSERIES_MESG_LENGTH,
                            UM_TIMEOUT);
  }

  /* Random states */
  if (pUltimate.random == true)
  {
    map[1] = 0x89; // 137 decimal
    map[2] = 0;

    /* ship this data off to the USB device */
    libusb_control_transfer(handle,
                            UM_REQUEST_TYPE,
                            UM_REQUEST,
                            IPACULTIMATE_VALUE,
                            interface,
                            map,
                            IPACSERIES_MESG_LENGTH,
                            UM_TIMEOUT);
  }

  /* Fade rate */
  if (pUltimate.fadeRate == true)
  {
    map[1] = 0xc0; // 192 decimal
    json_object_object_get_ex(jobj, "fade rate", &tmp);
    map[2] = convertDecimalToHex (json_object_get_int(tmp));

    /* ship this data off to the USB device */
    libusb_control_transfer(handle,
                            UM_REQUEST_TYPE,
                            UM_REQUEST,
                            IPACULTIMATE_VALUE,
                            interface,
                            map,
                            IPACSERIES_MESG_LENGTH,
                            UM_TIMEOUT);
  }

  if (pUltimate.pins == true)
  {
    /* Setup data to send to board */
     memset (&data, 0, sizeof(data));
     memcpy (&data, &header, sizeof(header));
     data[17] = 0xff;
     data[19] = 0xff;
     data[67] = 0xff;
     data[69] = 0xff;

     json_object_object_get_ex(jobj, "pins", &pins);

     for (idx = 0; idx < json_object_array_length(pins); ++ idx)
     {
       pin = json_object_array_get_idx(pins, idx);
       populateIPACUltimateData(pin, &data[4]);
       ++ipac_idx;
     }

     json_object_object_get_ex(jobj, "macros", &macros);
     if (macros != NULL)
     {
       populateUHidMacro(macros, &data[3]);
     }

     json_object_object_get_ex(jobj, "x threshold", &tmp);
     data[155] = convertDecimalToHex(json_object_get_int(tmp));

     json_object_object_get_ex(jobj, "y threshold", &tmp);
     data[163] = convertDecimalToHex(json_object_get_int(tmp));

     while (pos < IPACULTIMATE_DATA_SIZE)
     {
       memcpy(&map[1], &data[pos], 4);
       pos+=4;

       debug ("Writing out the following data (%i): %x, %x, %x, %x, %x", pos, map[0], map[1], map[2], map[3], map[4]);
       ret = libusb_control_transfer(handle,
                                     UM_REQUEST_TYPE,
                                     UM_REQUEST,
                                     IPACULTIMATE_VALUE,
                                     interface,
                                     map,
                                     IPACSERIES_MESG_LENGTH,
                                     UM_TIMEOUT);
       debug ("Write result: %i", ret);
     }
  }

  if (pUltimate.boardIDUpdate == true)
  {
    /* THIS IS NOT WORKING CORRECTLY */
    memcpy (&map[1], &header, sizeof(header));
    json_object_object_get_ex(jobj, "new board id", &tmp);
    map[1] = convertDecimalToHex((json_object_get_int(tmp) + 80));

    /* ship this data off to the USB device */
    ret = libusb_control_transfer(handle,
                            UM_REQUEST_TYPE,
                            UM_REQUEST,
                            IPACULTIMATE_VALUE,
                            interface,
                            map,
                            IPACSERIES_MESG_LENGTH,
                            UM_TIMEOUT);
  }

exit:
    closeUSB(ctx, handle, interface);
    return result;

error:
    return result;
}
