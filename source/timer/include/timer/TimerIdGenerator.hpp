//
// Created by junshen on 10/4/18.
//

#pragma once

namespace timerservice
{
    using TimerId = std::uint64_t;

    class TimerIdGenerator
    {
    public:
        TimerId operator()()
        {
            return id++;
        }

    private:
        TimerId id{0};
    };
} // namespace timerservice
