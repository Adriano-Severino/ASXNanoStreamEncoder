#ifndef ARDUINO_SHIM_H
#define ARDUINO_SHIM_H

#include <cstdint>
#include <cstddef>
#include <string>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <cstring>

inline bool isDigit(char c)
{
    return c >= '0' && c <= '9';
}

class String
{
private:
    std::string _str;
    static bool& reserveFailure() { static bool value = false; return value; }
    static bool& copyFailure() { static bool value = false; return value; }

public:
    String() : _str("") {}
    String(const String& other)
    {
        if (copyFailure()) copyFailure() = false;
        else _str = other._str;
    }
    String& operator=(const String&) = default;
    String(const char* cstr) : _str(cstr ? cstr : "") {}
    String(const std::string& s) : _str(s) {}
    String(int val) : _str(std::to_string(val)) {}
    String(unsigned int val) : _str(std::to_string(val)) {}
    String(long val) : _str(std::to_string(val)) {}
    String(unsigned long val) : _str(std::to_string(val)) {}
    String(long long val) : _str(std::to_string(val)) {}
    String(unsigned long long val) : _str(std::to_string(val)) {}
    String(float val) : _str(std::to_string(val)) {}
    String(double val) : _str(std::to_string(val)) {}

    size_t length() const { return _str.length(); }
    bool reserve(size_t size)
    {
        if (reserveFailure()) { reserveFailure() = false; return false; }
        _str.reserve(size);
        return true;
    }
    static void failNextReserve() { reserveFailure() = true; }
    static void failNextCopy() { copyFailure() = true; }
    static void resetAllocationFailures() { reserveFailure() = false; copyFailure() = false; }
    void clear() { _str.clear(); }
    const char* c_str() const { return _str.c_str(); }

    char charAt(size_t index) const
    {
        if (index < _str.length()) return _str[index];
        return '\0';
    }

    char operator[](size_t index) const
    {
        return charAt(index);
    }

    String& operator+=(const String& rhs)
    {
        _str += rhs._str;
        return *this;
    }

    String& operator+=(const char* rhs)
    {
        if (rhs) _str += rhs;
        return *this;
    }

    String& operator+=(char c)
    {
        _str += c;
        return *this;
    }

    String& operator+=(int val)
    {
        _str += std::to_string(val);
        return *this;
    }

    String& operator+=(unsigned int val)
    {
        _str += std::to_string(val);
        return *this;
    }

    String& operator+=(long val)
    {
        _str += std::to_string(val);
        return *this;
    }

    String& operator+=(unsigned long val)
    {
        _str += std::to_string(val);
        return *this;
    }

    String& operator+=(long long val)
    {
        _str += std::to_string(val);
        return *this;
    }

    String& operator+=(unsigned long long val)
    {
        _str += std::to_string(val);
        return *this;
    }

    bool operator==(const String& rhs) const
    {
        return _str == rhs._str;
    }

    bool operator==(const char* rhs) const
    {
        return rhs != nullptr && _str == rhs;
    }

    int indexOf(char c, size_t fromIndex = 0) const
    {
        size_t pos = _str.find(c, fromIndex);
        return (pos == std::string::npos) ? -1 : static_cast<int>(pos);
    }

    int indexOf(const String& target, size_t fromIndex = 0) const
    {
        size_t pos = _str.find(target._str, fromIndex);
        return (pos == std::string::npos) ? -1 : static_cast<int>(pos);
    }

    String substring(size_t fromIndex) const
    {
        if (fromIndex >= _str.length()) return String("");
        return String(_str.substr(fromIndex));
    }

    String substring(size_t fromIndex, size_t toIndex) const
    {
        if (fromIndex >= _str.length()) return String("");
        if (toIndex > _str.length()) toIndex = _str.length();
        if (toIndex <= fromIndex) return String("");
        return String(_str.substr(fromIndex, toIndex - fromIndex));
    }

    int toInt() const
    {
        if (_str.empty()) return 0;
        try {
            return std::stoi(_str);
        } catch (...) {
            return 0;
        }
    }

    const std::string& toStdString() const { return _str; }
};

inline String operator+(const String& lhs, const String& rhs)
{
    String res = lhs;
    res += rhs;
    return res;
}

inline String operator+(const String& lhs, const char* rhs)
{
    String res = lhs;
    res += rhs;
    return res;
}

inline String operator+(const char* lhs, const String& rhs)
{
    String res(lhs);
    res += rhs;
    return res;
}

#endif // ARDUINO_SHIM_H
