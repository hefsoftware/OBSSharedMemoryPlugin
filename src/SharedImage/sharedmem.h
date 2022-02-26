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
 * @brief Shared memory data exchange
 *
 * Creates a shared memory object that allows two processes (one "server" and one "client") to exchange "pages" of data.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define SHAREDMEM_VERSION 0x100

/**
 * @brief Shared memory layout
 *
 * The struct stores the size and alignment of the different regions of shared memory.
 * Alignments must be powers of two or zero for default alignment (16 bytes)
*/
typedef struct
{
  /// @brief Alignment of the header
  uint32_t headerAlign;
  /// @brief Size of the header
  uint32_t headerSize;
  /// @brief Alignment of pages header
  uint32_t pageHeaderAlign;
  /// @brief Size of pages header
  uint32_t pageHeaderSize;
  /// @brief Alignment of pages data
  uint32_t pageAlign;
  /// @brief Size of pages data
  uint32_t pageSize;
  /// @brief Number of pages
  uint32_t numPages;
} SharedMemInfo;

/** @struct SharedMemory
 *  @brief Opaque pointer representing a shared memory instance
 */
struct SharedMemory;

/**
 * @brief Creates a shared memory object
 *
 * Returned layout may not be the one passed with info parameter (in particular if the shared memory already existed). Call sharedInfo in this case to check it.
 * If the creation succed true will be returned and the created memory object will be returned in the pointer pointed by "shared" parameter.
 * The only functions that should be called on a freshly created object are sharedNeedInitialization() and sharedIsInitialized()
 * In case the memory needs to be initialized (sharedMemMustInitialize returns true) then the caller should initialize the shared memory
 * using a combination of sharedMemHeader, sharedMemPageHeader, sharedMemPageData sharedMemInitPage, sharedMemInitPageClient and sharedMemInitPageServer.
 * When initialization is completed sharedMemEndInitialization should be called. After that all "standard" function can be used.
 * If the memory has to be initialized from an external process (both sharedIsInitialized and sharedMemMustInitialize returns false)
 * the end of the object initialization should be waited. It is possible to used sharedWaitNotify or use on the notification handle for having a hint on
 * when this happens. Note that even after a notification sharedIsInitialized should be checked to return true before proceding with the standard calls.
 *
 * If false is returned an object may be returned. In this case it's possible to call sharedMemGetError on that object to get a description of the failure.
 * Returned object should in any case be destroyed with sharedMemDestroy.
 * If false is returned and no object is returned the cause could be a NULL shared parameter or an out-of-memory situation
 * @param utf8Name Unique name for the shared memory area
 * @param info Size and align of memory region allocated (if allocation will be done by this call)
 * @param memory  [out] Will be filled with a pointer to the allocated shared object
 * @param server [in] True if we are creating the "server" side of the shared memory
 * @return True on success
 */
bool sharedMemCreate(const char *utf8Name, const SharedMemInfo *info, struct SharedMemory **shared, uint32_t localSize, bool server);

/**
 * @brief Returns current error message
 *
 * Note the error message is not shared and specific of each process.
 * @param shared Shared memory
 * @return Pointer to an error message
 */
const char *sharedMemGetError(struct SharedMemory *shared);

/**
 * @brief Deletes the shared memory
 *
 * Note: false will be returned if and only if shared is NULL
 * @param shared Shared memory to be deleted
 * @return True on success
 */
bool sharedMemDestroy(struct SharedMemory *shared);

/**
 * @brief Returns a pointer to the process area of a shared memory
 * @param shared Shared memory
 */
void *sharedMemLocal(struct SharedMemory *shared);

/**
 * @brief Returns a pointer to the header of a shared memory
 * @param shared Shared memory
 */
void *sharedMemHeader(struct SharedMemory *shared);

/**
 * @brief Checks if the returned memory object needs to be initialized by this process
 * @param memory Shared memory object
 * @return True if objects needs initialization by this process
 */
bool sharedMemMustInitialize(struct SharedMemory *shared);

/**
 * @brief Returns the size/alignment of shared memory areas
 * @param shared Shared memory object
 * @return A const pointer to the effective info of shared memory
 */
const SharedMemInfo *sharedMemInfo(struct SharedMemory *shared);

/**
 * @brief Assigns a page as free page of the client
 *
 * Note: sharedMemMustInitialize must return true for this function to succeed
 * @param shared Shared memory
 * @param page Number of page
 */
void sharedMemInitPageClient(struct SharedMemory *shared, uint32_t page);

/**
 * @brief Assigns a page as free page of the server
 *
 * Note: sharedMemMustInitialize must return true for this function to succeed
 * @param shared Shared memory
 * @param page Number of page
 */
void sharedMemInitPageServer(struct SharedMemory *shared, uint32_t page);

/**
 * @brief Ends the initialization phase
 *
 * After a call to this function the shared memory will operate normally (and sharedMemIsInitialized will return true)
 * @param shared Shared memory
 */
void sharedMemEndInitialization(struct SharedMemory *shared);

/**
 * @brief Checks if the returned memory object is already initialized
 * @param memory Shared memory object
 * @return True if objects is already initialized.
 */
bool sharedMemIsInitialized(struct SharedMemory *shared);

/**
 * @brief Waits for a notification
 * @param shared Shared memory
 * @param timeoutMs Timeout to wait, in milliseconds
 * @return True if a notification happened
 */
bool sharedMemWaitNotify(struct SharedMemory *shared, uint32_t timeoutMs);

/**
 * @brief Gets the number of pages (both free or data) owned by this process
 * @param shared Shared memory object
 * @return Number of pages
 */
int32_t sharedMemGetNumOwnedPages(struct SharedMemory *shared);

/**
 * @brief Returns the first free page available to this process
 * @param memory Shared memory object
 * @param start First page to search
 * @return -1 if no page after start was found or start<0, index of page otherwise
 */
int32_t sharedMemGetFreePage(struct SharedMemory *shared, int32_t start);
/**
 * @brief Returns the first page returned from other process available to this process
 * @param memory Shared memory object
 * @param start First page to search
 * @return -1 if no page after start was found or start<0, index of page otherwise
 */
int32_t sharedMemGetDataPage(struct SharedMemory *shared, int32_t start);

/**
 * @brief Returns the first page owned by this process with a given state
 * @param memory Shared memory object
 * @param start First page to search
 * @return -1 if no page after start was found or start<0, index of page otherwise
 */
int32_t sharedMemGetFirstPageN(struct SharedMemory *shared, uint32_t state, int32_t start);

/**
 * @brief Sets the state of a page to the given value
 * Note: page should be owned by this process
 * @param memory Shared Memory object
 * @param page Page to free
 * @param state New state (1=free, 2=used, 3... custom)
 * @return True on success
 */
bool sharedMemSetPageN(struct SharedMemory *shared, uint32_t page, uint32_t state);

/**
 * @brief Frees a page, but keep it available to this process
 *
 * Note: page must be owned (free or returned) by this process
 * @param memory Shared Memory object
 * @param page Page to free
 * @return True on success
 */
bool sharedMemFreePage(struct SharedMemory *shared, uint32_t page);

/**
 * @brief Sends a page to the other process as "data" page
 *
 * Note: page must be owned (free or returned) by this process
 * @param memory Shared Memory object
 * @param page Page to send
 * @return True on success
 */
bool sharedMemSendData(struct SharedMemory *shared, uint32_t page);

/**
 * @brief Sends a page to the other process as "free" page
 *
 * Note: page must be owned (free or returned) by this process
 * @param memory Shared Memory object
 * @param page Page to send
 * @return True on success
 */
bool sharedMemSendFree(struct SharedMemory *shared, uint32_t page);

/**
 * @brief Gets the data part of a page
 * @param memory Shared Memory object
 * @param page Number of the page
 * @return Pointer to page data if page is correct, NULL otherwise
 */
volatile void *sharedMemPageData(struct SharedMemory *shared, uint32_t page);

/**
 * @brief Gets the header part of a page
 * @param memory Shared Memory object
 * @param page Number of the page
 * @return Pointer to page data if page is correct, NULL otherwise
 */
volatile void *sharedMemPageHeader(struct SharedMemory *shared, uint32_t page);

#if defined(SHAREDMEM_WIN32)
void *sharedMemNotificationHandle(struct SharedMemory *memory);
#endif

#ifdef __cplusplus
}
#endif
