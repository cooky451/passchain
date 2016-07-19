#pragma once

#include "database.hpp"

#include "resource.h"
#include "windows/utility.hpp"

INT_PTR CALLBACK dialogProcAutostart(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	const auto makeAutostartKey = [&]() -> HKEY
	{
		HKEY hkey;
		auto result = RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft"
			"\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hkey);

		if (result != ERROR_SUCCESS)
		{
			showMessageBox("Error", "Unable to open registry key.");
			return nullptr;
		}

		return hkey;
	};

	const auto setAutostartKey = [&](const std::string& filename, bool standby)
	{
		auto key = makeAutostartKey();
		if (key != nullptr)
		{
			auto buffer = toWideString(getCurrentModuleFileName()) + L" " + toWideString(filename);

			if (standby)
			{
				buffer += L" standby";
			}

			RegSetValueExW(key, L"passchain", 0, REG_SZ, 
				reinterpret_cast<const BYTE*>(&buffer[0]), 
				static_cast<DWORD>(buffer.size() * sizeof buffer[0]));
		}
	};

	auto filename = reinterpret_cast<std::string*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

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
	}	return true;

	case WM_COMMAND:
	{
		switch (LOWORD(wparam))
		{
		default:
		{}	break;

		case DIALOG_AUTOSTART_BUTTON_NORMAL:
		{
			setAutostartKey(*filename, false);
			PostMessageW(hwnd, WM_CLOSE, 0, 0);
		}	return true;

		case DIALOG_AUTOSTART_BUTTON_STANDBY:
		{
			setAutostartKey(*filename, true);
			PostMessageW(hwnd, WM_CLOSE, 0, 0);
		}	return true;

		case DIALOG_AUTOSTART_BUTTON_REMOVE:
		{
			auto hkey = makeAutostartKey();
			if (hkey != nullptr)
			{
				if (RegDeleteValueW(hkey, L"passchain") != ERROR_SUCCESS)
				{
					showMessageBox("Error", "Unable to delete registry value.");
				}
			}
			PostMessageW(hwnd, WM_CLOSE, 0, 0);
		}	return true;

		case DIALOG_AUTOSTART_BUTTON_CANCEL:
		{
			PostMessageW(hwnd, WM_CLOSE, 0, 0);
		}	return true;
		}
	}	break;

	}

	return false;
}

