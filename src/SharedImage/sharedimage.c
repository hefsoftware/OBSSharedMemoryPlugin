/*
 * This file is part of SharedImageIPC.
 *
 * (c) Marzocchi Alessandro
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#include "sharedimage.h"
#include "sharedmem.h"
#include <stdio.h>
#include <string.h>

typedef struct
{
  bool initialized;
  bool valid;
  int32_t lastPage;
}SharedImageLocal;
typedef struct
{
  uint32_t magic;
  uint32_t version;
} SharedImageHeader;
typedef struct
{
  SharedImageSetting setting;
} SharedImagePageHeader;

#define SHAREDMEMIMAGE_MAGIC 0x41B0D34A
#define SHAREDMEMIMAGE_VERSION 0x100
bool sharedImageCheckInitialized(struct SharedImage *image)
{
  struct SharedMemory *shared=(struct SharedMemory *)image;
  SharedImageLocal *local=(SharedImageLocal *)sharedMemLocal(shared);
  bool ret=local->initialized;
  if(local && !local->initialized && sharedMemIsInitialized(shared))
  {
    local->initialized=true;
    SharedImageHeader *header=(SharedImageHeader *)sharedMemHeader(shared);
    local->valid=(header->magic==SHAREDMEMIMAGE_MAGIC) && (header->version==SHAREDMEMIMAGE_VERSION);
    ret=local->valid;
  }
  return ret;
}

bool sharedImageCreate(const char *utf8Name, struct SharedImage **image, uint32_t numPixels, bool generator)
{
  struct SharedMemory *ret=NULL;
  SharedMemInfo info;
  memset(&info, 0, sizeof(info));
  info.numPages=2;
  info.headerSize=sizeof(SharedImageHeader);
  info.pageHeaderSize=sizeof(SharedImagePageHeader);
  info.pageSize=numPixels*sizeof(uint32_t);

  bool retValue;
  retValue=sharedMemCreate(utf8Name, &info, &ret, sizeof(SharedImageLocal), generator);
  *image=(struct SharedImage *)ret;
  if(retValue)
  {
    SharedImageLocal *local=(SharedImageLocal *)sharedMemLocal(ret);
    local->initialized=local->valid=false;
    local->lastPage=-1;
    volatile SharedImageHeader *header=(volatile SharedImageHeader *)sharedMemHeader(ret);
    if(sharedMemMustInitialize(ret))
    {
      header->magic=SHAREDMEMIMAGE_MAGIC;
      header->version=SHAREDMEMIMAGE_VERSION;
      for(unsigned i=0;i<info.numPages;i++)
        sharedMemInitPageClient(ret, i);
      sharedMemEndInitialization(ret);
      local->initialized=local->valid=true;
    }
  }
  return retValue;
}

bool sharedImageReceive(struct SharedImage *image, void **imageData, const SharedImageSetting **settings)
{
  bool ret=false;
  if(sharedImageCheckInitialized(image))
  {
    struct SharedMemory *shared=(struct SharedMemory *)image;
    SharedImageLocal *local=sharedMemLocal(shared);
    int32_t page=sharedMemGetFirstPageN(shared, 2, 0);
    if(page>=0)
    {
      ret=true;
      if(imageData)
        *imageData=(void *)sharedMemPageData(shared, page);
      if(settings)
        *settings=&((SharedImagePageHeader *)sharedMemPageHeader(shared, page))->setting;

      if(local->lastPage>=0) // If we had a previous page frees it
      {
        sharedMemFreePage(shared, local->lastPage);
        local->lastPage=-1;
      }
      local->lastPage=page;
      sharedMemSetPageN(shared, page, 3);
      for(;;) // Frees all other data pages, if any
      {
        page=sharedMemGetDataPage(shared, page+1);
        if(page<0)
          break;
        sharedMemFreePage(shared, page);
      }
    }
    int num=sharedMemGetNumOwnedPages(shared);
    if(num>1)
    {
      page=sharedMemGetFreePage(shared, 0);
      if(page>=0) // In theory this should always happen
        sharedMemSendFree(shared, page);
    }
  }
  return ret;
}

bool sharedImageOutBuffer(struct SharedImage *image, void **imageData, uint32_t *availablePixels)
{
  bool ret=false;
  if(sharedImageCheckInitialized(image))
  {
    struct SharedMemory *shared=(struct SharedMemory *)image;
    int32_t page=sharedMemGetFreePage(shared, 0);
    if(page>=0)
    {
      SharedImageLocal *local=(SharedImageLocal *)sharedMemLocal(shared);
      local->lastPage=page;
      ret=true;
      if(imageData)
        *imageData=(void *)sharedMemPageData(shared, page);
      if(availablePixels)
        *availablePixels=sharedMemInfo(shared)->pageSize/sizeof(uint32_t);
    }
  }
  return ret;
}

bool sharedImageSend(struct SharedImage *image, const SharedImageSetting *setting)
{
  bool ret=false;
  if(sharedImageCheckInitialized(image))
  {
    struct SharedMemory *shared=(struct SharedMemory *)image;
    SharedImageLocal *local=(SharedImageLocal *)sharedMemLocal(shared);
    if(local->lastPage>=0)
    {
      volatile SharedImagePageHeader *header=(volatile SharedImagePageHeader *)sharedMemPageHeader(shared, local->lastPage);
      header->setting=*setting;
      sharedMemSendData(shared, local->lastPage);
      local->lastPage=-1;
      ret=true;
    }
  }
  return ret;
}

bool sharedImageDestroy(struct SharedImage *image)
{
  return sharedMemDestroy((struct SharedMemory *)image);
}

#if defined(SHAREDMEM_WIN32)
void *sharedImageNotificationHandle(struct SharedImage *image)
{
  return sharedMemNotificationHandle((struct SharedMemory *)image);
}
#endif

const char *sharedImageGetError(struct SharedImage *image)
{
  return sharedMemGetError((struct SharedMemory *)image);
}

bool sharedImageWaitNotify(struct SharedImage *image, uint32_t timeoutMs)
{
  return sharedMemWaitNotify((struct SharedMemory *)image, timeoutMs);
}
