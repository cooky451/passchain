#pragma once

#include <string>

#include "database.hpp"

#include "windows/utility.hpp"

INT_PTR CALLBACK dialogProcShowDatabase(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
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
		SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(
			LoadIconW(GetModuleHandle(nullptr), MAKEINTRESOURCEW(ICON_DEFAULT))));

		// Full database is going to be given to the OS in text form, 
		// no reason to try to SecureZeroMemory() anything at this point.
		auto database = reinterpret_cast<LoginDatabase*>(lparam);
		setWindowText(GetDlgItem(hwnd, DIALOG_SHOWDATABASE_EDIT_TEXT), 
			insertCarriageReturns(database->serializeText()));
	}	return true;

	case WM_COMMAND:
	{
		switch (LOWORD(wparam))
		{
		default:
		{}	break;

		case DIALOG_SHOWDATABASE_BUTTON_COPY:
		{
			copyToClipboard(getWindowText(GetDlgItem(hwnd, DIALOG_SHOWDATABASE_EDIT_TEXT)));
		}	return true;
		}
	}	break;
	}

	return false;
}
