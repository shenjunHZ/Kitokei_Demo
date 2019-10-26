//
// Created by shenjun on 18-10-11.
//
#include <codecvt>
#include "CodeConverter.hpp"
#include "logger/Logger.hpp"

namespace common
{
    CodeConverter::CodeConverter(const char* fromCharset, const char* toCharset)
    {
        m_iconv = iconv_open(toCharset, fromCharset);
        if(m_iconv <= 0)
        {
            LOG_ERROR_MSG("Iconv open filed.");
        }
    }

    CodeConverter::~CodeConverter()
    {
        iconv_close(m_iconv);
    }

    int CodeConverter::decodeCoverter(char* input, size_t inLen,
                      char* output, size_t outLen)
    {
        int res = iconv(m_iconv, &input, &inLen, &output, &outLen);
        if(res < 0)
        {
            LOG_ERROR_MSG("Decode cover failed error : {}", errno);
        }
        return res;
    }

    std::wstring CodeConverter::stringToWstring(const std::string& strData)
    {
        using convert_typeX = std::codecvt_utf8<wchar_t>;
        std::wstring_convert<convert_typeX, wchar_t> converterX;

        return converterX.from_bytes(strData);

    }

    std::string CodeConverter::wstringToString(const std::wstring& wstrData)
    {
        using convert_typeX = std::codecvt_utf8<wchar_t>;
        std::wstring_convert<convert_typeX, wchar_t> converterX;

        return converterX.to_bytes(wstrData);

    }
}