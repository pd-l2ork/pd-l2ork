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

#ifndef __SENSEL_H__
#define __SENSEL_H__

#ifdef WIN32
  #include <windows.h>
#else
  #define WINAPI
#endif

#ifdef SENSEL_EXPORTS
  #define SENSEL_API __declspec(dllexport)
#else
  #define SENSEL_API __declspec(dllimport)
#endif

#ifndef WIN32
  #ifdef SENSEL_API
    #undef SENSEL_API
    #define SENSEL_API __attribute__ ((visibility ("default")))
  #endif
#endif

#define SENSEL_MAX_DEVICES             16    // Maximum number of devices supported by the API

#define FRAME_CONTENT_PRESSURE_MASK    0x01  // Mask indicating that the frame includes pressure data
#define FRAME_CONTENT_LABELS_MASK      0x02  // Mask indicating that the frame includes labels data
#define FRAME_CONTENT_CONTACTS_MASK    0x04  // Mask indicating that the frame includes contacts data
#define FRAME_CONTENT_ACCEL_MASK       0x08  // Mask indicating that the frame includes acceleromter data

#define CONTACT_MASK_ELLIPSE           0x01  // Mask indicating that the contact data contains ellipse info
#define CONTACT_MASK_DELTAS            0x02  // Mask indicating that the contact data contains deltas info
#define CONTACT_MASK_BOUNDING_BOX      0x04  // Mask indicating that the contact data contains bound box info
#define CONTACT_MASK_PEAK              0x08  // Mask indicating that the contact data contains peak info

#ifdef __cplusplus
extern "C" {
#endif

  /*!
   * @discussion Handle to a Sensel device
   */
  typedef void *SENSEL_HANDLE;

  /*!
   * @discussion Status returned by API calls
   */
  typedef enum
  {
    SENSEL_OK        =  0,              // Call was successful
    SENSEL_ERROR     = -1,              // Call returned an error
  } SenselStatus;

	/*!
	 * @discussion Device scan reporting mode
	 */
  typedef enum
  {
    SCAN_MODE_DISABLE,
    SCAN_MODE_SYNC,
    SCAN_MODE_ASYNC,
  } SenselScanMode;

  /*!
   * @discussion Describes the current state of a contact
   */
  typedef enum
  {
    SCAN_DETAIL_HIGH     = 0,           // Scan at full resolution
    SCAN_DETAIL_MEDIUM   = 1,           // Scan at half resolution
    SCAN_DETAIL_LOW      = 2,           // Scan at quarter resolution
    SCAN_DETAIL_UNKNOWN  = 3,
  } SenselScanDetail;

  /*!
   * @discussion Describes the current state of a contact
   */
  typedef enum
  {
    CONTACT_INVALID = 0,                // Contact is invalid
    CONTACT_START   = 1,                // Contact has started
    CONTACT_MOVE    = 2,                // Contact has moved
    CONTACT_END     = 3,                // Contact has ended
  } SenselContactState;

  /*!
   * @discussion Structure containing sensor information
   */
  typedef struct
  {
    unsigned char   max_contacts;       // Maximum number of contacts the sensor supports
    unsigned short  num_rows;           // Total number of rows
    unsigned short  num_cols;           // Total number of columns
    float           width;              // Width of the sensor in millimeters
    float           height;             // Height of the sensor in millimeters
  } SenselSensorInfo;

  /*!
   * @discussion Structure containing firmware information
   */
  typedef struct
  {
    unsigned char  fw_protocol_version; // Sensel communication protocol supported by the device
    unsigned char  fw_version_major;    // Major version of the firmware
    unsigned char  fw_version_minor;    // Minor version of the firmware
    unsigned short fw_version_build;    // ??
    unsigned char  fw_version_release;  // ??
    unsigned short device_id;           // Sensel device type
    unsigned char  device_revision;     // Device revision
  } SenselFirmwareInfo;


  /*!
   * @discussion Structure containing all information related to a detected contact
   *              The content_bit_mask reflects which data is valid
   */
	typedef struct
	{
    unsigned char        content_bit_mask;   // Mask of what contact data is valid
    unsigned char        id;                 // Contact id
    unsigned int         state;              // Contact state (enum SenselContactState)
    float                x_pos;              // X position in mm
    float                y_pos;              // Y position in mm
    float                total_force;        // Total contact force in grams
    float                area;               // Area in sensor elements

    // CONTACT_MASK_ELLIPSE
    float                orientation;        // Angle in degrees
    float                major_axis;         // Length of the major axis in mm
    float                minor_axis;         // Length of the minor axis in mm

    // CONTACT_MASK_DELTAS
    float                delta_x;            // X contact displacement in mm
    float                delta_y;            // Y contact displacement in mm
    float                delta_force;        // Force delta in grams
    float                delta_area;         // Area delta in sensor elements

    // CONTACT_MASK_BOUNDING_BOX
    float                min_x;              // Bounding box min X coordinate in mm
    float                min_y;              // Bounding box min Y coordinate in mm
    float                max_x;              // Bounding box max X coordinate in mm
    float                max_y;              // Bounding box max Y coordinate in mm

    // CONTACT_MASK_PEAK
    float                peak_x;             // X position of the peak in mm
    float                peak_y;             // Y position of the peak in mm
    float                peak_force;         // Peak force in grams
  } SenselContact;

  /*!
   * @discussion Accelerometer information
   */
  typedef struct
  {
    int x;                             // X axis acceleration
    int y;                             // Y axis acceleration
    int z;                             // Z axis acceleration
  } SenselAccelData;

  /*!
   * @discussion Container for one frame of data.
   *              Available content of the frame needs to be checked by looking at the content_bit_mask
   */
  typedef struct
  {
    unsigned char   content_bit_mask;  // Data contents of the frame
    int             lost_frame_count;  // Number of frames dropped
    unsigned char   n_contacts;        // Number of contacts
    SenselContact   *contacts;         // Array of contacts
    float           *force_array;      // Force image buffer
    unsigned char   *labels_array;     // Labels buffer
    SenselAccelData *accel_data;       // Accelerometer data
  } SenselFrameData;

  /*!
   * @discussion Sensel identifier information
   */
  typedef struct
  {
    unsigned char idx;                 // ID of the sensor
    unsigned char serial_num[64];      // Serial number of the sensor
    unsigned char com_port[64];        // Com port associated with the sensor
  } SenselDeviceID;

  /*!
   * @discussion List of connected Sensel devices
   */
  typedef struct
  {
    unsigned char  num_devices;                 // Num of devices found
    SenselDeviceID devices[SENSEL_MAX_DEVICES]; // Sensel device ID details
  } SenselDeviceList;

  /*!
   * @param      handle Sensel device handle to be allocated
   * @return     SENSEL_OK on success or error
   * @discussion Opens the first available Sensel sensor
   */
  SENSEL_API
  SenselStatus WINAPI senselOpen(SENSEL_HANDLE *handle);

  /*!
   * @param      list Device list to be populated
   * @return     SENSEL_OK on success or error
   * @discussion Scans for all availale sensel devices and populates list accordingly
   */
  SENSEL_API
  SenselStatus WINAPI senselGetDeviceList(SenselDeviceList *list);

  /*!
   * @param      handle Sensel device handle to be initialized
   * @param      idx    identifier of the device to open
   * @return     SENSEL_OK on success or error
   * @discussion Opens the devices associated to the given idx as returned by senselGetDeviceList.
   *              senselGetDeviceList must be called prior to this call
   */
  SENSEL_API
  SenselStatus WINAPI senselOpenDeviceByID(SENSEL_HANDLE *handle, unsigned char idx);

  /*!
   * @param      handle     Sensel device handle to be initialized
   * @param      serial_num serial_number of the device to open
   * @return     SENSEL_OK on success or error
   * @discussion Opens the devices associated to the given serial_num as returned by senselGetDeviceList.
   *              senselGetDeviceList must be called prior to this call
   */
  SENSEL_API
  SenselStatus WINAPI senselOpenDeviceBySerialNum(SENSEL_HANDLE *handle, unsigned char *serial_num);

  /*!
   * @param      handle   Sensel device handle to be initialized
   * @param      com_port com_port path of the device to open
   * @return     SENSEL_OK on success or error
   * @discussion Opens the devices associated to the given com_port as returned by senselGetDeviceList.
   *              senselGetDeviceList must be called prior to this call
   */
  SENSEL_API
  SenselStatus WINAPI senselOpenDeviceByComPort(SENSEL_HANDLE *handle, unsigned char *com_port);

  /*!
   * @param      handle Sensel device to be closed
   * @return     SENSEL_OK on success or error
   * @discussion Closes the device associated with handle and frees all memory.
   */
  SENSEL_API
  SenselStatus WINAPI senselClose(SENSEL_HANDLE handle);

  /*!
   * @param      handle Sensel device to reset
   * @return     SENSEL_OK on success or error
   * @discussion Executes a soft reset the device referenced by handle.
   *              All registers are reset to their initial state
   */
  SENSEL_API
  SenselStatus WINAPI senselSoftReset(SENSEL_HANDLE handle);

  /*!
   * @param      handle Sensel device handle to information about
   * @param      info   Pointer to a structure to populate
   * @return     SENSEL_OK on success or error
   * @discussion Retrieves the sensor information
   */
  SENSEL_API
  SenselStatus WINAPI senselGetSensorInfo(SENSEL_HANDLE handle, SenselSensorInfo *info);

  /*!
   * @param      handle Sensel device handle for which to create a FrameData structure for
   * @param      data   Pointer to FrameData to allocate.
   * @return     SENSEL_OK on success or error
   * @discussion Allocates a FrameData and initializes all buffers according to device capabilities.
   */
  SENSEL_API
  SenselStatus WINAPI senselAllocateFrameData(SENSEL_HANDLE handle, SenselFrameData **data);

  /*!
   * @param      handle Sensel device handle
   * @param      data   FrameData to free.
   * @return     SENSEL_OK on success or error
   * @discussion Frees all memory allocated to FrameData including the FrameData itself.
   */
  SENSEL_API
  SenselStatus WINAPI senselFreeFrameData(SENSEL_HANDLE handle, SenselFrameData *data);

  /*!
   * @param      handle Sensel device handle
   * @param      detail Scan detail level
   * @return     SENSEL_OK on success or error
   * @discussion Set the level of scanning detail returned by the device. This setting trades precision for performance.
   */
  SENSEL_API
  SenselStatus WINAPI senselSetScanDetail(SENSEL_HANDLE handle, SenselScanDetail detail);

  /*!
   * @param      handle Sensel device handle
   * @param      detail Pointer to scan detail level to retrieve
   * @return     SENSEL_OK on success or error
   * @discussion Get the current scanning level setting
   */
  SENSEL_API
  SenselStatus WINAPI senselGetScanDetail(SENSEL_HANDLE handle, SenselScanDetail *detail);

  /*!
   * @param      handle   Sensel device handle
   * @param      content  Pointer to frame content supported by device
   * @return     SENSEL_OK on success or error
   * @discussion Retrieve the frame content that the device supports
   */
  SENSEL_API
  SenselStatus WINAPI senselGetSupportedFrameContent(SENSEL_HANDLE handle, unsigned char *content);

  /*!
   * @param      handle   Sensel device handle
   * @param      content  Frame content mask
   * @return     SENSEL_OK on success or error
   * @discussion Sets the information returned by the sensor.
   *              content can be any combination of FRAME_CONTENT_*_MASK.
   *              FrameData returned in subsequent GetFrame calls will reflect this setting.
   */
  SENSEL_API
  SenselStatus WINAPI senselSetFrameContent(SENSEL_HANDLE handle, unsigned char content);

  /*!
   * @param      handle   Sensel device handle
   * @param      content  Pointer to content level to retrieve
   * @return     SENSEL_OK on success or error
   * @discussion Get the current frame content mask from the device
   */
  SENSEL_API
  SenselStatus WINAPI senselGetFrameContent(SENSEL_HANDLE handle, unsigned char *content);

  /*!
   * @param      handle Sensel device handle
   * @return     SENSEL_OK on success or error
   * @discussion Start sensor scanning
   */
  SENSEL_API
  SenselStatus WINAPI senselStartScanning(SENSEL_HANDLE handle);

  /*!
   * @param      handle Sensel device handle
   * @return     SENSEL_OK on success or error
   * @discussion Stop sensor scanning
   */
  SENSEL_API
  SenselStatus WINAPI senselStopScanning(SENSEL_HANDLE handle);

  /*!
   * @param      handle Sensel device handle
   * @return     SENSEL_OK on success or error
   * @discussion Reads out and stores all pending frames from the sensor
   */
  SENSEL_API
  SenselStatus WINAPI senselReadSensor(SENSEL_HANDLE handle);

  /*!
   * @param      handle           Sensel device handle
   * @param      num_avail_frames Will contain the number of frames available to GetFrame
   * @return     SENSEL_OK on success or error
   * @discussion Returns in num_avail_frames the number of data frames available.
   */
  SENSEL_API
  SenselStatus WINAPI senselGetNumAvailableFrames(SENSEL_HANDLE handle, unsigned int *num_avail_frames);

  /*!
   * @param      handle Sensel device handle
   * @param      data   Pointer to pre-allocated FrameData to populate
   * @return     SENSEL_OK on success or error
   * @discussion Returns one frame of data in data.
   */
  SENSEL_API
  SenselStatus WINAPI senselGetFrame(SENSEL_HANDLE handle, SenselFrameData *data);

  /*!
   * @param      handle   Sensel device handle
   * @param      num_leds Pointer to number of leds on device
   * @return     SENSEL_OK on success or error
   * @discussion Retrieve number of LEDs available on the device
   */
  SENSEL_API
  SenselStatus WINAPI senselGetNumAvailableLEDs(SENSEL_HANDLE handle, unsigned char *num_leds);

  /*!
   * @param      handle         Sensel device handle
   * @param      max_brightness Pointer to maximum per LED brightness
   * @return     SENSEL_OK on success or error
   * @discussion Retrieve the maximum brightness value an LED can be set to
   */
  SENSEL_API
  SenselStatus WINAPI senselGetMaxLEDBrightness(SENSEL_HANDLE handle, unsigned short *max_brightness);

  /*!
   * @param      handle         Sensel device handle
   * @param      led_id         Index of the LED to update
   * @param      brightness     Brightness setting
   * @return     SENSEL_OK on success or error
   * @discussion Update the brightness of one LED
   */
  SENSEL_API
  SenselStatus WINAPI senselSetLEDBrightness(SENSEL_HANDLE handle, unsigned char led_id, unsigned short brightness);

  /*!
   * @param      handle         Sensel device handle
   * @param      led_id         Index of the LED to update
   * @param      brightness     Pointer to brightness setting
   * @return     SENSEL_OK on success or error
   * @discussion Retrieve the current brightness of an LED
   */
  SENSEL_API
  SenselStatus WINAPI senselGetLEDBrightness(SENSEL_HANDLE handle, unsigned char led_id, unsigned short *brightness);

  /*!
   * @param      handle   Sensel device handle
   * @param      pressed  Pointer to hold state of power button
   * @return     SENSEL_OK on success or error
   * @discussion Pressed will be 1 if button was pressed or 0 otherwize
   */
  SENSEL_API
  SenselStatus WINAPI senselGetPowerButtonPressed(SENSEL_HANDLE handle, unsigned char *pressed);

	/*
	 * Advanced API
   * The following calls are advanced and can break functionality if not used properly.
	 */

  /*!
   * @param      handle  Sensel device handle
   * @param      fw_info Pointer to SenselFirmwareInfo structure to populate
   * @return     SENSEL_OK on success or error
   * @discussion Retrieve firmware device information
   */
  SENSEL_API
  SenselStatus WINAPI senselGetFirmwareInfo(SENSEL_HANDLE handle, SenselFirmwareInfo* fw_info);

  /*!
   * @param      handle Sensel device handle
   * @param      val    0 to disable - 1 to enable
   * @return     SENSEL_OK on success or error
   * @discussion Set contact blob merging setting
   */
  SENSEL_API
  SenselStatus WINAPI senselSetContactsEnableBlobMerge(SENSEL_HANDLE handle, unsigned char val);

  /*!
   * @param      handle Sensel device handle
   * @param      val    Pointer to contain current setting
   * @return     SENSEL_OK on success or error
   * @discussion Get contact blob merging setting
   */
  SENSEL_API
  SenselStatus WINAPI senselGetContactsEnableBlobMerge(SENSEL_HANDLE handle, unsigned char *val);

  /*!
   * @param      handle Sensel device handle
   * @param      val    Force value
   * @return     SENSEL_OK on success or error
   * @discussion Sets the minimum force a contact needs to have to be reported
   */
  SENSEL_API
  SenselStatus WINAPI senselSetContactsMinForce(SENSEL_HANDLE handle, unsigned short val);

  /*!
   * @param      handle Sensel device handle
   * @param      val    Pointer to contain current setting
   * @return     SENSEL_OK on success or error
   * @discussion Gets the minimum force a contact needs to have to be reported
   */
  SENSEL_API
  SenselStatus WINAPI senselGetContactsMinForce(SENSEL_HANDLE handle, unsigned short *val);

  /*!
   * @param      handle Sensel device handle
   * @param      val    true: Enabled - false: Disabled
   * @return     SENSEL_OK on success or error
   * @discussion Sets if the baseline is dynamic and evolves over time
   */
  SENSEL_API
  SenselStatus WINAPI senselSetDynamicBaselineEnabled(SENSEL_HANDLE handle, unsigned char val);

  /*!
   * @param      handle Sensel device handle
   * @param      val    Pointer to contain current setting
   * @return     SENSEL_OK on success or error
   * @discussion Retreives the current dynamic baselining setting
   */
  SENSEL_API
  SenselStatus WINAPI senselGetDynamicBaselineEnabled(SENSEL_HANDLE handle, unsigned char *val);

  /*!
   * @param      handle Sensel device handle
   * @param      num    Number of buffers
   * @return     SENSEL_OK on success or error
   * @discussion Sets the number of frame buffers the device should store internaly.
   */
  SENSEL_API
  SenselStatus WINAPI senselSetBufferControl(SENSEL_HANDLE handle, unsigned char num);

  /*!
   * @param      handle Sensel device handle
   * @param      num    Pointer to contain current setting
   * @return     SENSEL_OK on success or error
   * @discussion Gets the number of frame buffers the device should store internaly.
   */
  SENSEL_API
  SenselStatus WINAPI senselGetBufferControl(SENSEL_HANDLE handle, unsigned char *num);

  /*!
   * @param      handle Sensel device handle
   * @param      mode   Scan mode setting
   * @return     SENSEL_OK on success or error
   * @discussion Sets the current scan mode.
   */
  SENSEL_API
  SenselStatus WINAPI senselSetScanMode(SENSEL_HANDLE handle, SenselScanMode mode);

  /*!
   * @param      handle Sensel device handle
   * @param      mode   Pointer to retrieve Scan mode setting
   * @return     SENSEL_OK on success or error
   * @discussion Gets the current scan mode.
   */
  SENSEL_API
  SenselStatus WINAPI senselGetScanMode(SENSEL_HANDLE handle, SenselScanMode *mode);

  /*!
   * @param      handle Sensel device handle
   * @param      val    Max framerate
   * @return     SENSEL_OK on success or error
   * @discussion Sets the maximum framerate at which the device should report
   */
  SENSEL_API
  SenselStatus WINAPI senselSetMaxFrameRate(SENSEL_HANDLE handle, unsigned short val);

  /*!
   * @param      handle Sensel device handle
   * @param      val    Pointer to retrieve current max framerate
   * @return     SENSEL_OK on success or error
   * @discussion Gets the maximum framerate at which the device should report
   */
  SENSEL_API
  SenselStatus WINAPI senselGetMaxFrameRate(SENSEL_HANDLE handle, unsigned short *val);

  /*!
   * @param      handle Sensel device handle
   * @param      mask   Contact information mask
   * @return     SENSEL_OK on success or error
   * @discussion Sets the contact information reported by the sensor
   *              mask can be any combination of CONTACT_MASK_*
   *              Contacts returned in subsequent GetFrame calls will reflect this setting.
   */
  SENSEL_API
  SenselStatus WINAPI senselSetContactsMask(SENSEL_HANDLE handle, unsigned char mask);

  /*!
   * @param      handle Sensel device handle
   * @param      mask   Pointer to retrieve current max framerate
   * @return     SENSEL_OK on success or error
   * @discussion Gets the current contact mask setting for the device
   */
  SENSEL_API
  SenselStatus WINAPI senselGetContactsMask(SENSEL_HANDLE handle, unsigned char *mask);

  /*!
   * @param      handle Sensel device handle
   * @param      reg    Register to read
   * @param      size   Size of the register
   * @param      buf    Buffer to store the result
   * @return     SENSEL_OK on success or error
   * @discussion Reads size bytes from register reg and stores the value in buf.
   */
  SENSEL_API
  SenselStatus WINAPI senselReadReg(SENSEL_HANDLE handle, unsigned char reg, unsigned char size, unsigned char *buf);

  /*!
   * @param      handle Sensel device handle
   * @param      reg    Register to write
   * @param      size   Size of the register
   * @param      buf    Buffer containing the data to write
   * @return     SENSEL_OK on success or error
   * @discussion Writes size bytes from buf into register reg
   */
  SENSEL_API
  SenselStatus WINAPI senselWriteReg(SENSEL_HANDLE handle, unsigned char reg, unsigned char size, unsigned char *buf);

  /*!
   * @param      handle    Sensel device handle
   * @param      reg       Register to write
   * @param      buf_size  Size of the "buf" buffer
   * @param      buf       Buffer to store the result
   * @param      read_size Variable to store the number of bytes read from register
   * @return     SENSEL_OK on success or error
   * @discussion Reads up to buf_size bytes from register reg and stores it in buf. On success, read_size will
   *              contain the number of bytes read from the register.
   */
  SENSEL_API
  SenselStatus WINAPI senselReadRegVS(SENSEL_HANDLE handle, unsigned char reg, unsigned int buf_size, unsigned char *buf, unsigned int *read_size);

  /*!
   * @param      handle     Sensel device handle
   * @param      reg        Register to write
   * @param      size       Size of data to write from buf
   * @param      buf        Buffer holding data to write
   * @param      write_size Variable to store the number of bytes actually written
   * @return     SENSEL_OK on success or error
   * @discussion Write up to size bytes from buf to register reg. On success, write_size will
   *              contain the number of bytes written to the register.
   */
  SENSEL_API
  SenselStatus WINAPI senselWriteRegVS(SENSEL_HANDLE handle, unsigned char reg, unsigned int size, unsigned char *buf, unsigned int *write_size);

#ifdef __cplusplus
}
#endif

#endif //__SENSEL_H__
