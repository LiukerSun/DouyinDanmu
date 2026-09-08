#include <vector>
#include <sstream>
#include <iomanip>
#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#ifdef GetMessage
#undef GetMessage
#endif
#else
#include <openssl/sha.h>
#endif
static std::string digest(const std::string& s) {
#ifdef _WIN32
    BCRYPT_ALG_HANDLE algorithm=nullptr; BCRYPT_HASH_HANDLE hash=nullptr;
    unsigned char value[32]; DWORD object_size=0, written=0;
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)throw std::runtime_error("SHA256 provider failed");
    auto cleanup=[&]{if(hash)BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(algorithm,0);};
    if(BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&object_size),sizeof(object_size),&written,0)<0){cleanup();throw std::runtime_error("SHA256 property failed");}
    std::vector<unsigned char> object(object_size);
    if(BCryptCreateHash(algorithm,&hash,object.data(),object_size,nullptr,0,0)<0 ||
       BCryptHashData(hash,reinterpret_cast<PUCHAR>(const_cast<char*>(s.data())),static_cast<ULONG>(s.size()),0)<0 ||
       BCryptFinishHash(hash,value,sizeof(value),0)<0){cleanup();throw std::runtime_error("SHA256 failed");}
    cleanup();
#else
    unsigned char value[SHA256_DIGEST_LENGTH]; SHA256(reinterpret_cast<const unsigned char*>(s.data()), s.size(), value);
#endif
    std::ostringstream out; for (auto c : value) out << std::hex << std::setw(2) << std::setfill('0') << int(c); return out.str();
}
