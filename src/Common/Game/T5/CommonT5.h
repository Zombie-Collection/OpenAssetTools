#pragma once

class CommonT5
{
public:
    static int Com_HashKey(const char* str, int maxLen);
    static int Com_HashString(const char* str);
    static int Com_HashString(const char* str, int len);
};