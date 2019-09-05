//
// Created by junshen on 10/4/18.
//

#pragma once

namespace timerservice
{
    class Timer
    {
    public:
        virtual void cancel() = 0;
        virtual ~Timer() = default;
    };
} // namespace timerservice
