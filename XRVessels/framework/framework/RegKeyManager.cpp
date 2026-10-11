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

// Example usage:
//    RegKeyManager mgr;
//	  mgr.Initialize(HKEY_CURRENT_USER, ""AlteaAerospace\\XR\\WindowPos", nullptr);
//    DWORD xOut;
//    mgr.ReadRegistryDWORD("X", &xOut);

#include "RegKeyManager.h"
#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <QSettings>    // not upstream: values go to Config/XRGlobalSettings.ini instead of the registry
#include <QString>
#include <QByteArray>
#include <QVariant>

// not upstream: registry value name -> INI key; nullptr or "" is the key's default value, '@' as in .reg files
static QString RegValueKey(const char *pValueName)
{
    return (pValueName && *pValueName) ? QString::fromUtf8(pValueName) : QString("@");
}

// Constructor
RegKeyManager::RegKeyManager() :
    m_hKey(nullptr), m_bInitialized(false), m_hOwnerWindow(0), m_pSubkeyPath(nullptr)
{
}

// Destructor
RegKeyManager::~RegKeyManager()
{
    if (m_hKey)
        delete m_hKey;   // RegCloseKey counterpart
}

// Initialize this object; returns true on success.  No error message box is shown; the caller should handle that.
// hRootKey: root key; e.g., HKEY_CURRENT_USER
// pSubkeyPath: key from which to read values; e.g., "AlteaAerospace\\Orbiter\\XR\\WindowPos"
// hOwnerWindow: window that launched us; may be nullptr
//
// Should only be called once.
bool RegKeyManager::Initialize(const char *hRootKey, const char *pSubkeyPath, QWidget *const hOwnerWindow)
{
    assert(pSubkeyPath);
    assert(!m_bInitialized);

    m_hOwnerWindow = hOwnerWindow;
    m_hKey = new QSettings(QString::fromStdString(oapiResolvePath("Config\\XRGlobalSettings.ini")), QSettings::IniFormat);   // RegCreateKeyEx counterpart: the file is created on first write
    m_hKey->beginGroup(QString::fromUtf8(hRootKey) + "/" + QString::fromUtf8(pSubkeyPath).replace('\\', '/'));   // key path -> INI group
    m_bInitialized = ((m_hKey->status() == QSettings::NoError) && m_hKey->isWritable());   // KEY_ALL_ACCESS: must be writable
    return m_bInitialized;
}

//
// Registry access methods; each returns true on success or false on error. 
//
// Writes a registry string value; returns false if error occurs
bool RegKeyManager::WriteRegistryString(const char *pValueName, const char *pValue) const
{
    assert(m_bInitialized);

    // length includes the trailing zero byte
    m_hKey->setValue(RegValueKey(pValueName), QString::fromUtf8(pValue));   // RegSetValueEx REG_SZ counterpart
    m_hKey->sync();
    bool bSuccess = (m_hKey->status() == QSettings::NoError);

	return bSuccess;
}

// Reads a registry string value into valueOut; if error occurs (i.e., key does not exist), valueOut is unchanged.
// Returns false if error occurs
bool RegKeyManager::ReadRegistryString(const char *pValueName, std::string &valueOut) const
{
    assert(m_bInitialized);

	bool bSuccess = false;

    // get the size of the value
    if (m_hKey->contains(RegValueKey(pValueName)))   // RegQueryValueEx size query counterpart
    {
        const QVariant value = m_hKey->value(RegValueKey(pValueName));
        if (value.typeId() != QMetaType::QByteArray)   // REG_SZ check: blobs are stored as @ByteArray; an INI string always has its trailing zero, so no size check
        {
            // now retrieve the actual value
            valueOut = value.toString().toStdString();   // GetBufferSetLength/RegQueryValueEx/ReleaseBuffer counterpart
            bSuccess = true;
        }
        else
        {
            errno = EINVAL;   // SetLastError(ERROR_BAD_LENGTH) counterpart
        }
    }
    return bSuccess;
}

// writes a blob out to a registry value; shows a messagebox if an error occurs.
// Returns false if error occurs
bool RegKeyManager::WriteRegistryBlob(const char *pValueName, const DATA_BLOB &blob) const
{
    assert(m_bInitialized);

    // length includes the trailing zero byte
    m_hKey->setValue(RegValueKey(pValueName), QByteArray(reinterpret_cast<const char *>(blob.pbData), static_cast<qsizetype>(blob.cbData)));   // RegSetValueEx REG_BINARY counterpart
    m_hKey->sync();
    bool bSuccess = (m_hKey->status() == QSettings::NoError);

    return bSuccess;
}

// Reads a blog from the registry into blobOut; if error occurs, blobOut is unchanged.
// It is the caller's responsibility to eventually invoke LocalFree on blobOut.pbData.
// Returns false if error occurs.
bool RegKeyManager::ReadRegistryBlob(const char *pValueName, DATA_BLOB &blobOut) const
{
    assert(m_bInitialized);

	bool bSuccess = false;

    // get the size of the value
    if (m_hKey->contains(RegValueKey(pValueName)))   // RegQueryValueEx size query counterpart
    {
        const QVariant value = m_hKey->value(RegValueKey(pValueName));
        const QByteArray data = value.toByteArray();
        blobOut.cbData = static_cast<DWORD>(data.size());
        if ((value.typeId() == QMetaType::QByteArray) && (blobOut.cbData > 0))   // REG_BINARY check
        {
            // now retrieve the actual value
            // Note: we must use LocalAlloc here since that is what the cryptography functions use when working with DATA_BLOB objects.
            blobOut.pbData = static_cast<BYTE *>(malloc(blobOut.cbData));   // LocalAlloc(LMEM_FIXED) counterpart; caller frees with free()
            memcpy(blobOut.pbData, data.constData(), blobOut.cbData);
            bSuccess = true;
        }
        else
        {
            errno = EINVAL;   // SetLastError(ERROR_BAD_LENGTH) counterpart
        }
    }
    return bSuccess;
}

// Writes a registry DWORD value; shows a messagebox if an error occurs.
// Returns false if error occurs
bool RegKeyManager::WriteRegistryDWORD(const char *pValueName, const DWORD dwValue) const
{
    assert(m_bInitialized);

    // length includes the trailing zero byte
    m_hKey->setValue(RegValueKey(pValueName), static_cast<uint>(dwValue));   // RegSetValueEx REG_DWORD counterpart
    m_hKey->sync();
    bool bSuccess = (m_hKey->status() == QSettings::NoError);

    return bSuccess;
}

// Reads a registry DWORD value into valueOut; if error occurs (i.e., key does not exist), valueOut is unchanged.
// Returns false if error occurs
bool RegKeyManager::ReadRegistryDWORD(const char *pValueName, DWORD &valueOut) const
{
    assert(m_bInitialized);

    bool bSuccess = false;
    const QVariant value = m_hKey->value(RegValueKey(pValueName));   // RegQueryValueEx counterpart: missing or wider than 32 bits fails
    const uint dwValue = value.toUInt(&bSuccess);
    if (bSuccess)
        valueOut = static_cast<DWORD>(dwValue);

    return bSuccess;
}

// Writes a registry QWORD value; shows a messagebox if an error occurs.
// Returns false if error occurs
bool RegKeyManager::WriteRegistryQWORD(const char *pValueName, const uint64_t qwValue) const
{
    assert(m_bInitialized);

    // length includes the trailing zero byte
    m_hKey->setValue(RegValueKey(pValueName), static_cast<qulonglong>(qwValue));   // RegSetValueEx REG_QWORD counterpart
    m_hKey->sync();
    bool bSuccess = (m_hKey->status() == QSettings::NoError);

    return bSuccess;
}

// Reads a registry QWORD value into valueOut; if error occurs (i.e., key does not exist), valueOut is unchanged.
// Returns false if error occurs
bool RegKeyManager::ReadRegistryQWORD(const char *pValueName, uint64_t &valueOut) const
{
    assert(m_bInitialized);

    bool bSuccess = false;
    const QVariant value = m_hKey->value(RegValueKey(pValueName));   // RegQueryValueEx counterpart: missing value fails
    const qulonglong qwValue = value.toULongLong(&bSuccess);
    if (bSuccess)
        valueOut = static_cast<uint64_t>(qwValue);

    return bSuccess;
}

// Delete a registry value.  No error messagebox is displayed if the delete fails; the caller should
// check the return code from this call.
//
// pValueName: may be nullptr or empty, in which case the default value for this manager's key is deleted
// 
// Returns: error code defined in Winerror.h (ala GetLastError())
LONG RegKeyManager::DeleteRegistryValue(const char *pValueName) const
{
    if (!m_hKey->contains(RegValueKey(pValueName)))
        return ENOENT;   // RegDeleteValue's ERROR_FILE_NOT_FOUND (2) counterpart

    m_hKey->remove(RegValueKey(pValueName));   // RegDeleteValue counterpart
    m_hKey->sync();
    return ((m_hKey->status() == QSettings::NoError) ? 0 : EIO);   // 0 == ERROR_SUCCESS
}
