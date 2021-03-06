// Copyright (c) 2017-2018 The Multiverse developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef  MULTIVERSE_TIMESERIES_H
#define  MULTIVERSE_TIMESERIES_H

#include <boost/thread/thread.hpp>
#include <boost/filesystem.hpp>
#include <walleve/walleve.h>
#include "uint256.h"

namespace multiverse
{
namespace storage
{

class CDiskPos
{
    friend class walleve::CWalleveStream;
public:
    uint32 nFile;
    uint32 nOffset;
public:
    CDiskPos(uint32 nFileIn = 0,uint32 nOffsetIn = 0) : nFile(nFileIn),nOffset(nOffsetIn) {}
    bool IsNull() const { return (nFile == 0); }
    bool operator==(const CDiskPos& b) const { return (nFile == b.nFile && nOffset == b.nOffset); }
    bool operator!=(const CDiskPos& b) const { return (nFile != b.nFile || nOffset != b.nOffset); }
    bool operator<(const CDiskPos& b) const { return (nFile < b.nFile || (nFile == b.nFile && nOffset < b.nOffset)); }
protected:
    template <typename O>
    void WalleveSerialize(walleve::CWalleveStream& s,O& opt)
    {
        s.Serialize(nFile,opt);
        s.Serialize(nOffset,opt);
    }
};


template <typename T>
class CTSWalker
{
public:
    virtual bool Walk(const T& t,uint32 nFile,uint32 nOffset) = 0;
};

class CTimeSeries
{
public:
    CTimeSeries();
    ~CTimeSeries();
    bool Initialize(const boost::filesystem::path& pathLocationIn,const std::string& strPrefixIn);
    void Deinitialize();
    template <typename T>
    bool Write(const T& t,uint32& nFile,uint32& nOffset)
    {
        boost::unique_lock<boost::mutex> lock(mtxFile);
        std::string pathFile;
        if (!GetLastFilePath(nFile,pathFile))
        {
            return false;
        }
        try
        {
            walleve::CWalleveFileStream fs(pathFile.c_str());
            fs.SeekToEnd();
            uint32 nSize = fs.GetSerializeSize(t);
            fs << nMagicNum << nSize;
            nOffset = fs.GetCurPos();
            fs << t;
        }
        catch (...) 
        {
            return false;
        }
        if (!WriteToCache(t,CDiskPos(nFile,nOffset)))
        {
            ResetCache();
        }
        return true;
    }
    template <typename T>
    bool Read(T& t,uint32 nFile,uint32 nOffset)
    {
        boost::unique_lock<boost::mutex> lock(mtxFile);

        if (ReadFromCache(t,CDiskPos(nFile,nOffset)))
        {
            return true;
        }

        std::string pathFile;
        if (!GetFilePath(nFile,pathFile))
        {
            return false;
        }
        try
        {
            // Open history file to read
            walleve::CWalleveFileStream fs(pathFile.c_str());
            fs.Seek(nOffset);
            fs >> t;
        }
        catch(...)
        {
            return false;
        }

        if (!WriteToCache(t,CDiskPos(nFile,nOffset)))
        {
            ResetCache();
        }
        return true;
    }
    template <typename T>
    bool WalkThrough(CTSWalker<T>& walker,uint32& nLastFile,uint32& nLastPos)
    {
        boost::unique_lock<boost::mutex> lock(mtxFile);
        bool fRet = true;
        uint32 nFile = 1;
        uint32 nOffset = 0;
        nLastFile = 0;
        nLastPos = 0;
        std::string pathFile;

        while (GetFilePath(nFile,pathFile) && fRet)
        {
            nLastFile = nFile;
            try
            {
                walleve::CWalleveFileStream fs(pathFile.c_str());
                fs.Seek(0);
                nOffset = 0;

                while (!fs.IsEOF() && fRet)
                {
                    uint32 nMagic,nSize;
                    T t;
                    fs >> nMagic >> nSize >> t;
                    if (nMagic != nMagicNum || fs.GetCurPos() - nOffset - 8 != nSize
                        || !walker.Walk(t,nFile,nOffset + 8))
                    {
                        fRet = false;
                        break;
                    }
                    nOffset = fs.GetCurPos();
                }
            }
            catch (...)
            {
                fRet = false;
            }
            nFile++;
        }
        nLastPos = nOffset;
        return fRet;
    }
protected:
    bool CheckDiskSpace();
    const std::string FileName(uint32 nFile);
    bool GetFilePath(uint32 nFile,std::string& strPath);
    bool GetLastFilePath(uint32& nFile,std::string& strPath);
    void ResetCache();
    bool VacateCache(uint32 nNeeded);
    template <typename T>
    bool WriteToCache(const T& t,const CDiskPos& diskpos)
    {
        if (mapCachePos.count(diskpos))
        {
            return true;
        }
        uint32 nSize = cacheStream.GetSerializeSize(t);
        if (!VacateCache(nSize))
        {
            return false;
        }
        try
        {
            std::size_t nPos;
            cacheStream << diskpos << nSize;
            nPos = cacheStream.GetWritePos();
            cacheStream << t;
            mapCachePos.insert(std::make_pair(diskpos,nPos));
            return true;
        }
        catch (...) {}
        return false;
    }
    template <typename T>
    bool ReadFromCache(T& t,const CDiskPos& diskpos)
    {
        std::map<CDiskPos,size_t>::iterator it = mapCachePos.find(diskpos);
        if (it != mapCachePos.end())
        {
            if (cacheStream.Seek((*it).second))
            {
                try
                {
                    cacheStream >> t;
                    return true;
                }
                catch (...) {}
            }
            ResetCache();
        }
        return false;
    }
protected:
    enum {MAX_FILE_SIZE = 0x7F000000,FILE_CACHE_SIZE = 0x2000000,MAX_CHUNK_SIZE = 0x200000};
    boost::mutex mtxFile;
    boost::filesystem::path pathLocation;
    std::string strPrefix;
    uint32 nLastFile;
    walleve::CWalleveCircularStream cacheStream;
    std::map<CDiskPos,std::size_t> mapCachePos;
    static const uint32 nMagicNum;
};

} // namespace storage
} // namespace multiverse

#endif //MULTIVERSE_TIMESERIES_H

