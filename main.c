#include <winsock2.h>
#include <libloaderapi.h>
#include <memoryapi.h>
#include <psapi.h>
#include <windows.h>

#include <MinHook.h>

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include <wctype.h>

#define LOG_FILE "./beammp_launcher_win_shim.log"
#define STR(s) #s

#define HOOK(name) { \
	int create_status = MH_CreateHook(name, name##_hooked, (void **)&name##_orig); \
	if (create_status != MH_OK){ \
		LOG("%s: failed creating hook for %s, %d\n", __func__, STR(name), create_status); \
		exit(1); \
	} \
	\
	int enable_status = MH_EnableHook(name); \
	if (enable_status != MH_OK){ \
		LOG("%s: failed enabling hook for %s, %d\n", __func__, STR(name), enable_status); \
		exit(1); \
	} \
	LOG("%s: hooked %s\n", __func__, STR(name)); \
}

#define HOOK_API(mod_name, name) { \
	void *target; \
	int create_status = MH_CreateHookApiEx(mod_name, STR(name), name##_hooked, (void **)&name##_orig, &target); \
	if (create_status != MH_OK){ \
		LOG("%s: failed creating hook for %ls %s, %d\n", __func__, mod_name, STR(name), create_status); \
		exit(1); \
	} \
	\
	int enable_status = MH_EnableHook(target); \
	if (enable_status != MH_OK){ \
		LOG("%s: failed enabling hook for %ls %s, %d\n", __func__, mod_name, STR(name), enable_status); \
		exit(1); \
	} \
	LOG("%s: hooked %ls %s\n", __func__, mod_name, STR(name)); \
}

static LONG WINAPI (*WinVerifyTrust_real)(HWND hwnd,GUID *pgActionID,LPVOID pWVTData) = NULL;
LONG WINAPI WinVerifyTrust(HWND hwnd,GUID *pgActionID,LPVOID pWVTData){
	return WinVerifyTrust_real(hwnd, pgActionID, pWVTData);
}

void init_log(){
	FILE *log_file = fopen(LOG_FILE, "wb");
	if(log_file != NULL){
		fclose(log_file);
	}
}

void LOG(const char *fmt, ...){
	FILE *log_file = fopen(LOG_FILE, "ab");
	if (log_file == NULL){
		printf("log file open failed\n");
		return;
	}

	va_list args;
	va_start(args, fmt);
	vfprintf(log_file, fmt, args);
	va_end(args);

	fclose(log_file);
}

static SOCKET WSAAPI (*socket_orig)(int af,int type,int protocol) = NULL;
SOCKET WSAAPI socket_hooked(int af,int type,int protocol){
	SOCKET s = socket_orig(af, type, protocol);
	if (s == INVALID_SOCKET){
		return s;
	}

	int opt = 0;
	int optlen = sizeof(opt);
	getsockopt(s, SOL_SOCKET, SO_RCVBUF, (char *)&opt, &optlen);
	LOG("%s: SO_RCVBUF is %d\n", __func__, opt);

	optlen = sizeof(opt);
	getsockopt(s, SOL_SOCKET, SO_RCVBUF, (char *)&opt, &optlen);
	LOG("%s: SO_SNDBUF is %d\n", __func__, opt);

	opt = 212992 * 4;
	setsockopt(s, SOL_SOCKET, SO_RCVBUF, (char *)&opt, sizeof(opt));
	setsockopt(s, SOL_SOCKET, SO_SNDBUF, (char *)&opt, sizeof(opt));

	optlen = sizeof(opt);
	getsockopt(s, SOL_SOCKET, SO_RCVBUF, (char *)&opt, &optlen);
	LOG("%s: SO_RCVBUF is now %d\n", __func__, opt);

	optlen = sizeof(opt);
	getsockopt(s, SOL_SOCKET, SO_RCVBUF, (char *)&opt, &optlen);
	LOG("%s: SO_SNDBUF is now %d\n", __func__, opt);

	return s;
}

int WSAAPI (*recv_orig)(SOCKET s,char *buf,int len,int flags);
int WSAAPI recv_hooked(SOCKET s,char *buf,int len,int flags){
	if(flags != MSG_WAITALL){
		return recv_orig(s, buf, len, flags);
	}

	int offset = 0;
	while(offset < len){
		int recv_status = recv_orig(s, &buf[offset], len - offset, flags);
		if (recv_status == 0){
			return 0;
		}
		if (recv_status == -1){
			return -1;
		}
		offset += recv_status;
	}
	return offset;
}

void init_minhook(){
	MH_STATUS minhook_init_status = MH_Initialize();
	if (minhook_init_status != MH_OK && minhook_init_status != MH_ERROR_ALREADY_INITIALIZED){
		LOG("%s: failed initializing minhook, %d\n", __func__, minhook_init_status);
		exit(1);
	}
}

WINBOOL WINAPI (*CopyFileW_orig) (LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName, WINBOOL bFailIfExists) = NULL;
WINBOOL WINAPI CopyFileW_hooked (LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName, WINBOOL bFailIfExists){
	int to_len = wcslen(lpNewFileName);
	wchar_t to_lowered[1024] = {0};
	for(int i = 0;i < to_len;i++){
		to_lowered[i] = towlower(lpNewFileName[i]);
	}

	char from_buf[2048] = {0};
	char to_buf[2048] = {0};

	wcstombs(from_buf, lpExistingFileName, sizeof(from_buf) - 1);
	wcstombs(to_buf, to_lowered, sizeof(to_buf) - 1);

	LOG("%s: copying from %s to %s\n", __func__, from_buf, to_buf);
	return CopyFileW_orig(lpExistingFileName, to_lowered, bFailIfExists);
}

WINBOOL WINAPI (*MoveFileExW_orig) (LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName, DWORD dwFlags) = NULL;
WINBOOL WINAPI MoveFileExW_hooked (LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName, DWORD dwFlags){
	int to_len = wcslen(lpNewFileName);
	wchar_t to_lowered[1024] = {0};
	for(int i = 0;i < to_len;i++){
		to_lowered[i] = towlower(lpNewFileName[i]);
	}

	char from_buf[2048] = {0};
	char to_buf[2048] = {0};

	wcstombs(from_buf, lpExistingFileName, sizeof(from_buf) - 1);
	wcstombs(to_buf, to_lowered, sizeof(to_buf) - 1);

	LOG("%s: moving from %s to %s\n", __func__, from_buf, to_buf);
	return MoveFileExW_orig(lpExistingFileName, to_lowered, dwFlags);
}

uint8_t *find_pattern(uint8_t *pattern, int pattern_size, uint8_t *begin, int size){
	for(int offset = 0;offset + pattern_size <= size;offset++){
		if (memcmp(&begin[offset], pattern, pattern_size) == 0){
			return &begin[offset];
		}
	}
	return NULL;
}

bool find_and_patch_pattern(uint8_t *pattern, uint8_t *patch, int pattern_size, uint8_t *begin, int size){
	uint8_t *location = find_pattern(pattern, pattern_size, begin, size);
	if (location == NULL){
		return false;
	}

	DWORD orig_prot;
	VirtualProtect(location, pattern_size, PAGE_EXECUTE_READWRITE, &orig_prot);
	memcpy(location, patch, pattern_size);
	VirtualProtect(location, pattern_size, orig_prot, &orig_prot);
	return true;
}

void patch_mod_path(){
	const static wchar_t pattern[] = LR"(mods\multiplayer\BeamMP.zip)";
	const static wchar_t patch[] = LR"(mods\multiplayer\beammp.zip)";

	HMODULE module_handles[512] = {0};
	DWORD returned_size = 0;
	EnumProcessModules(GetCurrentProcess(), module_handles, sizeof(module_handles), &returned_size);

	HMODULE main_module_handle = NULL;
	int num_modules = returned_size / sizeof(HMODULE);

	for(int i = 0;i < num_modules;i++){
		char base_name[512] = {0};
		DWORD name_get_status = GetModuleBaseNameA(GetCurrentProcess(), module_handles[i], base_name, sizeof(base_name));
		if (name_get_status == 0){
			LOG("%s: failed getting name of module 0x%lx, 0x%x\n", __func__, module_handles[i], GetLastError());
			continue;
		}
		LOG("%s: module %s\n", __func__, base_name);
		if (strcmp(base_name, "BeamMP-Launcher.exe") == 0){
			main_module_handle = module_handles[i];
			break;
		}
	}

	if (main_module_handle == NULL){
		LOG("%s: main module not found\n", __func__);
		return;
	}

	MODULEINFO modinfo = {0};
	GetModuleInformation(GetCurrentProcess(), main_module_handle, &modinfo, sizeof(modinfo));

	bool patch_status = find_and_patch_pattern((uint8_t *)pattern, (uint8_t *)patch, sizeof(pattern), (uint8_t*)modinfo.lpBaseOfDll, modinfo.SizeOfImage);

	if (patch_status){
		LOG("%s: mod path patched\n", __func__);
	}else{
		LOG("%s: mod path not patched\n", __func__);
		exit(1);
	}
}

WINBOOL WINAPI (*CreateProcessW_orig) (LPCWSTR lpApplicationName, LPWSTR lpCommandLine, LPSECURITY_ATTRIBUTES lpProcessAttributes, LPSECURITY_ATTRIBUTES lpThreadAttributes, WINBOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment, LPCWSTR lpCurrentDirectory, LPSTARTUPINFOW lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation) = NULL;
WINBOOL WINAPI CreateProcessW_hooked (LPCWSTR lpApplicationName, LPWSTR lpCommandLine, LPSECURITY_ATTRIBUTES lpProcessAttributes, LPSECURITY_ATTRIBUTES lpThreadAttributes, WINBOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment, LPCWSTR lpCurrentDirectory, LPSTARTUPINFOW lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation){
	LOG("%s: %ls\n", __func__, lpCommandLine);
	char cur_command_buf[2048] = {0};
	wcstombs(cur_command_buf, lpCommandLine, sizeof(cur_command_buf) - 1);
	if (strstr(cur_command_buf, "BeamNG.drive") == NULL){
		return CreateProcessW_orig(lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes, bInheritHandles, dwCreationFlags, lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation);
	}
	wchar_t override_buf[2048] = {0};
	wchar_t *new_cmd = lpCommandLine;
	const char *override = getenv("BEAMMP_LAUNCH_OVERRIDE");
	if (override != NULL){
		LOG("%s: overriding launch command %ls to %s\n", __func__, lpCommandLine, override);
		mbstowcs(override_buf, override, sizeof(override_buf) - 2);
		new_cmd = override_buf;
	}
	return CreateProcessW_orig(lpApplicationName, new_cmd, lpProcessAttributes, lpThreadAttributes, bInheritHandles, dwCreationFlags, lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation);
}

void hook_functions(){
	HOOK(socket);
	HOOK(recv);
	HOOK(CopyFileW);
	HOOK(MoveFileExW);
	HOOK(CreateProcessW);
}

__attribute__((constructor))
int init(){
	init_log();

	char win_dir[1024] = {0};
	GetWindowsDirectoryA(win_dir, sizeof(win_dir) - 1);
	char dll_path[1024] = {0};
	sprintf(dll_path, "%s\\system32\\wintrust.dll", win_dir);
	HANDLE real_dll = LoadLibraryA(dll_path);
	WinVerifyTrust_real = (void *)GetProcAddress(real_dll, "WinVerifyTrust");

	init_minhook();
	hook_functions();
	patch_mod_path();

	LOG("%s: ready\n", __func__);
	printf("%s: beammp_launcher_win_shim ready\n", __func__);

	return 0;
}
