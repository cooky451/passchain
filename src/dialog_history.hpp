#pragma once

#include "utility/utility.hpp"

#include <ctime>
#include <string>

inline std::string makeHistoryString(const LoginData& data)
{
	std::string history;
	history.reserve(data.snapshots.size() * 2400);

	for (auto& sn : data.snapshots)
	{
		auto tm = *std::localtime(&sn.timestamp);

		history += formatString("---- ---- ---- ----\r\n"
			"Date:     %02d-%02d-%02d %02d:%02d:%02d\r\n"
			"Username: %s\r\n"
			"Password: %s\r\n",
			1900 + tm.tm_year, 1 + tm.tm_mon, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec,
			sn.username.c_str(),
			sn.password.c_str());
	}

	return history + "---- ---- ---- ----\r\n";
}

struct HistoryDialogCreateParams
{
	LoginDatabase* database;
	std::uint64_t uniqueId;
};

INT_PTR CALLBACK dialogProcHistory(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
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

		centerWindowOnScreen(hwnd);

		auto params = reinterpret_cast<HistoryDialogCreateParams*>(lparam);
		auto data = params->database->findEntry(params->uniqueId);

		if (data)
		{
			auto guard = params->database->transformGuard(*data);
			setWindowText(GetDlgItem(hwnd, DIALOG_HISTORY_EDIT_HISTORY), makeHistoryString(*data));
		}
		else
		{
			showMessageBox("Error", "Unable to find database entry.");
			PostMessageW(hwnd, WM_CLOSE, 0, 0);
		}
	}	return true;
	}

	return false;
}
