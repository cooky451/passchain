/* 
 * 
 */

#include "windows/utility.hpp"
#include "utility/utility.hpp"
#include "dialog_main.hpp"

#include <string>
#include <vector>

std::vector<std::string> getArgs(PWSTR cmdline)
{
	int argc;
	auto argv = CommandLineToArgvW(cmdline, &argc);

	std::vector<std::string> args;

	for (int i = 0; i < argc; ++i)
	{
		args.push_back(toUtf8(argv[i]));
	}

	return args;
}

int WINAPI wWinMain(HINSTANCE hinstance, HINSTANCE /* hprevinstance */, PWSTR cmdline, int /* cmdshow */)
{
	//AllocConsole();
	//freopen("CONOUT$", "w", stdout);

	// CommandLineToArgvW() is not usable because it
	// sets args[0] to GetModuleFileName if the app
	// is being called without parameters, but not
	// if the app is being called with parameters.
	auto args = parseWords(toUtf8(cmdline));

	auto standby = args.size() > 1 && args[1] == "standby";

	const auto instanceString = L"XKXjdwnFYYL2eVdB";
	auto singleInstanceEvent = HandlePtr(OpenEventW(EVENT_ALL_ACCESS, false, instanceString));

	if (singleInstanceEvent != nullptr)
	{
		if (!standby)
		{
			auto decision = MessageBox(nullptr, L"There is already an instance "
				"of passchain running. Start anyway?", L"Warning", MB_OKCANCEL);

			if (decision != IDOK)
			{
				return 0;
			}
		}
	}
	else
	{
		singleInstanceEvent = HandlePtr(CreateEventW(nullptr, false, false, instanceString));
	}

	std::unique_ptr<LoginDatabase> database;
	std::string password;
	std::string filename = args.size() > 0 ? args[0] : "passchain.dat";

	while (database == nullptr)
	{
		try
		{
			PasswordDialogCreateParams params;
			params.password = &password;
			params.filename = &filename;
			params.standby = standby;

			auto result = DialogBoxParamW(hinstance, MAKEINTRESOURCEW(DIALOG_PASSWORD), nullptr,
				dialogProcPassword, reinterpret_cast<LPARAM>(&params));

			if (result == 0)
			{ // Pressed cancel or close button
				return 0;
			}

			database = std::make_unique<LoginDatabase>(std::move(password));

			auto attributes = GetFileAttributesW(toWideString(filename).c_str());

			if (attributes != INVALID_FILE_ATTRIBUTES)
			{
				database->mergeFromEncryptedFile(filename);
			}
		}
		catch (std::exception& e)
		{
			database = nullptr;
			showMessageBox("Error", e.what());
		}
	}

	bool timeoutQuit = false;

	MainDialogCreateParams mainParams;
	mainParams.database = database.get();
	mainParams.filename = &filename;
	mainParams.timeoutQuit = &timeoutQuit;

	CreateDialogParamW(hinstance, MAKEINTRESOURCE(DIALOG_MAIN), nullptr, 
		dialogProcMain, reinterpret_cast<LPARAM>(&mainParams));

	MSG message;

	while (GetMessageW(&message, nullptr, 0, 0) > 0)
	{
		database->reseedRng(&message, sizeof message);

		if (!IsDialogMessageW(GetParent(message.hwnd), &message))
		{
			TranslateMessage(&message);
			DispatchMessageW(&message);
		}
	}

	if (timeoutQuit)
	{
		auto moduleName = getCurrentModuleFileName();
		auto callString = formatString("%s %s standby", moduleName.c_str(), filename.c_str());

		STARTUPINFOA startupInfo = { sizeof startupInfo };
		PROCESS_INFORMATION processInformation;

		if (0 == CreateProcessA(nullptr, &callString[0], nullptr, nullptr,
			false, 0, nullptr, nullptr, &startupInfo, &processInformation))
		{
			showMessageBox("Warning", "Failed to create standby process.");
		}
	}

	return static_cast<int>(message.wParam);
}
