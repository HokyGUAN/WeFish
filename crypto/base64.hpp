#ifndef CBASE64_H
#define CBASE64_H

#include <string>

class CBASE64
{
public:
    static std::string base64Encode(const char* bytes_to_encode, unsigned int in_len);

    static std::string base64Decode(const std::string & encoded_string);

};

#endif // CBASE64_H
