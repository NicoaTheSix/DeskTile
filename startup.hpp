#pragma once
#include <functional>
#include <string>
namespace desktile_startup {
// Only an explicit affirmative UI action may call the enable writer. Imports and startup
// have no consent token and cannot register automatic execution.
inline bool Change(bool enabled,bool explicitConsent,const std::function<bool(bool)>& writer) {
    if(enabled&&!explicitConsent)return false;
    return writer(enabled);
}
inline std::wstring Command(const std::wstring& executable,const std::wstring& directory) {
    if(executable.empty()||directory.empty()||executable.find_first_of(L"\"\r\n")!=std::wstring::npos||
        directory.find_first_of(L"\"\r\n")!=std::wstring::npos)return {};
    // Windows quoting must double trailing backslashes immediately before a closing quote.
    auto quote=[](const std::wstring& value){auto end=value.find_last_not_of(L'\\');
        size_t trailing=end==std::wstring::npos?value.size():value.size()-end-1;
        return L"\""+value+std::wstring(trailing,L'\\')+L"\"";};
    return quote(executable)+L" --startup --data-dir "+quote(directory);
}
}
