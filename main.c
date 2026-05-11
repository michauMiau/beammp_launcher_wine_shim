#include <winsock2.h>
#include <libloaderapi.h>
#include <windows.h>

#include <MinHook.h>

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

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
	\
	int enable_status = MH_EnableHook(name); \
	if (enable_status != MH_OK){ \
		LOG("%s: failed enabling hook for %s, %d\n", __func__, STR(name), enable_status); \
		exit(1); \
	} \
	LOG("%s: hooked %s\n", __func__, STR(name)); \
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

void hook_networking(){
	HOOK(socket);
	HOOK(recv);

	return;
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

void hook_resource_write(){
	HOOK(CopyFileW);
	HOOK(MoveFileExW);

	return;
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
	hook_networking();
	hook_resource_write();

	LOG("%s: ready\n", __func__);

	return 0;
}
