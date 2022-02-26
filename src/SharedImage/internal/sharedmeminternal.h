/*
 * This file is part of SharedImageIPC.
 *
 * (c) Marzocchi Alessandro
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */
#pragma once
#include "../sharedmem.h"
#include <stdint.h>
#include <stdbool.h>
#include "../arch/sharedmemarch.h"

#define SHAREDMEM_MAGIC 0x14BFA396
#define NAME_MAX_LENGTH 64
enum SharedMemBufferState
{
  Unitialized,
  SharedMemory_Initialized=0x6F43
};

typedef enum
{
  Ok,
  WrongParameters,
  SysCallError,
  CorruptedData
} SharedMemErrorCode;

enum SharedMemDefaultStates
{
  SharedMemPageDataClient=-2,
  SharedMemPageFreeClient=-1,
  SharedMemPageFreeServer= 1,
  SharedMemPageDataServer= 2
};

struct SharedMemLayout
{
  uint32_t headerStart;
  uint32_t firstPageStart;
  uint32_t wholePageSize;
  uint32_t libPageHeaderOffset; // Library header offset from start of page
  uint32_t appPageHeaderOffset; // Application header offset from start of page
  uint32_t dataOffset;      // Data offset from start of page. Pages will be found at firstPageStart+wholePageSize*NPage+dataOffset
  uint32_t fullSize; // Full size of allocated memory area
};

struct SharedMemInternalHeader
{
  uint32_t magic;
  uint32_t version;
  uint32_t state;
  SharedMemInfo info;
  struct SharedMemLayout layout;
};

struct SharedMemPageHeader
{
  int32_t state; // 0 is invalid/unassigned, >0 is server, <0 is client, abs(state)==1 free abs(state)>1 data
};

struct SharedMemory
{
  volatile struct SharedMemInternalHeader *data;
  char message[128];
  bool valid; // True if shared memory was correctly created
  bool needInitialize;
  bool server;
  SharedMemoryArch arch;
};

bool sharedMemCheckHeader(struct SharedMemory *shared);

void sharedMemArchNotify(struct SharedMemory *shared);
bool sharedMemArchWaitNotify(const struct SharedMemory *memory, uint32_t timeoutMs);
bool sharedMemCloseArch(struct SharedMemory *shared);
bool sharedMemCreateArch(const char *utf8Name, struct SharedMemory *shared, uint32_t requestedSize, bool server);

