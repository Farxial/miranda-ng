#include "stdafx.h"
#include "resource.h"

static void AutoSize(HWND hwnd)
{
  HDC hDC = GetDC(hwnd);
  HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
  HFONT hOldFont = (HFONT)SelectObject(hDC, hFont);

  wchar_t szBuf[MAX_PATH];
  int i = GetWindowText(hwnd, szBuf, _countof(szBuf));

  SIZE tS;
  GetTextExtentPoint32(hDC, szBuf, i, &tS);
  SelectObject(hDC, hOldFont);
  DeleteObject(hFont);
  ReleaseDC(hwnd, hDC);
  SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, tS.cx + 10, tS.cy, SWP_NOMOVE | SWP_FRAMECHANGED);
}

////////////////////////////////////////////////////////////////////////////////////////////

static wchar_t* COM_OKSTR[2] = {
	LPGENW("Problem, registration missing/deleted."),
	LPGENW("Successfully created shell registration.") };
static wchar_t* COM_APPROVEDSTR[2] = { LPGENW("Not Approved"), LPGENW("Approved") };

static void InitControls(HWND hwndDlg)
{
	int comReg = IsCOMRegistered();

	wchar_t szBuf[MAX_PATH];
	mir_snwprintf(szBuf, L"%s (%s)",
		TranslateW(COM_OKSTR[(comReg & COMREG_OK) != 0]),
		TranslateW(COM_APPROVEDSTR[(comReg & COMREG_APPROVED) != 0]));
	SetDlgItemText(hwndDlg, IDC_STATUS, szBuf);
	
	// auto size the static windows to fit their text
	// they're rendering in a font not selected into the DC.
	AutoSize(GetDlgItem(hwndDlg, IDC_CAPMENUS));
	AutoSize(GetDlgItem(hwndDlg, IDC_CAPSTATUS));
	AutoSize(GetDlgItem(hwndDlg, IDC_CAPSHLSTATUS));
	
	// show all the options
	int iCheck = g_plugin.getByte(SHLExt_UseGroups, BST_UNCHECKED);
	CheckDlgButton(hwndDlg, IDC_USEGROUPS, iCheck ? BST_CHECKED : BST_UNCHECKED);
	EnableWindow(GetDlgItem(hwndDlg, IDC_CLISTGROUPS), iCheck = BST_CHECKED);
	CheckDlgButton(hwndDlg, IDC_CLISTGROUPS, g_plugin.getByte(SHLExt_UseCListSetting, BST_UNCHECKED));
	CheckDlgButton(hwndDlg, IDC_NOPROF, g_plugin.getByte(SHLExt_ShowNoProfile, BST_UNCHECKED));
	CheckDlgButton(hwndDlg, IDC_SHOWFULL, g_plugin.getByte(SHLExt_UseHITContacts, BST_UNCHECKED));
	CheckDlgButton(hwndDlg, IDC_SHOWINVISIBLES, g_plugin.getByte(SHLExt_UseHIT2Contacts, BST_UNCHECKED));
	CheckDlgButton(hwndDlg, IDC_USEOWNERDRAW, g_plugin.getByte(SHLExt_ShowNoIcons, BST_UNCHECKED));
	CheckDlgButton(hwndDlg, IDC_HIDEOFFLINE, g_plugin.getByte(SHLExt_ShowNoOffline, BST_UNCHECKED));
	
	// give the Remove button a Vista icon
	SendDlgItemMessage(hwndDlg, IDC_REMOVE, BCM_SETSHIELD, 0, 1);
}

static INT_PTR CALLBACK OptDialogProc(HWND hwndDlg, UINT wMsg, WPARAM wParam, LPARAM lParam)
{
	switch (wMsg) {
	case WM_INITDIALOG:
		TranslateDialogDefault(hwndDlg);
		InitControls(hwndDlg);
		return TRUE;

	case WM_NOTIFY:
		if (((LPNMHDR)lParam)->code == PSN_APPLY) {
			g_plugin.setByte(SHLExt_UseGroups, IsDlgButtonChecked(hwndDlg, IDC_USEGROUPS));
			g_plugin.setByte(SHLExt_UseCListSetting, IsDlgButtonChecked(hwndDlg, IDC_CLISTGROUPS));
			g_plugin.setByte(SHLExt_ShowNoProfile, IsDlgButtonChecked(hwndDlg, IDC_NOPROF));
			g_plugin.setByte(SHLExt_UseHITContacts, IsDlgButtonChecked(hwndDlg, IDC_SHOWFULL));
			g_plugin.setByte(SHLExt_UseHIT2Contacts, IsDlgButtonChecked(hwndDlg, IDC_SHOWINVISIBLES));
			g_plugin.setByte(SHLExt_ShowNoIcons, IsDlgButtonChecked(hwndDlg, IDC_USEOWNERDRAW));
			g_plugin.setByte(SHLExt_ShowNoOffline, IsDlgButtonChecked(hwndDlg, IDC_HIDEOFFLINE));
		}
		break;

	case WM_COMMAND:
		// don't send the changed message if remove is clicked
		if (LOWORD(wParam) != IDC_REMOVE)
			SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);

		switch (LOWORD(wParam)) {
		case IDC_USEGROUPS:
			EnableWindow(GetDlgItem(hwndDlg, IDC_CLISTGROUPS), BST_CHECKED == IsDlgButtonChecked(hwndDlg, IDC_USEGROUPS));
			break;
		case IDC_REMOVE:
			if (IDYES == MessageBox(nullptr,
				TranslateT("Are you sure? This will remove all the settings stored in your database and all registry entries created for shlext to work with Explorer"),
				TranslateT("Disable/Remove shlext"), MB_YESNO | MB_ICONQUESTION)) {
				g_plugin.delSetting(SHLExt_UseGroups);
				g_plugin.delSetting(SHLExt_UseCListSetting);
				g_plugin.delSetting(SHLExt_UseHITContacts);
				g_plugin.delSetting(SHLExt_UseHIT2Contacts);
				g_plugin.delSetting(SHLExt_ShowNoProfile);
				g_plugin.delSetting(SHLExt_ShowNoIcons);
				g_plugin.delSetting(SHLExt_ShowNoOffline);

				CheckUnregisterServer();
				InitControls(hwndDlg);
			}
		}
		break;
	}

	return 0;
}

int OnOptionsInit(WPARAM wParam, LPARAM)
{
	OPTIONSDIALOGPAGE opt = { sizeof(opt) };
	opt.flags = ODPF_BOLDGROUPS;
	opt.szGroup.a = LPGEN("Services");
	opt.szTitle.a = LPGEN("Shell context menus");
	opt.position = -1066;
	opt.pszTemplate = MAKEINTRESOURCEA(IDD_SHLOPTS);
	opt.pfnDlgProc = OptDialogProc;
	g_plugin.addOptions(wParam, &opt);
	return 0;
}
