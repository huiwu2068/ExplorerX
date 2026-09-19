#pragma once

#include "BaseDialog.h"
#include "Plugins/LuaPlugin.h"

class AppServices;

class ScriptingDialog : public BaseDialog
{
public:
	static ScriptingDialog *Create(const ResourceLoader *resourceLoader, HWND hParent,
		AppServices *appServices);

protected:
	INT_PTR OnInitDialog() override;
	INT_PTR OnCommand(WPARAM wParam, LPARAM lParam) override;
	void OnRun();
	INT_PTR OnClose() override;

private:
	ScriptingDialog(const ResourceLoader *resourceLoader, HWND hParent, AppServices *appServices);
	~ScriptingDialog() = default;

	std::vector<ResizableDialogControl> GetResizableControls() override;

	std::wstring FormatResult(const sol::protected_function_result &result);
	void AppendToLog(const std::wstring &command, const std::wstring &result);

	Plugins::LuaPlugin m_luaPlugin;
};
