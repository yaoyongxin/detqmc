/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0.  See the enclosed file LICENSE for a copy or if
 * that was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * Copyright 2017 Max H. Gerlach
 * 
 * */

/*
 * timing.h
 *
 *  Created on: Mar 28, 2013
 *      Author: gerlach
 */

#ifndef TIMING_H_
#define TIMING_H_

#ifdef TIMING

#include <vector>
#include <string>
#include <map>
#include <iostream>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wshadow"
#include "boost/timer/timer.hpp"
#pragma GCC diagnostic pop

class Timing {
public:
    Timing() : insertOrder(), timers() {
    }
    //on destruction print timing summary
    ~Timing() {
        std::cout << "\nTimings:\n";
        for (const auto& timerName : insertOrder) {
            std::cout << timerName << ": " << timers[timerName].format();
        }
    }
    //start or resume a timer:
    void start(const std::string& timerKey) {
        if (timers.count(timerKey) == 0) {
            insertOrder.push_back(timerKey);
            timers[timerKey] = boost::timer::cpu_timer();
        }
        timers[timerKey].resume();
    }
    void stop(const std::string& timerKey) {
        timers.at(timerKey).stop();
    }
private:
    std::vector<std::string> insertOrder;
    std::map<std::string,boost::timer::cpu_timer> timers;
};

#else
//version that does nothing:
#include <string>
class Timing {
public:
    Timing() {
    }
    ~Timing() {
    }
    void start(const std::string& timerKey) {
        (void)timerKey;
    }
    void stop(const std::string& timerKey) {
        (void)timerKey;
    }
};

#endif //TIMING

extern Timing timing;       //one global timing object, defined in timing.cpp



#endif /* TIMING_H_ */
