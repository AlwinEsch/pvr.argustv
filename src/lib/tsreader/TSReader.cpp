/*
 *  Copyright (C) 2005-2020 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

/*
 *************************************************************************
 *  Parts of this file originate from Team MediaPortal's
 *  TsReader DirectShow filter
 *  MediaPortal is a GPL'ed HTPC-Application
 *  Copyright (C) 2005-2012 Team MediaPortal
 *  http://www.team-mediaportal.com
 *
 * Changes compared to Team MediaPortal's version:
 * - Code cleanup for PVR addon usage
 * - Code refactoring for cross platform usage
 *************************************************************************/

#include "TSReader.h"

#include "MultiFileReader.h"
#include "utils.h"

#include <kodi/General.h>
#include <kodi/tools/StringUtils.h>

template<typename T> void SafeDelete(T*& p)
{
  if (p)
  {
    delete p;
    p = nullptr;
  }
}
namespace ArgusTV
{
CTsReader::CTsReader()
{
#if defined(TARGET_WINDOWS)
  liDelta.QuadPart = liCount.QuadPart = 0;
#endif
}

bool CTsReader::Open(const std::string& fileName)
{
  kodi::Log(ADDON_LOG_DEBUG, "CTsReader::Open(%s)", fileName.c_str());

  m_fileName = fileName;

  //check file type
  if (m_fileName.length() < 9 || !kodi::tools::StringUtils::EndsWithNoCase(m_fileName, ".tsbuffer"))
  {
    //local .ts file
    m_bTimeShifting = false;
    m_bLiveTv = false;
    m_fileReader = new FileReader();
  }
  else
  {
    //local timeshift buffer file file
    m_bTimeShifting = true;
    m_bLiveTv = true;
    m_fileReader = new MultiFileReader();
  }

  //open file
  if (m_fileReader->SetFileName(m_fileName.c_str()) != true)
  {
    kodi::Log(ADDON_LOG_ERROR, "CTsReader::SetFileName failed.");
    return false;
  }
  if (m_fileReader->OpenFile() != true)
  {
    kodi::Log(ADDON_LOG_ERROR, "CTsReader::OpenFile failed.");
    return false;
  }
  m_fileReader->SetFilePointer(0LL, SEEK_SET);

  return true;
}

bool CTsReader::Read(unsigned char* pbData, unsigned long lDataLength, unsigned long* dwReadBytes)
{
#if defined(TARGET_WINDOWS)
  LARGE_INTEGER liFrequency;
  LARGE_INTEGER liCurrent;
  LARGE_INTEGER liLast;
#endif
  if (m_fileReader)
  {
#if defined(TARGET_WINDOWS)
    // Save the performance counter frequency for later use.
    if (!QueryPerformanceFrequency(&liFrequency))
      kodi::Log(ADDON_LOG_ERROR, "QPF() failed with error %d\n", GetLastError());

    if (!QueryPerformanceCounter(&liCurrent))
      kodi::Log(ADDON_LOG_ERROR, "QPC() failed with error %d\n", GetLastError());
    liLast = liCurrent;
#endif

    long rc = m_fileReader->Read(pbData, lDataLength, dwReadBytes);

#if defined(TARGET_WINDOWS)
    if (!QueryPerformanceCounter(&liCurrent))
      kodi::Log(ADDON_LOG_ERROR, "QPC() failed with error %d\n", GetLastError());

    // Convert difference in performance counter values to nanoseconds.
    liDelta.QuadPart += (((liCurrent.QuadPart - liLast.QuadPart) * 1000000) / liFrequency.QuadPart);
    liCount.QuadPart++;
#endif
    return rc;
  }

  dwReadBytes = 0;
  return 1;
}

void CTsReader::Close()
{
  if (m_fileReader)
  {
    m_fileReader->CloseFile();
    SafeDelete(m_fileReader);
  }
}

int64_t CTsReader::SetFilePointer(int64_t llDistanceToMove, unsigned long dwMoveMethod)
{
  return m_fileReader->SetFilePointer(llDistanceToMove, dwMoveMethod);
}

int64_t CTsReader::GetFileSize()
{
  return m_fileReader->GetFileSize();
}

int64_t CTsReader::GetFilePointer()
{
  return m_fileReader->GetFilePointer();
}

void CTsReader::OnZap(void)
{
  m_fileReader->OnZap();
}

#if defined(TARGET_WINDOWS)
long long CTsReader::sigmaTime()
{
  return liDelta.QuadPart;
}
long long CTsReader::sigmaCount()
{
  return liCount.QuadPart;
}
#endif
} // namespace ArgusTV
