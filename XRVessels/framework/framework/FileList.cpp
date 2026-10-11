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

// ==============================================================
// Parses a tree of files, optionally recursing subdirectories,
// and invoking an abstract callback method for each file and folder.
// ==============================================================

#include "FileList.h"
#include "Orbitersdk.h"   // for oapiRand
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <system_error>

// Convenience constructor for when you want to accept all file types
FileList::FileList(const char *pRootPath, const bool bRecurseSubfolders) :
    m_rootPath(pRootPath), m_bRecurseSubfolders(bRecurseSubfolders), m_previousRandomFileIndex(-1)
{
}

// Convenience constructor for when you only need to accept a single file type (e.g., "*.cfg")
FileList::FileList(const char *pRootPath, const bool bRecurseSubfolders, const char *pFileTypeToAccept) :
    FileList(pRootPath, bRecurseSubfolders)
{
    m_fileTypesToAccept.push_back(pFileTypeToAccept);   // copied by value
}

// Normal Constructor
//  pRootPath: base path to scan; may be relative or absolute path
//  bRecurseSubfolders: true to recurse, false to not
//  pFileTypesToAccept: vector of case-insensitive file extensions to accept (e.g., '.cfg', '.flac', etc).  If empty, all files are accepted.
FileList::FileList(const char *pRootPath, const bool bRecurseSubfolders, const vector<std::string> &fileTypesToAccept) :
    FileList(pRootPath, bRecurseSubfolders)
{
    m_fileTypesToAccept = fileTypesToAccept;     // copied by value (i.e., it is cloned)
}

// Destructor
FileList::~FileList()
{
}

// not upstream: size of a directory entry, 0 if it cannot be read (nFileSizeHigh/nFileSizeLow counterpart)
static uintmax_t EntryFileSize(const fs::directory_entry &fd)
{
    std::error_code ec;
    const uintmax_t size = fd.file_size(ec);
    return (ec ? 0 : size);
}

// not upstream: FILE_ATTRIBUTE_DIRECTORY counterpart
static bool EntryIsDirectory(const fs::directory_entry &fd)
{
    std::error_code ec;
    return fd.is_directory(ec);
}

#define STARTSWITH_DOT(fd)  (*(fd.path().filename().c_str()) == '.')   // cFileName -> path().filename()
#define IS_EMPTY(fd)        (EntryFileSize(fd) == 0)                    // nFileSizeHigh/nFileSizeLow counterpart
#define IS_DIRECTORY(fd)    (EntryIsDirectory(fd))                      // dwFileAttributes counterpart

// Resursive method to scan a tree of files, invoking clbkFilterNode for each.  
// If clbkFilterNode returns true:
//      For files, *and* the file is not empty, the file node is added to m_allFiles, and
//      For folders, that folder node is recursed into.
// Note: recursionLevel is just here for debugging purposes
void FileList::Scan(const char *pPath, const int recursionLevel)
{
    assert(pPath);
    assert(*pPath);

    // This code was broken out from XRPayloadClassData::InitializeXRPayloadClassData().

    // FindFirstFile("%s\\*") counterpart: pPath keeps the given '\' spelling; the directory is opened by its on-disk spelling
    std::error_code ec;
    fs::directory_iterator it(oapiResolvePath(pPath), ec);
    if (ec)
        return;    // directory is empty (should really never happen because "." and ".." are always present)

    // std::filesystem leaves out "." and "..", so there is no FindFirstFile/goto entry point here
    // now loop through all remaining files
    for (; it != fs::directory_iterator(); it.increment(ec))   // FindNextFile counterpart
    {
        if (!ec)
        {
            const fs::directory_entry &findFileData = *it;   // WIN32_FIND_DATA counterpart
            // build the new configdir-relative path
            char pNodeFilespec[MAX_PATH];
            snprintf(pNodeFilespec, sizeof(pNodeFilespec), "%s\\%s", pPath, findFileData.path().filename().c_str());  // e.g., "Vessels\XRParts.cfg"; sprintf_s -> snprintf, '\' kept as on Windows

            if (!STARTSWITH_DOT(findFileData))   // file not "." or ".."?
            {
                if (clbkFilterNode(pNodeFilespec, findFileData))
                {
                    // node should be included
                    if (IS_DIRECTORY(findFileData))
                    {
                        // this is a directory, so recurse down into it
                        Scan(pNodeFilespec, recursionLevel + 1);
                    }
                    else if (!IS_EMPTY(findFileData))  // it's a file node; is it not empty?
                    {
                        // it's a file and it's not empty, so add it to our master list of nodes and invoke the callback for subclasses to hook
                        m_allFiles.push_back(pNodeFilespec);
                        clbkProcessFile(pNodeFilespec, findFileData);
                    }
                }
            }
        }  // if (!ec)
        else   // FindNextFile failed
        {
            // an iterator error leaves the iterator at its end, so the listing cannot go on (Windows retried other errors)
            break;      // all done
        }
    }  // for (;;)
    // FindClose left out: the directory_iterator closes the directory
}

// Invoked for each file or folder node found.  The default method here looks at bRecurseSubfolders (for folder nodes) and
// pFileTypesToAccept (for file nodes) to decide whether accept a node or not.
// Subclasses should override this method if they want more advanced filtering.
//
// Returns true if file node should be included or folder should be recursed into, or false if the node should be skipped.
bool FileList::clbkFilterNode(const char *pPathOfNode, const fs::directory_entry &fd)   // WIN32_FIND_DATA -> directory_entry
{
    if (IS_DIRECTORY(fd))
        return m_bRecurseSubfolders; 

    // it's a file node
    bool bAcceptFile = false;
    if (!m_fileTypesToAccept.empty())
    {
        const std::string fileName = fd.path().filename().string();   // cFileName counterpart
        const char *pFileExtension = strrchr(fileName.c_str(), '.');
        if (pFileExtension)     // e.g., ".flac"
        {
            // see if we have a case-insensitive match for this extension in our master list
            for (vector<std::string>::const_iterator it = m_fileTypesToAccept.begin(); it != m_fileTypesToAccept.end(); it++)
            {
                if (strcasecmp(pFileExtension, it->c_str()) == 0)   // _stricmp -> strcasecmp
                {
                    bAcceptFile = true;
                    break;
                }
            }
        }
    }
    else
        bAcceptFile = true;     // accept all file types

    return bAcceptFile;
}

// Callback invoked for non-empty file nodes that passed the clbkFilterNode check; this is here for subclasses to hook.
void FileList::clbkProcessFile(const char *pFilespec, const fs::directory_entry &fd)   // WIN32_FIND_DATA -> directory_entry
{
    // no-op; this method is for subclasses to use
}

// Returns a random file entry from the list that is not a repeat of the previous one (provided there are at least two files in the list).
// Returns empty string if the list is empty.
const std::string FileList::GetRandomFile()
{
    const int fileCount = GetScannedFileCount();

    // sanity checks
    if (fileCount == 0)
        return "";
    else if (fileCount == 1)
        return m_allFiles[0];   // only one file

    // else we have a normal list of at least two entries, so choose one
    int fileIndex;
    do
    {
        fileIndex = (static_cast<int>(oapiRand() * RAND_MAX) % fileCount); // 0..count-1    
    } while (fileIndex == m_previousRandomFileIndex);

    m_previousRandomFileIndex = fileIndex;
    return m_allFiles[fileIndex];
}

// Returns a random file entry from the list that is not a repeat of the previous one (provided there are at least two files in the list).
//   index: 0..GetScannedFileCount()-1
// Returns empty string if the list is empty.
const std::string FileList::GetFile(const int index) const
{
    const int fileCount = GetScannedFileCount();

    // sanity checks
    if ((fileCount == 0) || (index < 0) || index >= fileCount)
        return "";  // index out-of-range

    return m_allFiles[index];
}

// Returns the first file in our file list with the specified basename (case-insensitive search), or nullptr if no file found.
const std::string *FileList::FindFileWithBasename(const char *pBasename) const
{
    assert(pBasename);
    assert(*pBasename);

    const std::string *pRetVal = nullptr;
    if (pBasename && *pBasename)
    {
        for (vector<std::string>::const_iterator it = m_allFiles.begin(); it != m_allFiles.end(); it++)
        {
            const std::string &filespec = *it;   // e.g., "foo\bar.flac", "C:\foo\bar.flac", etc.

            // locate the filename portion of the string
            const size_t separatorPos = filespec.rfind('\\');   // ReverseFind counterpart: npos -> -1
            int lastSeparatorIndex = ((separatorPos == std::string::npos) ? -1 : static_cast<int>(separatorPos));
            if (lastSeparatorIndex < 0)
                lastSeparatorIndex = -1;     // we have just a filename ("bar.flac") -- no leading path; adjust so it will start at index 0 below

            // see if the basename part matches what we're looking for
            // Note: it would be faster for very large filelists to use a hashmap, but this is faster for lists of a few hundred files or less
            const char *pCandidateFilename = filespec.c_str() + lastSeparatorIndex + 1;  // point to filename portion of string (skip over "\")
            for (int i=0;; i++)
            {
                if (!pCandidateFilename[i] || (pCandidateFilename[i] == '.'))   // end of filespec string or end of filename portion (if any)?
                {
                    if (pBasename[i] == 0)   // also end of filename we're searching for (i.e., base names are same length)?
                    {
                        return &filespec;   // this resides in our immutable list, so it's safe to return a pointer to it
                    }
                    break;  // not a match (different lengths)
                }

                if (!pBasename[i] || (tolower(pBasename[i]) != tolower(pCandidateFilename[i])))   // end of basename we're searching for OR characters don't match?
                {
                    break;  // not a match
                }
            }
        }
    }

    return nullptr;
}

