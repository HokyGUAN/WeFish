#ifndef CRYPTO_H
#define CRYPTO_H

#include <string>

class CBASE64
{
public:
    static std::string encode(const std::string& str);

    static std::string decode(const std::string& str);

private:
    static int __encode(unsigned char* pDest, const unsigned char* pSrc, size_t nSrcLen);

    static int __decode(unsigned char* pDest, const unsigned char* pSrc, size_t nSrcLen);

private:
    static unsigned char s_encTable[];
    static unsigned char s_decTable[];
};

#endif // CRYPTO_H
