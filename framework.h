// header.h : include file for standard system include files,
// or project specific include files
//

#pragma once

#if _WIN32
#include "targetver.h"
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#define NOMINMAX						// Exclude min/max function
// Windows Header Files
#include <windows.h>
#include <tchar.h>
#endif

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>