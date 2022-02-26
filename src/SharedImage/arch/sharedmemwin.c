/*
 * This file is part of SharedImageIPC.
 *
 * (c) Marzocchi Alessandro
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#include "../internal/sharedmeminternal.h"
#include "stdio.h"
#if defined(SHAREDMEM_WIN32)
void sharedMemArchNotify(struct SharedMemory *shared)
{
  SetEvent(shared->arch.eventToOther);
}

bool sharedMemArchWaitNotify(const struct SharedMemory *memory, uint32_t timeoutMs)
{
  DWORD waitRet;
  waitRet=WaitForSingleObject(memory->arch.eventFromOther, timeoutMs);
  return (waitRet==WAIT_OBJECT_0);
}

bool sharedMemCloseArch(struct SharedMemory *shared)
{
  if(shared->data)
  {
    UnmapViewOfFile((const void *)shared->data);
    shared->data=NULL;
  }
  if(shared->arch.handleShared)
  {
    CloseHandle(shared->arch.handleShared);
    shared->arch.handleShared=NULL;
  }
  if(shared->arch.eventFromOther)
  {
    CloseHandle(shared->arch.eventFromOther);
    shared->arch.eventFromOther=NULL;
  }
  if(shared->arch.eventToOther)
  {
    CloseHandle(shared->arch.eventToOther);
    shared->arch.eventToOther=NULL;
  }
  return true;
}

bool sharedMemCreateArch(const char *utf8Name, struct SharedMemory *shared, uint32_t requestedSize, bool server)
{
  bool ret=true;
  uint8_t *pBuf=NULL;

  if(strlen(utf8Name)>NAME_MAX_LENGTH)
  {
    snprintf(shared->message, sizeof(shared->message), "Shared memory name too long");
    ret=false;
  }
  if(ret) // Event 0
  {
    HANDLE hEvent=NULL;
    char tempEventName[strlen(utf8Name)+5];
    snprintf(tempEventName, sizeof(tempEventName), "shd%sA", utf8Name);
    int ncharsEvent=MultiByteToWideChar(CP_UTF8, 0, tempEventName, -1, NULL, 0);
    if(ncharsEvent>100)
    {
      snprintf(shared->message, sizeof(shared->message), "Shared memory name too long");
      ret=false;
    }
    else
    {
      wchar_t eventName[ncharsEvent];
      if(MultiByteToWideChar(CP_UTF8, 0, tempEventName, -1, eventName, ncharsEvent)!=ncharsEvent)
      {
        snprintf(shared->message, sizeof(shared->message), "Error in MultiByteToWideChar for event0");
        ret=false;
      }
      else if((hEvent=CreateEvent(
              NULL,
              FALSE, /* Manual reset */
              FALSE, /* Initial state */
              eventName))==NULL)
      {
          snprintf(shared->message, sizeof(shared->message), "Error in CreateEvent for event0");
          ret=false;
      }
      else if(server)
        shared->arch.eventFromOther=hEvent;
      else
        shared->arch.eventToOther=hEvent;
    }
  }
  if(ret) // Event 0
  {
    HANDLE hEvent=NULL;
    char tempEventName[strlen(utf8Name)+5];
    snprintf(tempEventName, sizeof(tempEventName), "shd%sB", utf8Name);
    int ncharsEvent=MultiByteToWideChar(CP_UTF8, 0, tempEventName, -1, NULL, 0);
    if(ncharsEvent>100)
    {
      snprintf(shared->message, sizeof(shared->message), "Shared memory name too long");
      ret=false;
    }
    else
    {
      wchar_t eventName[ncharsEvent];
      if(MultiByteToWideChar(CP_UTF8, 0, tempEventName, -1, eventName, ncharsEvent)!=ncharsEvent)
      {
        snprintf(shared->message, sizeof(shared->message), "Error in MultiByteToWideChar for eventB");
        ret=false;
      }
      else if((hEvent=CreateEvent(
              NULL,
              FALSE, /* Manual reset */
              FALSE, /* Initial state */
              eventName))==NULL)
      {
          snprintf(shared->message, sizeof(shared->message), "Error in CreateEvent for eventB");
          ret=false;
      }
      else if(server)
        shared->arch.eventToOther=hEvent;
      else
        shared->arch.eventFromOther=hEvent;
    }
  }
  if(ret)
  {
    char tempSharedName[strlen(utf8Name)+5];
    snprintf(tempSharedName, sizeof(tempSharedName), "shd%sD", utf8Name);
    int ncharsShared=MultiByteToWideChar(CP_UTF8, 0, tempSharedName, -1, NULL, 0);
    if(ncharsShared>100)
    {
      snprintf(shared->message, sizeof(shared->message), "Shared memory name too long");
      ret=false;
    }
    else
    {
      wchar_t sharedName[ncharsShared];
      if(MultiByteToWideChar(CP_UTF8, 0, tempSharedName, -1, sharedName, ncharsShared)!=ncharsShared)
      {
        snprintf(shared->message, sizeof(shared->message), "Error in MultiByteToWideChar for event1");
        ret=false;
      }
      if((shared->arch.handleShared = CreateFileMapping(
            INVALID_HANDLE_VALUE,    // use paging file
            NULL,                    // default security
            PAGE_READWRITE,          // read/write access
            0,                       // maximum object size (high-order DWORD)
            requestedSize,         // maximum object size (low-order DWORD)
            sharedName))             // name of mapping object
          ==NULL)
      {
        snprintf(shared->message, sizeof(shared->message), "Error in CreateFileMapping");
        ret=false;
      }
      else if(GetLastError()==ERROR_ALREADY_EXISTS)
      {
        // Printf connecting to an already existing memory buffer
        pBuf = MapViewOfFile(shared->arch.handleShared,   // handle to map object
                             FILE_MAP_ALL_ACCESS, // read/write permission
                             0,
                             0,
                             0);
        shared->data=(volatile struct SharedMemInternalHeader *)pBuf;
        shared->needInitialize=false;
        MEMORY_BASIC_INFORMATION memoryInfo;
        if(!pBuf)
        {
          snprintf(shared->message, sizeof(shared->message), "Error in MapViewOfFile (existing shared memory)");
          ret=false;
        }
        else if(VirtualQuery(pBuf, &memoryInfo, sizeof(memoryInfo))!=sizeof(memoryInfo))
        {
          snprintf(shared->message, sizeof(shared->message), "Error in VirtualQuery (existing shared memory)");
          ret=false;
        }
        else if(memoryInfo.RegionSize<sizeof(struct SharedMemInternalHeader))
        {
          snprintf(shared->message, sizeof(shared->message), "Existing shared memory too small to be a shared memory");
          ret=false;
        }
        else if(!sharedMemCheckHeader(shared))
          ret=false;
        else
        {
          uint32_t size=shared->data->layout.fullSize;
          UnmapViewOfFile(pBuf);
          CloseHandle(shared->arch.handleShared);
          shared->data=NULL;
          shared->arch.handleShared=NULL;
          if((shared->arch.handleShared = CreateFileMapping(
                INVALID_HANDLE_VALUE,    // use paging file
                NULL,                    // default security
                PAGE_READWRITE,          // read/write access
                0,                       // maximum object size (high-order DWORD)
                size,                    // maximum object size (low-order DWORD)
                sharedName))             // name of mapping object
             ==NULL)
          {
            snprintf(shared->message, sizeof(shared->message), "Error in second call to CreateFileMapping");
            ret=false;
          }
          else if(GetLastError()!=ERROR_ALREADY_EXISTS)
          {
            snprintf(shared->message, sizeof(shared->message), "BUG! Second call to CreateFileMapping returned a new shared memory");
            ret=false;
          }
          else if((pBuf=MapViewOfFile(shared->arch.handleShared,   // handle to map object
                                 FILE_MAP_ALL_ACCESS, // read/write permission
                                 0,
                                 0,
                                 size))==NULL)
          {
            snprintf(shared->message, sizeof(shared->message), "Error in second call to MapViewOfFile");
            ret=false;
          }
          else
            shared->data=(volatile struct SharedMemInternalHeader *)pBuf;
        }
      }
      else
      {
        // New file. Initialize all the data
        if((pBuf = MapViewOfFile(shared->arch.handleShared,   // handle to map object
                             FILE_MAP_ALL_ACCESS, // read/write permission
                             0,
                             0,
                             requestedSize))==NULL)
        {
          snprintf(shared->message, sizeof(shared->message), "Error in call for new to MapViewOfFile");
          ret=false;
        }
        else
        {
          shared->data=(volatile struct SharedMemInternalHeader *)pBuf;
          shared->needInitialize=true;
        }
      }
    }
  }
  if(!ret)
    sharedMemCloseArch(shared);
  return true;
}

void *sharedMemNotificationHandle(struct SharedMemory *memory)
{
  return memory->arch.eventFromOther;
}

#endif
