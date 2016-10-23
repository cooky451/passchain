#pragma once

#include "resource.h"
#include "windows/utility.hpp"

class PasswordDialogCreateParams
{
public:
	std::string* password = nullptr;
	std::string* filename = nullptr;
	std::unique_ptr<NotifyIcon> notifyIcon;
	UINT taskbarCreatedMessage = RegisterWindowMessageW(L"TaskbarCreated");
	bool standby = false;

	void remakeNotifyIcon(HWND hwnd)
	{
		notifyIcon = nullptr; // We need to delete it first.
		notifyIcon = std::make_unique<NotifyIcon>(hwnd, WM_TRAYNOTIFY, MAKEINTRESOURCEW(ICON_DEFAULT));
	}
};

INT_PTR CALLBACK dialogProcPassword(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	auto params = reinterpret_cast<PasswordDialogCreateParams*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

	switch (message)
	{
	default:
	{
		if (params != nullptr && message == params->taskbarCreatedMessage)
		{
			params->remakeNotifyIcon(hwnd);
		}
	}	break;

	case WM_CLOSE:
	{
		EndDialog(hwnd, 0);
	}	return true;

	case WM_INITDIALOG:
	{
		SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(
			LoadIconW(GetModuleHandle(nullptr), MAKEINTRESOURCEW(ICON_DEFAULT))));

		SetWindowLongPtrW(hwnd, GWLP_USERDATA, lparam);
		params = reinterpret_cast<PasswordDialogCreateParams*>(lparam);
		params->remakeNotifyIcon(hwnd);
		SetDlgItemTextW(hwnd, DIALOG_PASSWORD_EDIT_FILENAME, toWideString(*params->filename).c_str());

		if (params->standby)
		{
			// Prevents short flashing of window, it's only possible
			// to do this with this workaround for modal dialogs.
			SetWindowPos(hwnd, nullptr, 60000, 0, 0, 0, SWP_NOSIZE);
			PostMessageW(hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
		}
		else
		{
			centerWindowOnScreen(hwnd);
		}
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

	case WM_COMMAND:
	{
		switch (LOWORD(wparam))
		{
		default:
		{}	break;

		case DIALOG_PASSWORD_BUTTON_SELECTFILE:
		{
			wchar_t filename[0x1000] = {};
			OPENFILENAME ofn = {};
			ofn.lStructSize = sizeof ofn;
			ofn.hwndOwner = hwnd;
			ofn.lpstrFilter = L"Data Files\0*.dat\0All Files\0*.*\0\0";
			ofn.lpstrFile = filename;
			ofn.nMaxFile = sizeof filename / sizeof filename[0];
			ofn.Flags = OFN_PATHMUSTEXIST;

			if (GetOpenFileNameW(&ofn))
			{
				SetDlgItemTextW(hwnd, DIALOG_PASSWORD_EDIT_FILENAME, filename);
			}
		}	return true;

		case DIALOG_PASSWORD_BUTTON_OK:
		{
			*params->filename = getWindowText(GetDlgItem(hwnd, DIALOG_PASSWORD_EDIT_FILENAME));

			auto& password = *params->password;

			wchar_t buffer[2400];
			GetWindowTextW(GetDlgItem(hwnd, DIALOG_PASSWORD_EDIT_PASSWORD), buffer, sizeof buffer);

			SecureZeroMemory(&password[0], password.size()); // &password[0] is always valid
			password = toUtf8(buffer);
			SecureZeroMemory(buffer, sizeof buffer);

			EndDialog(hwnd, 1);
		}	return true;

		case DIALOG_PASSWORD_BUTTON_CANCEL:
		{
			EndDialog(hwnd, 0);
		}	return true;
		}
	}	break;

	case WM_TRAYNOTIFY:
	{
		if (wparam == 0)
		{
			switch (lparam)
			{
			default:
			{}	break;

			case WM_RBUTTONDOWN:
			{
				POINT p;
				GetCursorPos(&p);

				auto hmenu = LoadMenuW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(MENU_STANDBYTRAY));
				auto htrack = GetSubMenu(hmenu, 0);

				switch (TrackPopupMenu(htrack, TPM_RIGHTALIGN | TPM_RETURNCMD, p.x, p.y, 0, hwnd, nullptr))
				{
				default:
					break;

				case MENU_STANDBYTRAY_SHOW:
					if (params->standby)
					{
						centerWindowOnScreen(hwnd);
						params->standby = false;
					}

					ShowWindow(hwnd, SW_SHOW);
					break;

				case MENU_STANDBYTRAY_CLOSE:
					EndDialog(hwnd, 0);
					break;
				}
			}	return true;

			case WM_LBUTTONDBLCLK:
			{
				if (params->standby)
				{
					centerWindowOnScreen(hwnd);
					params->standby = false;
				}

				ShowWindow(hwnd, SW_SHOW);
				SetForegroundWindow(hwnd);
			}	return true;
			}
		}
	}	break;
	}

	return false;
}
