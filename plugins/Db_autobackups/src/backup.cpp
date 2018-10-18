#include "stdafx.h"

static UINT_PTR timer_id = 0;
static volatile long g_iState = 0;

static LRESULT CALLBACK DlgProcPopup(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_COMMAND:
		{
			wchar_t *ptszPath = (wchar_t*)PUGetPluginData(hWnd);
			if (ptszPath != nullptr)
				ShellExecute(nullptr, L"open", ptszPath, nullptr, nullptr, SW_SHOW);

			PUDeletePopup(hWnd);
			break;
		}
	case WM_CONTEXTMENU:
		PUDeletePopup(hWnd);
		break;

	case UM_FREEPLUGINDATA:
		mir_free(PUGetPluginData(hWnd));
		break;
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

static void ShowPopup(const wchar_t *ptszText, wchar_t *ptszHeader, wchar_t *ptszPath)
{
	POPUPDATAT ppd = { 0 };

	wcsncpy_s(ppd.lptzText, ptszText, _TRUNCATE);
	wcsncpy_s(ppd.lptzContactName, ptszHeader, _TRUNCATE);
	if (ptszPath != nullptr)
		ppd.PluginData = (void*)mir_wstrdup(ptszPath);
	ppd.PluginWindowProc = DlgProcPopup;
	ppd.lchIcon = IcoLib_GetIcon(iconList[0].szName);

	PUAddPopupT(&ppd);
}

static INT_PTR CALLBACK DlgProcProgress(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM)
{
	switch (msg) {
	case WM_INITDIALOG:
		TranslateDialogDefault(hwndDlg);
		SendDlgItemMessage(hwndDlg, IDC_PROGRESS, PBM_SETPOS, 0, 0);
		break;
	case WM_COMMAND:
		if (HIWORD(wParam) != BN_CLICKED || LOWORD(wParam) != IDCANCEL)
			break;
		// in the progress dialog, use the user data to indicate that the user has pressed cancel
		SetWindowLongPtr(hwndDlg, GWLP_USERDATA, 1);
		return TRUE;
		break;
	}
	return FALSE;
}

static bool MakeZip_Dir(LPCWSTR szDir, LPCWSTR pwszProfile, LPCWSTR szDest, LPCWSTR pwszBackupFolder, HWND progress_dialog)
{
	HWND hProgBar = GetDlgItem(progress_dialog, IDC_PROGRESS);
	size_t count = 0, folderNameLen = mir_wstrlen(pwszBackupFolder);
	OBJLIST<ZipFile> lstFiles(15);

	wchar_t wszTempName[MAX_PATH];
	if (!GetTempPathW(_countof(wszTempName), wszTempName))
		return false;

	if (!GetTempFileNameW(wszTempName, L"mir_backup_", 0, wszTempName))
		return false;

	if (db_get_current()->Backup(wszTempName))
		return false;

	lstFiles.insert(new ZipFile(wszTempName, pwszProfile));

	CMStringW wszProfile;
	wszProfile.Format(L"%s\\%s", szDir, pwszProfile);

	for (auto it = fs::recursive_directory_iterator(fs::path(szDir)); it != fs::recursive_directory_iterator(); ++it) {
		const auto& file = it->path();
		if (fs::is_directory(file))
			continue;

		const std::wstring &filepath = file.wstring();
		if (wszProfile == filepath.c_str())
			continue;

		if (filepath.find(szDest) != std::wstring::npos || !mir_wstrncmpi(filepath.c_str(), pwszBackupFolder, folderNameLen))
			continue;

		const std::wstring rpath = filepath.substr(filepath.find(szDir) + mir_wstrlen(szDir) + 1);
		lstFiles.insert(new ZipFile(filepath, rpath));
		count++;
	}

	if (count == 0)
		return 1;

	CreateZipFile(szDest, lstFiles, [&](size_t i)->bool {
		SendMessage(hProgBar, PBM_SETPOS, (WPARAM)(100 * i / count), 0);
		return GetWindowLongPtr(progress_dialog, GWLP_USERDATA) != 1;
	});

	DeleteFileW(wszTempName);
	return 1;
}

static bool MakeZip(wchar_t *tszDest, wchar_t *dbname, HWND progress_dialog)
{
	HWND hProgBar = GetDlgItem(progress_dialog, IDC_PROGRESS);

	wchar_t wszTempName[MAX_PATH];
	if (!GetTempPathW(_countof(wszTempName), wszTempName))
		return false;

	if (!GetTempFileNameW(wszTempName, L"mir_backup_", 0, wszTempName))
		return false;

	if (db_get_current()->Backup(wszTempName))
		return false;

	OBJLIST<ZipFile> lstFiles(1);
	lstFiles.insert(new ZipFile(wszTempName, dbname));

	CreateZipFile(tszDest, lstFiles, [&](size_t)->bool { SendMessage(hProgBar, PBM_SETPOS, (WPARAM)(100), 0); return true; });
	DeleteFileW(wszTempName);
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////

struct backupFile
{
	wchar_t Name[MAX_PATH];
	FILETIME CreationTime;
};

static int Comp(const void *i, const void *j)
{
	backupFile *pi = (backupFile*)i;
	backupFile *pj = (backupFile*)j;

	if (pi->CreationTime.dwHighDateTime > pj->CreationTime.dwHighDateTime ||
		(pi->CreationTime.dwHighDateTime == pj->CreationTime.dwHighDateTime && pi->CreationTime.dwLowDateTime > pj->CreationTime.dwLowDateTime))
		return -1;
	return 1;
}

static int RotateBackups(wchar_t *backupfolder, wchar_t *dbname)
{
	if (g_plugin.num_backups == 0) // Rotation disabled?
		return 0; 

	backupFile *bf = nullptr, *bftmp;

	wchar_t backupfolderTmp[MAX_PATH];
	mir_snwprintf(backupfolderTmp, L"%s\\%s*.%s", backupfolder, dbname, g_plugin.use_zip ? L"zip" : L"dat");

	WIN32_FIND_DATA FindFileData;
	HANDLE hFind = FindFirstFile(backupfolderTmp, &FindFileData);
	if (hFind == INVALID_HANDLE_VALUE)
		return 0;

	int i = 0;
	do {
		if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			continue;
		bftmp = (backupFile*)mir_realloc(bf, ((i + 1) * sizeof(backupFile)));
		if (bftmp == nullptr)
			goto err_out;
		bf = bftmp;
		wcsncpy_s(bf[i].Name, FindFileData.cFileName, _TRUNCATE);
		bf[i].CreationTime = FindFileData.ftCreationTime;
		i++;
	}
		while (FindNextFile(hFind, &FindFileData));

	// Sort the list of found files by date in descending order.
	if (i > 0)
		qsort(bf, i, sizeof(backupFile), Comp);
	
	for (; i >= g_plugin.num_backups; i--) {
		mir_snwprintf(backupfolderTmp, L"%s\\%s", backupfolder, bf[(i - 1)].Name);
		DeleteFile(backupfolderTmp);
	}

err_out:
	FindClose(hFind);
	mir_free(bf);
	return 0;
}

static int Backup(wchar_t *backup_filename)
{
	bool bZip = false;
	wchar_t dbname[MAX_PATH], dest_file[MAX_PATH];
	HWND progress_dialog = nullptr;

	Profile_GetNameW(_countof(dbname), dbname);

	wchar_t backupfolder[MAX_PATH];
	PathToAbsoluteW(VARSW(g_plugin.folder), backupfolder);

	// ensure the backup folder exists (either create it or return non-zero signifying error)
	int err = CreateDirectoryTreeW(backupfolder);
	if (err != ERROR_ALREADY_EXISTS && err != 0) {
		mir_free(backupfolder);
		return 1;
	}

	if (backup_filename == nullptr) {
		bZip = g_plugin.use_zip != 0;
		RotateBackups(backupfolder, dbname);

		SYSTEMTIME st;
		GetLocalTime(&st);

		wchar_t buffer[MAX_COMPUTERNAME_LENGTH + 1];
		DWORD size = _countof(buffer);
		GetComputerName(buffer, &size);
		mir_snwprintf(dest_file, L"%s\\%s_%02d.%02d.%02d@%02d-%02d-%02d_%s.%s", backupfolder, dbname, st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, buffer, bZip ? L"zip" : L"dat");
	}
	else {
		wcsncpy_s(dest_file, backup_filename, _TRUNCATE);
		if (!mir_wstrcmp(wcsrchr(backup_filename, '.'), L".zip"))
			bZip = true;
	}
	
	if (!g_plugin.disable_popups)
		ShowPopup(dbname, TranslateT("Backup in progress"), nullptr);

	if (!g_plugin.disable_progress)
		progress_dialog = CreateDialog(g_plugin.getInst(), MAKEINTRESOURCE(IDD_COPYPROGRESS), nullptr, DlgProcProgress);

	SetDlgItemText(progress_dialog, IDC_PROGRESSMESSAGE, TranslateT("Copying database file..."));

	BOOL res;
	if (bZip) {
		res = g_plugin.backup_profile
			? MakeZip_Dir(VARSW(L"%miranda_userdata%"), dbname, dest_file, backupfolder, progress_dialog)
			: MakeZip(dest_file, dbname, progress_dialog);
	}
	else res = db_get_current()->Backup(dest_file) == ERROR_SUCCESS;

	if (res) {
		if (!bZip) { // Set the backup file to the current time for rotator's correct work
			SYSTEMTIME st;
			GetSystemTime(&st);

			HANDLE hFile = CreateFile(dest_file, FILE_WRITE_ATTRIBUTES, NULL, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
			FILETIME ft;
			SystemTimeToFileTime(&st, &ft);
			SetFileTime(hFile, nullptr, nullptr, &ft);
			CloseHandle(hFile);
		}
		
		SendDlgItemMessage(progress_dialog, IDC_PROGRESS, PBM_SETPOS, (WPARAM)(100), 0);
		UpdateWindow(progress_dialog);
		g_plugin.setDword("LastBackupTimestamp", (DWORD)time(0));

		if (g_plugin.use_cloudfile) {
			CFUPLOADDATA ui = { g_plugin.cloudfile_service, dest_file, L"Backups" };
			if (CallService(MS_CLOUDFILE_UPLOAD, (LPARAM)&ui))
				ShowPopup(TranslateT("Uploading to cloud failed"), TranslateT("Error"), nullptr);
		}

		wchar_t *pd = wcsrchr(dest_file, '\\');

		if (!g_plugin.disable_popups) {
			CMStringW puText;

			if (pd && mir_wstrlen(dest_file) > 50) {
				puText.Append(dest_file, pd - dest_file);
				puText.AppendChar('\n');
				puText.Append(pd + 1);
			}
			else puText = dest_file;

			// Now we need to know, which folder we made a backup. Let's break unnecessary variables :)
			if (pd) *pd = 0;
			ShowPopup(puText, TranslateT("Database backed up"), dest_file);
		}
	}
	else DeleteFileW(dest_file);

	DestroyWindow(progress_dialog);
	return 0;
}

static void BackupThread(void *backup_filename)
{
	Backup((wchar_t*)backup_filename);
	InterlockedExchange(&g_iState, 0); // Backup done.
	mir_free(backup_filename);
}

void BackupStart(wchar_t *backup_filename)
{
	LONG cur_state = InterlockedCompareExchange(&g_iState, 1, 0);
	if (cur_state != 0) { // Backup allready in process.
		ShowPopup(TranslateT("Database back up in process..."), TranslateT("Error"), nullptr);
		return;
	}
	
	wchar_t *tm = mir_wstrdup(backup_filename);
	if (mir_forkthread(BackupThread, tm) == INVALID_HANDLE_VALUE) {
		InterlockedExchange(&g_iState, 0); // Backup done.
		mir_free(tm);
	}
}

VOID CALLBACK TimerProc(HWND, UINT, UINT_PTR, DWORD)
{
	time_t t = time(0);
	time_t diff = t - (time_t)g_plugin.getDword("LastBackupTimestamp");
	if (diff > (time_t)(g_plugin.period * (g_plugin.period_type == PT_MINUTES ? 60 : (g_plugin.period_type == PT_HOURS ? (60 * 60) : (60 * 60 * 24)))))
		BackupStart(nullptr);
}

int SetBackupTimer(void)
{
	if (timer_id != 0) {
		KillTimer(nullptr, timer_id);
		timer_id = 0;
	}
	if (g_plugin.backup_types & BT_PERIODIC)
		timer_id = SetTimer(nullptr, 0, (1000 * 60), TimerProc);
	return 0;
}
