/*
 * This file is part of SharedImageIPC.
 *
 * (c) Marzocchi Alessandro
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sharedmem.h"
#include "internal/sharedmeminternal.h"
#define CLEAR_ERROR(memory) memory->message[0]='\0'
#define SET_ERROR(memory, ...) snprintf(memory->message, sizeof(memory->message), __VA_ARGS__)
static const uint32_t sharedDefaultAlignment=16;
static bool sharedMemIsFreeState(int32_t state)
{
  return (state==-1) || (state==1);
}
static bool sharedMemIsDataState(int32_t state)
{
  return (state<-1) || (state>1);
}
static bool sharedMemIsServer(int32_t state)
{
  return state>0;
}

static uint32_t alignOf(uint32_t align)
{
  return align?align:sharedDefaultAlignment;
}

static uint32_t upboundn(uint32_t value, uint32_t align)
{
  align=alignOf(align);
  return ((value+(align-1))/align)*align;
}

static bool checkValueMultiple2(uint32_t value)
{
  bool ret=true;
  for(;value;value>>=1)
  {
    if(value&1)
    {
      ret=!(value>>1);
      break;
    }
  }
  return ret;
}
static uint32_t maxUint32(uint32_t a, uint32_t b)
{
  return (a>b)?a:b;
}

static bool sharedCheckValidOrInitializing(struct SharedMemory *shared)
{
  bool ret=false;
  if(shared)
  {
    if(shared->data)
    {
      if(shared->data->state==SharedMemory_Initialized || shared->needInitialize)
        ret=true;
      else
        SET_ERROR(shared, "Shared memory awaiting initialization");
    }
    else
      SET_ERROR(shared, "Invalid shared memory");
  }
  return ret;
}

static bool sharedCheckInitialized(struct SharedMemory *shared)
{
  bool ret=false;
  if(shared)
  {
    if(shared->data)
    {
      if(shared->data->state==SharedMemory_Initialized)
        ret=true;
      else
        SET_ERROR(shared, "Shared memory not initialized");
    }
    else
      SET_ERROR(shared, "Invalid shared memory");
  }
  return ret;
}

static volatile struct SharedMemPageHeader *sharedMemPageLibHeader(struct SharedMemory *shared, uint32_t page)
{
  volatile struct SharedMemPageHeader *ret=NULL;
  if(page<shared->data->info.numPages)
  {
    ret=(volatile struct SharedMemPageHeader *)(((volatile uint8_t *)shared->data)+shared->data->layout.firstPageStart+shared->data->layout.wholePageSize*page+shared->data->layout.libPageHeaderOffset);
    CLEAR_ERROR(shared);
  }
  else
    SET_ERROR(shared, "Invalid page number");
  return ret;
}

volatile void *sharedMemPageHeader(struct SharedMemory *shared, uint32_t page)
{
  volatile uint8_t *ret=NULL;
  if(sharedCheckValidOrInitializing(shared))
  {
    if(page<shared->data->info.numPages)
    {
      ret=((volatile uint8_t *)shared->data)+shared->data->layout.firstPageStart+shared->data->layout.wholePageSize*page+shared->data->layout.appPageHeaderOffset;
      CLEAR_ERROR(shared);
    }
    else
      SET_ERROR(shared, "Invalid page number");
  }
  return ret;
}
volatile void *sharedMemPageData(struct SharedMemory *shared, uint32_t page)
{
  volatile uint8_t *ret=NULL;
  if(sharedCheckValidOrInitializing(shared))
  {
    if(page<shared->data->info.numPages)
    {
      ret=((volatile uint8_t *)shared->data)+shared->data->layout.firstPageStart+shared->data->layout.wholePageSize*page+shared->data->layout.dataOffset;
      CLEAR_ERROR(shared);
    }
    else
      SET_ERROR(shared, "Invalid page number");
  }
  return ret;
}
static bool sharedCalculateLayout(const SharedMemInfo *info, struct SharedMemLayout *layout)
{
  bool ret=false;
  if(layout)
    memset(layout, 0, sizeof(*layout));
  if(info && layout && checkValueMultiple2(info->headerAlign) && checkValueMultiple2(info->pageHeaderAlign) && checkValueMultiple2(info->pageAlign))
  {
    layout->headerStart=upboundn(sizeof(struct SharedMemInternalHeader), info->headerAlign);
    layout->firstPageStart=upboundn(layout->headerStart+info->headerSize, 0);
    layout->libPageHeaderOffset=0;
    layout->appPageHeaderOffset=upboundn(layout->firstPageStart+sizeof(struct SharedMemPageHeader), info->pageHeaderAlign)-layout->firstPageStart;
    layout->dataOffset=upboundn(layout->firstPageStart+layout->appPageHeaderOffset+info->pageHeaderSize, info->pageAlign)-layout->firstPageStart;
    layout->wholePageSize=upboundn(layout->firstPageStart+layout->dataOffset+info->pageSize,maxUint32(alignOf(0), maxUint32(alignOf(info->pageHeaderAlign), alignOf(info->pageAlign))));
    layout->fullSize=layout->firstPageStart+layout->wholePageSize*info->numPages;
    ret=true;
  }
  return ret;
}

bool sharedMemCheckHeader(struct SharedMemory *shared)
{
  bool ret=(shared->data->version==SHAREDMEM_VERSION && shared->data->magic==SHAREDMEM_MAGIC);
  if(!ret)
    SET_ERROR(shared, "Incompatible header");
  return ret;
}

bool sharedMemCreate(const char *utf8Name, const SharedMemInfo *info, struct SharedMemory **shared, uint32_t localSize, bool server)
{
  bool ret=false;
  if(shared)
  {
    struct SharedMemory *sharedRet=NULL;
    struct SharedMemLayout layout;
    int baseSize=upboundn(sizeof(struct SharedMemory), 0);
    sharedRet=malloc(baseSize+localSize);
    *shared=sharedRet;
    if(sharedRet)
      memset(sharedRet, 0, baseSize+localSize);
    if(!sharedRet)
    {
    }
    else if(!info)
      SET_ERROR(sharedRet, "Info parameter NULL");
    else if(!utf8Name)
      SET_ERROR(sharedRet, "Id parameter NULL");
    else if(!sharedCalculateLayout(info, &layout))
      SET_ERROR(sharedRet, "Invalid layout info");
    else if(sharedMemCreateArch(utf8Name, sharedRet, layout.fullSize, server))
    {
      if(!sharedRet->data)
        SET_ERROR(sharedRet, "BUG! sharedMemCreateArch returned true but set no data pointer");
      else
      {
        sharedRet->server=server;
        if(sharedRet->needInitialize)
        {
          sharedRet->data->state=Unitialized; // Should already be zero, but for safety
          sharedRet->data->info=*info;
          sharedRet->data->layout=layout;
          sharedRet->data->info.headerAlign=alignOf(sharedRet->data->info.headerAlign);
          sharedRet->data->info.pageHeaderAlign=alignOf(sharedRet->data->info.pageHeaderAlign);
          sharedRet->data->info.pageAlign=alignOf(sharedRet->data->info.pageAlign);
          uint8_t *pBuf=(uint8_t *)sharedRet->data;
          for(unsigned i=0;i<sharedRet->data->info.numPages;i++)
          {
            struct SharedMemPageHeader *header=(struct SharedMemPageHeader *)(pBuf+layout.firstPageStart+i*layout.wholePageSize+layout.libPageHeaderOffset);
            header->state=1; // Free to server
          }
          sharedRet->data->version=SHAREDMEM_VERSION;
          sharedRet->data->magic=SHAREDMEM_MAGIC;
        }
        ret=true;
      }
    }
  }
  return ret;
}

bool sharedMemDestroy(struct SharedMemory *shared)
{
  bool ret=false;
  if(shared)
  {
    sharedMemCloseArch(shared);
    free(shared);
    ret=true;
  }
  return ret;
}

bool sharedMemMustInitialize(struct SharedMemory *shared)
{
  return (shared && shared->needInitialize);
}

const SharedMemInfo *sharedMemInfo(struct SharedMemory *shared)
{
  const SharedMemInfo *ret=NULL;
  if(shared && shared->data)
    ret=(const SharedMemInfo *)&shared->data->info;
  return ret;
}

void sharedMemEndInitialization(struct SharedMemory *shared)
{
  if(shared && shared->data && shared->needInitialize)
  {
    shared->needInitialize=false;
    shared->data->state=SharedMemory_Initialized;
    sharedMemArchNotify(shared);
  }
}

bool sharedMemIsInitialized(struct SharedMemory *shared)
{
  return (shared && shared->data->state==SharedMemory_Initialized);
}

bool sharedMemWaitNotify(struct SharedMemory *shared, uint32_t timeoutMs)
{
  return sharedMemArchWaitNotify(shared, timeoutMs);
}

int32_t sharedMemGetFreePage(struct SharedMemory *shared, int32_t start)
{
  if(!sharedCheckInitialized(shared))
    start=-1;
  else if(start<0)
    SET_ERROR(shared, "Start<0");
  else
  {
    CLEAR_ERROR(shared);
    for(;;start++)
    {
      if(start>=(int32_t)shared->data->info.numPages)
      {
        start=-1;
        break;
      }
      uint32_t state=sharedMemPageLibHeader(shared, start)->state;
      if(sharedMemIsFreeState(state) && sharedMemIsServer(state)==shared->server)
        break;
    }
  }
  return start;
}

int32_t sharedMemGetDataPage(struct SharedMemory *shared, int32_t start)
{
  if(!sharedCheckInitialized(shared))
    start=-1;
  else if(start<0)
    SET_ERROR(shared, "Start<0");
  else
  {
    CLEAR_ERROR(shared);
    for(;;start++)
    {
      if(start>=(int32_t)shared->data->info.numPages)
      {
        start=-1;
        break;
      }
      uint32_t state=sharedMemPageLibHeader(shared, start)->state;
      if(sharedMemIsDataState(state) && sharedMemIsServer(state)==shared->server)
        break;
    }
  }
  return start;
}

bool sharedMemSetPageN(struct SharedMemory *shared, uint32_t page, uint32_t state)
{
  bool ret=false;
  if(sharedCheckInitialized(shared))
  {
    if(page>=shared->data->info.numPages)
      SET_ERROR(shared, "Invalid page");
    else if(sharedMemIsServer(sharedMemPageLibHeader(shared, page)->state)!=shared->server)
      SET_ERROR(shared, "Process does not own the page");
    else
    {
      sharedMemPageLibHeader(shared, page)->state=(shared->server?(int)state:-(int)state);
      CLEAR_ERROR(shared);
      ret=true;
    }
  }
  return ret;
}

bool sharedMemFreePage(struct SharedMemory *shared, uint32_t page)
{
  bool ret=false;
  if(sharedCheckInitialized(shared))
  {
    if(page>=shared->data->info.numPages)
      SET_ERROR(shared, "Invalid page");
    else if(sharedMemIsServer(sharedMemPageLibHeader(shared, page)->state)!=shared->server)
      SET_ERROR(shared, "Process does not own the page");
    else
    {
      sharedMemPageLibHeader(shared, page)->state=(shared->server?SharedMemPageFreeServer:SharedMemPageFreeClient);
      CLEAR_ERROR(shared);
      ret=true;
    }
  }
  return ret;
}
bool sharedMemSendData(struct SharedMemory *shared, uint32_t page)
{
  bool ret=false;
  if(sharedCheckInitialized(shared))
  {
    if(page>=shared->data->info.numPages)
      SET_ERROR(shared, "Invalid page");
    else if(sharedMemIsServer(sharedMemPageLibHeader(shared, page)->state)!=shared->server)
      SET_ERROR(shared, "Process does not own the page");
    else
    {
      sharedMemPageLibHeader(shared, page)->state=(shared->server?SharedMemPageDataClient:SharedMemPageDataServer);
      sharedMemArchNotify(shared);
      CLEAR_ERROR(shared);
      ret=true;
    }
  }
  return ret;
}

bool sharedMemSendFree(struct SharedMemory *shared, uint32_t page)
{
  bool ret=false;
  if(sharedCheckInitialized(shared))
  {
    if(page>=shared->data->info.numPages)
      SET_ERROR(shared, "Invalid page");
    else if(sharedMemIsServer(sharedMemPageLibHeader(shared, page)->state)!=shared->server)
      SET_ERROR(shared, "Process does not own the page");
    else
    {
      sharedMemPageLibHeader(shared, page)->state=(shared->server?SharedMemPageFreeClient:SharedMemPageFreeServer);
      sharedMemArchNotify(shared);
      CLEAR_ERROR(shared);
      ret=true;
    }
  }
  return ret;
}

void *sharedMemLocal(struct SharedMemory *shared)
{
  return (shared && shared->data)?(&((uint8_t *)shared)[upboundn(sizeof(struct SharedMemory), 0)]): NULL;
}

void *sharedMemHeader(struct SharedMemory *shared)
{
  void *ret=NULL;
  if(sharedCheckValidOrInitializing(shared))
  {
    ret=((uint8_t *)shared->data)+shared->data->layout.headerStart;
    CLEAR_ERROR(shared);
  }
  return ret;
}

int32_t sharedMemGetNumOwnedPages(struct SharedMemory *shared)
{
  int32_t ret=0;
  if(sharedCheckInitialized(shared))
  {
    for(int i=0;i<(int32_t)shared->data->info.numPages;i++)
    {
      uint32_t state=sharedMemPageLibHeader(shared, i)->state;
      if(sharedMemIsServer(state)==shared->server)
        ret++;
    }
  }
  return ret;
}

void sharedMemInitPageClient(struct SharedMemory *shared, uint32_t page)
{
  if(shared && shared->data && shared->needInitialize && page<shared->data->info.numPages)
    sharedMemPageLibHeader(shared, page)->state=SharedMemPageFreeClient;
}

void sharedMemInitPageServer(struct SharedMemory *shared, uint32_t page)
{
  if(shared && shared->data && shared->needInitialize && page<shared->data->info.numPages)
    sharedMemPageLibHeader(shared, page)->state=SharedMemPageFreeServer;
}

int32_t sharedMemGetFirstPageN(struct SharedMemory *shared, uint32_t state, int32_t start)
{
  if(!sharedCheckInitialized(shared))
    start=-1;
  else if(start>=0)
  {
    for(;;start++)
    {
      if(start>=(int32_t)shared->data->info.numPages)
      {
        start=-1;
        break;
      }
      int32_t curState=sharedMemPageLibHeader(shared, start)->state;
      if((((int32_t)state)==curState || ((int32_t)state)==-curState) && sharedMemIsServer(curState)==shared->server)
        break;
    }
  }
  return start;
}

const char *sharedMemGetError(struct SharedMemory *shared)
{
  shared->message[sizeof(shared->message)-1]='\0';
  return shared->message;
}
