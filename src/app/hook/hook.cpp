#include "hook.hpp"
#include "logger/logger.hpp"
#include "fs/fs.hpp"

bool hook::load(process_t proc)
{

#ifdef _M_AMD64
	if (!proc.arch.compare("x86"))
	{
		ShellExecuteA(0, "open", "x86\\helper.exe", &logger::va("--pid %i --arch %s --hwnd %u", proc.pid, &proc.arch[0], proc.hwnd)[0], 0, 1);
		return true;
	}
#else
	if (!proc.arch.compare("x86_64"))
	{
		global::msg_box("Radio.Garten Overlay", "Unable to inject into x64 application in x86 mode!\nPlease consider using the x64 build.");
		return false;
	}
#endif

	for (const std::string& dll : hook::dlls)
	{
#ifdef _M_AMD64
		std::string dll_path = fs::get_cur_dir().append(logger::va("x86_64\\%s", &dll[0]));
#else
		std::string dll_path = fs::get_cur_dir().append(logger::va("x86\\%s", &dll[0]));
#endif
		logger::log_debug(logger::va("Loading %s", &dll_path[0]));

		HANDLE handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, proc.pid);
		if (!handle)
		{
			logger::log("HOOK_ERR", "Failed to open target");
			return false;
		}

		LPVOID alloc = VirtualAllocEx(handle, 0, dll_path.length(), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
		if (!alloc)
		{
			logger::log("HOOK_ERR", "Failed to allocate memory in target");
			return false;
		}

		if (!WriteProcessMemory(handle, alloc, (LPVOID)&dll_path[0], dll_path.length(), 0))
		{
			logger::log("HOOK_ERR", "Failed to write memory in target");
			return false;
		}

		DWORD thread_id;
		LPVOID loadlib = (LPVOID)GetProcAddress(LoadLibraryA("kernel32.dll"), "LoadLibraryA");
		if (!CreateRemoteThread(handle, 0, 0, (LPTHREAD_START_ROUTINE)loadlib, alloc, 0, &thread_id))
		{
			logger::log("HOOK_ERR", "Failed to create remote thread");
			return false;
		}
	}

	//Not sure one does the trick but they all sound nice
	BringWindowToTop(proc.hwnd);
	SetForegroundWindow(proc.hwnd);
	SetFocus(proc.hwnd);
	//Set to top most temporarily
	SetWindowPos(proc.hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_DRAWFRAME | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
	//Set back
	SetWindowPos(proc.hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_DRAWFRAME | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
	ShowWindow(proc.hwnd, SW_NORMAL);

	return true;
}

int CALLBACK hook::get_window(HWND hWnd, LPARAM lparam)
{
	bool do_not_add = false;
	int length = GetWindowTextLengthA(hWnd);
	char* buffer = new char[length + 1];
	GetWindowTextA(hWnd, buffer, length + 1);
	std::string window_title(buffer);

	if (IsWindowVisible(hWnd) && length != 0)
	{
		std::string arch;
		std::string exe;
		std::uint32_t proc_id;

		//Get proc_id
		GetWindowThreadProcessId(hWnd, (LPDWORD)&proc_id);

		//Get handle for arch detection
		if (HANDLE handle = OpenProcess(PROCESS_ALL_ACCESS, 0, proc_id))
		{
			int target = 0;
			if (IsWow64Process(handle, &target))
			{
				if (!target)
				{
					arch = "x86_64";
				}
				else
				{
					arch = "x86";
				}
			}
			else
			{
				arch = "???";
				logger::log_error(logger::va("Unable to determine arch! (0x%x)", GetLastError()));
			}

			char exe_path[MAX_PATH];
			K32GetModuleFileNameExA(handle, 0, exe_path, sizeof(exe_path));
			exe = exe_path;

			for (std::string look_up : hook::blacklist)
			{
				if (exe.find(look_up) != std::string::npos)
				{
					logger::log_debug(logger::va("Found %s [%s]", &look_up[0], &exe[0]));
					do_not_add = true;
					break;
				}
			}
		}
		else if(!handle)
		{
			arch = "???";
			logger::log_error("Unable to open proc for arch detection!");
		}

#ifndef _M_AMD64
		if (!arch.compare("x86_64"))
		{
			do_not_add = true;
		}
#endif
		if (!do_not_add)
		{
			hook::processes.emplace_back(process_t{ window_title, arch, std::string(exe), proc_id, hWnd });
		}
	}

	return 1;
}

void hook::get_procs()
{
	hook::processes.clear();

	EnumWindows(hook::get_window, 0);
}

std::vector<process_t> hook::processes;

std::initializer_list<std::string> hook::blacklist
{
	"explorer.exe",
	"radio.garten.exe",
	"ApplicationFrameHost.exe",
	"ShellExperienceHost.exe",
	"Discord.exe",
	"Video.UI.exe",
	"TextInputHost.exe",
	"SystemSettings.exe",
	"Calculator.exe",
	"devenv.exe",
};

std::initializer_list<std::string> hook::dlls
{
	"bass.dll",
	"discord_game_sdk.dll",
#ifdef _M_AMD64
	"overlay.radio.garten.x86_64.dll",
#else
	"overlay.radio.garten.x86.dll",
#endif
};

bool hook::auto_refresh = false;