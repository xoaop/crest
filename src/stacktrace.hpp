#ifndef CREST_STACKTRACE_HPP
#define CREST_STACKTRACE_HPP

// 调用栈抓取 + 符号化：Windows 走 dbghelp，POSIX 走 execinfo。
// 每帧一行，符号取不到时退化成地址。

#include <string>
#include <vector>


// skip 是要跳过的帧数（含 xp_capture_stacktrace 自身这一帧）
std::vector<std::string> xp_capture_stacktrace(int skip);


#endif // CREST_STACKTRACE_HPP
