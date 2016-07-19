#pragma once

#include "database.hpp"

#include "windows/utility.hpp"

INT_PTR CALLBACK dialogProcMergeText(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	auto parent = GetParent(hwnd);
	auto database = reinterpret_cast<LoginDatabase*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

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

		SetWindowLongPtrW(hwnd, GWLP_USERDATA, lparam);
		SendMessageW(GetDlgItem(hwnd, DIALOG_MERGETEXT_EDIT_TEXT), EM_LIMITTEXT, 0, 0);
	}	return true;

	case WM_COMMAND:
	{
		switch (LOWORD(wparam))
		{
		default:
		{}	break;

		case DIALOG_MERGETEXT_BUTTON_MERGE:
		{
			// The data will be everywhere in memory and there's nothing we can do about it.
			database->mergeFromText(getWindowText(GetDlgItem(hwnd, DIALOG_MERGETEXT_EDIT_TEXT)).c_str());
			PostMessageW(parent, WM_CHANGES_SAVED, 0, 0);
			PostMessageW(hwnd, WM_CLOSE, 0, 0);
		}	return true;
		}
	}	break;
	}

	return false;
}

