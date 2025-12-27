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
        std::string temp(std::format(__fmt, std::forward<_Args>(__args)...).c_str());
        temp = std::format("[Info] {}\n", temp);
        
        if (print_terminal) {
            std::print("[Log]{}", temp);
        }
        stream << temp;
    }
    
    void info(const std::string& s) {
        info("{}", s);
    }
    
    template <class... _Args>
    void warn(std::format_string<_Args...> __fmt, _Args&&... __args) {
        std::string temp(std::format(__fmt, std::forward<_Args>(__args)...).c_str());
        temp = std::format("[Warning] {}\n", temp);
        
        if (print_terminal) {
            std::print("[Log]{}", temp);
        }
        stream << temp;
    }
    
    void warn(const std::string& s) {
        warn("{}", s);
    }
    
    template <class... _Args>
    void error(std::format_string<_Args...> __fmt, _Args&&... __args) {
        std::string temp(std::format(__fmt, std::forward<_Args>(__args)...).c_str());
        temp = std::format("[Error] {}\n", temp);
        
        if (print_terminal) {
            std::print("[Log]{}", temp);
        }
        
        if (error_counter < 100) {
            stream << temp;
						error_notification = true;
						if (++error_counter == 100) stream << "\nCLOSING STREAM, TOO MANY ERRORS\n";
        }
    }
    
    void error(const std::string& s) {
        error("{}", s);
    }
    
    const char* viewBuffer(void) {
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
    unsigned long error_counter = 0;
    std::stringstream stream;

    #ifdef DEBUG
        bool print_terminal = true;
    #else
        bool print_terminal = false;
    #endif
    
public:
    Log(Log&) = delete;
    Log& operator=(Log&) = delete;
};

#endif /* Log_h */
