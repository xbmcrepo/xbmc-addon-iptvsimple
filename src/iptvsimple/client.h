#pragma once
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

#include "libXBMC_addon.h"
#include "libXBMC_pvr.h"
#include "libXBMC_gui.h"

#define PVR_CLIENT_VERSION     "2.0.0"
#define M3U_FILE_NAME_FORMAT   "iptv_%i.m3u.cache"
#define TVG_FILE_NAME_FORMAT   "xmltv_%i.xml.cache"

/*!
 * @brief PVR macros for string exchange
 */
#define PVR_STRCPY(dest, source) do { strncpy(dest, source, sizeof(dest)-1); dest[sizeof(dest)-1] = '\0'; } while(0)
#define PVR_STRCLR(dest) memset(dest, 0, sizeof(dest))

extern bool                          m_bCreated;
extern std::string                   g_strUserPath;
extern std::string                   g_strClientPath;
extern ADDON::CHelper_libXBMC_addon *XBMC;
extern CHelper_libXBMC_pvr          *PVR;

extern std::string PathCombine(const std::string &strPath, const std::string &strFileName);
extern std::string GetClientFilePath(const std::string &strFileName);
extern std::string GetUserFilePath(const std::string &strFileName);
