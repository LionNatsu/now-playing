# CMakeList.txt : now-playing の CMake プロジェクト。ソースを含めて、次を定義します:
# プロジェクト専用ロジックはこちらです。
#
cmake_minimum_required (VERSION 3.8)

project ("now-playing")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ソースをこのプロジェクトの実行可能ファイルに追加します。
add_executable (now-playing "now-playing.cpp")
target_include_directories(now-playing PRIVATE third_party/include)

install(TARGETS now-playing)
