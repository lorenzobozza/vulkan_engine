//
//  Log.hpp
//  
//
//  Created by Lorenzo Bozza on 15/10/25.
//

#ifndef Log_h
#define Log_h

#include <print>

class Log {
public:
    static Log* getInstance(void) {
        static Log __logger;
        return &__logger;
    }
    
    template <class... _Args>
    void info(std::format_string<_Args...> __fmt, _Args&&... __args) {
        std::print("[Info] ");
        std::println(__fmt, std::forward<_Args>(__args)...);
    }
    
    void info(const std::string& s) {
        std::println("[Info] {}", s);
    }
    
    template <class... _Args>
    void warn(std::format_string<_Args...> __fmt, _Args&&... __args) {
        std::print("[Warning] ");
        std::println(__fmt, std::forward<_Args>(__args)...);
    }
    
    void warn(const std::string& s) {
        std::println("[Warning] {}", s);
    }
    
    template <class... _Args>
    void error(std::format_string<_Args...> __fmt, _Args&&... __args) {
        std::print("[Error] ");
        std::println(__fmt, std::forward<_Args>(__args)...);
    }
    
    void error(const std::string& s) {
        std::println("[Error] {}", s);
    }

private:
    Log() {}

public:
    Log(Log& conLogt) = delete;
    void operator=(Log& conLogt) = delete;
};

#endif /* Log_h */
