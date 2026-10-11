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
// Implements our common payload dialog handler class; this is NOT USED
// by the XR1: it for subclasses to use.
// ==============================================================

#pragma once

#include "DeltaGliderXR1.h"
#include "XR1PayloadDialog.h"
#include "resource_common.h"  // for our dialog controls that must match in each subclass that uses this dialog
#include "ScnEditorAPI.h"
#include "DlgCtrl.h"
#include "XRPayload.h"
#include "XRPayloadBaySlot.h"
#include "OrbiterResource.h"  // not upstream: Qt dialog helpers (DlgItem, oapiConnectDlgCommands)
#include <QComboBox>          // not upstream: Qt dialog controls
#include <QAbstractButton>
#include <QLabel>
#include <QPixmap>
#include <QFont>
#include <QTimer>
#include <cassert>
#include <cstring>
#include <string>
#include <algorithm>

// static font handles
QFont *XR1PayloadDialog::s_hOrgFont;      // normal button font handle
QFont *XR1PayloadDialog::s_hBoldFont;     // bold button font handle

// Invoked at initialization time when the user clicks our "payload" button; instantiates a 
// new XR1PayloadDialog instance and dispatches messages.  Returns when dialog is closed.
void XR1PayloadDialog::EditorFunc(OBJHANDLE hVessel)
{
    DeltaGliderXR1 *pXR1 = static_cast<DeltaGliderXR1 *>(oapiGetVesselInterface(hVessel));
    Launch(hVessel);
}

// method invoked by the Vanguard to open a new instance
// Returns: handle to new dialog
QWidget *XR1PayloadDialog::Launch(OBJHANDLE hVessel)
{
    DeltaGliderXR1 *pXR1 = static_cast<DeltaGliderXR1 *>(oapiGetVesselInterface(hVessel));
    QWidget *hDlg = oapiOpenDialogEx(g_hDLL, GLOBAL_IDD_PAYLOAD_EDITOR, Proc, DLG_CAPTIONCLOSE, pXR1);

    return hDlg;
}

// WM_TERMINATE counterpart: the vessel wants us to close; pRequestingXR1 = DeltaGliderXR1 * requesting the close
void XR1PayloadDialog::Terminate(QWidget *hDlg, DeltaGliderXR1 *pRequestingXR1)
{
    if (hDlg == nullptr)   // not upstream: SendMessage to a null window did nothing
        return;
    CloseDialog(hDlg);
}

#define TIMERID_REFRESH_MASS 1
#define TIMERID_REFRESH_BAY  2

// SetTimer counterpart: a QTimer child of the dialog, named after its timer ID; timerFunc is always nullptr (WM_TIMER)
void XR1PayloadDialog::SetTimer(QWidget *hDlg, const int timerID, const int elapse, void *timerFunc)
{
    assert(timerFunc == nullptr);
    KillTimer(hDlg, timerID);   // SetTimer with an existing ID replaces that timer
    QTimer *pTimer = new QTimer(hDlg);
    pTimer->setObjectName(QString("XRTimer%1").arg(timerID));
    QObject::connect(pTimer, &QTimer::timeout, hDlg, [hDlg, timerID]() { TimerProc(hDlg, timerID); });
    pTimer->start(elapse);
}

// KillTimer counterpart
void XR1PayloadDialog::KillTimer(QWidget *hDlg, const int timerID)
{
    QTimer *pTimer = hDlg->findChild<QTimer *>(QString("XRTimer%1").arg(timerID), Qt::FindDirectChildrenOnly);
    if (pTimer != nullptr)
    {
        pTimer->stop();
        pTimer->deleteLater();
    }
}

// DLGINIT that replaces the Windows message proc: runs the WM_INITDIALOG body and connects the WM_COMMAND handler
void XR1PayloadDialog::Proc(QWidget *hDlg, void *context)
{
    /* Note: for some reason Orbiter appears to be trapping keystrokes, so this will not work.
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
            goto closeDialog;   // bye, bye
        break;  // pass it on
    */

	// WM_INITDIALOG
        {
            // pointer to vessel instance was passed as dialog context
            DeltaGliderXR1 &xr1 = *reinterpret_cast<DeltaGliderXR1 *>(context);

            // Then walk through list of all vessels in the Orbiter config directory and 
            // add each XRPayload object to our payload combo box.
            QComboBox *hListBox = DlgItem<QComboBox>(hDlg, IDC_COMBO_SELECTED_PAYLOAD_OBJECT);
            const XRPayloadClassData **ppAllPayloadClasses = XRPayloadClassData::GetAllAvailableXRPayloads();  // this is a static global array that should not be freed by us
            for (const XRPayloadClassData **ppClassData = ppAllPayloadClasses; *ppClassData != nullptr; ppClassData++)
            {
                const char *pClassname = (*ppClassData)->GetClassname();
                oapiComboAddString(hListBox, pClassname);   // CB_ADDSTRING counterpart (CBS_SORT honoured)
            }

            // now select the first one in the list
            // NOTE: it is possible that the list is empty, although that would only happen if the user deliberately deleted the sample payload container(s) 
            // installed with the Vanguard.
            hListBox->setCurrentIndex(0);    // index 0; CB_SETCURSEL counterpart

            // set initial focus to our combo-box
            hListBox->setFocus();

            // WM_SETFOCUS counterpart: focus given to the dialog goes to our selected payload combo-box
            hDlg->setFocusProxy(hListBox);

            // scan the payload bay and update the slot button states and the ship's mass fields
            RescanBayAndUpdateButtonStates(hDlg, &xr1);  // must pass in the XR1 ptr here
            ProcessSelectedPayloadChanged(hDlg, &xr1);   // ditto

            // create a timer for 1/20th-second so we can refresh the ship and payload mass values automatically
            SetTimer(hDlg, TIMERID_REFRESH_MASS, 50, nullptr);

            // Create a time for 1/5th-second so we can refresh the bay contents automatically in case 
            // the user deploys or adds cargo via the ship's controls.
            SetTimer(hDlg, TIMERID_REFRESH_BAY, 200, nullptr);
        }  // case WM_INITDIALOG

	// WM_COMMAND
    oapiConnectDlgCommands(hDlg, [hDlg](int id, int code, QWidget *hCtrl)
    {
        // handle bay slot buttons here via a for loop since there are an unknown number of them
        const int loWord = id;    // get resource ID of button that was clicked
        for (int i=0; i < slotCount; i++)
        {
            if (loWord == slotResourceIDs[i])
            {
                const WORD notificationMsg = static_cast<WORD>(code);    // e.g., RESN_CLICKED
                bool wasProcessed = ProcessSlotButtonMsg(hDlg, i+1, hCtrl, notificationMsg);  // slots are one-based
                if (wasProcessed)
                    return;
                // else fall through to the switch below
            }
        }

		switch (id) 
        {
        case IDC_COMBO_SELECTED_PAYLOAD_OBJECT:
            {
                if (code == RESN_SELCHANGE)   // CBN_SELENDOK counterpart
                {
                    // value in combo box changed: read the selected classname and populate the payload values for it
                    ProcessSelectedPayloadChanged(hDlg);
                }
                return;
            }

        case IDC_BACK:
		case IDCANCEL:
            // WARNING: do not invoke GetXR1() here!  Orbiter has cleared the context pointer at this point!
            CloseDialog(hDlg);
			return;

		case IDHELP:
			// TODO: add help
			return;

        case IDC_EMPTY_BAY:
            // remove all payload in the bay
            GetXR1(hDlg).m_pPayloadBay->DeleteAllAttachedPayloadVessels();
            RescanBayAndUpdateButtonStates(hDlg);
            GetXR1(hDlg).PlaySound(GetXR1(hDlg).SwitchOff, DeltaGliderXR1::ST_Other, MED_CLICK);  // medium click 
            return;

        case IDC_FILL_PAYLOAD_BAY:
            {
                // Fill all open slots in the bay with the currently-selected payload;
                // this will walk through each slot in order and try to add a module to each.

                // retrieve selected vessel classname
                char pSelectedClassname[256];
                if (GetSelectedPayloadClassname(hDlg, pSelectedClassname, sizeof(pSelectedClassname)) > 0)  // anything selected in the box?
                {
                    DeltaGliderXR1 &xr1 = GetXR1(hDlg);
                    xr1.m_pPayloadBay->CreateAndAttachPayloadVesselInAllSlots(pSelectedClassname);
                    RescanBayAndUpdateButtonStates(hDlg);   // update the button enabled/disabled pushed/unpushed states based on the new payload in the bay
                }
                GetXR1(hDlg).PlaySound(GetXR1(hDlg).SwitchOn, DeltaGliderXR1::ST_Other, MED_CLICK);  // medium click 
                return;
            }

        case IDC_SELECTED_REMOVE_ALL:
            {
                // Remove all items of the currently-selected payload.

                // retrieve selected vessel classname
                char pSelectedClassname[256];
                if (GetSelectedPayloadClassname(hDlg, pSelectedClassname, sizeof(pSelectedClassname)) > 0)  // anything selected in the box?
                {
                    DeltaGliderXR1 &xr1 = GetXR1(hDlg);
                    xr1.m_pPayloadBay->DeleteAllAttachedPayloadVesselsOfClassname(pSelectedClassname);
                    RescanBayAndUpdateButtonStates(hDlg);   // update the button enabled/disabled pushed/unpushed states based on the new payload in the bay
                }
                GetXR1(hDlg).PlaySound(GetXR1(hDlg).SwitchOff, DeltaGliderXR1::ST_Other, MED_CLICK);  // medium click 
                return;
            }

		}  // case WM_COMMAND
    });

    // WM_CLOSE counterpart: the dialog is destroyed without CloseDialog
    QObject::connect(hDlg, &QObject::destroyed, [hDlg]()
    {
         if (DeltaGliderXR1::s_hPayloadEditorDialog == hDlg)
             DeltaGliderXR1::s_hPayloadEditorDialog = 0;   // in case the sim is closing
    });
}

// WM_TIMER counterpart
void XR1PayloadDialog::TimerProc(QWidget *hDlg, const int timerID)
{
        switch (timerID)
        {
        case TIMERID_REFRESH_MASS:     // refresh the mass readout values
            UpdateMassValues(hDlg, GetXR1(hDlg));
            return;

        case TIMERID_REFRESH_BAY:     // refresh the bay contents
            RescanBayAndUpdateButtonStates(hDlg, &GetXR1(hDlg));
            return;
            
        // no default case
        }
}

// close this dialog
void XR1PayloadDialog::CloseDialog(QWidget *hDlg)
{
    // WARNING: do not invoke GetXR1() in this method!  Orbiter may have cleared the context pointer at this point!

    // clean up dialog-specific resources
    KillTimer(hDlg, TIMERID_REFRESH_MASS);
    KillTimer(hDlg, TIMERID_REFRESH_BAY);

    if (s_hOrgFont != nullptr)
    {
        delete s_hOrgFont;    // DeleteObject counterpart
        delete s_hBoldFont;
        s_hOrgFont = s_hBoldFont = nullptr;  // reset for when the next dialog is instantiated
    }

    DeltaGliderXR1::s_hPayloadEditorDialog = 0;   // tell the ship we are closing

    // terminate
    oapiCloseDialog(hDlg);
}

// Update the payload fields on the dialog using the supplied vessel classname
void XR1PayloadDialog::UpdatePayloadFields(QWidget *hDlg, const char *pClassname)
{
    char msg[80];  // reused

    const XRPayloadClassData &pd = XRPayloadClassData::GetXRPayloadClassDataForClassname(pClassname);

    // description
    oapiSetDlgItemText(hDlg, IDC_STATIC_DESCRIPTION, pd.GetDescription());  // SetDlgItemText counterpart
    
    // mass
    sprintf(msg, "%.3f", pd.GetMass());
    oapiSetDlgItemText(hDlg, IDC_STATIC_MASS, msg);

    // dimensions
    VECTOR3 dim = pd.GetDimensions();
    sprintf(msg, "%.2f L x %.2f W x %.2f H", dim.z, dim.x, dim.y); 
    oapiSetDlgItemText(hDlg, IDC_STATIC_DIMENSIONS, msg);

    // slots occupied
    VECTOR3 slotsOcc = pd.GetSlotsOccupied();  // renamed from "slots": a Qt keyword macro
    sprintf(msg, "%.1f L x %.1f W x %.1f H", slotsOcc.z, slotsOcc.x, slotsOcc.y); 
    oapiSetDlgItemText(hDlg, IDC_STATIC_SLOTS_OCCUPIED, msg);

    // show the bitmap preview, if any (may be null)
    QImage *const hBmp = pd.GetThumbnailBitmapHandle();
    QLabel *hPictureCtrl = DlgItem<QLabel>(hDlg, IDC_STATIC_THUMBNAIL_BMP);
    if (hBmp != nullptr)   // STM_SETIMAGE counterpart
    {
        hPictureCtrl->setPixmap(QPixmap::fromImage(*hBmp));
        hPictureCtrl->adjustSize();   // an SS_BITMAP static sizes itself to its bitmap
    }
    else
        hPictureCtrl->clear();        // a null bitmap leaves the static blank
}

// Refresh vessel and payload mass readouts
void XR1PayloadDialog::UpdateMassValues(QWidget *hDlg, const DeltaGliderXR1 &xr1)
{
    const double vesselMass = xr1.GetMass();
    const double payloadMass = xr1.GetPayloadMass();

    char msg[40];
    sprintf(msg, "%10.1f kg (%10.1lf lb)", payloadMass, payloadMass * 2.20462262);
    oapiSetDlgItemText(hDlg, IDC_STATIC_PAYLOAD_MASS, msg);  // SetDlgItemText counterpart

    sprintf(msg, "%10.1f kg (%10.1lf lb)", vesselMass, vesselMass * 2.20462262);
    oapiSetDlgItemText(hDlg, IDC_STATIC_VESSEL_MASS, msg);
}

// Process a payload button click message
// hDlg = main dialog window handle
// slotNumber = 1...36
// hButton = handle of button that was checked or unchecked
// notificationMsg = button notification code; e.g., RESN_CLICKED
// Returns: true if message processed, false if message not processed
bool XR1PayloadDialog::ProcessSlotButtonMsg(QWidget *hDlg, const int slotNumber, QWidget *const hButton, const WORD notificationMsg)
{
    bool retVal = false;

    // Note: disabled checkboxes (like buttons) do not send BN_CLICKED notifications
    if (notificationMsg == RESN_CLICKED)   // BN_CLICKED counterpart
    {
        // retrieve the state of this button
        const bool buttonState = qobject_cast<QAbstractButton *>(hButton)->isChecked();   // BM_GETCHECK counterpart
        if (buttonState)
        {
            AddPayloadToSlot(slotNumber, hDlg, hButton);
            GetXR1(hDlg).PlaySound(GetXR1(hDlg).SwitchOn, DeltaGliderXR1::ST_Other, MED_CLICK);  // medium click 
        }
        else
        {
            RemovePayloadFromSlot(slotNumber, hDlg, hButton);
            GetXR1(hDlg).PlaySound(GetXR1(hDlg).SwitchOff, DeltaGliderXR1::ST_Other, MED_CLICK);  // medium click 
        }

        retVal = true;
    }

    return retVal;
}

// Instantiate a new instance of the selected payload vessel and add it to the
// specified slot if there is room.
// Returns: true on success, false if slot is already occupied or if there is no room for the new vessel
bool XR1PayloadDialog::AddPayloadToSlot(const int slotNumber, QWidget *hDlg, QWidget *hButton)
{
    assert(slotNumber > 0);  // _ASSERTE counterpart
    bool retVal = false;

    // retrieve selected vessel classname
    char pSelectedClassname[256];
    if (GetSelectedPayloadClassname(hDlg, pSelectedClassname, sizeof(pSelectedClassname)) > 0)  // anything selected in the box?
    {
        DeltaGliderXR1 &xr1 = GetXR1(hDlg);
        retVal = xr1.m_pPayloadBay->CreateAndAttachPayloadVessel(pSelectedClassname, slotNumber);

        // update the button enabled/disabled pushed/unpushed states based on the new payload in the bay
        // We must always rescan in order to "unpush" any failed buttons.
        RescanBayAndUpdateButtonStates(hDlg);   
        if (retVal == false)     // no room for payload?
            xr1.PlaySound(DeltaGliderXR1::Error1, DeltaGliderXR1::ST_Other, ERROR1_VOL);      // error beep
    }
    
    return retVal;
}
  
// Delete the vessel in the selected slot
// Returns: true on success, false if no payload is in the selected slot.
bool XR1PayloadDialog::RemovePayloadFromSlot(const int slotNumber, QWidget *hDlg, QWidget *hButton)
{
    assert(slotNumber > 0);  // _ASSERTE counterpart
    bool retVal = false;

    DeltaGliderXR1 &xr1 = GetXR1(hDlg);
    XRPayloadBay *pBay = xr1.m_pPayloadBay;

    // delete the attached vessel
    retVal = pBay->DeleteAttachedPayloadVessel(slotNumber);
    if (retVal) // success?
    {
        // update the button enabled/disabled pushed/unpushed states based on the new payload in the bay
        RescanBayAndUpdateButtonStates(hDlg);   
    }
    
    return retVal;
}

// Rescan the payload bay and update button states based on the state of the payload bay;
// a slot that is occupied by a neighboring payload will be disabled.  Note that primary
// slots (slots to which a payload is attached) are *always* enabled, as are empty slots.
// pXR1: if nullptr, look up XR1 via oapiGetDialogContext; WARNING: if WM_INITDIALOG, you must pass in pXR1 because the context is not set up yet!
// This method will also update the pushed/unpushed state based on whether this slot is a primary slot.  Note that 
// disabled slots do not need to be pushed or unpushed since it is irrelevant.
//
// This method must be invoked each time a payload is added to/removed from the bay.
void XR1PayloadDialog::RescanBayAndUpdateButtonStates(QWidget *hDlg, DeltaGliderXR1 *pXR1)
{
    if (pXR1 == nullptr)
        pXR1 = &GetXR1(hDlg);

    XR1PayloadBay *pBay = static_cast<XR1PayloadBay *>(pXR1->m_pPayloadBay);

    // retrieve selected vessel classname
    char pSelectedClassname[256];
    GetSelectedPayloadClassname(hDlg, pSelectedClassname, sizeof(pSelectedClassname));  // pSelectedClassname will be empty if no payload selected

    // walk through each slot and set the corresponding button's enabled/disabled pushed/unpushed state
    for (int i=0; i < pBay->GetSlotCount(); i++)
    {
        const int slotNumber = i+1;   // slot numbers are one-based
        const XRPayloadBaySlot *pSlot = pBay->GetSlot(slotNumber);  
        const bool isEnabled = pSlot->IsEnabled();    
        int buttonResourceID = slotResourceIDs[i];
        
        // Set pushed/unpushed based on whether payload is in this slot
        QAbstractButton *const hButton = DlgItem<QAbstractButton>(hDlg, buttonResourceID);
        hButton->setChecked(pSlot->IsOccupied());   // BM_SETCHECK counterpart

        // enable/disable the button for this slot
        hButton->setEnabled(isEnabled);   // EnableWindow counterpart

        // Set the text to BOLD if this button contains content that is of the selected payload object class; 
        // otherwise, set it back to normal.
        bool isBold = false;
        VESSEL *pChildVessel = pSlot->GetChild();
        if (pChildVessel != nullptr)
        {
            if (strcmp(pSelectedClassname, pChildVessel->GetClassName()) == 0)
            {
                // classnames match; this button should be in BOLD font
                isBold = true;
            }
        }

        // Retrieve the button font.  NOTE: this font handle is tied to this dialog, so we cannot cache it statically.
        // However, there is only ever *one* instance of this dialog active at one time, so we can recreate it for each
        // dialog instance and free it when the dialog is destroyed.
        if (s_hOrgFont == nullptr)
        {
            // First time through; retrieve its original font and create the new bold font
            // These handles will persist until the dialog closes.
            QFont lf = hButton->font();   // WM_GETFONT + GetObject(LOGFONT) counterpart

            // Ensure that the org font is NOT bold; for some reason Windows seems to use a default BOLD 
            // font on very rare occasions.
            // do not free old font since Windows owns it
            lf.setWeight(QFont::Normal);   // FW_NORMAL
            s_hOrgFont = new QFont(lf);    // normal font; CreateFontIndirect counterpart

            // create a new bold font, set the button font, and force a redraw
            lf.setWeight(QFont::ExtraBold);   // FW_EXTRABOLD
            s_hBoldFont = new QFont(lf);
        }        

        hButton->setFont(*(isBold ? s_hBoldFont : s_hOrgFont));   // WM_SETFONT counterpart
    }

    // update the ship's mass fields as well
    UpdateMassValues(hDlg, *pXR1);
}

// Retrieve the selected payload classname and store it in pOut.
// pOut will be empty if no payload is present.
// outLength = size of pOut buffer
// Returns # of characters read from the drop-down list
int XR1PayloadDialog::GetSelectedPayloadClassname(QWidget *const hDlg, char *pOut, const int outLength)
{
    // value in combo box changed: read the selected classname and populate the payload values for it
    QComboBox *hListBox = DlgItem<QComboBox>(hDlg, IDC_COMBO_SELECTED_PAYLOAD_OBJECT);
    const std::string text = hListBox->currentText().toStdString();   // WM_GETTEXT counterpart: copies at most outLength-1 chars
    const size_t result = std::min<size_t>(text.size(), static_cast<size_t>(outLength - 1));
    memcpy(pOut, text.c_str(), result);
    pOut[result] = 0;

    return static_cast<int>(result);
}

// invoked whenever the selected payload type changed
// pXR1: pointer to our parent vessel; may be nullptr only if not invoked from WM_INIT.
void XR1PayloadDialog::ProcessSelectedPayloadChanged(QWidget *hDlg, DeltaGliderXR1 *pXR1)
{
    // retrieve the new classname value from the combo box
    char pSelectedClassname[256];
    if (GetSelectedPayloadClassname(hDlg, pSelectedClassname, sizeof(pSelectedClassname)) > 0)  // anything selected in the box?
    {
        // we have the selected classname; update the payload data on the dialog
        UpdatePayloadFields(hDlg, pSelectedClassname);

        // also update button label font to show bold for slots with new selected payload
        RescanBayAndUpdateButtonStates(hDlg, pXR1);
    }
}
