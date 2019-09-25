/**
* 
* @file
*
* @brief  Abyss' Highest Experience support implementation
*
* @author vitamin.caig@gmail.com
*
**/

//local includes
#include "formats/chiptune/digital/abysshighestexperience.h"
#include "formats/chiptune/container.h"
//common includes
#include <byteorder.h>
#include <make_ptr.h>
//library includes
#include <binary/format_factories.h>
#include <binary/input_stream.h>
#include <strings/encoding.h>
#include <strings/trim.h>
//std includes
#include <array>
//text includes
#include <formats/text/chiptune.h>

namespace Formats
{
namespace Chiptune
{
  namespace AbyssHighestExperience
  {
    typedef std::array<uint8_t, 3> IdentifierType;
    
    const IdentifierType ID_AHX = {{'T', 'H', 'X'}};
    const IdentifierType ID_HVL = {{'H', 'V', 'L'}};

/*
    struct Header
    {
      //+0
      IdentifierType Identifier;
      //+4
      uint16_t NamesOffset;
      //+6
      uint16_t PositionsCountAndFlags;
      //+8
      uint16_t LoopPosition;//HVL: upper 6 bits are additional channels count
      //+10
      uint8_t TrackSize;
      //+11
      uint8_t TracksCount;
      //+12
      uint8_t SamplesCount;
      //+13
      uint8_t SubsongsCount;
      //HVL:
      //+14
      uint8_t MixGain;
      //+15
      uint8_t Defstereo;
    };
    
    struct Subsong
    {
      uint16_t Position;
    };
    
    struct Position
    {
      struct Channel
      {
        uint8_t Track;
        int8_t Transposition;
      };
      std::array<Channel, 4> Channels;//HVL: according to total channels count
    };
    
    struct Note
    {
      uint16_t NoteSampleCommand;
      uint8_t CommandData;
    };
    
    struct HVLNote
    {
      uint8_t Note;//if 0x3f, no other data stored
      uint8_t Sample;
      uint8_t Effect;
      uint16_t EffectParams;
    };
    
    struct Sample
    {
      uint8_t MasterVolume;
      uint8_t Flags;
      uint8_t AttackLength;
      uint8_t AttackVolume;
      uint8_t DecayLength;
      uint8_t DecayVolume;
      uint8_t SustainLength;
      uint8_t ReleaseLength;
      uint8_t ReleaseVolume;
      uint8_t Unused[3];
      uint8_t FilterModulationSpeedLowerLimit;
      uint8_t VibratoDelay;
      uint8_t HardcutVibratoDepth;
      uint8_t VibratoSpeed;
      uint8_t SquareModulationLowerLimit;
      uint8_t SquareModulationUpperLimit;
      uint8_t SquareModulationSpeed;
      uint8_t FilterModulationSpeedUpperLimit;
      uint8_t Speed;
      uint8_t Length;
      
      struct Entry
      {
        uint8_t Data[4];//HVL: 5 bytes
      };
    };
*/ 
    
    struct Header
    {
      const IdentifierType Id;
      const uint8_t Version;
      const std::size_t NamesOffset;
      const uint_t PositionsCount;
      const uint_t ChannelsCount;
      const uint_t TrackSize;
      const uint_t TracksCount;
      const uint_t SamplesCount;
      const uint_t SubsongsCount;
      
      explicit Header(Binary::InputStream& stream)
        : Id(stream.ReadField<IdentifierType>())
        , Version(stream.ReadField<uint8_t>())
        , NamesOffset(fromBE(stream.ReadField<uint16_t>()))
        , PositionsCount(fromBE(stream.ReadField<uint16_t>()) & 0xfff)
        , ChannelsCount(4 + (fromBE(stream.ReadField<uint16_t>()) >> 10))
        , TrackSize(stream.ReadField<uint8_t>())
        , TracksCount(stream.ReadField<uint8_t>())
        , SamplesCount(stream.ReadField<uint8_t>())
        , SubsongsCount(stream.ReadField<uint8_t>())
      {
      }
      
      bool IsAHX() const
      {
        return Id == ID_AHX && Version < 2;
      }
      
      bool IsHVL() const
      {
        return Id == ID_HVL && Version < 2;
      }
      
      std::size_t GetTracksOffset() const
      {
        const std::size_t HEADER_SIZE = IsAHX() ? 14 : 16;
        const std::size_t SUBSONG_SIZE = 2;
        const std::size_t POSITION_CHANNEL_SIZE = 2;
        return HEADER_SIZE + SubsongsCount * SUBSONG_SIZE + PositionsCount * ChannelsCount * POSITION_CHANNEL_SIZE;
      }
    };
    
    const std::size_t MIN_SIZE = 32;
    
    class StubBuilder : public Builder
    {
    public:
      MetaBuilder& GetMetaBuilder() override
      {
        return GetStubMetaBuilder();
      }
    };
    
    Builder& GetStubBuilder()
    {
      static StubBuilder stub;
      return stub;
    }
    
    class Format
    {
    public:
      explicit Format(const Binary::Container& data)
        : Stream(data)
        , Source(Stream)
      {
      }
      
      void Parse(Builder& target)
      {
        //TODO: add and use Stream.Seek
        Require(Stream.GetPosition() <= Source.NamesOffset);
        Stream.Skip(Source.NamesOffset - Stream.GetPosition());
        MetaBuilder& meta = target.GetMetaBuilder();
        const auto title = Strings::TrimSpaces(Stream.ReadCString(Stream.GetRestSize()));
        meta.SetTitle(Strings::ToAutoUtf8(title));
        ParseSampleNames(meta);
        ParseProgram(meta);
      }
      
      Formats::Chiptune::Container::Ptr GetContainer() const
      {
        const Binary::Container::Ptr rawData = Stream.GetReadData();
        const auto tracksOffset = Source.GetTracksOffset();
        return CreateCalculatingCrcContainer(rawData, tracksOffset, Source.NamesOffset - tracksOffset);
      }
    private:
      void ParseSampleNames(MetaBuilder& meta)
      {
        const uint_t count = Source.SamplesCount;
        Strings::Array names(count);
        for (uint_t smp = 0; smp < count; ++smp)
        {
          const auto name = Strings::TrimSpaces(Stream.ReadCString(Stream.GetRestSize()));
          names[smp] = Strings::ToAutoUtf8(name);
        }
        meta.SetStrings(std::move(names));
      }
      
      void ParseProgram(MetaBuilder& meta)
      {
        if (Source.IsAHX())
        {
          if (Source.Version == 0)
          {
            meta.SetProgram(Text::ABYSSHIGHESTEXPERIENCE_EDITOR_OLD);
          }
          else
          {
            meta.SetProgram(Text::ABYSSHIGHESTEXPERIENCE_EDITOR_NEW);
          }
        }
        else if (Source.IsHVL())
        {
          if (Source.Version == 0)
          {
            meta.SetProgram(Text::HIVELYTRACKER_EDITOR_OLD);
          }
          else
          {
            meta.SetProgram(Text::HIVELYTRACKER_EDITOR_NEW);
          }
        }
      }
    private:
      Binary::InputStream Stream;
      const Header Source;
    };
    
    bool FastCheck(const Binary::Container& rawData)
    {
      const auto size = rawData.Size();
      if (size < MIN_SIZE)
      {
        return false;
      }
      Binary::InputStream stream(rawData);
      const Header hdr(stream);
      const auto tracksOffset = hdr.GetTracksOffset();
      return hdr.NamesOffset > tracksOffset && size > hdr.NamesOffset;
    }
    
    struct FormatTraits
    {
      const char* Format;
      const Char* Description;
    };
    
    const FormatTraits AHXTraits = {
        "'T'H'X 00-01" //signature
        "??"           //names offset
        "%xxxx00xx ?"  //flags and positions count 0-0x3e7
        "%000000xx ?"  //restart position 0-0x3e6
        "01-40"        //track len
        "?"            //tracks count
        "00-3f"        //samples count
        "?"            //subsongs count
        ,
        Text::ABYSSHIGHESTEXPERIENCE_DECODER_DESCRIPTION
    };
    
    const FormatTraits HVLTraits = {
      "'H'V'L 00-01" //signature
      "??"           //names offset
      "%xxxx00xx ?"  //flags and positions count 0-0x3e7
      "%00xxxxxx ?"  //restart position 0-0x3e6, channels 0..12
      "01-40"        //track len
      "?"            //tracks count
      "00-3f"        //samples count
      "?"            //subsongs count
      "01-ff"        //mixgain, not zero
      "00-04"        //defstereo
      ,
      Text::HIVELYTRACKER_DECODER_DESCRIPTION
    };
    
    class VersionedDecoder : public Decoder
    {
    public:
      explicit VersionedDecoder(const FormatTraits& traits)
        : Traits(traits)
        , Header(Binary::CreateFormat(Traits.Format, MIN_SIZE))
      {
      }

      String GetDescription() const override
      {
        return Traits.Description;;
      }

      Binary::Format::Ptr GetFormat() const override
      {
        return Header;
      }

      bool Check(const Binary::Container& rawData) const override
      {
        return Header->Match(rawData) && FastCheck(rawData);
      }

      Formats::Chiptune::Container::Ptr Decode(const Binary::Container& rawData) const override
      {
        Builder& stub = GetStubBuilder();
        return Parse(rawData, stub);
      }

      Formats::Chiptune::Container::Ptr Parse(const Binary::Container& data, Builder& target) const override
      {
        if (!Check(data))
        {
          return Formats::Chiptune::Container::Ptr();
        }

        try
        {
          Format format(data);
          format.Parse(target);
          return format.GetContainer();
        }
        catch (const std::exception&)
        {
          return Formats::Chiptune::Container::Ptr();
        }
      }
    private:
      const FormatTraits Traits;
      const Binary::Format::Ptr Header;
    };

    Decoder::Ptr CreateDecoder()
    {
      return MakePtr<VersionedDecoder>(AHXTraits);
    }
    
    namespace HivelyTracker
    {
      Decoder::Ptr CreateDecoder()
      {
        return MakePtr<VersionedDecoder>(HVLTraits);
      }
    }
  }//namespace AbyssHighestExperience
  
  Decoder::Ptr CreateAbyssHighestExperienceDecoder()
  {
    return AbyssHighestExperience::CreateDecoder();
  }

  Decoder::Ptr CreateHivelyTrackerDecoder()
  {
    return AbyssHighestExperience::HivelyTracker::CreateDecoder();
  }
}
}
