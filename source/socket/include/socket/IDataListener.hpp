//
// Created by junshen on 10/2/18.
//

#pragma once
#include <string>

namespace endpoints
{
    class IDataListener
    {
    public:
        virtual ~IDataListener() = default;
        virtual void onDataMessage(const std::string& dataMessage) = 0;
    };
}
