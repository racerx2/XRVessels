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
// XR1 Base Class Library
// These classes extend and use the XR Framework classes
//
// XR1PayloadDialog.h
// Defines our common payload dialog handler class; this is NOT USED by
// the XR1; it is for use by sublcasses.
// ==============================================================

#pragma once

#include "DeltaGliderXR1.h"
#include "XR1PayloadBay.h"

// custom messages
// WM_XR1_UPDATEMASSVALUES left out: never sent; UpdateMassValues is called directly
// WM_TERMINATE -> XR1PayloadDialog::Terminate (clean up and close dialog gracefully)

// static payload handler class
class XR1PayloadDialog
{
public:
    static void EditorFunc(OBJHANDLE hVessel);
    static QWidget *Launch(OBJHANDLE hVessel);
    static void Terminate(QWidget *hDlg, DeltaGliderXR1 *pRequestingXR1);  // WM_TERMINATE counterpart
    
protected:
    static DeltaGliderXR1 &GetXR1(QWidget *hDlg) { return *static_cast<DeltaGliderXR1 *>(oapiGetDialogContext(hDlg)); }  // WARNING: not valid during WM_INITDIALOG! 
    static void Proc(QWidget *hDlg, void *context);  // DLGINIT: WM_INITDIALOG body, connects WM_COMMAND
    static void TimerProc(QWidget *hDlg, const int timerID);  // WM_TIMER counterpart
    static void SetTimer(QWidget *hDlg, const int timerID, const int elapse, void *timerFunc);  // SetTimer counterpart (QTimer)
    static void KillTimer(QWidget *hDlg, const int timerID);  // KillTimer counterpart
    static void UpdateMassValues(QWidget *hDlg, const DeltaGliderXR1 &xr1);
    static void UpdatePayloadFields(QWidget *hDlg, const char *pClassname);
    static bool ProcessSlotButtonMsg(QWidget *hDlg, const int slotNumber, QWidget *const hButton, const WORD notificationMsg);

    static bool AddPayloadToSlot(const int slotNumber, QWidget *hDlg, QWidget *hButton);
    static bool RemovePayloadFromSlot(const int slotNumber, QWidget *hDlg, QWidget *hButton);

    static void RescanBayAndUpdateButtonStates(QWidget *hDlg, DeltaGliderXR1 *pXR1 = nullptr);
    static void ProcessSelectedPayloadChanged(QWidget *hDlg, DeltaGliderXR1 *pXR1 = nullptr);

    static void CloseDialog(QWidget *hDlg);

    // utility methods
    static int GetSelectedPayloadClassname(QWidget *const hDlg, char *pOut, const int outLength);

    // data
    static const int slotCount;          // total # of slots in the bay
    static const int slotResourceIDs[];  // array of button resource IDs in slot order
    static QFont *s_hOrgFont;      // normal button font handle
    static QFont *s_hBoldFont;     // bold button font handle
};
