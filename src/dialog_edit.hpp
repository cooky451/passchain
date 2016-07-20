#pragma once

#include "database.hpp"

#include "dialog_history.hpp"

#include "resource.h"
#include "windows/utility.hpp"

struct EditDialogData
{
	LoginDatabase* database;
	std::uint64_t uniqueId;
};

void updatePasswordStrength(HWND hwnd, const PasswordGeneratorDesc& generatorDesc)
{
	auto strength = calculateBitStrength(generatorDesc);

	SetDlgItemInt(hwnd, DIALOG_EDITDATA_EDIT_PWSTRENGTH, 
		static_cast<UINT>(std::round(strength)), false);
}

PasswordGeneratorDesc makeGeneratorDesc(HWND hwnd)
{
	PasswordGeneratorDesc generatorDesc;

	generatorDesc.genLetters = false !=
		SendMessage(GetDlgItem(hwnd, DIALOG_EDITDATA_CHECKBOX_LETTERS), BM_GETCHECK, 0, 0);

	generatorDesc.genNumbers = false !=
		SendMessage(GetDlgItem(hwnd, DIALOG_EDITDATA_CHECKBOX_NUMBERS), BM_GETCHECK, 0, 0);

	generatorDesc.genSpecial = false !=
		SendMessage(GetDlgItem(hwnd, DIALOG_EDITDATA_CHECKBOX_SPECIAL), BM_GETCHECK, 0, 0);

	generatorDesc.genExtra = false !=
		SendMessage(GetDlgItem(hwnd, DIALOG_EDITDATA_CHECKBOX_EXTRA), BM_GETCHECK, 0, 0);

	auto n = GetDlgItemInt(hwnd, DIALOG_EDITDATA_EDIT_PWLENGTH, nullptr, false);
	generatorDesc.passwordLength = static_cast<std::uint16_t>(std::min(0xFFFFu, std::max(1u, n)));
	generatorDesc.extraAlphabet = getWindowText(GetDlgItem(hwnd, DIALOG_EDITDATA_EDIT_EXTRA));

	return generatorDesc;
}

void saveEditedData(HWND hwnd, EditDialogData* dialog)
{
	auto data = dialog->database->findEntry(dialog->uniqueId);
	
	while (data == nullptr)
	{
		LoginData newData;

		newData.uniqueId = dialog->database->makeUniqueId();

		if (newData.uniqueId == 0 || 
			dialog->database->findEntry(newData.uniqueId) != nullptr)
		{
			showMessageBox("Warning", "You should buy a lottery ticket.");
		}
		else
		{
			data = dialog->database->pushEntry(std::move(newData));
		}
	}

	auto guard = dialog->database->transformGuard(*data);	
	
	data->timestamp = std::time(nullptr);
	data->name = getWindowText(GetDlgItem(hwnd, DIALOG_EDITDATA_EDIT_NAME));
	data->comment = getWindowText(GetDlgItem(hwnd, DIALOG_EDITDATA_EDIT_COMMENT));
	data->generatorDesc = makeGeneratorDesc(hwnd);

	Snapshot snapshot;
	snapshot.username = getWindowText(GetDlgItem(hwnd, DIALOG_EDITDATA_EDIT_USERNAME));
	snapshot.password = getWindowText(GetDlgItem(hwnd, DIALOG_EDITDATA_EDIT_PASSWORD));
	snapshot.timestamp = std::time(nullptr);

	auto str = toWideString(snapshot.password);

	if (data->snapshots.size() == 0 || 
		data->snapshots.back().username != snapshot.username || 
		data->snapshots.back().password != snapshot.password)
	{
		data->snapshots.push_back(std::move(snapshot));
	}
}

INT_PTR CALLBACK dialogProcEdit(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	//static bool ctrl_pressed = false;

	auto parent = GetParent(hwnd);
	auto dialog = reinterpret_cast<EditDialogData*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

	switch (message)
	{
	default:
	{}	break;

	case WM_DESTROY:
	{
		delete dialog;
	}	return true;

	case WM_CLOSE:
	{
		DestroyWindow(hwnd);
	}	return true;

	case WM_INITDIALOG:
	{
		SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(
			LoadIconW(GetModuleHandle(nullptr), MAKEINTRESOURCEW(ICON_DEFAULT))));

		dialog = new EditDialogData;
		SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(dialog));

		dialog->uniqueId = 0;
		dialog->database = reinterpret_cast<LoginDatabase*>(lparam);

		auto index = ListBox_GetCurSel(GetDlgItem(parent, DIALOG_MAIN_LISTBOX_LIST));

		PasswordGeneratorDesc generatorDesc;

		if (index != LB_ERR)
		{
			auto data = dialog->database->getEntry(static_cast<std::size_t>(index));

			if (data)
			{
				dialog->uniqueId = data->uniqueId;

				auto guard = dialog->database->transformGuard(*data);

				SetDlgItemTextW(hwnd, DIALOG_EDITDATA_EDIT_NAME, toWideString(data->name).c_str());
				SetDlgItemTextW(hwnd, DIALOG_EDITDATA_EDIT_COMMENT, toWideString(data->comment).c_str());

				if (data->snapshots.size() > 0)
				{
					SetDlgItemTextW(hwnd, DIALOG_EDITDATA_EDIT_USERNAME,
						toWideString(data->snapshots.back().username).c_str());
					SetDlgItemTextW(hwnd, DIALOG_EDITDATA_EDIT_PASSWORD,
						toWideString(data->snapshots.back().password).c_str());
				}

				generatorDesc = data->generatorDesc;
			}
			else
			{
				showMessageBox("Error", "Invalid database index.");
				PostMessage(hwnd, WM_CLOSE, 0, 0);
				break;
			}
		}

		SendMessageW(GetDlgItem(hwnd, DIALOG_EDITDATA_CHECKBOX_LETTERS), BM_SETCHECK,
			generatorDesc.genLetters ? BST_CHECKED : BST_UNCHECKED, 0);
		SendMessageW(GetDlgItem(hwnd, DIALOG_EDITDATA_CHECKBOX_NUMBERS), BM_SETCHECK,
			generatorDesc.genNumbers ? BST_CHECKED : BST_UNCHECKED, 0);
		SendMessageW(GetDlgItem(hwnd, DIALOG_EDITDATA_CHECKBOX_SPECIAL), BM_SETCHECK,
			generatorDesc.genSpecial ? BST_CHECKED : BST_UNCHECKED, 0);
		SendMessageW(GetDlgItem(hwnd, DIALOG_EDITDATA_CHECKBOX_EXTRA), BM_SETCHECK,
			generatorDesc.genExtra ? BST_CHECKED : BST_UNCHECKED, 0);

		SetDlgItemTextW(hwnd, DIALOG_EDITDATA_EDIT_EXTRA, toWideString(generatorDesc.extraAlphabet).c_str());
		SetDlgItemInt(hwnd, DIALOG_EDITDATA_EDIT_PWLENGTH, generatorDesc.passwordLength, false);

		updatePasswordStrength(hwnd, generatorDesc);

	}	return true;

	case WM_COMMAND:
	{
		switch (LOWORD(wparam))
		{
		default:
		{}	break;

		case DIALOG_EDITDATA_BUTTON_SAVE:
		{
			saveEditedData(hwnd, dialog);
			PostMessageW(parent, WM_CHANGES_SAVED, 0, 0);
			PostMessageW(hwnd, WM_CLOSE, 0, 0);
		}	return true;

		case DIALOG_EDITDATA_BUTTON_CANCEL:
		{
			PostMessageW(hwnd, WM_CLOSE, 0, 0);
		}	return true;

		case DIALOG_EDITDATA_BUTTON_GENERATE:
		{
			try
			{
				auto desc = makeGeneratorDesc(hwnd);
				auto password = dialog->database->generatePassword(desc);
				SetDlgItemTextW(hwnd, DIALOG_EDITDATA_EDIT_PASSWORD, toWideString(password).c_str());
			}
			catch (std::exception& e)
			{
				showMessageBox("Error", e.what());
			}
		}	return true;

		case DIALOG_EDITDATA_BUTTON_HISTORY:
		{
			HistoryDialogCreateParams params;
			params.database = dialog->database;
			params.uniqueId = dialog->uniqueId;
			CreateDialogParamW(nullptr, MAKEINTRESOURCEW(DIALOG_HISTORY), nullptr, 
				dialogProcHistory, reinterpret_cast<LPARAM>(&params));
		}	return true;

		case DIALOG_EDITDATA_EDIT_PWLENGTH:
		{
			if (HIWORD(wparam) == EN_CHANGE)
			{
				updatePasswordStrength(hwnd, makeGeneratorDesc(hwnd));
			}
		}	return true;

		case DIALOG_EDITDATA_EDIT_EXTRA:
		{
			if (HIWORD(wparam) == EN_CHANGE)
			{
				updatePasswordStrength(hwnd, makeGeneratorDesc(hwnd));
			}
		}	return true;

		case DIALOG_EDITDATA_CHECKBOX_LETTERS:
		{
			updatePasswordStrength(hwnd, makeGeneratorDesc(hwnd));
		}	return true;

		case DIALOG_EDITDATA_CHECKBOX_NUMBERS:
		{
			updatePasswordStrength(hwnd, makeGeneratorDesc(hwnd));
		}	return true;

		case DIALOG_EDITDATA_CHECKBOX_SPECIAL:
		{
			updatePasswordStrength(hwnd, makeGeneratorDesc(hwnd));
		}	return true;

		case DIALOG_EDITDATA_CHECKBOX_EXTRA:
		{
			updatePasswordStrength(hwnd, makeGeneratorDesc(hwnd));
		}	return true;
		}
	}	break;
	}

	return false;
}
