#include <chrono>
#include <thread>
#include <vector>
#include <map>
#include <fstream>

#define SPDLOG_WCHAR_TO_UTF8_SUPPORT
#include <spdlog/spdlog.h>
#include <wil/resource.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <tlhelp32.h>

using namespace std::chrono_literals;

std::vector<std::tuple<int, int>> find_process_tids(std::wstring process_name) {
    std::vector<int> pids;
    auto snapshot = wil::unique_handle(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS | TH32CS_SNAPTHREAD, 0));
    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(entry);
    if (!Process32FirstW(snapshot.get(), &entry)) return {};
    while (Process32NextW(snapshot.get(), &entry)) {
        if (wcsicmp(entry.szExeFile, process_name.c_str()) == 0) {
            pids.push_back(entry.th32ProcessID);
        }
    }
    std::vector<std::tuple<int, int>> tids;
    for (auto pid : pids) {
        THREADENTRY32 entry;
        entry.dwSize = sizeof(entry);
        if (!Thread32First(snapshot.get(), &entry)) continue;
        while (Thread32Next(snapshot.get(), &entry)) {
            if (entry.th32OwnerProcessID == pid) {
                tids.push_back({ pid, entry.th32ThreadID });
            }
        }
    }
    return tids;
}

std::wstring get_window_title(HWND window) {
    if (GetWindowLongPtr(window, GWL_STYLE) & WS_CHILD) return {};
    if (GetWindowLongPtr(window, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) return {};
    if (!IsWindowVisible(window)) return {};
    auto len = GetWindowTextLengthW(window);
    if (!len) return {};
    std::wstring title(len, L'\0');
    GetWindowTextW(window, title.data(), title.size() + 1);
    return title;
}

std::map<HWND, std::wstring> find_named_windows(int tid) {
    std::map<HWND, std::wstring> windows;
    EnumThreadWindows(tid, [](HWND window, LPARAM data) -> BOOL {
        auto title = get_window_title(window);
        if (!title.empty()) {
            (*reinterpret_cast<decltype(&windows)>(data))[window] = title;
        }
        return true;
    }, reinterpret_cast<LPARAM>(&windows));
    return windows;
}

std::string to_utf8(const std::wstring &wstr) {
    if (!wstr.empty()) {
        int len = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), wstr.size(), nullptr, 0, nullptr, nullptr);
        std::string ret(len, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wstr.data(), wstr.size(), ret.data(), ret.size(), nullptr, nullptr);
        return ret;
    }
    return {};
}

void find_player(std::wstring process, std::wstring player);
void update_player(std::wstring process, std::wstring player, HWND window);

void find_player(std::wstring process, std::wstring player) {
    while (true) {
        std::this_thread::sleep_for(1s);
        auto tids = find_process_tids(process);
        for (auto [pid, tid] : tids) {
            auto windows = find_named_windows(tid);
            if (windows.size() == 1) {
                auto [window, title] = *windows.begin();
                spdlog::info(L"{} +", player, title);
                std::thread(update_player, process, player, window).detach();
                return;
            }
        }
    }
}

void update_player(std::wstring process, std::wstring player, HWND window) {
    decltype(get_window_title(window)) title;
    while (true) {
        std::this_thread::sleep_for(1s);
        if (!IsWindowVisible(window)) {
            spdlog::info(L"{} -", player, int(window));
            std::ofstream ofs(player + L".txt", std::ios::binary);
            auto utf8_title = to_utf8(L"当前未播放歌曲");
            ofs.write(utf8_title.data(), utf8_title.size());
            std::thread(find_player, process, player).detach();
            return;
        }
        auto new_title = get_window_title(window);
        if (new_title != title) {
            title = new_title;
            spdlog::info(L"{} => {}", player, title);
            std::ofstream ofs(player + L".txt", std::ios::binary);
            auto utf8_title = to_utf8(title);
            ofs.write(utf8_title.data(), utf8_title.size());
        }
    }
}

int main() {
    SetConsoleCP(65001);
    SetConsoleOutputCP(65001);
    spdlog::info(L"当前歌曲名将会实时写入文本文件，输出的文本文件与本程序位于同一文件夹。");
    spdlog::info(L"支持网易云音乐和QQ音乐。请保持相应播放器窗口处于打开状态，可最小化。");
    spdlog::info(L"开始！");
    std::thread(find_player, L"cloudmusic.exe", L"网易云").detach();
    std::thread(find_player, L"qqmusic.exe", L"QQ音乐").detach();
    while (true) {
        // Loop forever
        std::this_thread::sleep_for(1s);
    }
}
