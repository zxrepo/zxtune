/**
*
* @file
*
* @brief  Metainfo operating implementation
*
* @author vitamin.caig@gmail.com
*
**/

//local includes
#include "formats/chiptune/metainfo.h"
//common includes
#include <byteorder.h>
#include <contract.h>
#include <make_ptr.h>
//library includes
#include <binary/container_factories.h>
//std includes
#include <set>
#include <map>

namespace Formats
{
namespace Chiptune
{
  class Patcher : public PatchedDataBuilder
  {
  public:
    explicit Patcher(Binary::View src)
      : Source(src)
      , SizeAddon(0)
    {
    }

    void InsertData(std::size_t offset, Binary::View data) override
    {
      Require(Insertions.insert(BlobsMap::value_type(offset, data)).second);
      SizeAddon += data.Size();
    }

    void OverwriteData(std::size_t offset, Binary::View data) override
    {
      Require(offset + data.Size() <= Source.Size());
      Require(Overwrites.insert(BlobsMap::value_type(offset, data)).second);
    }

    void FixLEWord(std::size_t offset, int_t delta) override
    {
      Require(offset + sizeof(uint16_t) <= Source.Size());
      Require(LEWordFixes.insert(FixesMap::value_type(offset, delta)).second);
    }

    Binary::Container::Ptr GetResult() const override
    {
      const uint8_t* const srcData = static_cast<const uint8_t*>(Source.Start());
      std::unique_ptr<Dump> result(new Dump(srcData, srcData + Source.Size()));
      ApplyFixes(*result);
      ApplyOverwrites(*result);
      ApplyInsertions(*result);
      return Binary::CreateContainer(std::move(result));
    }
  private:
    void ApplyFixes(Dump& result) const
    {
      for (const auto& fix : LEWordFixes)
      {
        Fix<uint16_t>(static_cast<void*>(&result[fix.first]), fix.second);
      }
    }

    template<class T>
    static void Fix(void* data, int_t delta)
    {
      T* const ptr = static_cast<T*>(data);
      const T val = fromLE(*ptr);
      const T fixedVal = static_cast<T>(val + delta);
      *ptr = fromLE(fixedVal);
    }

    void ApplyOverwrites(Dump& result) const
    {
      for (const auto& over : Overwrites)
      {
        std::memcpy(result.data() + over.first, over.second.Start(), over.second.Size());
      }
    }

    void ApplyInsertions(Dump& result) const
    {
      if (0 == SizeAddon)
      {
        return;
      }
      Dump tmp(result.size() + SizeAddon);
      Dump::const_iterator src = result.begin();
      const Dump::const_iterator srcEnd = result.end();
      auto dst = tmp.begin();
      std::size_t oldOffset = 0;
      for (const auto& ins : Insertions)
      {
        if (const std::size_t toCopy = ins.first - oldOffset)
        {
          const Dump::const_iterator nextEnd = src + toCopy;
          dst = std::copy(src, nextEnd, dst);
          src = nextEnd;
          oldOffset += toCopy;
        }
        std::memcpy(&*dst, ins.second.Start(), ins.second.Size());
        dst += ins.second.Size();
      }
      std::copy(src, srcEnd, dst);
      result.swap(tmp);
    }
  private:
    const Binary::View Source;
    typedef std::map<std::size_t, Binary::View> BlobsMap;
    typedef std::map<std::size_t, int_t> FixesMap;
    BlobsMap Insertions;
    BlobsMap Overwrites;
    FixesMap LEWordFixes;
    std::size_t SizeAddon;
  };
}
}

namespace Formats
{
  namespace Chiptune
  {
    PatchedDataBuilder::Ptr PatchedDataBuilder::Create(Binary::View data)
    {
      return MakePtr<Patcher>(data);
    }
  }
}
