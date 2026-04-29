//
// $Id: Log.h,v 1.3 2004/05/26 22:41:51 clamatius Exp $

// Copyright (c) 2003, WhichBot Project
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of the WhichBot Project nor the names of its
//       contributors may be used to endorse or promote products derived from
//       this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
// IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


// Provides a basic logging interface.

#ifndef __FRAMEWORK_LOG_H
#define __FRAMEWORK_LOG_H

#include <format>
#include <print>
#include <source_location>
#include <string>
#include <string_view>

namespace wb_log
{
constexpr std::string getFileName(const std::source_location& path)
{
    std::string fname(path.file_name());
    auto last = fname.find_last_of("/");
    if(last == std::string::npos) return fname;
    return fname.substr(last+1);
}
enum class LogLevel { Info, Warn, Error, Debug, All };

std::array<bool, static_cast<size_t>(LogLevel::All)+1> enabled{{true,true,true,false,false}};

template <typename... Args>
void log_impl(LogLevel level, 
              const std::source_location loc, 
              std::format_string<Args...> fmt, 
              Args&&... args) 
{
    std::string_view level_str;
    switch (level) {
        case LogLevel::Info:  level_str = "INFO";  break;
        case LogLevel::Warn:  level_str = "WARN";  break;
        case LogLevel::Error: level_str = "ERROR"; break;
        case LogLevel::Debug: level_str = "DEBUG"; break;
    }

    std::println("[{}] {}:{}: {}", 
                 level_str, 
                 getFileName(loc), 
                 loc.line(), 
                 std::format(fmt, std::forward<Args>(args)...));
}
};// End namespace wb
#define WB_LOG_INFO(fmt, ...)  wb_log::log_impl(wb_log::LogLevel::Info,  std::source_location::current(), fmt, ##__VA_ARGS__)
#define WB_LOG_WARN(fmt, ...)  wb_log::log_impl(wb_log::LogLevel::Warn,  std::source_location::current(), fmt, ##__VA_ARGS__)
#define WB_LOG_ERROR(fmt, ...) wb_log::log_impl(wb_log::LogLevel::Error, std::source_location::current(), fmt, ##__VA_ARGS__)
#define WB_LOG_DEBUG(fmt, ...) wb_log::log_impl(wb_log::LogLevel::Debug, std::source_location::current(), fmt, ##__VA_ARGS__)
#endif // __FRAMEWORK_LOG_H
