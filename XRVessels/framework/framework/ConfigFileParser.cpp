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
// ConfigFileParser.cpp
// Abstract base class to parse a configuration file.
// Blank lines and lines beginning with "#" are ignored.
//
// Format is:
//
// [SECTION]
// name=value [,value2,value3,...]
//
// [SECTION-2]
// ...
// ==============================================================

#include "ConfigFileParser.h"

// Shlwapi.h left out: PathFileExists is std::filesystem::exists where used
#include <string.h>
// atlstr.h left out: std::string stands in for CString
#include <cstdio>
#include <ctime>
#include <cerrno>
#include <strings.h>   // not upstream: strcasecmp
#include <QMessageBox>
#include "OrbiterAPI.h"   // not upstream: oapiResolvePath

// Constructor
// pDefaultFilename = path to default config file; may be relative to Orbiter root or absolute
// pLogFilename = path to optional (but highly recommended) log file; may be null
ConfigFileParser::ConfigFileParser(const char *pDefaultFilename, const char *pLogFilename) :
    m_pLogFile(nullptr), m_parseFailed(false) 
{
    m_csDefaultFilename = pDefaultFilename;
    
    if (pLogFilename != nullptr)
    {
        // open the log file in APPEND and SHARED mode so we can share it between multiple ship instances
        m_pLogFile = fopen(pLogFilename, "a+t");
        if (m_pLogFile == nullptr)
        {
            char temp[256];
            snprintf(temp, sizeof(temp), "Error opening log file '%s' for writing; attempting to continue", pLogFilename);   // snprintf: FORTIFY aborts on overflow
            QMessageBox::warning(nullptr, "XR Framework Warning", temp);   // MessageBox counterpart
        }
    }
}

// Destructor
ConfigFileParser::~ConfigFileParser()
{
    // clean up and close our logfile
    fclose(m_pLogFile);
}

//
// Main config file parse method.  If you parse multiple files via the same parser object, newer values
// will overwite any previous values.  This allows you to create optional "override" configuration files as well
// as the normal ("default") file.
//
// pFilename: (optional) path\filename to parse.  If missing or nullptr, parses the default config filename that was passed to the constructor.
//
// Returns: true on success, false if file does not exist or I/O error or parse error occurs
bool ConfigFileParser::ParseFile(const char *pFilename)
{
    if (pFilename == nullptr)
        pFilename = GetDefaultFilename();

    const bool bParsingOverrideFile = (strcasecmp(pFilename, GetDefaultFilename()) != 0);  // true if we are parsing an override file; _stricmp -> strcasecmp

    static char temp[MAX_LINE_LENGTH + 512]; // reused for messages; not upstream: room for a full config line plus the file name

    // open the config file
    snprintf(temp, sizeof(temp), "Parsing config file '%s'", pFilename);   // snprintf: FORTIFY aborts on overflow
    WriteLog(temp);

    FILE *pFile = fopen(oapiResolvePath(pFilename).c_str(), "rt");   // Orbiter-relative path with '\' and any letter case

    if (pFile == nullptr)
    {
        snprintf(temp, sizeof(temp), "ERROR: fopen failed for '%s'; errno=0x%X", pFilename, errno);   // GetLastError -> errno; snprintf: FORTIFY aborts on overflow
        WriteLog(temp);
        m_parseFailed = true;
        return false;       // could not open file
    }
    
    // read line loop
    bool retVal = true;     // assume success
    for (int lineNumber = 1;; lineNumber++)
    {
        if (fgets(m_buffer, MAX_LINE_LENGTH, pFile) == nullptr)
        {
            if (feof(pFile) == false)    // not at EOF yet?
                retVal = false;          // error parsing file

            // we reached EOF; stop parsing
            break;
        }

        //
        // We have a line -- parse it
        //
        TrimString(m_buffer);
        
        // check for a blank line or comment line
        if ((*m_buffer == 0) || (*m_buffer == '#'))
            continue;   // skip this line

        // check for a new section marker
        if (*m_buffer == '[')
        {
            bool foundClosingBracket = false;
            // copy all characters up to the trailing ']'
            int i;
            for (i=0; i < sizeof(m_section)-1; i++)
            {
                const char c = m_buffer[i+1];   // skip leading '['
                if (c == 0)
                    break;      // end of string, but no bracket found!

                if (c == ']')
                {
                    foundClosingBracket = true;
                    break;
                }
                
                // normal char; copy it 
                m_section[i] = c;
            }

            m_section[i] = 0;       // terminate section string
        }
        else    // this is a normal line
        {
            // find the '=' separating name from value
            const char *pEquals = strchr(m_buffer, '=');
            if (pEquals == nullptr)
            {
                snprintf(temp, sizeof(temp), "Error parsing line #%d of file '%s': missing '=' character.  Line='%s'", lineNumber, pFilename, m_buffer);   // snprintf: FORTIFY aborts on overflow
                WriteLog(temp);
                retVal = false;
                continue;  // do NOT stop parsing; just skip to the next line
            }

            // validate name length
            const int nameLength = static_cast<int>((pEquals - m_buffer));    // # of chars in parsed name
            if (nameLength > MAX_NAME_LENGTH)
            {
                snprintf(temp, sizeof(temp), "Error parsing line #%d of file '%s': name parameter too long (exceeds %d characters).  Line='%s'",   // snprintf: FORTIFY aborts on overflow
                    lineNumber, pFilename, MAX_NAME_LENGTH, m_buffer);
                WriteLog(temp);
                retVal = false;
                continue;  // do NOT stop parsing; just skip to the next line
            }

            // validate value length
            const char *pValue = pEquals+1;   // skip '='
            const int valueLength = static_cast<int>(strlen(pValue));
            if (valueLength > MAX_VALUE_LENGTH)
            {
                snprintf(temp, sizeof(temp), "Error parsing line #%d of file '%s': value parameter too long (exceeds %d characters).  Line='%s'",   // snprintf: FORTIFY aborts on overflow
                    lineNumber, pFilename, MAX_VALUE_LENGTH, m_buffer);
                WriteLog(temp);
                retVal = false;
                continue;  // do NOT stop parsing; just skip to the next line
            }

            // copy parsed name and value strings so they can be sent to the subclass for processing
            // the subclass is free to modify these buffers since they are overwritten for each line
            memmove(m_parsedName, m_buffer, nameLength);    // copy name string
            m_parsedName[nameLength] = 0;   // terminate string
            strcpy(m_parsedValue, pValue);  // copy value string

            // trim leading and trailing whitespace from the name and value
            TrimString(m_parsedName);
            TrimString(m_parsedValue);

            // invoke the subclass to parse these values
            if (ParseLine(m_section, m_parsedName, m_parsedValue, bParsingOverrideFile) == false)
            {
                snprintf(temp, sizeof(temp), "Name/Value error parsing line #%d of file '%s': Line='%s'.  Check the above log message for details.",   // snprintf: FORTIFY aborts on overflow
                    lineNumber, pFilename, m_buffer);
                WriteLog(temp);
                retVal = false;
                continue;  // do NOT stop parsing; just skip to the next line
            }
            // parse was successful; return to the top of the loop to parse the next line
        }
    }

    // reset the active section to empty
    *m_section = 0;

    // clean up and exit
    fclose(pFile);
    if (retVal)     // success?
    {
        snprintf(temp, sizeof(temp), "Successfully parsed configuration file '%s'", pFilename);   // snprintf: FORTIFY aborts on overflow
        WriteLog(temp);
    }
    else
        m_parseFailed = true;
    
    return retVal;
}

//
// Static utility method to remove all whitespace (nonprintable characters) from the beginning and end of a string
//
// pStr = string to be trimmed
//
void ConfigFileParser::TrimString(char *pStr) 
{
    char * const pOrgStart = pStr;   // original start of string
    char *pStart = pStr;  // start of string; chases pStr until 1st non-whitespace char found

    // pass #1: delete all leading whitespace
    bool trim = true;
    for (char *pOut = pStr;; pOut++, pStr++)
    {
        char c = *pStr;
        if (c == 0)
            break;  // end of string

        if ((c > 32) && (c < 127))
        {
            // first real character reached, so stop timming whitespace
            trim = false;  
        }
       
        if (trim)       // are we skipping this byte?
            pStart++;   // update new start of string ptr
        else            // NOT trimming this byte
            *pOut = c;  // write to output
    }

    // pass #2: delete all trailing whitespace
    // pStr == end of string (on nullptr byte)
    // iterate in reverse
    for (;;)
    {
        if (pStr == pStart)
        {
            // since we reached the start of the string, we found NO non-whitespace characters from the end, so the string was ALL whitespace
            *pStart = 0;    // empty
            break;
        }

        pStr--;   // bump to previous byte

        char c = *pStr;
        if ((c > 32) && (c < 127))
        {
            // found valid end-of-string (non-whitespace)
            pStr[1] = 0;  // terminate it after this character
            break;
        }
    }

    // final step: shift string left to delete trimmed whitespace
    strcpy(pOrgStart, pStart);
}


// log a message
void ConfigFileParser::WriteLog(const char *pMsg) const
{
    // nothing to do if msg is null or if logging disabled
    if ((pMsg == nullptr) || (m_pLogFile == nullptr)) 
        return;

    char csMsg[MAX_LINE_LENGTH + 2048];   // CStringA counterpart: room for the timestamp, the prefix and a full-line message
    // get and format the current time
    timespec ts; clock_gettime(CLOCK_REALTIME, &ts); tm st; localtime_r(&ts.tv_sec, &st);   // GetLocalTime counterpart
    std::string csPrefix;
    if (!GetLogPrefix().empty())
        csPrefix = "[" + GetLogPrefix() + "] ";   // CString::Format("[%s] ") counterpart

    snprintf(csMsg, sizeof(csMsg), "%02d.%02d.%04d %02d:%02d:%02d.%03d - %s%s\n",   // CString::Format counterpart
        st.tm_mon + 1, st.tm_mday, st.tm_year + 1900,   // SYSTEMTIME wMonth/wDay/wYear
        st.tm_hour, st.tm_min, st.tm_sec, static_cast<int>(ts.tv_nsec / 1000000),   // wHour/wMinute/wSecond/wMilliseconds
        csPrefix.c_str(), pMsg);

    // no point in checking for error here
    fputs(csMsg, stderr);   // OutputDebugString counterpart: send to debug console
    fwrite(csMsg, 1, strlen(csMsg), m_pLogFile);

    // flush to disk in case we crash or are terminated
    fflush(m_pLogFile);
}

// logs an error and returns false if the supplied value is out-of-range
bool ConfigFileParser::ValidateInt(const int value, const int min, const int max) const
{
    bool retVal = true; // assume success

    char temp[256];
    if (value < min)
    {
        snprintf(temp, sizeof(temp), "Integer value '%d' is below minimum value of '%d'", value, min);   // snprintf: FORTIFY aborts on overflow
        WriteLog(temp);
        retVal = false;
    }
    else if (value > max)
    {
        snprintf(temp, sizeof(temp), "Integer value '%d' is above maximum value of '%d'", value, max);   // snprintf: FORTIFY aborts on overflow
        WriteLog(temp);
        retVal = false;
    }

    return retVal;
}

// logs an error and returns false if the supplied value is out-of-range
bool ConfigFileParser::ValidateDouble(const double value, const double min, const double max) const
{
    bool retVal = true; // assume success

    char temp[256];
    if (value < min)
    {
        snprintf(temp, sizeof(temp), "Double value '%lf' is below minimum value of '%lf'", value, min);   // snprintf: FORTIFY aborts on overflow
        WriteLog(temp);
        retVal = false;
    }
    else if (value > max)
    {
        snprintf(temp, sizeof(temp), "Double value '%lf' is above maximum value of '%lf'", value, max);   // snprintf: FORTIFY aborts on overflow
        WriteLog(temp);
        retVal = false;
    }

    return retVal;
}

// logs an error and returns false if the supplied value is out-of-range
bool ConfigFileParser::ValidateFloat(const float value, const float min, const float max) const
{
    bool retVal = true; // assume success

    char temp[256];
    if (value < min)
    {
        snprintf(temp, sizeof(temp), "Float value '%f' is below minimum value of '%f'", value, min);   // snprintf: FORTIFY aborts on overflow
        WriteLog(temp);
        retVal = false;
    }
    else if (value > max)
    {
        snprintf(temp, sizeof(temp), "Float value '%f' is above maximum value of '%f'", value, max);   // snprintf: FORTIFY aborts on overflow
        WriteLog(temp);
        retVal = false;
    }

    return retVal;
}

