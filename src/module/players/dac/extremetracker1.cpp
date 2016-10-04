/**
* 
* @file
*
* @brief  ExtremeTracker1 chiptune factory implementation
*
* @author vitamin.caig@gmail.com
*
**/

//local includes
#include "extremetracker1.h"
#include "dac_properties_helper.h"
#include "dac_simple.h"
//common includes
#include <make_ptr.h>
//library includes
#include <devices/dac/sample_factories.h>
#include <formats/chiptune/digital/extremetracker1.h>
#include <module/players/properties_meta.h>
#include <module/players/simple_orderlist.h>
#include <module/players/tracking.h>

namespace Module
{
namespace ExtremeTracker1
{
  const std::size_t CHANNELS_COUNT = 4;

  const uint_t C_1_STEP_GLISS = 0x50;
  
  //supported tracking commands
  enum CmdType
  {
    //no parameters
    EMPTY,
    //1 param
    GLISS,
  };

  inline int_t StepToHz(int_t step)
  {
    //C-1 frequency is 32.7Hz
    //step * 32.7 / c-1_step
    return step * 3270 / int_t(C_1_STEP_GLISS * 100);
  }
  
  typedef DAC::SimpleModuleData ModuleData;

  class DataBuilder : public Formats::Chiptune::ExtremeTracker1::Builder
  {
  public:
    explicit DataBuilder(DAC::PropertiesHelper& props)
      : Data(MakeRWPtr<ModuleData>())
      , Properties(props)
      , Meta(props)
      , Patterns(PatternsBuilder::Create<CHANNELS_COUNT>())
    {
      Data->Patterns = Patterns.GetResult();
    }

    virtual Formats::Chiptune::MetaBuilder& GetMetaBuilder()
    {
      return Meta;
    }

    virtual void SetInitialTempo(uint_t tempo)
    {
      Data->InitialTempo = tempo;
    }

    virtual void SetSamplesFrequency(uint_t freq)
    {
      Properties.SetSamplesFrequency(freq);
    }

    virtual void SetSample(uint_t index, std::size_t loop, Binary::Data::Ptr sample)
    {
      Data->Samples.Add(index, Devices::DAC::CreateU8Sample(sample, loop));
    }

    virtual void SetPositions(const std::vector<uint_t>& positions, uint_t loop)
    {
      Data->Order = MakePtr<SimpleOrderList>(loop, positions.begin(), positions.end());
    }

    virtual Formats::Chiptune::PatternBuilder& StartPattern(uint_t index)
    {
      Patterns.SetPattern(index);
      return Patterns;
    }

    virtual void StartChannel(uint_t index)
    {
      Patterns.SetChannel(index);
    }

    virtual void SetRest()
    {
      Patterns.GetChannel().SetEnabled(false);
    }

    virtual void SetNote(uint_t note)
    {
      Patterns.GetChannel().SetEnabled(true);
      Patterns.GetChannel().SetNote(note);
    }

    virtual void SetSample(uint_t sample)
    {
      Patterns.GetChannel().SetSample(sample);
    }

    virtual void SetVolume(uint_t volume)
    {
      Patterns.GetChannel().SetVolume(volume);
    }

    virtual void SetGliss(int_t gliss)
    {
      Patterns.GetChannel().AddCommand(GLISS, gliss);
    }
    
    ModuleData::Ptr GetResult() const
    {
      return Data;
    }
  private:
    const ModuleData::RWPtr Data;
    DAC::PropertiesHelper& Properties;
    MetaProperties Meta;
    PatternsBuilder Patterns;
  };
  
  struct GlissData
  {
    GlissData() : Sliding(), Glissade()
    {
    }
    int_t Sliding;
    int_t Glissade;

    void Reset()
    {
      Sliding = Glissade = 0;
    }

    bool Update()
    {
      Sliding += Glissade;
      return Glissade != 0;
    }
  };

  class DataRenderer : public DAC::DataRenderer
  {
  public:
    explicit DataRenderer(ModuleData::Ptr data)
      : Data(data)
    {
      Reset();
    }

    virtual void Reset()
    {
      std::fill(Gliss.begin(), Gliss.end(), GlissData());
    }

    virtual void SynthesizeData(const TrackModelState& state, DAC::TrackBuilder& track)
    {
      SynthesizeChannelsData(track);
      if (0 == state.Quirk())
      {
        GetNewLineState(state, track);
      }
    }
  private:
    void SynthesizeChannelsData(DAC::TrackBuilder& track)
    {
      for (uint_t chan = 0; chan != CHANNELS_COUNT; ++chan)
      {
        GlissData& gliss(Gliss[chan]);
        if (gliss.Update())
        {
          DAC::ChannelDataBuilder builder = track.GetChannel(chan);
          builder.SetFreqSlideHz(StepToHz(gliss.Sliding));
        }
      }
    }

    void GetNewLineState(const TrackModelState& state, DAC::TrackBuilder& track)
    {
      if (const Line::Ptr line = state.LineObject())
      {
        for (uint_t chan = 0; chan != CHANNELS_COUNT; ++chan)
        {
          if (const Cell::Ptr src = line->GetChannel(chan))
          {
            DAC::ChannelDataBuilder builder = track.GetChannel(chan);
            GetNewChannelState(*src, Gliss[chan], builder);
          }
        }
      }
    }

    void GetNewChannelState(const Cell& src, GlissData& gliss, DAC::ChannelDataBuilder& builder)
    {
      if (src.HasData())
      {
        builder.SetLevelInPercents(100);
      }
      if (const bool* enabled = src.GetEnabled())
      {
        builder.SetEnabled(*enabled);
        if (!*enabled)
        {
          builder.SetPosInSample(0);
        }
      }
      if (const uint_t* note = src.GetNote())
      {
        builder.SetNote(*note);
      }
      if (const uint_t* sample = src.GetSample())
      {
        builder.SetSampleNum(*sample);
        builder.SetPosInSample(0);
        builder.SetFreqSlideHz(0);
        gliss.Sliding = 0;
      }
      if (const uint_t* volume = src.GetVolume())
      {
        const uint_t level = *volume;
        builder.SetLevelInPercents(100 * level / 16);
      }
      for (CommandsIterator it = src.GetCommands(); it; ++it)
      {
        switch (it->Type)
        {
        case GLISS:
          gliss.Glissade = it->Param1;
          break;
        default:
          assert(!"Invalid command");
        }
      }
    }
  private:
    const ModuleData::Ptr Data;
    std::array<GlissData, CHANNELS_COUNT> Gliss;
  };

  class Chiptune : public DAC::Chiptune
  {
  public:
    Chiptune(ModuleData::Ptr data, Parameters::Accessor::Ptr properties)
      : Data(data)
      , Properties(properties)
      , Info(CreateTrackInfo(Data, CHANNELS_COUNT))
    {
    }

    virtual Information::Ptr GetInformation() const
    {
      return Info;
    }

    virtual Parameters::Accessor::Ptr GetProperties() const
    {
      return Properties;
    }

    virtual DAC::DataIterator::Ptr CreateDataIterator() const
    {
      const TrackStateIterator::Ptr iterator = CreateTrackStateIterator(Data);
      const DAC::DataRenderer::Ptr renderer = MakePtr<DataRenderer>(Data);
      return DAC::CreateDataIterator(iterator, renderer);
    }

    virtual void GetSamples(Devices::DAC::Chip::Ptr chip) const
    {
      for (uint_t idx = 0, lim = Data->Samples.Size(); idx != lim; ++idx)
      {
        chip->SetSample(idx, Data->Samples.Get(idx));
      }
    }
  private:
    const ModuleData::Ptr Data;
    const Parameters::Accessor::Ptr Properties;
    const Information::Ptr Info;
  };

  class Factory : public DAC::Factory
  {
  public:
    virtual DAC::Chiptune::Ptr CreateChiptune(const Binary::Container& rawData, Parameters::Container::Ptr properties) const
    {
      DAC::PropertiesHelper props(*properties);
      DataBuilder dataBuilder(props);
      if (const Formats::Chiptune::Container::Ptr container = Formats::Chiptune::ExtremeTracker1::Parse(rawData, dataBuilder))
      {
        props.SetSource(*container);
        return MakePtr<Chiptune>(dataBuilder.GetResult(), properties);
      }
      else
      {
        return DAC::Chiptune::Ptr();
      }
    }
  };
  
  Factory::Ptr CreateFactory()
  {
    return MakePtr<Factory>();
  }
}
}
