#pragma once

#include "database.hpp"

#include <map>

#include "resource.h"
#include "windows/utility.hpp"
#include "dialog_edit.hpp"
#include "dialog_history.hpp"
#include "dialog_password.hpp"
#include "dialog_showdatabase.hpp"
#include "dialog_mergetext.hpp"
#include "dialog_autostart.hpp"

struct MainDialogCreateParams
{
	LoginDatabase* database;
	std::string* filename;
	bool* timeoutQuit;
};

enum class TimerId : std::uint32_t
{
	CLEAR_CLIPBOARD = 0x72,
	CHECK_INACTIVE_TIME,
};

enum class HotkeyAction : std::uint32_t
{
	TYPE_TEXT,
	COPY_TO_CLIPBOARD,
};

enum class ClipboardState : std::uint32_t
{
	CLEAN,
	USERNAME_COPIED,
	PASSWORD_COPIED,
};

void sendKeyInput(WORD scan, bool character, bool keyUp)
{
	INPUT input = {};

	input.type = INPUT_KEYBOARD;
	input.ki.wVk = character ? 0 : scan;
	input.ki.wScan = character ? scan : 0;
	input.ki.dwFlags = (character ? KEYEVENTF_UNICODE : 0) | (keyUp ? KEYEVENTF_KEYUP : 0);

	SendInput(1, &input, sizeof(INPUT));
}

void writeString(HWND, const std::string& str)
{
	sendKeyInput(VK_CONTROL, false, true);
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	sendKeyInput('B', false, true);
	std::this_thread::sleep_for(std::chrono::milliseconds(10));

	auto wstr = toWideString(str);

	for (const auto& c : wstr)
	{
		sendKeyInput(c, true, false);
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		sendKeyInput(c, true, true);
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	SecureZeroMemory(&wstr[0], wstr.size() * sizeof wstr[0]);
}

class MainDialog
{
	static constexpr std::uint64_t INVALID_SELECTION = 0;

	std::unique_ptr<NotifyIcon> _notifyIcon;
	LoginDatabase* _database;
	std::string* _storeFilename;
	bool* _timeoutQuit;
	UINT _taskbarCreatedMessage;
	ClipboardState _clipboardState = ClipboardState::CLEAN;
	std::uint64_t _selection = INVALID_SELECTION;
	std::chrono::steady_clock::time_point _lastTypeTime = {};
	HotkeyAction _hotkeyAction = HotkeyAction::TYPE_TEXT;
	std::uint32_t _typeTimeout = 2000;
	std::uint32_t _clipboardTimeUsername = 4000;
	std::uint32_t _clipboardTimePassword = 2000;
	std::uint32_t _timeoutTime = 20 * 60 * 1000;
	bool _showHiddenEntries = false;

public:
	MainDialog(MainDialogCreateParams* params, HWND hwnd)
		: _database(params->database)
		, _storeFilename(params->filename)
		, _timeoutQuit(params->timeoutQuit)
		, _taskbarCreatedMessage(RegisterWindowMessageW(L"TaskbarCreated"))
	{
		remakeNotifyIcon(hwnd);
		SetTimer(hwnd, static_cast<UINT_PTR>(TimerId::CHECK_INACTIVE_TIME), 1000, nullptr);

		char path[MAX_PATH];
		SHGetSpecialFolderPathA(nullptr, path, CSIDL_APPDATA, true);
		auto filepath = std::string(path) + "\\passchain";

		CreateDirectoryA(filepath.c_str(), nullptr);

		filepath += "\\passchain.cfg";

		PropertyNode node(nullptr, "");
		node.setSaveOnDestruct(filepath);
		node.parse(readFileBinaryAsString(filepath));
		node.loadOrStore("hotkey_action", reinterpret_cast<std::uint32_t&>(_hotkeyAction));
		node.loadOrStore("type_timeout_ms", _typeTimeout);
		node.loadOrStore("clipboard_timeout_username_ms", _clipboardTimeUsername);
		node.loadOrStore("clipboard_timeout_password_ms", _clipboardTimePassword);
		node.loadOrStore("standby_timeout_inactivity_ms", _timeoutTime);
		node.loadOrStore("show_hidden_entries", _showHiddenEntries);
	}

	LoginDatabase& database()
	{
		return *_database;
	}

	std::string* storeFilename()
	{
		return _storeFilename;
	}

	std::uint32_t timeoutTime() const
	{
		return _timeoutTime;
	}

	void setTimeoutQuit(bool value)
	{
		*_timeoutQuit = value;
	}
	
	void swapShowHiddenEntries()
	{
		_showHiddenEntries = !_showHiddenEntries;
	}

	void writeDatabaseToFile()
	{
		auto buf = _database->serializeBinary();
		FileHandle file(std::fopen(_storeFilename->c_str(), "wb"));
		std::fwrite(buf.data(), 1, buf.size(), file.get());
	}

	UINT taskbarCreatedMessage() const
	{
		return _taskbarCreatedMessage;
	}

	void remakeNotifyIcon(HWND hwnd)
	{
		_notifyIcon = nullptr; // We need to delete it first.
		_notifyIcon = std::make_unique<NotifyIcon>(hwnd, WM_TRAYNOTIFY, MAKEINTRESOURCEW(ICON_DEFAULT));
	}

	void updateSelection(int index)
	{
		auto data = database().getEntry(static_cast<std::size_t>(index));

		if (data)
		{
			_selection = data->uniqueId;
		}
	}

	void copyUsernameToClipboard(LoginData& data)
	{
		auto guard = database().transformGuard(data);
		copyToClipboard(data.snapshots.back().username);
	}
	
	void copyPasswordToClipboard(LoginData& data)
	{
		auto guard = database().transformGuard(data);
		copyToClipboard(data.snapshots.back().password);
	}

	void hotkeyPressed(HWND hwnd)
	{
		auto data = database().findEntry(_selection);

		if (data != nullptr)
		{
			switch (_hotkeyAction)
			{
			default:
			case HotkeyAction::TYPE_TEXT:
			{
				auto guard = database().transformGuard(*data);

				if (_lastTypeTime.time_since_epoch().count() != 0 && 
					std::chrono::duration_cast<std::chrono::milliseconds>
					(std::chrono::steady_clock::now() - _lastTypeTime).count() < _typeTimeout)
				{
					writeString(GetForegroundWindow(), data->snapshots.back().password);
					_lastTypeTime = {};
				}
				else
				{
					writeString(GetForegroundWindow(), data->snapshots.back().username);
					_lastTypeTime = std::chrono::steady_clock::now();
				}
			}	break;

			case HotkeyAction::COPY_TO_CLIPBOARD:
			{
				switch (_clipboardState)
				{
				default:
				case ClipboardState::CLEAN:
				{
					copyUsernameToClipboard(*data);
					_clipboardState = ClipboardState::USERNAME_COPIED;
					KillTimer(hwnd, static_cast<UINT_PTR>(TimerId::CLEAR_CLIPBOARD));
					SetTimer(hwnd, static_cast<UINT_PTR>(TimerId::CLEAR_CLIPBOARD), _clipboardTimeUsername, nullptr);
				}	break;
				case ClipboardState::USERNAME_COPIED:
				case ClipboardState::PASSWORD_COPIED:
				{
					copyPasswordToClipboard(*data);
					_clipboardState = ClipboardState::PASSWORD_COPIED;
					KillTimer(hwnd, static_cast<UINT_PTR>(TimerId::CLEAR_CLIPBOARD));
					SetTimer(hwnd, static_cast<UINT_PTR>(TimerId::CLEAR_CLIPBOARD), _clipboardTimePassword, nullptr);
				}	break;
				}
			}	break;
			}
		}
		else
		{
			showMessageBox("Warning", "Unable to find selected database entry.");
		}
	}

	void clearClipboard(HWND hwnd)
	{
		KillTimer(hwnd, static_cast<UINT_PTR>(TimerId::CLEAR_CLIPBOARD));
		_clipboardState = ClipboardState::CLEAN;

		auto data = database().findEntry(_selection);

		if (data)
		{
			auto clipboardString = copyFromClipboard();
			auto guard = database().transformGuard(*data);

			if (data->snapshots.back().username == clipboardString ||
				data->snapshots.back().password == clipboardString)
			{
				copyToClipboard("");
				SecureZeroMemory(&clipboardString[0], clipboardString.size()); // [0] is always valid.
			}
		}
		else
		{
			copyToClipboard("");
		}
	}

	void updateListbox(HWND hwnd)
	{
		auto hlist = GetDlgItem(hwnd, DIALOG_MAIN_LISTBOX_LIST);

		SendMessageW(hlist, WM_SETREDRAW, false, 0);

		ListBox_ResetContent(hlist);

		for (std::size_t i = 0;; ++i)
		{
			auto data = database().getEntry(i);

			if (data == nullptr)
			{
				break;
			}

			auto name = data->name;

			if (name.size() == 0)
			{
				name = "[Empty Name]";
			}

			if (data->hide)
			{
				if (!_showHiddenEntries)
				{
					continue;
				}
				else
				{
					name += " [H]";
				}
			}

			ListBox_AddString(hlist, toWideString(name).c_str());
		}

		SendMessageW(hlist, WM_SETREDRAW, true, 0);
	}
	
	void updateSearchStringAndListbox(HWND hwnd)
	{
		database().sort(getWindowText(GetDlgItem(hwnd, DIALOG_MAIN_EDIT_SEARCH)));
		updateListbox(hwnd);
	}

	void hideOrShowEntry(HWND hwnd, int index)
	{
		auto data = database().getEntry(static_cast<std::size_t>(index));

		if (data != nullptr)
		{
			data->hide = !data->hide;
			PostMessageW(hwnd, WM_CHANGES_SAVED, 0, 0);
		}
		else
		{
			showMessageBox("Error", "Invalid database entry.");
		}
	}
};

INT_PTR CALLBACK dialogProcMain(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{	
	auto dialog = reinterpret_cast<MainDialog*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

	switch (message)
	{
	default:
	{
		if (dialog != nullptr && message == dialog->taskbarCreatedMessage())
		{
			dialog->remakeNotifyIcon(hwnd);
		}
	}	break;

	case WM_DESTROY:
	{
		ShowWindow(hwnd, SW_HIDE);
		delete dialog;
		PostQuitMessage(0);
	}	return true;

	case WM_CLOSE:
	{
		DestroyWindow(hwnd);
	}	return true;

	case WM_INITDIALOG:
	{
		SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(
			LoadIconW(GetModuleHandle(nullptr), MAKEINTRESOURCEW(ICON_DEFAULT))));

		auto menu = LoadMenuW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(MENU_MAINDIALOG));

		if (!SetMenu(hwnd, menu))
		{
			showMessageBox("Warning", "Failed to set context menu.");
		}

		centerWindowOnScreen(hwnd);

		auto params = reinterpret_cast<MainDialogCreateParams*>(lparam);
		dialog = new MainDialog(params, hwnd);
		SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LPARAM>(dialog));

		dialog->updateSearchStringAndListbox(hwnd);

		RegisterHotKey(hwnd, 0, MOD_CONTROL, 'B');
	}	return true;

	case WM_CHANGES_SAVED:
	{
		dialog->writeDatabaseToFile();
		dialog->updateSearchStringAndListbox(hwnd);
	}	return true;

	case WM_SYSCOMMAND:
	{
		switch (wparam)
		{
		default:
		{}	break;

		case SC_MINIMIZE:
		{
			ShowWindow(hwnd, SW_HIDE);
		}	return true;
		}
	}	break;

	case WM_HOTKEY:
	{
		if (wparam == 0)
		{
			dialog->hotkeyPressed(hwnd);
			return true;
		}
	}	break;

	case WM_POWERBROADCAST:
	{
		switch (wparam)
		{
		default:
		{}	break;

		case PBT_APMSUSPEND:
		{
			dialog->setTimeoutQuit(true);
			PostMessageW(hwnd, WM_CLOSE, 0, 0);
		}	return true;

		case PBT_APMRESUMEAUTOMATIC:
		{
			showMessageBox("Warning", "System returned from low powered state, "
				"but entering this state wasn't detected.");
		}	return true;
		}

	}	break;

	case WM_TIMER:
	{
		switch (static_cast<TimerId>(wparam))
		{
		default:
		{}	break;

		case TimerId::CLEAR_CLIPBOARD:
		{
			dialog->clearClipboard(hwnd);
		}	return true;

		case TimerId::CHECK_INACTIVE_TIME:
		{
			LASTINPUTINFO lastInputInfo = { sizeof(LASTINPUTINFO) };
			if (GetLastInputInfo(&lastInputInfo))
			{
				// Don't use GetTickCount64() here, since there's no 64-bit
				// GetLastInputInfo(), we rely on both counters to overflow.
				if (dialog->timeoutTime() > 0 &&
					dialog->timeoutTime() < (static_cast<std::int64_t>(GetTickCount()) -
						static_cast<std::int64_t>(lastInputInfo.dwTime)))
				{
					dialog->setTimeoutQuit(true);
					PostMessageW(hwnd, WM_CLOSE, 0, 0);
				}
			}
			else
			{ // Always timeout if this fails
				dialog->setTimeoutQuit(true);
				PostMessageW(hwnd, WM_CLOSE, 0, 0);
			}
		}	return true;
		}
	}	break;

	case WM_CONTEXTMENU:
	{
		const auto hlist = GetDlgItem(hwnd, DIALOG_MAIN_LISTBOX_LIST);

		if (reinterpret_cast<HWND>(wparam) == hlist)
		{
			POINT sp;
			sp.x = GET_X_LPARAM(lparam);
			sp.y = GET_Y_LPARAM(lparam);

			POINT cp = sp;
			ScreenToClient(hlist, &cp);

			auto index = SendMessage(hlist, LB_ITEMFROMPOINT, 0, MAKELPARAM(cp.x, cp.y));
			ListBox_SetCurSel(hlist, LOWORD(index));

			auto hmenu = LoadMenu(GetModuleHandle(nullptr), MAKEINTRESOURCE(MENU_LISTBOXCONTEXT));
			auto htrack = GetSubMenu(hmenu, 0);
			TrackPopupMenu(htrack, TPM_TOPALIGN | TPM_LEFTALIGN, sp.x, sp.y, 0, hwnd, nullptr);

			return true;
		}
	}	break;

	case WM_TRAYNOTIFY:
	{
		switch (lparam)
		{
		default:
		{}	break;

		case WM_RBUTTONUP:
		{
			POINT p;
			GetCursorPos(&p);

			auto hmenu = LoadMenuW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(MENU_TRAY));
			auto htrack = GetSubMenu(hmenu, 0);

			switch (TrackPopupMenu(htrack, TPM_RIGHTALIGN | TPM_RETURNCMD, p.x, p.y, 0, hwnd, nullptr))
			{
			default:
				break;

			case MENU_TRAY_SHOW:
				ShowWindow(hwnd, SW_SHOW);
				break;

			case MENU_TRAY_STANDBY:
			{
				dialog->setTimeoutQuit(true);
				PostMessageW(hwnd, WM_CLOSE, 0, 0);
			}	break;

			case MENU_TRAY_CLOSE:
				PostMessageW(hwnd, WM_CLOSE, 0, 0);
				break;
			}
		}	return true;

		case WM_LBUTTONDBLCLK:
		{
			ShowWindow(hwnd, SW_SHOW);
		}	return true;
		}
	}	break;

	case WM_COMMAND:
	{
		switch (LOWORD(wparam))
		{
		default:
		{}	break;

		case MENU_MAINDIALOG_FILE_ADD_ENTRY:
		{
			ListBox_SetCurSel(GetDlgItem(hwnd, DIALOG_MAIN_LISTBOX_LIST), -1);
			CreateDialogParamW(nullptr, MAKEINTRESOURCEW(DIALOG_EDITDATA), hwnd,
				dialogProcEdit, reinterpret_cast<LPARAM>(&dialog->database()));
		}	return true;

		case MENU_MAINDIALOG_FILE_STANDBY:
		{
			dialog->setTimeoutQuit(true);
			PostMessageW(hwnd, WM_CLOSE, 0, 0);
		}	return true;

		case MENU_MAINDIALOG_FILE_EXIT:
		{
			PostMessageW(hwnd, WM_CLOSE, 0, 0);
		}	return true;

		case MENU_MAINDIALOG_TOOLS_CONFIGURE_AUTOSTART:
		{
			CreateDialogParamW(nullptr, MAKEINTRESOURCEW(DIALOG_AUTOSTART), hwnd,
				dialogProcAutostart, reinterpret_cast<LPARAM>(dialog->storeFilename()));
		}	return true;

		case MENU_MAINDIALOG_TOOLS_SHOW_HIDDEN_ENTRIES:
		{
			dialog->swapShowHiddenEntries();
			dialog->updateListbox(hwnd);
		}	return true;

		case MENU_MAINDIALOG_TOOLS_SHOW_DATABASE:
		{
			CreateDialogParamW(nullptr, MAKEINTRESOURCEW(DIALOG_SHOWDATABASE), 
				hwnd, dialogProcShowDatabase, reinterpret_cast<LPARAM>(&dialog->database()));
		}	return true;

		case MENU_MAINDIALOG_TOOLS_MERGE_TEXT:
		{
			CreateDialogParamW(nullptr, MAKEINTRESOURCEW(DIALOG_MERGETEXT),
				hwnd, dialogProcMergeText, reinterpret_cast<LPARAM>(&dialog->database()));
		}	return true;

		case MENU_MAINDIALOG_ABOUT_INFO:
		{
			MessageBoxW(nullptr, L"https://github.com/cooky451/passchain", L"Help", MB_OK);
		}	return true;

		case DIALOG_MAIN_EDIT_SEARCH:
		{
			if (HIWORD(wparam) == EN_CHANGE)
			{
				dialog->updateSearchStringAndListbox(hwnd);
			}
		}	return true;

		case DIALOG_MAIN_BUTTON_COPY_RESET:
		{
			dialog->updateSelection(0);
			SetDlgItemTextW(hwnd, DIALOG_MAIN_EDIT_SEARCH, L"");
			dialog->updateSearchStringAndListbox(hwnd);
		}	return true;

		case DIALOG_MAIN_BUTTON_ADD:
		{
			ListBox_SetCurSel(GetDlgItem(hwnd, DIALOG_MAIN_LISTBOX_LIST), -1);
			CreateDialogParamW(nullptr, MAKEINTRESOURCEW(DIALOG_EDITDATA), hwnd,
				dialogProcEdit, reinterpret_cast<LPARAM>(&dialog->database()));
		}	return true;

		case DIALOG_MAIN_LISTBOX_LIST:
		{
			if (HIWORD(wparam) == LBN_DBLCLK)
			{
				auto index = ListBox_GetCurSel(GetDlgItem(hwnd, DIALOG_MAIN_LISTBOX_LIST));
				dialog->updateSelection(index);
			}
		}	return true;

		case MENU_LISTBOXCONTEXT_EDIT:
		{
			CreateDialogParamW(nullptr, MAKEINTRESOURCEW(DIALOG_EDITDATA), hwnd,
				dialogProcEdit, reinterpret_cast<LPARAM>(&dialog->database()));
		}	return true;

		case MENU_LISTBOXCONTEXT_HIDESHOW:
		{
			auto index = ListBox_GetCurSel(GetDlgItem(hwnd, DIALOG_MAIN_LISTBOX_LIST));
			dialog->hideOrShowEntry(hwnd, index);
		}	return true;
		}
	}	break;
	}

	return false;
}
