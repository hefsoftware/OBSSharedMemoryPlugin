/*
 * This file is part of SharedImageIPC.
 *
 * (c) Marzocchi Alessandro
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

/**
 * @file
 * @brief Image exchange via shared memory
 *
 * Allows a process (the generator) to feed images to another (the consumer).
 *
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** @struct SharedMemory
 *  @brief Opaque pointer representing a shared image object
 */
struct SharedImage;

/// @brief Settings for a shared image
typedef struct
{
  uint32_t width;
  uint32_t height;
  uint32_t bytesPerLine;
} SharedImageSetting;

/**
 * @brief Creates a shared image object
 *
 * Note that even on error an object may be returned and it can be only checked for error
 * @param utf8Name Name of the shared object
 * @param image Pointer that will receive a pointer to the created object
 * @param numPixels Number of pixels that should be allocated for each image
 * @param generator True if we are creating the generator object, false if the consumer is being created
 * @return True on success
 */
bool sharedImageCreate(const char *utf8Name, struct SharedImage **image, uint32_t numPixels, bool generator);

/**
 * @brief Destroys a shared image object
 * @param image The object to destroy
 * @return True on success
 */
bool sharedImageDestroy(struct SharedImage *image);

/**
 * @brief Returns current error message
 *
 * Note the error message is not shared and specific of each process.
 * @param shared Shared memory
 * @return Pointer to an error message
 */
const char *sharedImageGetError(struct SharedImage *image);


/**
* @brief Receives image data on consumer side
*
* If return value is true imageData and settings will be filled with valid values. The returned values will be valid until the next sucessfull call to sharedImageReceive.
* @param image Pointer to returned shared image object
* @param imageData Pointer to returned image buffer
* @param settings Will be filled with a pointer to settings of buffer returned in imageData
* @return True on success
*/
bool sharedImageReceive(struct SharedImage *image, void **imageData, const SharedImageSetting **settings);

/**
 * @brief Called by producer to returns an output buffer for the generated image
 * @param image Shared image object
 * @param imageData Pointer to the returned pointer to output buffer that should be used
 * @param availablePixels Pointer that will be filled with number of pixels available in
 * @return True if a send buffer is available to generator
 */
bool sharedImageOutBuffer(struct SharedImage *image, void **imageData, uint32_t *availablePixels);

/**
 * @brief Sends an image from producer to the consumer
 *
 * Use this function after sucessufully getting a output buffer with sharedImageOutBuffer
 * @param image Shared image object
 * @param setting Settings for the image that was put in the buffer
 * @return True on success
 */
bool sharedImageSend(struct SharedImage *image, const SharedImageSetting *setting);

/**
 * @brief Waits for a notification
 * @param shared Shared memory
 * @param timeoutMs Timeout to wait, in milliseconds
 * @return True if a notification happened
 */
bool sharedImageWaitNotify(struct SharedImage *image, uint32_t timeoutMs);

/** @fn void sharedImageNotificationHandle(struct SharedImage *image)
 * @brief Returns an architecture-dependent notification handle
 */
#if defined(SHAREDMEM_WIN32)
void *sharedImageNotificationHandle(struct SharedImage *image);
#endif

#ifdef __cplusplus
}
#endif
