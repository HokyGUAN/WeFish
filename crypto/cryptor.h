#ifndef CRYPTO_H
#define CRYPTO_H
#include <iostream>
#include <string>


std::string AESEncrypt(const std::string& plain, std::string& keyStr);

std::string AESDecrypt(std::string& cipher, std::string& keyStr);

#endif
