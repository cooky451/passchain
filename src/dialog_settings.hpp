#pragma once

#include "database.hpp"

#include "resource.h"
#include "windows/utility.hpp"

enum class AutostartSetting : std::uint32_t
{
	DISABLE, 
	START_NORMALLY, 
	START_IN_STANDBY, 
};

struct HotkeySettings
{
	char character;
	bool shift;
	bool control;
	bool alt;
};

struct ProgramSettings
{
	AutostartSetting autostartSetting = AutostartSetting::DISABLE;

	std::uint32_t clipboardUsernameTimeout = 4000;
	std::uint32_t clipboardPasswordTimeout = 2000;
	std::uint32_t autotyperTimeout = 2000;
	std::uint32_t idleTimeout = 20 * 60 * 1000;

	HotkeySettings clipboardHotkeySettings = { 'B', false, true, false };
	HotkeySettings autotyperHotkeySettings = { 'Q', false, true, false };

	bool showHiddenEntries = false;
};

inline int shortTimeoutToIndex(std::uint32_t timeout)
{
	return static_cast<int>(std::max(0.0, std::min(4.0, std::round(timeout / 1000.0) - 1.0)));
}

inline std::uint32_t indexToShortTimeout(int index)
{
	return static_cast<std::uint32_t>(std::max(1, std::min(10000, (index + 1) * 1000)));
}

inline int longTimeoutToIndex(std::uint32_t timeout)
{
	if (timeout == 0)
		return 0;

	if (timeout < (60 + 30) * 1000)
		return 1;

	if (timeout < (120 + 30) * 1000)
		return 2;

	if (timeout < (180 + 30) * 1000)
		return 3;

	if (timeout < (300 + 60) * 1000)
		return 4;

	if (timeout < (10 + 5) * 60 * 1000)
		return 5;

	if (timeout < (20 + 5) * 60 * 1000)
		return 6;

	if (timeout < (30 + 10) * 60 * 1000)
		return 7;

	return 8;
}

inline std::uint32_t indexToLongTimeout(int index)
{
	switch (index)
	{
	default:
		return 10 * 60 * 1000;
	case 0:
		return 0;
	case 1:
		return 1 * 60 * 1000;
	case 2:
		return 2 * 60 * 1000;
	case 3:
		return 3 * 60 * 1000;
	case 4:
		return 5 * 60 * 1000;
	case 5:
		return 10 * 60 * 1000;
	case 6:
		return 20 * 60 * 1000;
	case 7:
		return 30 * 60 * 1000;
	case 8:
		return 60 * 60 * 1000;
	}
}

inline HKEY makeAutostartKey()
{
	HKEY hkey;
	auto result = RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft"
		"\\Windows\\CurrentVersion\\Run", 0, KEY_ALL_ACCESS, &hkey);

	if (result != ERROR_SUCCESS)
	{
		showMessageBox("Error", "Unable to open registry key.");
		return nullptr;
	}

	return hkey;
}

inline void setAutostart(HKEY hkey, const std::string& filename, bool standby)
{
	if (hkey != nullptr)
	{
		auto buffer = toWideString(getCurrentModuleFileName()) + L" " + toWideString(filename);

		if (standby)
		{
			buffer += L" standby";
		}

		RegSetValueExW(hkey, L"passchain", 0, REG_SZ,
			reinterpret_cast<const BYTE*>(&buffer[0]),
			static_cast<DWORD>(buffer.size() * sizeof buffer[0]));
	}
}

inline AutostartSetting getAutostartSetting(HKEY hkey)
{
	if (hkey != nullptr)
	{
		wchar_t buffer[0x202] = {};
		DWORD bufsize = static_cast<DWORD>(sizeof buffer - 2);
		DWORD type = REG_SZ;
		auto ret = RegQueryValueExW(hkey, L"passchain", 0, &type, 
			reinterpret_cast<BYTE*>(&buffer[0]), &bufsize);

		if (ret == ERROR_SUCCESS && type == REG_SZ)
		{
			auto words = parseWords(toUtf8(buffer));

			if (words.size() == 0)
			{
				return AutostartSetting::DISABLE;
			}

			if (words.back() == "standby")
			{
				return AutostartSetting::START_IN_STANDBY;
			}

			return AutostartSetting::START_NORMALLY;
		}
	}

	return AutostartSetting::DISABLE;
}

INT_PTR CALLBACK dialogProcSettings(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	auto settings = reinterpret_cast<ProgramSettings*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

	switch (message)
	{
	default:
	{}	break;

	case WM_CLOSE:
	{
		DestroyWindow(hwnd);
	}	return true;

	case WM_INITDIALOG:
	{
		SetWindowLongPtrW(hwnd, GWLP_USERDATA, lparam);
		settings = reinterpret_cast<ProgramSettings*>(lparam);

		auto comboHandle = GetDlgItem(hwnd, DIALOG_SETTINGS_COMBOBOX_AUTOSTART);

		ComboBox_AddItemData(comboHandle, L"No autostart");
		ComboBox_AddItemData(comboHandle, L"Start normally");
		ComboBox_AddItemData(comboHandle, L"Start in standby");
		ComboBox_SetCurSel(comboHandle, static_cast<int>(settings->autostartSetting));

		comboHandle = GetDlgItem(hwnd, DIALOG_SETTINGS_COMBOBOX_USERNAME_TIMEOUT);

		ComboBox_AddItemData(comboHandle, L"1 second");
		ComboBox_AddItemData(comboHandle, L"2 seconds");
		ComboBox_AddItemData(comboHandle, L"3 seconds");
		ComboBox_AddItemData(comboHandle, L"4 seconds");
		ComboBox_AddItemData(comboHandle, L"5 seconds");
		ComboBox_SetCurSel(comboHandle, shortTimeoutToIndex(settings->clipboardUsernameTimeout));

		comboHandle = GetDlgItem(hwnd, DIALOG_SETTINGS_COMBOBOX_PASSWORD_TIMEOUT);

		ComboBox_AddItemData(comboHandle, L"1 second");
		ComboBox_AddItemData(comboHandle, L"2 seconds");
		ComboBox_AddItemData(comboHandle, L"3 seconds");
		ComboBox_AddItemData(comboHandle, L"4 seconds");
		ComboBox_AddItemData(comboHandle, L"5 seconds");
		ComboBox_SetCurSel(comboHandle, shortTimeoutToIndex(settings->clipboardPasswordTimeout));

		comboHandle = GetDlgItem(hwnd, DIALOG_SETTINGS_COMBOBOX_AUTOTYPER_TIMEOUT);

		ComboBox_AddItemData(comboHandle, L"1 second");
		ComboBox_AddItemData(comboHandle, L"2 seconds");
		ComboBox_AddItemData(comboHandle, L"3 seconds");
		ComboBox_AddItemData(comboHandle, L"4 seconds");
		ComboBox_AddItemData(comboHandle, L"5 seconds");
		ComboBox_SetCurSel(comboHandle, shortTimeoutToIndex(settings->autotyperTimeout));

		comboHandle = GetDlgItem(hwnd, DIALOG_SETTINGS_COMBOBOX_IDLE_TIMEOUT);

		ComboBox_AddItemData(comboHandle, L"(none)");
		ComboBox_AddItemData(comboHandle, L"1 minute");
		ComboBox_AddItemData(comboHandle, L"2 minutes");
		ComboBox_AddItemData(comboHandle, L"3 minutes");
		ComboBox_AddItemData(comboHandle, L"5 minutes");
		ComboBox_AddItemData(comboHandle, L"10 minutes");
		ComboBox_AddItemData(comboHandle, L"20 minutes");
		ComboBox_AddItemData(comboHandle, L"30 minutes");
		ComboBox_AddItemData(comboHandle, L"1 hour");
		ComboBox_SetCurSel(comboHandle, longTimeoutToIndex(settings->idleTimeout));

		for (wchar_t c = 'A'; c <= 'Z'; ++c)
		{
			wchar_t buf[2] = { c, '\0' };
			ComboBox_AddItemData(GetDlgItem(hwnd, DIALOG_SETTINGS_COMBOBOX_CLIPBOARD_HOTKEY), buf);
			ComboBox_AddItemData(GetDlgItem(hwnd, DIALOG_SETTINGS_COMBOBOX_AUTOTYPER_HOTKEY), buf);
		}

		{
			wchar_t buf0[2] = { static_cast<wchar_t>(settings->clipboardHotkeySettings.character), '\0' };
			ComboBox_SelectString(GetDlgItem(hwnd, DIALOG_SETTINGS_COMBOBOX_CLIPBOARD_HOTKEY), 0, buf0);

			wchar_t buf1[2] = { static_cast<wchar_t>(settings->autotyperHotkeySettings.character), '\0' };
			ComboBox_SelectString(GetDlgItem(hwnd, DIALOG_SETTINGS_COMBOBOX_AUTOTYPER_HOTKEY), 0, buf1);
		}

		SendMessageW(GetDlgItem(hwnd, DIALOG_SETTINGS_CHECKBOX_CLIPBOARD_HOTKEY_CTRL), BM_SETCHECK,
			settings->clipboardHotkeySettings.control ? BST_CHECKED : BST_UNCHECKED, 0);
		SendMessageW(GetDlgItem(hwnd, DIALOG_SETTINGS_CHECKBOX_CLIPBOARD_HOTKEY_SHIFT), BM_SETCHECK,
			settings->clipboardHotkeySettings.shift ? BST_CHECKED : BST_UNCHECKED, 0);
		SendMessageW(GetDlgItem(hwnd, DIALOG_SETTINGS_CHECKBOX_CLIPBOARD_HOTKEY_ALT), BM_SETCHECK,
			settings->clipboardHotkeySettings.alt ? BST_CHECKED : BST_UNCHECKED, 0);

		SendMessageW(GetDlgItem(hwnd, DIALOG_SETTINGS_CHECKBOX_AUTOTYPER_HOTKEY_CTRL), BM_SETCHECK,
			settings->autotyperHotkeySettings.control ? BST_CHECKED : BST_UNCHECKED, 0);
		SendMessageW(GetDlgItem(hwnd, DIALOG_SETTINGS_CHECKBOX_AUTOTYPER_HOTKEY_SHIFT), BM_SETCHECK,
			settings->autotyperHotkeySettings.shift ? BST_CHECKED : BST_UNCHECKED, 0);
		SendMessageW(GetDlgItem(hwnd, DIALOG_SETTINGS_CHECKBOX_AUTOTYPER_HOTKEY_ALT), BM_SETCHECK,
			settings->autotyperHotkeySettings.alt ? BST_CHECKED : BST_UNCHECKED, 0);
	}	return true;

	case WM_COMMAND:
	{
		switch (LOWORD(wparam))
		{
		default:
		{}	break;

		case DIALOG_SETTINGS_BUTTON_APPLY:
		{
			settings->autostartSetting = static_cast<AutostartSetting>(
				ComboBox_GetCurSel(GetDlgItem(hwnd, DIALOG_SETTINGS_COMBOBOX_AUTOSTART)));

			settings->clipboardUsernameTimeout = indexToShortTimeout(
				ComboBox_GetCurSel(GetDlgItem(hwnd, DIALOG_SETTINGS_COMBOBOX_USERNAME_TIMEOUT)));
			settings->clipboardPasswordTimeout = indexToShortTimeout(
				ComboBox_GetCurSel(GetDlgItem(hwnd, DIALOG_SETTINGS_COMBOBOX_PASSWORD_TIMEOUT)));
			settings->autotyperTimeout = indexToShortTimeout(
				ComboBox_GetCurSel(GetDlgItem(hwnd, DIALOG_SETTINGS_COMBOBOX_AUTOTYPER_TIMEOUT)));
			settings->idleTimeout = indexToLongTimeout(
				ComboBox_GetCurSel(GetDlgItem(hwnd, DIALOG_SETTINGS_COMBOBOX_IDLE_TIMEOUT)));

			settings->clipboardHotkeySettings.character = 'A' + 
				ComboBox_GetCurSel(GetDlgItem(hwnd, DIALOG_SETTINGS_COMBOBOX_CLIPBOARD_HOTKEY));
			settings->clipboardHotkeySettings.control = false !=
				SendMessageW(GetDlgItem(hwnd, DIALOG_SETTINGS_CHECKBOX_CLIPBOARD_HOTKEY_CTRL), BM_GETCHECK, 0, 0);
			settings->clipboardHotkeySettings.shift = false !=
				SendMessageW(GetDlgItem(hwnd, DIALOG_SETTINGS_CHECKBOX_CLIPBOARD_HOTKEY_SHIFT), BM_GETCHECK, 0, 0);
			settings->clipboardHotkeySettings.alt = false !=
				SendMessageW(GetDlgItem(hwnd, DIALOG_SETTINGS_CHECKBOX_CLIPBOARD_HOTKEY_ALT), BM_GETCHECK, 0, 0);

			settings->autotyperHotkeySettings.character = 'A' +
				ComboBox_GetCurSel(GetDlgItem(hwnd, DIALOG_SETTINGS_COMBOBOX_AUTOTYPER_HOTKEY));
			settings->autotyperHotkeySettings.control = false !=
				SendMessageW(GetDlgItem(hwnd, DIALOG_SETTINGS_CHECKBOX_AUTOTYPER_HOTKEY_CTRL), BM_GETCHECK, 0, 0);
			settings->autotyperHotkeySettings.shift = false !=
				SendMessageW(GetDlgItem(hwnd, DIALOG_SETTINGS_CHECKBOX_AUTOTYPER_HOTKEY_SHIFT), BM_GETCHECK, 0, 0);
			settings->autotyperHotkeySettings.alt = false !=
				SendMessageW(GetDlgItem(hwnd, DIALOG_SETTINGS_CHECKBOX_AUTOTYPER_HOTKEY_ALT), BM_GETCHECK, 0, 0);

			PostMessageW(GetParent(hwnd), WM_SETTINGS_APPLIED, 0, 0);
			PostMessageW(hwnd, WM_CLOSE, 0, 0);
		}	return true;

		case DIALOG_SETTINGS_BUTTON_CANCEL:
		{
			PostMessageW(hwnd, WM_CLOSE, 0, 0);
		}	return true;
		}
	}	break;

	}

	return false;
}

