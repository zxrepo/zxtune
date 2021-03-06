/**
* 
* @file
*
* @brief  MUSE compressor support
*
* @author vitamin.caig@gmail.com
*
**/

//local includes
#include "formats/packed/container.h"
//common includes
#include <error.h>
#include <make_ptr.h>
#include <pointers.h>
//library includes
#include <binary/format_factories.h>
#include <binary/compression/zlib_container.h>
#include <binary/input_stream.h>
#include <debug/log.h>
#include <formats/packed.h>
//std includes
#include <memory>
//text includes
#include <formats/text/packed.h>

namespace Formats
{
namespace Packed
{
  namespace Muse
  {
    const Debug::Stream Dbg("Formats::Packed::Muse");

    //checkers
    const StringView HEADER_PATTERN =
      "'M'U'S'E"
      "de ad be|ba af|be"
      "????" //file size
      "????" //crc
      "????" //packed size
      "????" //unpacked size
    ;

    class Decoder : public Formats::Packed::Decoder
    {
    public:
      Decoder()
        : Depacker(Binary::CreateFormat(HEADER_PATTERN))
      {
      }

      String GetDescription() const override
      {
        return Text::MUSE_DECODER_DESCRIPTION;
      }

      Binary::Format::Ptr GetFormat() const override
      {
        return Depacker;
      }

      Container::Ptr Decode(const Binary::Container& rawData) const override
      {
        if (!Depacker->Match(rawData))
        {
          return {};
        }
        try
        {
          Binary::DataInputStream input(rawData);
          input.Skip(4);
          const auto sign = input.ReadBE<uint32_t>();
          Require(sign == 0xdeadbabe || sign == 0xdeadbeaf);
          const auto fileSize = input.ReadLE<uint32_t>();
          const auto crc = input.ReadLE<uint32_t>();
          const auto packedSize = input.ReadLE<uint32_t>();
          const auto unpackedSize = input.ReadLE<uint32_t>();
          const auto packedData = input.ReadData(packedSize);
          auto unpackedData = Binary::Compression::Zlib::Decompress(packedData, unpackedSize);
          return CreateContainer(std::move(unpackedData), std::min<std::size_t>(fileSize, input.GetPosition()));
        }
        catch (const std::exception& e)
        {
          Dbg("Failed to uncompress: %1%", e.what());
        }
        catch (const Error& e)
        {
          Dbg("Failed to uncompress: %1%", e.ToString());
        }
        return {};
      }
    private:
      const Binary::Format::Ptr Depacker;
    };
  }

  Decoder::Ptr CreateMUSEDecoder()
  {
    return MakePtr<Muse::Decoder>();
  }
}//namespace Packed
}//namespace Formats
