/**
  XR Vessel add-ons for OpenOrbiter Space Flight Simulator
  Copyright (C) 2006-2025 Douglas Beachy

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <https://www.gnu.org/licenses/>.

  Email: mailto:doug.beachy@outlook.com
  Web: https://www.alteaaerospace.com
**/

#pragma once

// windows.h and WinCrypt.h left out: the values go to an INI file through QSettings, no registry on Linux
#include <string>      // atlstr.h left out: std::string stands in for CString
#include <cstdint>
#include "OrbiterAPI.h"

class QSettings;
class QWidget;

// not upstream: WinCrypt.h's DATA_BLOB
struct DATA_BLOB { DWORD cbData; BYTE *pbData; };

// not upstream: root key names; the key path becomes a group in Config/XRGlobalSettings.ini
#define HKEY_CURRENT_USER "HKEY_CURRENT_USER"

// Reads & writes our values to the registry
class RegKeyManager
{
public:
    RegKeyManager();
    virtual ~RegKeyManager();

    bool Initialize(const char *hRootKey, const char *pSubkeyPath, QWidget *const hOwnerWindow);
    bool IsInitialized() const { return m_bInitialized; }

    bool WriteRegistryString(const char *pValueName, const char *pValue) const;
    bool ReadRegistryString(const char *pValueName, std::string &valueOut) const;
    
    bool WriteRegistryBlob(const char *pValueName, const DATA_BLOB &blob) const;
    bool ReadRegistryBlob(const char *pValueName, DATA_BLOB &blobOut) const;
    
    bool WriteRegistryDWORD(const char *pValueName, const DWORD dwValue) const;
    bool ReadRegistryDWORD(const char *pValueName, DWORD &valueOut) const;
    
    bool WriteRegistryQWORD(const char *pValueName, const uint64_t dwValue) const;
    bool ReadRegistryQWORD(const char *pValueName, uint64_t &valueOut) const;

    LONG DeleteRegistryValue(const char *pValueName) const;

private:
    QSettings *m_hKey;
    QWidget *m_hOwnerWindow;  
    bool m_bInitialized;

    char *m_pSubkeyPath;  // preserved for error messages
};
