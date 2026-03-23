//
//  Stopwatch.hpp
//
//
//  Created by Lorenzo Bozza on 03/03/26.
//

#ifndef Stopwatch_h
#define Stopwatch_h

#include "Log.hpp"
#include <chrono>

struct Stopwatch {
private:
    std::chrono::high_resolution_clock::time_point _start{};
    std::chrono::high_resolution_clock::time_point _stop{};
    float _time{.001f};
public:
    void begin(void) {_start = std::chrono::high_resolution_clock::now();}
    void end(std::string prefix = "") {
        _stop = std::chrono::high_resolution_clock::now();
        _time = std::chrono::duration<float, std::chrono::seconds::period>(_stop - _start).count();
        Log::getInstance()->debug("{}{} ms", prefix, _time * 1000.f);
    }
};

#endif /* Stopwatch_h */
