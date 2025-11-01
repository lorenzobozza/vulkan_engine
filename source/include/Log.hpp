//
//  Log.hpp
//  
//
//  Created by Lorenzo Bozza on 15/10/25.
//

#ifndef Log_h
#define Log_h

#include <format>
#include <print>
#include <sstream>

class Log {
public:
    static Log* getInstance(void) {
        static Log __logger;
        return &__logger;
    }
    
    template <class... _Args>
    void info(std::format_string<_Args...> __fmt, _Args&&... __args) {
        if (print_terminal) {
            std::print("[Info] ");
            std::println(__fmt, std::forward<_Args>(__args)...);
        }
        
        stream << "[Info] " << std::format(__fmt, std::forward<_Args>(__args)...) << '\n';
    }
    
    void info(const std::string& s) {
        info("{}", s);
    }
    
    template <class... _Args>
    void warn(std::format_string<_Args...> __fmt, _Args&&... __args) {
        if (print_terminal) {
            std::print("[Warning] ");
            std::println(__fmt, std::forward<_Args>(__args)...);
        }
        
        stream << "[Warning] " << std::format(__fmt, std::forward<_Args>(__args)...) << '\n';
    }
    
    void warn(const std::string& s) {
        warn("{}", s);
    }
    
    template <class... _Args>
    void error(std::format_string<_Args...> __fmt, _Args&&... __args) {
        if (print_terminal) {
            std::print("[Error] ");
            std::println(__fmt, std::forward<_Args>(__args)...);
        }
        
        stream << "[Error] " << std::format(__fmt, std::forward<_Args>(__args)...) << '\n';
        
        error_notification = true;
    }
    
    void error(const std::string& s) {
        error("{}", s);
    }
    
    const char* getBuffer(void) {
        return stream.view().data();
    }
    bool notifyErrors(void) {
        if (error_notification) {
            error_notification = false;
            return true;
        }
        return false;
    }

private:
    Log() {}
    
    bool error_notification = false;
    bool print_terminal = false;
    std::stringstream stream;

public:
    Log(Log& conLogt) = delete;
    void operator=(Log& conLogt) = delete;
};

#endif /* Log_h */
