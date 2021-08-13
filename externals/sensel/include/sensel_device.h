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

#ifndef __SENSEL_ADVANCED_H__
#define __SENSEL_ADVANCED_H__

#include "sensel.h"
#include "sensel_protocol.h"

#define DEFAULT_BOARD_ADDR             0x01

#define SENSEL_MAGIC                   "S3NS31"
#define SENSEL_MAGIC_LEN               6
#define SENSEL_NULL_LABEL              255

#define CONTACT_DEFAULT_SEND_SIZE      10
#define CONTACT_ELLIPSE_SEND_SIZE      6
#define CONTACT_DELTAS_SEND_SIZE       8
#define CONTACT_BOUNDING_BOX_SEND_SIZE 8
#define CONTACT_PEAK_SEND_SIZE         6

typedef void *SENSEL_DECOMP_HANDLE;

#ifdef __cplusplus
extern "C" {
#endif

	typedef struct
	{
		#if WIN32
      void*  serial_handle;
		#else
      int    serial_fd;
		#endif
	} SenselSerialHandle;

  typedef struct sensel_device_s
  {
    SenselSerialHandle          sensor_serial;            // Handle to the serial interface
    SenselFirmwareInfo          fw_info;									// Device firmware information
    SenselSensorInfo            sensor_info;							// Sensor information
    SENSEL_DECOMP_HANDLE        decomp_handle;            // Decompression handle

    unsigned char               supported_frame_content;  // Content the device supports
    // Temporary buffers for force frame decompression
    unsigned char               *frame_buffer;
    int                         frame_buffer_capacity;
    int                         frame_buffer_size;

    // Conversion factors
    float                       dims_value_scale;         // Dimension value scale
    float                       force_value_scale;        // Force value scale
    float                       angle_value_scale;        // Angle value scale
    float                       area_value_scale;         // Area value scale

    // Variables to keep track of sensel scan state
    unsigned char               frame_content_control;		// Curent frame content control
    unsigned char               scan_buffer_control;      // Number of scan buffers currently enabled
    SenselScanMode              scan_mode;                // Current scan mode setting
    unsigned char               scanning_active;          // Is scanning enabled / disabled
    int                         num_buffered_frames;      // Number of frames currently buffered
    unsigned char               prev_rolling_frame_counter;
    unsigned int                prev_timestamp;           // Timestamp of the previous frame

    unsigned char               dynamic_baseline_enabled; // Is dynamic baselining enabled

    // Used for LED control
    unsigned char               num_leds;                 // Maximum number of LEDs
    unsigned short              max_led_brightness;       // Maximum brightness value
    unsigned char               led_reg_size;             // Size of the LED brightness register
		void                        *led_array;								// LED brightness array
  } SenselDevice;

#ifdef __cplusplus
}
#endif

#endif //__SENSEL_ADVANCED_H__
