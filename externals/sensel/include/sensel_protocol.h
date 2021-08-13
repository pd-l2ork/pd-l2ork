/******************************************************************************************
* MIT License
*
* Copyright (c) 2013-2017 Sensel, Inc.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
******************************************************************************************/

#ifndef __SENSEL_PROTOCOL_H__
#define __SENSEL_PROTOCOL_H__

#include "sensel_types.h"

#ifdef __MINGW32__
  #define PACK( __Declaration__, __Name__ ) __Declaration__ __attribute__((__packed__)) __Name__
#elif defined WIN32
  #define PACK( __Declaration__, __Name__ ) __pragma( pack(push, 1) )     \
  __Declaration__ __Name__ __pragma( pack(pop) )
#else
  #define PACK( __Declaration__, __Name__ ) __Declaration__ __attribute__((__packed__)) __Name__
#endif

#ifdef __cplusplus
extern "C" {
#endif

  ////////////////////////////////////////////////////////////////////////////////
  // Type declarations
  typedef int16  vel_t;
  typedef uint8  label_t;
  typedef uint8  blobid_t;
  typedef uint8  grid_coord_t;
  typedef uint8  contact_type_t;
  typedef uint16 pressure_t;
  typedef uint16 force_t;
  typedef uint32 uid_t;

PACK(
    typedef struct
    {
      uint8 r_w_addr;
      uint8 reg;
      uint8 size;
      uint8 padding;
    },
    sensel_protocol_cmd_t);

PACK(
    typedef struct
    {
      uint8 r_w_addr;
      uint8 reg;
      uint8 size;
      uint8 header_size;
      uint32 vs_size;
      uint8 checksum;
      uint8 padding;
    },
    sensel_protocol_cmd_vs_t);

PACK(
    typedef struct
    {
      uint8  fw_protocol_version;
      uint8  fw_version_major;
      uint8  fw_version_minor;
      uint16 fw_version_build;
      uint8  fw_version_release;
      uint16 device_id;
      uint8  device_revision;
    },
    sensel_firmware_info_t);

PACK(
    typedef struct
    {
      int16 x;
      int16 y;
      int16 z;
    },
    sensel_accel_data_t);

  // This type is for storing contact information
  // NOTE: I use unsigned types for some of these fields. I may want to consider signed types for faster processing.
PACK(
  typedef struct
  {
    label_t        id;          // TODO: type could be something like contact_id
    contact_type_t type;
    uint16         x_pos;       // x position multiplied by 256
    uint16         y_pos;       // y position multiplied by 256
    uint16         total_force;
    uint16         area;        // area in sensor elements

    int16          orientation; // angle from -90 to 90 multiplied by 16
    uint16         major_axis;  // length of the major axis multiplied by 256
    uint16         minor_axis;  // length of the minor axis multiplied by 256

    int16          delta_x;
    int16          delta_y;
    int16          delta_force;
    int16          delta_area;

    uint16         min_x;
    uint16         min_y;
    uint16         max_x;
    uint16         max_y;

    uint16         peak_x;
    uint16         peak_y;
    uint16         peak_force;
  },
  contact_raw_t);

  // This type is for storing x,y locations on the grid
PACK(
  typedef struct
  {
    grid_coord_t x;
    grid_coord_t y;
  },
  grid_loc_t);


#ifdef __cplusplus
}
#endif

#endif //__SENSEL_PROTOCOL_H__
