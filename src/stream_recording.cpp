/*
 *  Copyright (C) 2020 Team Kodi (https://kodi.tv)
 *  Copyright (C) 2010-2012 Marcel Groothuis, Fred Hoogduin
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "stream_recording.h"

#include "channel.h"
#include "lib/tsreader/TSReader.h"
#include "pvrclient-argustv.h"
#include "utils.h"

#include <chrono>
#include <kodi/General.h>
#include <thread>

CStreamRecording::CStreamRecording(const CArgusTVAddon& base,
                         KODI_HANDLE instance,
                         const std::string& kodiVersion)
  : kodi::addon::CInstanceInputStream(instance), m_base(base)
{
}

void CStreamRecording::GetCapabilities(INPUTSTREAM_CAPABILITIES& capabilities)
{
  capabilities.m_mask |= INPUTSTREAM_CAPABILITIES::SUPPORTS_SEEK;
  capabilities.m_mask |= INPUTSTREAM_CAPABILITIES::SUPPORTS_PAUSE;
}

bool CStreamRecording::Open(INPUTSTREAM& props)
{
  cPVRClientArgusTV* client = nullptr;
  std::string recordingId;

  for (size_t i = 0; i < props.m_nCountInfoValues; ++i)
  {
    if (props.m_ListItemProperties[i].m_strKey == std::string("pvr.argustv.process_class"))
    {
      client =
          reinterpret_cast<cPVRClientArgusTV*>(atoll(props.m_ListItemProperties[i].m_strValue));
    }
    else if (props.m_ListItemProperties[i].m_strKey == std::string("pvr.argustv.recording_id"))
    {
      recordingId = props.m_ListItemProperties[i].m_strValue;
    }
  }

  if (client == nullptr || recordingId.empty())
    return false;

  m_client = client;
  std::string UNCname;

  if (!m_client->FindRecEntry(recordingId, UNCname))
    return false;

  kodi::Log(ADDON_LOG_DEBUG, "->OpenRecordedStream(%s)", UNCname.c_str());

  if (m_tsreader != nullptr)
  {
    kodi::Log(ADDON_LOG_DEBUG, "Close existing TsReader...");
    m_tsreader->Close();
    delete m_tsreader;
  }
  m_tsreader = new ArgusTV::CTsReader();
  if (!m_tsreader->Open(UNCname))
  {
    delete m_tsreader;
    m_tsreader = nullptr;
    return false;
  }

  return true;
}

void CStreamRecording::Close()
{
  kodi::Log(ADDON_LOG_DEBUG, "->CloseRecordedStream()");

  if (!m_client)
    return;

  if (m_tsreader)
  {
    kodi::Log(ADDON_LOG_DEBUG, "Close TsReader");
    m_tsreader->Close();
    delete m_tsreader;
    m_tsreader = nullptr;
  }

  m_client = nullptr;
}

int CStreamRecording::ReadStream(uint8_t* buffer, unsigned int bufferSize)
{
  unsigned long read_done = 0;

  // kodi::Log(ADDON_LOG_DEBUG, "->ReadRecordedStream(buf_size=%i)", iBufferSize);
  if (!m_tsreader)
    return -1;

  if (!m_tsreader->Read(buffer, bufferSize, &read_done))
  {
    kodi::Log(ADDON_LOG_INFO, "ReadRecordedStream requested %d but only read %d bytes.",
              bufferSize, read_done);
  }
  return read_done;
}

int64_t CStreamRecording::SeekStream(int64_t position, int whence)
{
  if (!m_tsreader)
  {
    return -1;
  }
  if (whence == 0x10) // Check seek possible
  {
    return 1;
  }
  if (position == 0 && whence == SEEK_CUR)
  {
    return m_tsreader->GetFilePointer();
  }
  return m_tsreader->SetFilePointer(position, whence);
}

int64_t CStreamRecording::LengthStream()
{
  if (!m_tsreader)
  {
    return -1;
  }
  return m_tsreader->GetFileSize();
}

bool CStreamRecording::IsRealTimeStream()
{
  return false;
}
