/*
 *      Copyright (C) 2013 Anton Fedchin
 *      http://github.com/afedchin/xbmc-addon-iptvsimple/
 *
 *      Copyright (C) 2011 Pulse-Eight
 *      http://www.pulse-eight.com/
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301  USA
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include <sstream>
#include <string>
#include <fstream>
#include <map>
#include "zlib.h"
#include "rapidxml/rapidxml.hpp"
#include "PVRIptvData.h"

#define M3U_START_MARKER        "#EXTM3U"
#define M3U_INFO_MARKER         "#EXTINF"
#define TVG_INFO_ID_MARKER      "tvg-id="
#define TVG_INFO_NAME_MARKER    "tvg-name="
#define TVG_INFO_LOGO_MARKER    "tvg-logo="
#define TVG_INFO_SHIFT_MARKER   "tvg-shift="
#define GROUP_NAME_MARKER       "group-title="
#define RADIO_MARKER            "radio="
#define CHANNEL_LOGO_EXTENSION  ".png"
#define SECONDS_IN_DAY          86400

using namespace std;
using namespace ADDON;
using namespace rapidxml;

template<class Ch>
inline bool GetNodeValue(const xml_node<Ch> * pRootNode, const char* strTag, CStdString& strStringValue)
{
  xml_node<Ch> *pChildNode = pRootNode->first_node(strTag);
  if (pChildNode == NULL)
  {
    return false;
  }
  strStringValue = pChildNode->value();
  return true;
}

template<class Ch>
inline bool GetAttributeValue(const xml_node<Ch> * pNode, const char* strAttributeName, CStdString& strStringValue)
{
  xml_attribute<Ch> *pAttribute = pNode->first_attribute(strAttributeName);
  if (pAttribute == NULL)
  {
    return false;
  }
  strStringValue = pAttribute->value();
  return true;
}

PVRIptvData::PVRIptvData()
{
  m_iLastStart    = 0;
  m_iLastEnd      = 0;
  m_iStartNumber  = 1;
  m_bEGPLoaded = false;
  LoadSettings();

  if (LoadPlayList())
  {
    XBMC->QueueNotification(QUEUE_INFO, "%d channels loaded.", m_channels.size());
  }
}

void *PVRIptvData::Process(void)
{
  return NULL;
}

PVRIptvData::~PVRIptvData(void)
{
  m_sources.clear();
  m_channels.clear();
  m_groups.clear();
  m_epg.clear();
}

void PVRIptvData::LoadSettings() {
  const char   *fmt = "%s_%i";
  float         fShift;
  char          nameBuffer[128];
  char          valueBuffer[1024];
  int           iPathType = 0;
  int           iSourceCount = 1;

  // Get number of configured sources
  if (!XBMC->GetSetting("sourceCount", &iSourceCount))
  {
    XBMC->Log(LOG_NOTICE, "No sources configured");
  }
  if (iSourceCount <= 0)
  {
    iSourceCount = 1;
  }
  else
  {
    iSourceCount ++;
  }

  // Get start number
  if (!XBMC->GetSetting("startNum", &m_iStartNumber))
  {
    m_iStartNumber = 1;
  }

  for (int i = 0; i <= iSourceCount; i++)
  {
    PVRIptvSource source;
    source.iId = i;

    // Get M3U settings
    if (i != 0) // Source 0 has only default settings for EPG and logos.
    {
      sprintf(nameBuffer, fmt, "m3uPathType", i);
      if (!XBMC->GetSetting(nameBuffer, &iPathType))
      {
        iPathType = 1;
      }
      if (iPathType)
      {
        sprintf(nameBuffer, fmt, "m3uUrl", i);
        if (XBMC->GetSetting(nameBuffer, &valueBuffer))
        {
          source.strM3UPath = valueBuffer;
        }
        sprintf(nameBuffer, fmt, "m3uCache", i);
        if (!XBMC->GetSetting(nameBuffer, &(source.bCacheM3U)))
        {
          source.bCacheM3U = true;
        }
      }
      else
      {
        sprintf(nameBuffer, fmt, "m3uPath", i);
        if (XBMC->GetSetting(nameBuffer, &valueBuffer))
        {
          source.strM3UPath = valueBuffer;
        }
        source.bCacheM3U = false;
      }
    }

    // Get name formats
    sprintf(nameBuffer, fmt, "groupNameFormat", i);
    if (XBMC->GetSetting(nameBuffer, &valueBuffer))
    {
      source.strGroupNameFormat = valueBuffer;
    }
    else
    {
      source.strGroupNameFormat = "%s";
    }
    sprintf(nameBuffer, fmt, "channelNameFormat", i);
    if (XBMC->GetSetting(nameBuffer, &valueBuffer))
    {
      source.strChannelNameFormat = valueBuffer;
    }
    else
    {
      source.strChannelNameFormat = "%s";
    }
    sprintf(nameBuffer, fmt, "logoFileNameFormat", i);
    if (XBMC->GetSetting(nameBuffer, &valueBuffer))
    {
      source.strLogoFileNameFormat = valueBuffer;
    }
    else
    {
      source.strLogoFileNameFormat = "%s";
    }

    // Get EPG settings
    sprintf(nameBuffer, fmt, "epgPathType", i);
    if (!XBMC->GetSetting(nameBuffer, &iPathType))
    {
      iPathType = 1;
    }
    if (iPathType)
    {
      sprintf(nameBuffer, fmt, "epgUrl", i);
      if (XBMC->GetSetting(nameBuffer, &valueBuffer))
      {
        source.strTvgPath = valueBuffer;
      }
      sprintf(nameBuffer, fmt, "epgCache", i);
      if (!XBMC->GetSetting(nameBuffer, &(source.bCacheEPG)))
      {
        source.bCacheEPG = true;
      }
    }
    else
    {
      sprintf(nameBuffer, fmt, "epgPath", i);
      if (XBMC->GetSetting(nameBuffer, &valueBuffer))
      {
        source.strTvgPath = valueBuffer;
      }
      source.bCacheEPG = false;
    }
    sprintf(nameBuffer, fmt, "epgTimeShift", i);
    if (XBMC->GetSetting(nameBuffer, &fShift))
    {
      source.iEPGTimeShift = (int)(fShift * 3600.0); // hours to seconds
    }
    else
    {
      source.iEPGTimeShift = 0;
    }
    sprintf(nameBuffer, fmt, "epgTSOverride", i);
    if (!XBMC->GetSetting(nameBuffer, &(source.bTSOverride)))
    {
      source.bTSOverride = false;
    }

    // Get logos settings
    sprintf(nameBuffer, fmt, "logoPathType", i);
    if (!XBMC->GetSetting(nameBuffer, &iPathType))
    {
      iPathType = 1;
    }
    if (iPathType)
    {
      sprintf(nameBuffer, fmt, "logoBaseUrl", i);
    }
    else
    {
      sprintf(nameBuffer, fmt, "logoPath", i);
    }
    if (XBMC->GetSetting(nameBuffer, &valueBuffer))
    {
      source.strLogoPath = valueBuffer;
    }
    if ((i == 0) && (source.strLogoPath == ""))
    {
      source.strLogoPath = GetClientFilePath("icons/");
    }

    m_sources.push_back(source);
  }
}

bool PVRIptvData::LoadEPG(time_t iStart, time_t iEnd)
{
  bool loaded      = false;
  int iBroadCastId = 0;

  // clear previously loaded epg
  if (m_epg.size() > 0)
  {
    m_epg.clear();
  }

  for (int i = 0; i < m_sources.size(); i++)
  {
    if (LoadEPG(iStart, iEnd, m_sources.at(i), iBroadCastId))
    {
      loaded = true;
    }
  }

  m_bEGPLoaded = true;
  return loaded;
}

bool PVRIptvData::LoadEPG(time_t iStart, time_t iEnd, PVRIptvSource &source, int &iBroadCastId)
{
  if (source.strTvgPath.IsEmpty())
  {
    return false;
  }

  std::string data;
  std::string decompressed;
  int iReaded = 0;
  CStdString strCachedName;
  strCachedName.Fmt(TVG_FILE_NAME_FORMAT, source.iId);

  int iCount = 0;
  while(iCount < 3) // max 3 tries
  {
    if ((iReaded = GetCachedFileContents(strCachedName, source.strTvgPath, data, source.bCacheEPG)) != 0)
    {
      break;
    }
    XBMC->Log(LOG_ERROR, "Unable to load EPG file '%s':  file is missing or empty. :%dth try.", source.strTvgPath.c_str(), ++iCount);
    if (iCount < 3)
    {
      usleep(2 * 1000 * 1000); // sleep 2 sec before next try.
    }
  }
  
  if (iReaded == 0)
  {
    XBMC->Log(LOG_ERROR, "Unable to load EPG file '%s':  file is missing or empty. After %d tries.", source.strTvgPath.c_str(), iCount);
    m_iLastStart = iStart;
    m_iLastEnd = iEnd;
    return false;
  }

  char * buffer;

  // gzip packed
  if (data[0] == '\x1F' && data[1] == '\x8B' && data[2] == '\x08') 
  {
    if (!GzipInflate(data, decompressed))
    {
      XBMC->Log(LOG_ERROR, "Invalid EPG file '%s': unable to decompress file.", source.strTvgPath.c_str());
      return false;
    }
    buffer = &(decompressed[0]);
  }
  else
  {
    buffer = &(data[0]);
  }

  // xml should starts with '<?xml'
  if (buffer[0] != '\x3C' || buffer[1] != '\x3F' || buffer[2] != '\x78' ||
      buffer[3] != '\x6D' || buffer[4] != '\x6C')
  {
    // check for BOM
    if (buffer[0] != '\xEF' || buffer[1] != '\xBB' || buffer[2] != '\xBF')
    {
      // check for tar archive
      if (strcmp(buffer + 0x101, "ustar") || strcmp(buffer + 0x101, "GNUtar"))
      {
        buffer += 0x200; // RECORDSIZE = 512
      }
      else
      {
        XBMC->Log(LOG_ERROR, "Invalid EPG file '%s': unable to parse file.", source.strTvgPath.c_str());
        return false;
      }
    }
  }

  xml_document<> xmlDoc;
  try 
  {
    xmlDoc.parse<0>(buffer);
  } 
  catch(parse_error &p)
  {
    XBMC->Log(LOG_ERROR, "Unable parse EPG XML: %s", p.what());
    return false;
  }

  xml_node<> *pRootElement = xmlDoc.first_node("tv");
  if (!pRootElement)
  {
    XBMC->Log(LOG_ERROR, "Invalid EPG XML: no <tv> tag found");
    return false;
  }

  xml_node<> *pChannelNode = NULL;
  for(pChannelNode = pRootElement->first_node("channel"); pChannelNode; pChannelNode = pChannelNode->next_sibling("channel"))
  {
    CStdString strName;
    CStdString strId;
    if(!GetAttributeValue(pChannelNode, "id", strId))
    {
      continue;
    }
    GetNodeValue(pChannelNode, "display-name", strName);

    if (FindChannel(strId, strName) == NULL)
    {
      continue;
    }

    PVRIptvEpgChannel epgChannel;
    epgChannel.iSourceId = source.iId;
    epgChannel.strId = strId;
    epgChannel.strName = strName;

    m_epg.push_back(epgChannel);
  }

  if (m_epg.size() == 0) 
  {
    XBMC->Log(LOG_ERROR, "EPG channels not found.");
    return false;
  }
  
  int iMinShiftTime = source.iEPGTimeShift;
  int iMaxShiftTime = source.iEPGTimeShift;

  if (!source.bTSOverride)
  {
    iMinShiftTime = SECONDS_IN_DAY;
    iMaxShiftTime = -SECONDS_IN_DAY;

    vector<PVRIptvChannel>::iterator it;
    for (it = m_channels.begin(); it < m_channels.end(); it++)
    {
      if (it->iTvgShift + source.iEPGTimeShift < iMinShiftTime)
        iMinShiftTime = it->iTvgShift + source.iEPGTimeShift;
      if (it->iTvgShift + source.iEPGTimeShift > iMaxShiftTime)
        iMaxShiftTime = it->iTvgShift + source.iEPGTimeShift;
    }
  }

  CStdString strEmpty = "";
  PVRIptvEpgChannel *epg = NULL;
  for(pChannelNode = pRootElement->first_node("programme"); pChannelNode; pChannelNode = pChannelNode->next_sibling("programme"))
  {
    CStdString strId;
    if (!GetAttributeValue(pChannelNode, "channel", strId))
      continue;

    if (epg == NULL || epg->strId != strId) 
    {
      if ((epg = FindEpg(strId, source.iId)) == NULL)
        continue;
    }

    CStdString strStart;
    CStdString strStop;

    if (!GetAttributeValue(pChannelNode, "start", strStart) || !GetAttributeValue(pChannelNode, "stop", strStop)) 
    {
      continue;
    }

    int iTmpStart = ParseDateTime(strStart);
    int iTmpEnd = ParseDateTime(strStop);

    if ((iTmpEnd + iMaxShiftTime < iStart) || (iTmpStart + iMinShiftTime > iEnd))
    {
      continue;
    }

    CStdString strTitle;
    CStdString strCategory;
    CStdString strDesc;

    GetNodeValue(pChannelNode, "title", strTitle);
    GetNodeValue(pChannelNode, "category", strCategory);
    GetNodeValue(pChannelNode, "desc", strDesc);

    CStdString strIconPath;
    xml_node<> *pIconNode = pChannelNode->first_node("icon");
    if (pIconNode != NULL)
    {
      if (!GetAttributeValue(pIconNode, "src", strIconPath)) 
      {
        strIconPath = "";
      }
    }


    PVRIptvEpgEntry entry;
    entry.iBroadcastId    = ++iBroadCastId;
    entry.iGenreType      = 0;
    entry.iGenreSubType   = 0;
    entry.strTitle        = strTitle;
    entry.strPlot         = strDesc;
    entry.strPlotOutline  = "";
    entry.strIconPath     = strIconPath;
    entry.startTime       = iTmpStart;
    entry.endTime         = iTmpEnd;
    entry.strGenreString  = strCategory;

    epg->epg.push_back(entry);
  }

  xmlDoc.clear();
  XBMC->Log(LOG_NOTICE, "EPG Loaded: %s", source.strTvgPath.c_str());

  return true;
}

bool PVRIptvData::LoadPlayList(void)
{
  bool loaded        = false;
  int iChannelIndex  = 0;
  int iChannelNum    = m_iStartNumber;
  int iUniqueGroupId = 0;

  for (int i = 1; i < m_sources.size(); i++)
  {
    if (LoadPlayList(m_sources.at(i), iChannelIndex, iChannelNum, iUniqueGroupId))
    {
      loaded = true;
    }
  }

  XBMC->Log(LOG_NOTICE, "Loaded %d channels and %d groups.", m_channels.size(), m_groups.size());
  ApplyChannelsLogos();
  return loaded;
}
bool PVRIptvData::LoadPlayList(PVRIptvSource &source, int &iChannelIndex, int &iChannelNum, int &iUniqueGroupId)
{
  if (source.strM3UPath.IsEmpty())
  {
    XBMC->Log(LOG_NOTICE, "Playlist file path for source %i is not configured. Channels not loaded.", source.iId);
    return false;
  }
  else
  {
    XBMC->Log(LOG_NOTICE, "Loading playlist: %s", source.strM3UPath.c_str());
  }

  CStdString strFormat;
  CStdString strPlaylistContent;

  strFormat.Fmt(M3U_FILE_NAME_FORMAT, source.iId);
  if (!GetCachedFileContents(strFormat.c_str(), source.strM3UPath, strPlaylistContent, source.bCacheM3U))
  {
    XBMC->Log(LOG_ERROR, "Unable to load playlist file '%s':  file is missing or empty.", source.strM3UPath.c_str());
    return false;
  }

  std::stringstream stream(strPlaylistContent);

  /* load channels */
  bool bFirst = true;
  int iCurrentGroupId   = 0;
  int iEPGTimeShift     = 0;

  PVRIptvChannel tmpChannel;
  tmpChannel.strTvgId       = "";
  tmpChannel.strChannelName = "";
  tmpChannel.strTvgName     = "";
  tmpChannel.strTvgLogo     = "";
  tmpChannel.iTvgShift      = 0;

  char szLine[1024];
  while(stream.getline(szLine, 1024)) 
  {
  
    CStdString strLine = "";
    strLine.append(szLine);
    strLine.TrimRight(" \t\r\n");
    strLine.TrimLeft(" \t");

    if (strLine.IsEmpty())
    {
      continue;
    }

    if (bFirst) 
    {
      bFirst = false;
      if (strLine.Left(3) == "\xEF\xBB\xBF")
      {
        strLine.Delete(0, 3);
      }
      if (strLine.Left((int)strlen(M3U_START_MARKER)) == M3U_START_MARKER) 
      {
        double fTvgShift = atof(ReadMarkerValue(strLine, TVG_INFO_SHIFT_MARKER));
        iEPGTimeShift = (int) (fTvgShift * 3600.0);
        continue;
      }
      else
      {
        break;
      }
    }

    if (strLine.Left((int)strlen(M3U_INFO_MARKER)) == M3U_INFO_MARKER) 
    {
      bool        bRadio       = false;
      double      fTvgShift    = 0;
      CStdString  strChnlName  = "";
      CStdString  strTvgId     = "";
      CStdString  strTvgName   = "";
      CStdString  strTvgLogo   = "";
      CStdString  strGroupName = "";
      CStdString  strRadio     = "";

      // parse line
      int iColon = (int)strLine.Find(':');
      int iComma = (int)strLine.ReverseFind(',');
      if (iColon >= 0 && iComma >= 0 && iComma > iColon) 
      {
        // parse name
        iComma++;
        strChnlName = strLine.Right((int)strLine.size() - iComma).Trim();
        tmpChannel.strChannelName = XBMC->UnknownToUTF8(strChnlName);

        // parse info
        CStdString strInfoLine = strLine.Mid(++iColon, --iComma - iColon);

        strTvgId      = ReadMarkerValue(strInfoLine, TVG_INFO_ID_MARKER);
        strTvgName    = ReadMarkerValue(strInfoLine, TVG_INFO_NAME_MARKER);
        strTvgLogo    = ReadMarkerValue(strInfoLine, TVG_INFO_LOGO_MARKER);
        strRadio      = ReadMarkerValue(strInfoLine, RADIO_MARKER);
        fTvgShift     = atof(ReadMarkerValue(strInfoLine, TVG_INFO_SHIFT_MARKER));
        strGroupName  = ReadMarkerValue(strInfoLine, GROUP_NAME_MARKER);
        strFormat.Fmt(source.strGroupNameFormat, XBMC->UnknownToUTF8(strGroupName));
        strGroupName = strFormat;
        strFormat.Fmt(m_sources.at(0).strGroupNameFormat, strGroupName.c_str());
        strGroupName = strFormat;

        if (strTvgId.IsEmpty())
        {
          char buff[255];
          sprintf(buff, "%d", atoi(strInfoLine));
          strTvgId.append(buff);
        }
        if (strTvgLogo.IsEmpty())
        {
          strTvgLogo = strChnlName;
        }

        bRadio                = !strRadio.CompareNoCase("true");
        tmpChannel.strTvgId   = strTvgId;
        tmpChannel.strTvgName = XBMC->UnknownToUTF8(strTvgName);
        tmpChannel.strTvgLogo = XBMC->UnknownToUTF8(strTvgLogo);
        tmpChannel.iTvgShift  = (int)(fTvgShift * 3600.0);
        tmpChannel.bRadio     = bRadio;

        if (tmpChannel.iTvgShift == 0 && iEPGTimeShift != 0)
        {
          tmpChannel.iTvgShift = iEPGTimeShift;
        }

        if (!strGroupName.IsEmpty())
        {
          PVRIptvChannelGroup * pGroup;
          if ((pGroup = FindGroup(strGroupName)) == NULL)
          {
            PVRIptvChannelGroup group;
            group.strGroupName = strGroupName;
            group.iGroupId = ++iUniqueGroupId;
            group.bRadio = bRadio;

            m_groups.push_back(group);
            iCurrentGroupId = iUniqueGroupId;
          }
          else
          {
            iCurrentGroupId = pGroup->iGroupId;
          }
        }
      }
    } 
    else if (strLine[0] != '#')
    {
      PVRIptvChannel channel;
      channel.iUniqueId         = GetChannelId(tmpChannel.strChannelName.c_str(), strLine);
      channel.iSourceId         = source.iId;
      channel.iChannelNumber    = iChannelNum++;
      channel.strTvgId          = tmpChannel.strTvgId;
      channel.strChannelName    = tmpChannel.strChannelName;
      channel.strTvgName        = tmpChannel.strTvgName;
      channel.strTvgLogo        = tmpChannel.strTvgLogo;
      channel.iTvgShift         = tmpChannel.iTvgShift;
      channel.bRadio            = tmpChannel.bRadio;
      channel.strStreamURL      = strLine;
      channel.iEncryptionSystem = 0;

      strFormat.Fmt(source.strChannelNameFormat, tmpChannel.strChannelName.c_str());
      tmpChannel.strChannelName = strFormat;
      strFormat.Fmt(m_sources.at(0).strChannelNameFormat, tmpChannel.strChannelName.c_str());
      channel.strChannelDisplayName = strFormat;

      if (iCurrentGroupId > 0) 
      {
        channel.bRadio = m_groups.at(iCurrentGroupId - 1).bRadio;
        m_groups.at(iCurrentGroupId - 1).members.push_back(iChannelIndex);
      }

      m_channels.push_back(channel);
      iChannelIndex++;

      tmpChannel.strTvgId       = "";
      tmpChannel.strChannelName = "";
      tmpChannel.strTvgName     = "";
      tmpChannel.strTvgLogo     = "";
      tmpChannel.iTvgShift      = 0;
      tmpChannel.bRadio         = false;
    }
  }
  
  stream.clear();

  if (m_channels.size() == 0)
  {
    XBMC->Log(LOG_ERROR, "Unable to load channels from file '%s':  file is corrupted.", source.strM3UPath.c_str());
    return false;
  }

  return true;
}

int PVRIptvData::GetChannelsAmount(void)
{
  return m_channels.size();
}

PVR_ERROR PVRIptvData::GetChannels(ADDON_HANDLE handle, bool bRadio)
{
  for (unsigned int iChannelPtr = 0; iChannelPtr < m_channels.size(); iChannelPtr++)
  {
    PVRIptvChannel &channel = m_channels.at(iChannelPtr);
    if (channel.bRadio == bRadio)
    {
      PVR_CHANNEL xbmcChannel;
      memset(&xbmcChannel, 0, sizeof(PVR_CHANNEL));

      xbmcChannel.iUniqueId         = channel.iUniqueId;
      xbmcChannel.bIsRadio          = channel.bRadio;
      xbmcChannel.iChannelNumber    = channel.iChannelNumber;
      strncpy(xbmcChannel.strChannelName, channel.strChannelDisplayName.c_str(), sizeof(xbmcChannel.strChannelName) - 1);
      strncpy(xbmcChannel.strStreamURL, channel.strStreamURL.c_str(), sizeof(xbmcChannel.strStreamURL) - 1);
      xbmcChannel.iEncryptionSystem = channel.iEncryptionSystem;
      strncpy(xbmcChannel.strIconPath, channel.strLogoPath.c_str(), sizeof(xbmcChannel.strIconPath) - 1);
      xbmcChannel.bIsHidden         = false;

      PVR->TransferChannelEntry(handle, &xbmcChannel);
    }
  }

  return PVR_ERROR_NO_ERROR;
}

bool PVRIptvData::GetChannel(const PVR_CHANNEL &channel, PVRIptvChannel &myChannel)
{
  for (unsigned int iChannelPtr = 0; iChannelPtr < m_channels.size(); iChannelPtr++)
  {
    PVRIptvChannel &thisChannel = m_channels.at(iChannelPtr);
    if (thisChannel.iUniqueId == (int) channel.iUniqueId)
    {
      myChannel.iUniqueId             = thisChannel.iUniqueId;
      myChannel.bRadio                = thisChannel.bRadio;
      myChannel.iChannelNumber        = thisChannel.iChannelNumber;
      myChannel.iEncryptionSystem     = thisChannel.iEncryptionSystem;
      myChannel.strChannelName        = thisChannel.strChannelName;
      myChannel.strChannelDisplayName = thisChannel.strChannelDisplayName;
      myChannel.strLogoPath           = thisChannel.strLogoPath;
      myChannel.strStreamURL          = thisChannel.strStreamURL;

      return true;
    }
  }

  return false;
}

int PVRIptvData::GetChannelGroupsAmount(void)
{
  return m_groups.size();
}

PVR_ERROR PVRIptvData::GetChannelGroups(ADDON_HANDLE handle, bool bRadio)
{
  for (unsigned int iGroupPtr = 0; iGroupPtr < m_groups.size(); iGroupPtr++)
  {
    PVRIptvChannelGroup &group = m_groups.at(iGroupPtr);
    if (group.bRadio == bRadio)
    {
      PVR_CHANNEL_GROUP xbmcGroup;
      memset(&xbmcGroup, 0, sizeof(PVR_CHANNEL_GROUP));

      xbmcGroup.bIsRadio = bRadio;
      strncpy(xbmcGroup.strGroupName, group.strGroupName.c_str(), sizeof(xbmcGroup.strGroupName) - 1);

      PVR->TransferChannelGroup(handle, &xbmcGroup);
    }
  }

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR PVRIptvData::GetChannelGroupMembers(ADDON_HANDLE handle, const PVR_CHANNEL_GROUP &group)
{
  PVRIptvChannelGroup *myGroup;
  if ((myGroup = FindGroup(group.strGroupName)) != NULL)
  {
    for (unsigned int iPtr = 0; iPtr < myGroup->members.size(); iPtr++)
    {
      int iIndex = myGroup->members.at(iPtr);
      if (iIndex < 0 || iIndex >= (int) m_channels.size())
        continue;

      PVRIptvChannel &channel = m_channels.at(iIndex);
      PVR_CHANNEL_GROUP_MEMBER xbmcGroupMember;
      memset(&xbmcGroupMember, 0, sizeof(PVR_CHANNEL_GROUP_MEMBER));

      strncpy(xbmcGroupMember.strGroupName, group.strGroupName, sizeof(xbmcGroupMember.strGroupName) - 1);
      xbmcGroupMember.iChannelUniqueId = channel.iUniqueId;
      xbmcGroupMember.iChannelNumber   = channel.iChannelNumber;

      PVR->TransferChannelGroupMember(handle, &xbmcGroupMember);
    }
  }

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR PVRIptvData::GetEPGForChannel(ADDON_HANDLE handle, const PVR_CHANNEL &channel, time_t iStart, time_t iEnd)
{
  vector<PVRIptvChannel>::iterator myChannel;
  for (myChannel = m_channels.begin(); myChannel < m_channels.end(); myChannel++)
  {
    if (myChannel->iUniqueId != (int) channel.iUniqueId)
    {
      continue;
    }

    if (!m_bEGPLoaded || iStart > m_iLastStart || iEnd > m_iLastEnd) 
    {
      if (LoadEPG(iStart, iEnd))
      {
        m_iLastStart = iStart;
        m_iLastEnd = iEnd;
      }
    }

    PVRIptvEpgChannel *epg;
    if ((epg = FindEpgForChannel(*myChannel)) == NULL || epg->epg.size() == 0)
    {
      return PVR_ERROR_NO_ERROR;
    }

    PVRIptvSource &source = m_sources.at(myChannel->iSourceId);
    int iShift = source.bTSOverride ? source.iEPGTimeShift : myChannel->iTvgShift + source.iEPGTimeShift;

    vector<PVRIptvEpgEntry>::iterator myTag;
    for (myTag = epg->epg.begin(); myTag < epg->epg.end(); myTag++)
    {
      if ((myTag->endTime + iShift) < iStart) 
        continue;

      EPG_TAG tag;
      memset(&tag, 0, sizeof(EPG_TAG));

      tag.iUniqueBroadcastId  = myTag->iBroadcastId;
      tag.strTitle            = myTag->strTitle.c_str();
      tag.iChannelNumber      = myTag->iChannelId;
      tag.startTime           = myTag->startTime + iShift;
      tag.endTime             = myTag->endTime + iShift;
      tag.strPlotOutline      = myTag->strPlotOutline.c_str();
      tag.strPlot             = myTag->strPlot.c_str();
      tag.strIconPath         = myTag->strIconPath.c_str();
      tag.iGenreType          = EPG_GENRE_USE_STRING;        //myTag.iGenreType;
      tag.iGenreSubType       = 0;                           //myTag.iGenreSubType;
      tag.strGenreDescription = myTag->strGenreString.c_str();

      PVR->TransferEpgEntry(handle, &tag);

      if ((myTag->startTime + iShift) > iEnd)
        break;
    }

    return PVR_ERROR_NO_ERROR;
  }

  return PVR_ERROR_NO_ERROR;
}

int PVRIptvData::GetFileContents(CStdString& url, std::string &strContent)
{
  strContent.clear();
  void* fileHandle = XBMC->OpenFile(url.c_str(), 0);
  if (fileHandle)
  {
    char buffer[1024];
    while (int bytesRead = XBMC->ReadFile(fileHandle, buffer, 1024))
      strContent.append(buffer, bytesRead);
    XBMC->CloseFile(fileHandle);
  }

  return strContent.length();
}

int PVRIptvData::ParseDateTime(CStdString strDate, bool iDateFormat)
{
  struct tm timeinfo;
  memset(&timeinfo, 0, sizeof(tm));

  if (iDateFormat)
  {
    sscanf(strDate, "%04d%02d%02d%02d%02d%02d", &timeinfo.tm_year, &timeinfo.tm_mon, &timeinfo.tm_mday, &timeinfo.tm_hour, &timeinfo.tm_min, &timeinfo.tm_sec);
  }
  else
  {
    sscanf(strDate, "%02d.%02d.%04d%02d:%02d:%02d", &timeinfo.tm_mday, &timeinfo.tm_mon, &timeinfo.tm_year, &timeinfo.tm_hour, &timeinfo.tm_min, &timeinfo.tm_sec);
  }

  timeinfo.tm_mon  -= 1;
  timeinfo.tm_year -= 1900;
  timeinfo.tm_isdst = -1;

  return mktime(&timeinfo);
}

PVRIptvChannel * PVRIptvData::FindChannel(const std::string &strId, const std::string &strName)
{
  CStdString strTvgName = strName;
  strTvgName.Replace(' ', '_');

  vector<PVRIptvChannel>::iterator it;
  for(it = m_channels.begin(); it < m_channels.end(); it++)
  {
    if (it->strTvgId == strId)
    {
      return &*it;
    }
    if (strTvgName == "") 
    {
      continue;
    }
    if (it->strTvgName == strTvgName)
    {
      return &*it;
    }
    if (it->strChannelName == strName)
    {
      return &*it;
    }
  }

  return NULL;
}

PVRIptvChannelGroup * PVRIptvData::FindGroup(const std::string &strName)
{
  vector<PVRIptvChannelGroup>::iterator it;
  for(it = m_groups.begin(); it < m_groups.end(); it++)
  {
    if (it->strGroupName == strName)
    {
      return &*it;
    }
  }

  return NULL;
}

PVRIptvEpgChannel * PVRIptvData::FindEpg(const std::string &strId, int iSourceId)
{
  vector<PVRIptvEpgChannel>::iterator it;
  for(it = m_epg.begin(); it < m_epg.end(); it++)
  {
    if ((it->iSourceId == iSourceId) && (it->strId == strId))
    {
      return &*it;
    }
  }

  return NULL;
}

PVRIptvEpgChannel * PVRIptvData::FindEpgForChannel(PVRIptvChannel &channel)
{
  PVRIptvEpgChannel * epg = NULL;

  vector<PVRIptvEpgChannel>::iterator it;
  for(it = m_epg.begin(); it < m_epg.end(); it++)
  {
    if (it->strId == channel.strTvgId)
    {
      if (it->iSourceId == channel.iSourceId)
      {
        return &*it;
      }
      else if (it->iSourceId == 0)
      {
        epg = &*it;
      }
    }
    CStdString strName = it->strName;
    strName.Replace(' ', '_');
    if (strName == channel.strTvgName
      || it->strName == channel.strTvgName)
    {
      if (it->iSourceId == channel.iSourceId)
      {
        return &*it;
      }
      else if (it->iSourceId == 0)
      {
        epg = &*it;
      }
    }
    if (it->strName == channel.strChannelName)
    {
      if (it->iSourceId == channel.iSourceId)
      {
        return &*it;
      }
      else if (it->iSourceId == 0)
      {
        epg = &*it;
      }
    }
  }

  return epg;
}

/*
 * This method uses zlib to decompress a gzipped file in memory.
 * Author: Andrew Lim Chong Liang
 * http://windrealm.org
 */
bool PVRIptvData::GzipInflate( const std::string& compressedBytes, std::string& uncompressedBytes ) {  

#define HANDLE_CALL_ZLIB(status) {   \
  if(status != Z_OK) {        \
    free(uncomp);             \
    return false;             \
  }                           \
}

  if ( compressedBytes.size() == 0 ) 
  {  
    uncompressedBytes = compressedBytes ;  
    return true ;  
  }  
  
  uncompressedBytes.clear() ;  
  
  unsigned full_length = compressedBytes.size() ;  
  unsigned half_length = compressedBytes.size() / 2;  
  
  unsigned uncompLength = full_length ;  
  char* uncomp = (char*) calloc( sizeof(char), uncompLength );  
  
  z_stream strm;  
  strm.next_in = (Bytef *) compressedBytes.c_str();  
  strm.avail_in = compressedBytes.size() ;  
  strm.total_out = 0;  
  strm.zalloc = Z_NULL;  
  strm.zfree = Z_NULL;  
  
  bool done = false ;  
  
  HANDLE_CALL_ZLIB(inflateInit2(&strm, (16+MAX_WBITS)));
  
  while (!done) 
  {  
    // If our output buffer is too small  
    if (strm.total_out >= uncompLength ) 
    {
      // Increase size of output buffer  
      uncomp = (char *) realloc(uncomp, uncompLength + half_length);
      if (uncomp == NULL)
        return false;
      uncompLength += half_length ;  
    }  
  
    strm.next_out = (Bytef *) (uncomp + strm.total_out);  
    strm.avail_out = uncompLength - strm.total_out;  
  
    // Inflate another chunk.  
    int err = inflate (&strm, Z_SYNC_FLUSH);  
    if (err == Z_STREAM_END) 
      done = true;  
    else if (err != Z_OK)  
    {  
      break;  
    }  
  }  
  
  HANDLE_CALL_ZLIB(inflateEnd (&strm));
  
  for ( size_t i=0; i<strm.total_out; ++i ) 
  {  
    uncompressedBytes += uncomp[ i ];  
  }  

  free( uncomp );  
  return true ;  
}  

int PVRIptvData::GetCachedFileContents(const std::string &strCachedName, const std::string &filePath, 
                                       std::string &strContents, const bool bUseCache /* false */)
{
  bool bNeedReload = false;
  CStdString strCachedPath = GetUserFilePath(strCachedName);
  CStdString strFilePath = filePath;

  // check cached file is exists
  if (bUseCache && XBMC->FileExists(strCachedPath, false)) 
  {
    struct __stat64 statCached;
    struct __stat64 statOrig;

    XBMC->StatFile(strCachedPath, &statCached);
    XBMC->StatFile(strFilePath, &statOrig);

    bNeedReload = statCached.st_mtime < statOrig.st_mtime || statOrig.st_mtime == 0;
  } 
  else 
  {
    bNeedReload = true;
  }

  if (bNeedReload) 
  {
    GetFileContents(strFilePath, strContents);

    // write to cache
    if (bUseCache && strContents.length() > 0) 
    {
      void* fileHandle = XBMC->OpenFileForWrite(strCachedPath, true);
      if (fileHandle)
      {
        XBMC->WriteFile(fileHandle, strContents.c_str(), strContents.length());
        XBMC->CloseFile(fileHandle);
      }
    }
    return strContents.length();
  } 

  return GetFileContents(strCachedPath, strContents);
}

void PVRIptvData::ApplyChannelsLogos()
{
  CStdString fileName;
  vector<PVRIptvChannel>::iterator channel;
  PVRIptvSource defaultSource = m_sources.at(0);

  for(channel = m_channels.begin(); channel < m_channels.end(); channel++)
  {
    PVRIptvSource &source = m_sources.at(channel->iSourceId);

    if (!source.strLogoPath.IsEmpty())
    {
      fileName.Fmt(source.strLogoFileNameFormat, channel->strTvgLogo.c_str());
      channel->strLogoPath = PathCombine(source.strLogoPath, fileName.c_str());
    }
    else
    {
      fileName.Fmt(defaultSource.strLogoFileNameFormat, channel->strTvgLogo.c_str());
      channel->strLogoPath = PathCombine(defaultSource.strLogoPath, fileName.c_str());
    }
  }
}

CStdString PVRIptvData::ReadMarkerValue(std::string &strLine, const char* strMarkerName)
{
  int iMarkerStart = (int) strLine.find(strMarkerName);
  if (iMarkerStart >= 0)
  {
    std::string strMarker = strMarkerName;
    iMarkerStart += strMarker.length();
    if (iMarkerStart < (int)strLine.length())
    {
      char cFind = ' ';
      if (strLine[iMarkerStart] == '"')
      {
        cFind = '"';
        iMarkerStart++;
      }
      int iMarkerEnd = (int)strLine.find(cFind, iMarkerStart);
      if (iMarkerEnd < 0)
      {
        iMarkerEnd = strLine.length();
      }
      return strLine.substr(iMarkerStart, iMarkerEnd - iMarkerStart);
    }
  }

  return std::string("");
}

int PVRIptvData::GetChannelId(const char * strChannelName, const char * strStreamUrl) 
{
  std::string concat(strChannelName);
  concat.append(strStreamUrl);

  const char* strString = concat.c_str();
  int iId = 0;
  int c;
  while (c = *strString++)
    iId = ((iId << 5) + iId) + c; /* iId * 33 + c */

  return abs(iId);
}
