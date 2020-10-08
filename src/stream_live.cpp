/*
 *  Copyright (C) 2020 Team Kodi (https://kodi.tv)
 *  Copyright (C) 2010-2012 Marcel Groothuis, Fred Hoogduin
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "stream_live.h"

#include "channel.h"
#include "lib/tsreader/TSReader.h"
#include "pvrclient-argustv.h"
#include "utils.h"

#include <chrono>
#include <kodi/General.h>
#include <thread>

CStreamLive::CStreamLive(const CArgusTVAddon& base,
                         KODI_HANDLE instance,
                         const std::string& kodiVersion)
  : kodi::addon::CInstanceInputStream(instance), m_base(base)
{
}

bool CStreamLive::Open(INPUTSTREAM& props)
{
  cPVRClientArgusTV* client = nullptr;
  int channelUniqueId = -1;

  for (size_t i = 0; i < props.m_nCountInfoValues; ++i)
  {
    if (props.m_ListItemProperties[i].m_strKey == std::string("pvr.argustv.process_class"))
    {
      client =
          reinterpret_cast<cPVRClientArgusTV*>(atoll(props.m_ListItemProperties[i].m_strValue));
    }
    else if (props.m_ListItemProperties[i].m_strKey == std::string("pvr.argustv.channel_unique_id"))
    {
      channelUniqueId = atoi(props.m_ListItemProperties[i].m_strValue);
    }
  }

  if (client == nullptr || channelUniqueId == -1)
    return false;

  m_client = client;

  kodi::Log(ADDON_LOG_DEBUG, "->_OpenLiveStream(%i)", channelUniqueId);

  if (channelUniqueId == m_iCurrentChannel)
  {
    kodi::Log(ADDON_LOG_INFO,
              "New channel uid equal to the already streaming channel. Skipping re-tune.");
    return true;
  }

  m_iCurrentChannel =
      -1; // make sure that it is not a valid channel nr in case it will fail lateron

  cChannel* channel = m_client->FetchChannel(channelUniqueId);

  if (channel)
  {
    std::string filename;
    kodi::Log(ADDON_LOG_INFO, "Tune Kodi channel: %i", channelUniqueId);
    kodi::Log(ADDON_LOG_INFO, "Corresponding ARGUS TV channel: %s", channel->Guid().c_str());

    int retval = m_client->GetRPC().TuneLiveStream(channel->Guid(), channel->Type(),
                                                   channel->Name(), filename);
    if (retval == m_client->GetRPC().NoReTunePossible)
    {
      // Ok, we can't re-tune with the current live stream still running
      // So stop it and re-try
      Close();
      kodi::Log(ADDON_LOG_INFO, "Re-Tune Kodi channel: %i", channelUniqueId);
      retval = m_client->GetRPC().TuneLiveStream(channel->Guid(), channel->Type(), channel->Name(),
                                                 filename);
    }

    if (retval != E_SUCCESS)
    {
      switch (retval)
      {
        case CArgusTV::NoFreeCardFound:
          kodi::Log(ADDON_LOG_INFO, "No free tuner found.");
          kodi::QueueNotification(QUEUE_ERROR, "", "No free tuner found!");
          return false;
        case CArgusTV::IsScrambled:
          kodi::Log(ADDON_LOG_INFO, "Scrambled channel.");
          kodi::QueueNotification(QUEUE_ERROR, "", "Scrambled channel!");
          return false;
        case CArgusTV::ChannelTuneFailed:
          kodi::Log(ADDON_LOG_INFO, "Tuning failed.");
          kodi::QueueNotification(QUEUE_ERROR, "", "Tuning failed!");
          return false;
        default:
          kodi::Log(ADDON_LOG_ERROR, "Tuning failed, unknown error");
          kodi::QueueNotification(QUEUE_ERROR, "", "Unknown error!");
          return false;
      }
    }

    filename = ToCIFS(filename);
    InsertUser(m_base, filename);

    if (retval != E_SUCCESS || filename.empty())
    {
      kodi::Log(ADDON_LOG_ERROR, "Could not start the timeshift for channel %i (%s)",
                channelUniqueId, channel->Guid().c_str());
      Close();
      return false;
    }

    // reset the signal quality poll interval after tuning
    m_client->ResetSignalQuality();

    kodi::Log(ADDON_LOG_INFO, "Live stream file: %s", filename.c_str());
    m_bTimeShiftStarted = true;
    m_iCurrentChannel = channelUniqueId;
    m_client->KeepAlive()->StartThread();

#if defined(ATV_DUMPTS)
    if (ofd != -1)
      close(ofd);
    strncpy(ofn, "/tmp/atv.XXXXXX", sizeof(ofn));
    if ((ofd = mkostemp(ofn, O_CREAT | O_TRUNC)) == -1)
    {
      kodi::Log(ADDON_LOG_ERROR, "couldn't open dumpfile %s (error %d: %s).", ofn, errno,
                strerror(errno));
    }
    else
    {
      kodi::Log(ADDON_LOG_INFO, "opened dumpfile %s.", ofn);
    }
#endif

    if (m_tsreader != nullptr)
    {
      //kodi::Log(ADDON_LOG_DEBUG, "Re-using existing TsReader...");
      //std::this_thread::sleep_for(std::chrono::milliseconds(5000));
      //m_tsreader->OnZap();
      kodi::Log(ADDON_LOG_DEBUG, "Close existing and open new TsReader...");
      m_tsreader->Close();
      delete m_tsreader;
    }
    // Open Timeshift buffer
    // TODO: rtsp support
    m_tsreader = new ArgusTV::CTsReader();
    kodi::Log(ADDON_LOG_DEBUG, "Open TsReader");

    m_tsreader->Open(filename);
    m_tsreader->OnZap();
    kodi::Log(ADDON_LOG_DEBUG, "Delaying %ld milliseconds.", m_base.GetSettings().TuneDelay());
    std::this_thread::sleep_for(std::chrono::milliseconds(m_base.GetSettings().TuneDelay()));
    return true;
  }
  else
  {
    kodi::Log(ADDON_LOG_ERROR, "Could not get ARGUS TV channel guid for channel %i.",
              channelUniqueId);
    kodi::QueueNotification(QUEUE_ERROR, "", "Kodi Channel to GUID");
  }

  Close();
  return false;
}

void CStreamLive::Close()
{
  kodi::Log(ADDON_LOG_INFO, "CloseLiveStream");

  if (!m_client)
    return;

  m_client->KeepAlive()->StopThread();

#if defined(ATV_DUMPTS)
  if (ofd != -1)
  {
    if (close(ofd) == -1)
    {
      kodi::Log(ADDON_LOG_ERROR, "couldn't close dumpfile %s (error %d: %s).", ofn, errno,
                strerror(errno));
    }
    ofd = -1;
  }
#endif

  if (m_bTimeShiftStarted)
  {
    if (m_tsreader)
    {
      kodi::Log(ADDON_LOG_DEBUG, "Close TsReader");
      m_tsreader->Close();
#if defined(TARGET_WINDOWS)
      kodi::Log(ADDON_LOG_DEBUG, "ReadLiveStream: %I64d calls took %I64d nanoseconds.",
                m_tsreader->sigmaCount(), m_tsreader->sigmaTime());
#endif
      delete m_tsreader;
      m_tsreader = nullptr;
    }
    m_client->GetRPC().StopLiveStream();
    m_bTimeShiftStarted = false;
    m_iCurrentChannel = -1;
  }
  else
  {
    kodi::Log(ADDON_LOG_DEBUG, "CloseLiveStream: Nothing to do.");
  }

  m_client = nullptr;
}

int CStreamLive::ReadStream(uint8_t* buffer, unsigned int bufferSize)
{
  unsigned long read_wanted = bufferSize;
  unsigned long read_done = 0;
  static int read_timeouts = 0;
  unsigned char* bufptr = buffer;

  // kodi::Log(ADDON_LOG_DEBUG, "->ReadLiveStream(buf_size=%i)", bufferSize);
  if (!m_tsreader)
    return -1;

  while (read_done < (unsigned long)bufferSize)
  {
    read_wanted = bufferSize - read_done;

    if (!m_tsreader->Read(bufptr, read_wanted, &read_wanted))
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(400));
      read_timeouts++;
      kodi::Log(ADDON_LOG_INFO, "ReadLiveStream requested %d but only read %d bytes.", bufferSize,
                read_wanted);
      return read_wanted;
    }
    read_done += read_wanted;

    if (read_done < (unsigned long)bufferSize)
    {
      if (read_timeouts > 25)
      {
        kodi::Log(ADDON_LOG_INFO, "No data in 1 second");
        read_timeouts = 0;
        return read_done;
      }
      bufptr += read_wanted;
      read_timeouts++;
      std::this_thread::sleep_for(std::chrono::milliseconds(40));
    }
  }
#if defined(ATV_DUMPTS)
  if (write(ofd, buffer, read_done) < 0)
  {
    kodi::Log(ADDON_LOG_ERROR, "couldn't write %d bytes to dumpfile %s (error %d: %s).", read_done,
              ofn, errno, strerror(errno));
  }
#endif
  // kodi::Log(ADDON_LOG_DEBUG, "ReadLiveStream(buf_size=%i), %d timeouts", bufferSize, read_timeouts);
  read_timeouts = 0;
  return read_done;
}

int64_t CStreamLive::SeekStream(int64_t position, int whence)
{
  kodi::Log(ADDON_LOG_DEBUG, "SeekLiveStream (%lld, %i).", position, whence);
  if (!m_tsreader)
  {
    return -1;
  }
  return m_tsreader->SetFilePointer(position, whence);
}

int64_t CStreamLive::LengthStream()
{
  if (!m_tsreader)
  {
    return -1;
  }
  return m_tsreader->GetFileSize();
}

bool CStreamLive::IsRealTimeStream()
{
  return true;
}
