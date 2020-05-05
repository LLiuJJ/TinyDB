/**
 * @file CppStringPlus.cpp
 * 
 * 这个文件是一些对标准 c++ string 扩展函的实现
 * 
 * Copyright © 2014-2019 by LiuJ
 * 
 */

#include <CppStringPlus/CppStringPlus.hpp>
#include <stdlib.h>
#include <vector>
#include <limits>
#include <cstdarg>
#include <sstream>

namespace CppStringPlus {

    std::string ToLower(const std::string& inString) {
        std::string outString;
        outString.reserve(inString.size());
        for (char c : inString) {
            outString.push_back(tolower(c));
        }
        return outString;
    }

    ToIntegerResult ToInteger(
        const std::string& numberString,
        intmax_t& number
    ) {
        size_t index = 0;
        size_t state = 0;
        bool negative = false;
        intmax_t value = 0;
        while (index < numberString.size()) {
            switch (state)
            {
                case 0:
                    if (numberString[index] == '-')
                    {
                        negative = true;
                        ++index;
                    }
                    state = 1;
                    break;
               
                case 1: {
                    if (numberString[index] == '0') {
                        state = 2;
                    } else if ((numberString[index] >= '1') && (numberString[index] <= '9')) {
                        state = 3;
                        value = (decltype(value))(numberString[index] - '0');
                        value = (value * (negative ? -1 : 1));
                    } else {
                        return ToIntegerResult::NotANumber;
                    }
                    ++index;
                } break;

                case 2: {
                    return ToIntegerResult::NotANumber;
                } break;

                case 3: {
                    if(
                        (numberString[index] >= '0')
                        && (numberString[index] <= '9')
                    ) {
                        const auto digit = (decltype(value))(numberString[index] - '0');
                        if (negative) {
                            if ((std::numeric_limits< decltype(value) >::lowest() + digit) / 10 > value) {
                                return ToIntegerResult::Overflow;
                            }
                        } else {
                            if ((std::numeric_limits< decltype(value) >::max() - digit) / 10 < value) {
                                return ToIntegerResult::Overflow;
                            }
                        }
                        value *= 10;
                        if (negative) {
                            value -= digit;
                        } else {
                            value += digit;
                        }
                        ++ index;
                    } else {
                        return ToIntegerResult::NotANumber;
                    }
                } break; 
            } 
        }
        if (state >= 2) {
            number = value;
            return ToIntegerResult::Success;
        } else {
            return ToIntegerResult::NotANumber;
        }
    }

    std::string vsprintf(const char* format, va_list args) {
        va_list argsCopy;
        va_copy(argsCopy, args);
        const int required = vsnprintf(nullptr, 0, format, args);
        va_end(args);
        if (required < 0) {
            va_end(argsCopy);
            return "";
        }
        std::vector< char > buffer(required + 1);
        const int result = vsnprintf(&buffer[0], required + 1, format, argsCopy);
        va_end(argsCopy);
        if (result < 0) {
            return "";
        }
        return std::string(buffer.begin(), buffer.begin() + required);
    }

    std::string sprintf(const char* format, ...) {
        va_list args;
        va_start(args, format);
        return vsprintf(format, args);
    }

    std::string Trim(const std::string& s) {
        size_t i = 0;
        while (
            (i < s.length())
            && (s[i] <= 32)
        ) {
            ++i;
        }
        size_t j = s.length();
        while (
            (j > 0)
            && (s[j - 1] <= 32)
        ) {
            --j;
        }
        return s.substr(i, j - i);
    }

    std::vector< std::string > Split(
        const std::string& s,
        char d
    ) {
        std::vector< std::string > values;
        auto remainder = Trim(s);
        while (!remainder.empty()) {
            auto delimiter = remainder.find_first_of(d);
            if (delimiter == std::string::npos) {
                values.push_back(remainder);
                remainder.clear();
            } else {
                values.push_back(Trim(remainder.substr(0, delimiter)));
                remainder = Trim(remainder.substr(delimiter + 1));
            }
        }
        return values;
    }

} // namespace CppStringPlus