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

#include "DeltaGliderXR1.h"
#include "ScnEditorAPI.h"
#include "DlgCtrl.h"
#include "OrbiterResource.h"  // not upstream: Qt dialog helpers (DlgItem, oapiConnectDlgCommands)

static HELPCONTEXT g_hc = {
    (char *)"html/vessels/deltaglider.chm",  // literal -> char * cast
        0,
        (char *)"html/vessels/deltaglider.chm::/deltaglider.hhc",  // literal -> char * cast
        (char *)"html/vessels/deltaglider.chm::/deltaglider.hhk"  // literal -> char * cast
};

// static helper methods
static void UpdateDamage (QWidget *hTab, DeltaGliderXR1 *dg)  // HWND -> QWidget *
{
    int i;
    char cbuf[256];
    i = static_cast<int>((dg->lwingstatus*100.0+0.5));
    sprintf (cbuf, "%d %%", i);
    oapiSetDlgItemText (hTab, IDC_LEFTWING_STATUS, cbuf);  // SetWindowText(GetDlgItem) counterpart
    oapiSetGaugePos (oapiResDlgItem (hTab, IDC_LEFTWING_SLIDER), i);  // GetDlgItem counterpart
    i = static_cast<int>((dg->rwingstatus*100.0+0.5));
    sprintf (cbuf, "%d %%", i);
    oapiSetDlgItemText (hTab, IDC_RIGHTWING_STATUS, cbuf);  // SetWindowText(GetDlgItem) counterpart
    oapiSetGaugePos (oapiResDlgItem (hTab, IDC_RIGHTWING_SLIDER), i);  // GetDlgItem counterpart
}

// ==============================================================
// Scenario editor interface
// ==============================================================

DeltaGliderXR1 *GetDG (QWidget *hDlg)  // HWND -> QWidget *
{
    // retrieve DG interface from scenario editor
    OBJHANDLE vessel;
    ScnEditorMsg (hDlg, SE_GETVESSEL, reinterpret_cast<LPARAM>(&vessel));  // SendMessage WM_SCNEDITOR counterpart
    return static_cast<DeltaGliderXR1 *>(oapiGetVesselInterface(vessel));
}

// --------------------------------------------------------------
// Message procedure for editor page 1 (animation settings)
// --------------------------------------------------------------
void EdPg1Proc (QWidget *hTab, void *context)  // DLGPROC -> DLGINIT
{
    oapiConnectDlgCommands (hTab, [hTab](int id, int code, QWidget *hCtrl) {  // WM_COMMAND counterpart
        switch (id) {  // LOWORD(wParam) counterpart
        case IDHELP:
            g_hc.topic = (char *)"/SE_Anim.htm";  // literal -> char * cast
            oapiOpenHelp (&g_hc);
            return;  // return TRUE counterpart
        case IDC_GEAR_UP:
            GetDG(hTab)->ActivateLandingGear(DoorStatus::DOOR_CLOSED);
            return;
        case IDC_GEAR_DOWN:
            GetDG(hTab)->ActivateLandingGear(DoorStatus::DOOR_OPEN);
            return;
        case IDC_RETRO_CLOSE:
            GetDG(hTab)->ActivateRCover(DoorStatus::DOOR_CLOSED);
            return;
        case IDC_RETRO_OPEN:
            GetDG(hTab)->ActivateRCover(DoorStatus::DOOR_OPEN);
            return;
        case IDC_OLOCK_CLOSE:
            GetDG(hTab)->ActivateOuterAirlock(DoorStatus::DOOR_CLOSED);
            return;
        case IDC_OLOCK_OPEN:
            GetDG(hTab)->ActivateOuterAirlock(DoorStatus::DOOR_OPEN);
            return;
        case IDC_ILOCK_CLOSE:
            GetDG(hTab)->ActivateInnerAirlock(DoorStatus::DOOR_CLOSED);
            return;
        case IDC_ILOCK_OPEN:
            GetDG(hTab)->ActivateInnerAirlock(DoorStatus::DOOR_OPEN);
            return;
        case IDC_NCONE_CLOSE:
            GetDG(hTab)->ActivateOuterAirlock(DoorStatus::DOOR_CLOSED);  // NOTE: outer airlock must close, too!
            GetDG(hTab)->ActivateNoseCone(DoorStatus::DOOR_CLOSED);
            return;
        case IDC_NCONE_OPEN:
            GetDG(hTab)->ActivateNoseCone(DoorStatus::DOOR_OPEN);
            return;
        case IDC_LADDER_RETRACT:
            GetDG(hTab)->ActivateLadder(DoorStatus::DOOR_CLOSED);
            return;
        case IDC_LADDER_EXTEND:
            GetDG(hTab)->ActivateLadder(DoorStatus::DOOR_OPEN);
            return;
        case IDC_HATCH_CLOSE:
            GetDG(hTab)->ActivateHatch(DoorStatus::DOOR_CLOSED);
            return;
        case IDC_HATCH_OPEN:
            GetDG(hTab)->ActivateHatch(DoorStatus::DOOR_OPEN);
            return;
        case IDC_RADIATOR_RETRACT:
            GetDG(hTab)->ActivateRadiator(DoorStatus::DOOR_CLOSED);
            return;
        case IDC_RADIATOR_EXTEND:
            GetDG(hTab)->ActivateRadiator(DoorStatus::DOOR_OPEN);
            return;
        }
    });  // end of WM_COMMAND counterpart
}

#if 0  // this was not compatible with UMMU to begin with
// --------------------------------------------------------------
// Message procedure for editor page 2 (passengers)
// --------------------------------------------------------------
INT_PTR CALLBACK EdPg2Proc (HWND hTab, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    DeltaGliderXR1 *dg;
    
    switch (uMsg) {
    case WM_INITDIALOG: 
        {
            char cbuf[256];
            dg = static_cast<DeltaGliderXR1 *>(oapiGetVesselInterface((OBJHANDLE)lParam));
            for (int i=0; i < MAX_PASSENGERS; i++)  
            {
                const char *pName = dg->GetCrewNameBySlotNumber(i);
                bool passengerOnBoard = dg->IsCrewMemberOnBoard(i);
                SendDlgItemMessage (hTab, IDC_CHECK1+i, BM_SETCHECK, passengerOnBoard ? BST_CHECKED : BST_UNCHECKED, 0);
            }
            sprintf (cbuf, "%0.2f kg", dg->GetMass());
            SetWindowText (GetDlgItem (hTab, IDC_MASS), cbuf);
            break;
        } 
    case WM_COMMAND:
        switch (LOWORD (wParam))
        {
        case IDC_CHECK1:
        case IDC_CHECK2:
        case IDC_CHECK3:
        case IDC_CHECK4: 
        case IDC_CHECK5: 
            {
                // WARNING: this is not compatible with UMmu crew management because UMmu reorders the slots (0-4) each time a crew member is added or removed.
                char cbuf[256];
                LRESULT lResult = SendDlgItemMessage (hTab, LOWORD(wParam), BM_GETCHECK, 0, 0);
                dg = GetDG(hTab);

                // get current crew member for this slot
                const int index = LOWORD(wParam)-IDC_CHECK1; // 0...n
                const char *pName = dg->GetCrewNameBySlotNumber(index);
                if (lResult)    // adding crew member?
                {
                    if (strlen(pName) == 0)     // not already on board?
                    {
                        // add default crew member for this slot
                        CrewMember *pCM = dg->GetXR1Config()->CrewMembers + index;
                        dg->UMmu.AddCrewMember(pCM->name, pCM->age, pCM->pulse, pCM->mass, pCM->rank);
                    }
                }
                else    // removing crew member
                {
                    if (strlen(pName) > 0)  // is crew member on board?
                    {
                        // UMmu bug: must cast away constness here
                        dg->UMmu.RemoveCrewMember(const_cast<char *>(pName));   // remove him
                    }
                }

                dg->SetPassengerVisuals();
                dg->SetEmptyMass();
                sprintf (cbuf, "%0.2f kg", dg->GetMass());
                SetWindowText (GetDlgItem (hTab, IDC_MASS), cbuf);
                break;
            } 
        }
        break;
    }
    return FALSE;
}
#endif

// --------------------------------------------------------------
// Message procedure for editor page 3 (damage)
// --------------------------------------------------------------
void EdPg3Proc (QWidget *hTab, void *context)  // DLGPROC -> DLGINIT
{
    DeltaGliderXR1 *dg;
    
    {  // WM_INITDIALOG
        dg = static_cast<DeltaGliderXR1 *>(oapiGetVesselInterface(static_cast<OBJHANDLE>(context)));  // lParam -> DLGINIT context
        GAUGEPARAM gp = { 0, 100, GAUGEPARAM::LEFT, GAUGEPARAM::BLACK };
        oapiSetGaugeParams (oapiResDlgItem (hTab, IDC_LEFTWING_SLIDER), &gp);  // GetDlgItem counterpart
        oapiSetGaugeParams (oapiResDlgItem (hTab, IDC_RIGHTWING_SLIDER), &gp);  // GetDlgItem counterpart
        UpdateDamage (hTab, dg);
                        }
    // WM_COMMAND: no handler; its fall-through into WM_HSCROLL matched no scroll request
        /* NOT IMPLEMENTED
        switch (LOWORD (wParam)) {
        case IDC_REPAIR:
            dg = GetDG(hTab);
            dg->RepairDamage ();
            UpdateDamage (hTab, dg);
            return TRUE;
        }
        break;
        */
    for (int sliderId : {IDC_LEFTWING_SLIDER, IDC_RIGHTWING_SLIDER}) {  // WM_HSCROLL: GaugeCtrl::scrolled of both sliders
        QObject::connect (DlgItem<GaugeCtrl> (hTab, sliderId), &GaugeCtrl::scrolled, hTab, [hTab, sliderId](int request, int pos) {
            DeltaGliderXR1 *dg = GetDG(hTab);
            int id = sliderId;  // GetDlgCtrlID(lParam) counterpart
            switch (id) {
            case IDC_LEFTWING_SLIDER:
            case IDC_RIGHTWING_SLIDER:
                switch (request) {  // LOWORD(wParam) counterpart
                case GAUGE_THUMBTRACK:  // SB_THUMBTRACK
                case GAUGE_LINEDEC:     // SB_LINELEFT
                case GAUGE_LINEINC:     // SB_LINERIGHT
                    if (id == IDC_LEFTWING_SLIDER)
                        dg->lwingstatus = pos*0.01;  // HIWORD(wParam) -> pos
                    else
                        dg->rwingstatus = pos*0.01;  // HIWORD(wParam) -> pos
                    dg->ApplyDamage ();
                    UpdateDamage (hTab, dg);
                    return;  // return TRUE counterpart
                }
                break;
            }
        });
    }
}

// --------------------------------------------------------------
// Add vessel-specific pages into scenario editor
// --------------------------------------------------------------
DLLCLBK void secInit (QWidget *hEditor, OBJHANDLE vessel)  // HWND -> QWidget *
{
    DeltaGliderXR1 *dg = static_cast<DeltaGliderXR1 *>(oapiGetVesselInterface(vessel));
    
    EditorPageSpec eps1 = {"Animations", g_hDLL, IDD_EDITOR_PG1, EdPg1Proc};
    ScnEditorMsg (hEditor, SE_ADDPAGEBUTTON, reinterpret_cast<LPARAM>(&eps1));  // SendMessage WM_SCNEDITOR counterpart
    EditorPageSpec eps3 = {"Damage", g_hDLL, IDD_EDITOR_PG3, EdPg3Proc};
    ScnEditorMsg (hEditor, SE_ADDPAGEBUTTON, reinterpret_cast<LPARAM>(&eps3));  // SendMessage WM_SCNEDITOR counterpart
}
