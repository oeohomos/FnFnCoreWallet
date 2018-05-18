// Copyright (c) 2017-2018 The Multiverse developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef  MULTIVERSE_KEY_H
#define  MULTIVERSE_KEY_H

#include "crypto.h"

namespace multiverse
{
namespace crypto
{

class CPubKey : public uint256
{
public:
    CPubKey();
    CPubKey(const uint256& pubkey);
    bool Verify(const uint256& hash,const std::vector<uint8>& vchSig);
};

class CKey
{
public:
    CKey();
    CKey(const CKey& key);
    CKey& operator=(const CKey& key);
    ~CKey();
    int GetVersion() const;
    bool IsNull() const;
    bool IsLocked() const;
    bool Renew();
    bool Load(int nVersionIn,const CCryptoCipher& cipherIn,const CPubKey& pubkeyIn);
    bool SetSecret(const CCryptoKeyData& vchSecret);
    bool GetSecret(CCryptoKeyData& vchSecret) const;
    CPubKey GetPubKey() const;
    const CCryptoCipher& GetCipher() const;
    bool Sign(const uint256& hash,std::vector<uint8>& vchSig) const;
    bool Encrypt(const CCryptoString& strPassphrase,
                 const CCryptoString& strCurrentPassphrase = "");
    void Lock();
    bool Unlock(const CCryptoString& strPassphrase = "");
protected:
    bool UpdateCipher(int nVersionIn = 0,const CCryptoString& strPassphrase = "");
protected:
    int nVersion;
    CCryptoKey* pCryptoKey;
    CCryptoCipher cipher;
};

} // namespace crypto
} // namespace multiverse

#endif //MULTIVERSE_KEY_H
