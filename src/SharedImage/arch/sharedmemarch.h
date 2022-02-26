/*
 * This file is part of SharedImageIPC.
 *
 * (c) Marzocchi Alessandro
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#pragma once
#if defined(SHAREDMEM_WIN32)
  #include "windows.h"
  typedef struct
  {
    HANDLE handleShared;
    HANDLE eventFromOther;
    HANDLE eventToOther;
  } SharedMemoryArch;
#endif
