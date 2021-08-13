/******************************************************************************************
* SENSEL CONFIDENTIAL
*
* Copyright (c) 2013-2017 Sensel, Inc.
* All Rights Reserved.
*
* NOTICE:  All information contained herein is, and remains the property of Sensel, Inc.
* The intellectual and technical concepts contained herein are proprietary to Sensel, Inc.
* and may be covered by U.S. and Foreign Patents, patents in process, and are protected
* by trade secret or copyright law. Dissemination of this information or reproduction of
* this material is strictly forbidden unless prior written permission is obtained from
* Sensel, Inc.
******************************************************************************************/


#ifndef __SENSEL_DECOMPRESS_H___
#define __SENSEL_DECOMPRESS_H__

#include "sensel.h"

#ifdef __cplusplus
extern "C" {
#endif

  /*!
   * @param       handle Sensel device handle where to allocate decompression handle
   * @return      SENSEL_OK on success or error
   * @discussion  Allocates a decompression handle inside the device handle
   */
  SENSEL_API
  SenselStatus WINAPI senselInitDecompressionHandle(SENSEL_HANDLE handle, unsigned char *data);

  /*!
   * @param       handle Sensel device handle from which to free the decompression handle
   * @return      SENSEL_OK on success or error
   * @discussion  Frees the decompression handle from the provided device handle
   */
  SENSEL_API
  SenselStatus WINAPI senselFreeDecompressionHandle(SENSEL_HANDLE handle);

  /*!
   * @param       handle Sensel device handle
   * @param       data Metadata in raw form
   * @return      SENSEL_OK on success or error
   * @discussion  Notifies the decompression engine that a scan detail change was requested
   */
  SENSEL_API
  SenselStatus WINAPI senselDecompressionTriggerDetailChange(SENSEL_HANDLE handle, unsigned char *data);

  /*!
   * @param       handle Sensel device handle
   * @param       frame_data Raw protocol payload to process
   * @param       data_size Size of the payload
   * @param       content_mask Content mask as reported for this frame
   * @param       data Frame data to store the decompression result
   * @param       decompress_bytes_read Will hold the number of bytes read from the payload
   * @return      SENSEL_OK on success or error
   * @discussion  Decompresses the payload pointed to by frame_data and fills the FrameData structure
   *              accordingly.
   */
  SENSEL_API
  SenselStatus WINAPI senselDecompressFrame(SENSEL_HANDLE handle, unsigned char* frame_data, int data_size,
                                     unsigned char content_mask, SenselFrameData *data, unsigned int *decompress_bytes_read);

#ifdef __cplusplus
}
#endif


#endif //__SENSEL_DECOMPRESS_H__
