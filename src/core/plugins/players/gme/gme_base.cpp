/**
* 
* @file
*
* @brief  Game Music Emu-based formats support plugin
*
* @author vitamin.caig@gmail.com
*
**/

//local includes
#include "core/plugins/player_plugins_registrator.h"
#include "core/plugins/players/duration.h"
#include "core/plugins/players/plugin.h"
#include "core/plugins/players/properties_helper.h"
#include "core/plugins/players/streaming.h"
//common includes
#include <contract.h>
#include <make_ptr.h>
//library includes
#include <binary/format_factories.h>
#include <core/module_attrs.h>
#include <core/plugin_attrs.h>
#include <debug/log.h>
#include <devices/details/analysis_map.h>
#include <formats/chiptune/decoders.h>
#include <formats/multitrack/decoders.h>
#include <formats/chiptune/multitrack/decoders.h>
#include <formats/chiptune/multitrack/multitrack.h>
#include <math/numeric.h>
#include <parameters/tracking_helper.h>
#include <sound/chunk_builder.h>
#include <sound/render_params.h>
#include <sound/sound_parameters.h>
//std includes
#include <map>
//boost includes
#include <boost/range/end.hpp>
#include <boost/algorithm/string/predicate.hpp>
//3rdparty
#include <3rdparty/gme/gme/Gbs_Emu.h>
#include <3rdparty/gme/gme/Hes_Emu.h>
#include <3rdparty/gme/gme/Nsf_Emu.h>
#include <3rdparty/gme/gme/Nsfe_Emu.h>
#include <3rdparty/gme/gme/Sap_Emu.h>
#include <3rdparty/gme/gme/Vgm_Emu.h>
#include <3rdparty/gme/gme/Gym_Emu.h>

namespace
{
  const Debug::Stream Dbg("Core::GMESupp");
}

namespace Module
{
namespace GME
{
  typedef boost::shared_ptr< ::Music_Emu> EmuPtr;

  typedef EmuPtr (*EmuCreator)();
  
  template<class EmuType>
  EmuPtr Create()
  {
    return EmuPtr(new EmuType());
  }
  
  class GME : public Module::Analyzer
  {
  public:
    typedef boost::shared_ptr<GME> Ptr;
    
    GME(EmuCreator create, Binary::Data::Ptr data, uint_t track)
      : CreateEmu(create)
      , Data(data)
      , Track(track)
      , SoundFreq(0)
    {
      Load(Parameters::ZXTune::Sound::FREQUENCY_DEFAULT);
    }
    
    uint_t GetTotalTracks() const
    {
      return Emu->track_count();
    }
    
    void GetInfo(::track_info_t& info) const
    {
      CheckError(Emu->track_info(&info, Track));
    }
    
    void Reset()
    {
      CheckError(Emu->start_track(Track));
    }
    
    void Render(uint_t samples, Sound::ChunkBuilder& target)
    {
      BOOST_STATIC_ASSERT(Sound::Sample::CHANNELS == 2);
      BOOST_STATIC_ASSERT(Sound::Sample::BITS == 16);
      ::Music_Emu::sample_t* const buffer = safe_ptr_cast< ::Music_Emu::sample_t*>(target.Allocate(samples));
      CheckError(Emu->play(samples * Sound::Sample::CHANNELS, buffer));
    }
    
    void Skip(uint_t samples)
    {
      CheckError(Emu->skip(samples));
    }
    
    void SetSoundFreq(uint_t soundFreq)
    {
      if (soundFreq != SoundFreq)
      {
        Load(soundFreq);
      }
    }
    
    virtual void GetState(std::vector<ChannelState>& channels) const
    {
      std::vector<voice_status_t> voices(Emu->voice_count());
      const int actual = Emu->voices_status(&voices[0], voices.size());
      std::vector<ChannelState> result;
      for (int chan = 0; chan < actual; ++chan)
      {
        const voice_status_t& in = voices[chan];
        Devices::Details::AnalysisMap& analysis = GetAnalysisFor(in.frequency);
        ChannelState state;
        state.Level = in.level * 100 / voice_max_level;
        state.Band = analysis.GetBandByPeriod(in.divider);
        result.push_back(state);
      }
      channels.swap(result);
    }
  private:
    inline static void CheckError(::blargg_err_t err)
    {
      if (err)
      {
        throw std::runtime_error(err);
      }
    }

    void Load(uint_t soundFreq)
    {
      const int oldPos = Emu ? Emu->tell() : 0;
      EmuPtr emu = CreateEmu();
      CheckError(emu->set_sample_rate(soundFreq));
      CheckError(emu->load_mem(Data->Start(), Data->Size()));
      CheckError(emu->start_track(Track));
      if (oldPos)
      {
        CheckError(emu->seek(oldPos));
      }
      Emu.swap(emu);
      SoundFreq = soundFreq;
    }

    Devices::Details::AnalysisMap& GetAnalysisFor(int freq) const
    {
      Devices::Details::AnalysisMap& result = Analysis[freq];
      result.SetClockRate(freq);
      return result;
    }
  private:
    const EmuCreator CreateEmu;
    const Binary::Data::Ptr Data;
    const uint_t Track;
    uint_t SoundFreq;
    EmuPtr Emu;
    mutable std::map<int, Devices::Details::AnalysisMap> Analysis;
  };
  
  class Renderer : public Module::Renderer
  {
  public:
    Renderer(GME::Ptr tune, StateIterator::Ptr iterator, Sound::Receiver::Ptr target, Parameters::Accessor::Ptr params)
      : Tune(tune)
      , Iterator(iterator)
      , State(Iterator->GetStateObserver())
      , SoundParams(Sound::RenderParameters::Create(params))
      , Target(target)
      , Looped()
    {
      ApplyParameters();
    }

    virtual TrackState::Ptr GetTrackState() const
    {
      return State;
    }

    virtual Module::Analyzer::Ptr GetAnalyzer() const
    {
      return Tune;
    }

    virtual bool RenderFrame()
    {
      try
      {
        ApplyParameters();

        Sound::ChunkBuilder builder;
        const uint_t samplesPerFrame = SoundParams->SamplesPerFrame();
        builder.Reserve(samplesPerFrame);
        Tune->Render(samplesPerFrame, builder);
        Target->ApplyData(builder.GetResult());
        Iterator->NextFrame(Looped);
        return Iterator->IsValid();
      }
      catch (const std::exception&)
      {
        return false;
      }
    }

    virtual void Reset()
    {
      try
      {
        SoundParams.Reset();
        Iterator->Reset();
        Tune->Reset();
      }
      catch (const std::exception& e)
      {
        Dbg(e.what());
      }
    }

    virtual void SetPosition(uint_t frame)
    {
      try
      {
        SeekTune(frame);
      }
      catch (const std::exception& e)
      {
        Dbg(e.what());
      }
      Module::SeekIterator(*Iterator, frame);
    }
  private:
    void ApplyParameters()
    {
      if (SoundParams.IsChanged())
      {
        Looped = SoundParams->Looped();
        Tune->SetSoundFreq(SoundParams->SoundFreq());
      }
    }

    void SeekTune(uint_t frame)
    {
      uint_t current = State->Frame();
      if (frame < current)
      {
        Tune->Reset();
        current = 0;
      }
      if (const uint_t delta = frame - current)
      {
        Tune->Skip(delta * SoundParams->SamplesPerFrame());
      }
    }
  private:
    const GME::Ptr Tune;
    const StateIterator::Ptr Iterator;
    const TrackState::Ptr State;
    Parameters::TrackingHelper<Sound::RenderParameters> SoundParams;
    const Sound::Receiver::Ptr Target;
    bool Looped;
  };
  
  class Holder : public Module::Holder
  {
  public:
    Holder(GME::Ptr tune, Information::Ptr info, Parameters::Accessor::Ptr props)
      : Tune(tune)
      , Info(info)
      , Properties(props)
    {
    }

    virtual Module::Information::Ptr GetModuleInformation() const
    {
      return Info;
    }

    virtual Parameters::Accessor::Ptr GetModuleProperties() const
    {
      return Properties;
    }

    virtual Renderer::Ptr CreateRenderer(Parameters::Accessor::Ptr params, Sound::Receiver::Ptr target) const
    {
      return MakePtr<Renderer>(Tune, Module::CreateStreamStateIterator(Info), target, params);
    }
  private:
    const GME::Ptr Tune;
    const Information::Ptr Info;
    const Parameters::Accessor::Ptr Properties;
  };

  struct PluginDescription
  {
    const String Id;
    const uint_t ChiptuneCaps;
    const EmuCreator CreateEmu;
  };
  
  const Time::Milliseconds PERIOD(20);
  
  Time::Milliseconds GetProperties(const GME& gme, PropertiesHelper& props)
  {
    ::track_info_t info;
    gme.GetInfo(info);
    
    const String& system = FromStdString(info.system);
    const String& song = FromStdString(info.song);
    const String& game = FromStdString(info.game);
    const String& author = FromStdString(info.author);
    const String& comment = FromStdString(info.comment);
    const String& copyright = FromStdString(info.copyright);
    const String& dumper = FromStdString(info.dumper);
    
    props.SetComputer(system);
    props.SetTitle(game);
    props.SetTitle(song);
    props.SetProgram(game);
    props.SetAuthor(dumper);
    props.SetAuthor(author);
    props.SetComment(copyright);
    props.SetComment(comment);
    props.SetFramesFrequency(Time::GetFrequencyForPeriod(PERIOD));

    if (info.length > 0)
    {
      return Time::Milliseconds(info.length);
    }
    else if (info.loop_length > 0)
    {
      return Time::Milliseconds(info.intro_length + info.loop_length);
    }
    else
    {
      return Time::Milliseconds();
    }
  }
  
  class MultitrackFactory : public Module::Factory
  {
  public:
    MultitrackFactory(const PluginDescription& desc, Formats::Multitrack::Decoder::Ptr decoder)
      : Desc(desc)
      , Decoder(decoder)
    {
    }
    
    virtual Module::Holder::Ptr CreateModule(const Parameters::Accessor& params, const Binary::Container& rawData, Parameters::Container::Ptr properties) const
    {
      try
      {
        if (const Formats::Multitrack::Container::Ptr container = Decoder->Decode(rawData))
        {
          if (container->TracksCount() > 1)
          {
            Require(HasContainer(Desc.Id, properties));
          }

          const GME::Ptr tune = MakePtr<GME>(Desc.CreateEmu, container, container->StartTrackIndex());
          PropertiesHelper props(*properties);
          const Time::Milliseconds storedDuration = GetProperties(*tune, props);
          const Time::Milliseconds duration = storedDuration == Time::Milliseconds() ? Time::Milliseconds(GetDuration(params)) : storedDuration;
          const Information::Ptr info = CreateStreamInfo(duration.Get() / PERIOD.Get());
        
          props.SetSource(*Formats::Chiptune::CreateMultitrackChiptuneContainer(container));
        
          return MakePtr<Holder>(tune, info, properties);
        }
      }
      catch (const std::exception& e)
      {
        Dbg("Failed to create %1%: %2%", Desc.Id, e.what());
      }
      return Module::Holder::Ptr();
    }
  private:
    static bool HasContainer(const String& type, Parameters::Accessor::Ptr params)
    {
      Parameters::StringType container;
      Require(params->FindValue(Module::ATTR_CONTAINER, container));
      return container == type || boost::algorithm::ends_with(container, ">" + type);
    }
  private:
    const PluginDescription Desc;
    const Formats::Multitrack::Decoder::Ptr Decoder;
  };
  
  class SingletrackFactory : public Module::Factory
  {
  public:
    SingletrackFactory(const PluginDescription& desc, Formats::Chiptune::Decoder::Ptr decoder)
      : Desc(desc)
      , Decoder(decoder)
    {
    }
    
    virtual Module::Holder::Ptr CreateModule(const Parameters::Accessor& params, const Binary::Container& rawData, Parameters::Container::Ptr properties) const
    {
      try
      {
        if (const Formats::Chiptune::Container::Ptr container = Decoder->Decode(rawData))
        {
          const GME::Ptr tune = MakePtr<GME>(Desc.CreateEmu, container, 0);
          PropertiesHelper props(*properties);
          const Time::Milliseconds storedDuration = GetProperties(*tune, props);
          const Time::Milliseconds duration = storedDuration == Time::Milliseconds() ? Time::Milliseconds(GetDuration(params)) : storedDuration;
          const Information::Ptr info = CreateStreamInfo(duration.Get() / PERIOD.Get());
        
          props.SetSource(*container);
        
          return MakePtr<Holder>(tune, info, properties);
        }
      }
      catch (const std::exception& e)
      {
        Dbg("Failed to create %1%: %2%", Desc.Id, e.what());
      }
      return Module::Holder::Ptr();
    }
  private:
    const PluginDescription Desc;
    const Formats::Chiptune::Decoder::Ptr Decoder;
  };

  struct MultitrackPluginDescription
  {
    typedef Formats::Multitrack::Decoder::Ptr (*MultitrackDecoderCreator)();
    typedef Formats::Chiptune::Decoder::Ptr (*ChiptuneDecoderCreator)(Formats::Multitrack::Decoder::Ptr);

    PluginDescription Desc;
    const MultitrackDecoderCreator CreateMultitrackDecoder;
    const ChiptuneDecoderCreator CreateChiptuneDecoder;
  };
  
  const MultitrackPluginDescription MULTITRACK_PLUGINS[] =
  {
    //nsf
    {
      {
        "NSF",
        ZXTune::Capabilities::Module::Type::MEMORYDUMP | ZXTune::Capabilities::Module::Device::RP2A0X,
        &Create< ::Nsf_Emu>
      },
      &Formats::Multitrack::CreateNSFDecoder,
      &Formats::Chiptune::CreateNSFDecoder,
    },
    //nsfe
    {
      {
        "NSFE",
        ZXTune::Capabilities::Module::Type::MEMORYDUMP | ZXTune::Capabilities::Module::Device::RP2A0X,
        &Create< ::Nsfe_Emu>
      },
      &Formats::Multitrack::CreateNSFEDecoder,
      &Formats::Chiptune::CreateNSFEDecoder,
    },
    //gbs
    {
      {
        "GBS",
        ZXTune::Capabilities::Module::Type::MEMORYDUMP | ZXTune::Capabilities::Module::Device::LR35902,
        &Create< ::Gbs_Emu>
      },
      &Formats::Multitrack::CreateGBSDecoder,
      &Formats::Chiptune::CreateGBSDecoder,
    },
    //sap
    {
      {
        "SAP",
        ZXTune::Capabilities::Module::Type::MEMORYDUMP | ZXTune::Capabilities::Module::Device::CO12294,
        &Create< ::Sap_Emu>
      },
      &Formats::Multitrack::CreateSAPDecoder,
      &Formats::Chiptune::CreateSAPDecoder,
    }
  };
  
  struct SingletrackPluginDescription
  {
    typedef Formats::Chiptune::Decoder::Ptr (*ChiptuneDecoderCreator)();

    PluginDescription Desc;
    const ChiptuneDecoderCreator CreateChiptuneDecoder;
  };

  const SingletrackPluginDescription SINGLETRACK_PLUGINS[] =
  {
    //hes
    {
      {
        "HES",
        ZXTune::Capabilities::Module::Type::MEMORYDUMP | ZXTune::Capabilities::Module::Device::HUC6270,
        &Create< ::Hes_Emu>
      },
      &Formats::Chiptune::CreateHESDecoder
    },
    //vgm
    {
      {
        "VGM",
        ZXTune::Capabilities::Module::Type::STREAM | ZXTune::Capabilities::Module::Device::MULTI,
        &Create< ::Vgm_Emu>
      },
      &Formats::Chiptune::CreateVideoGameMusicDecoder
    },
    //vgm
    {
      {
        "GYM",
        ZXTune::Capabilities::Module::Type::STREAM | ZXTune::Capabilities::Module::Device::MULTI,
        &Create< ::Gym_Emu>
      },
      &Formats::Chiptune::CreateGYMDecoder
    }
  };
}
}

namespace ZXTune
{
  void RegisterMultitrackGMEPlugins(PlayerPluginsRegistrator& registrator)
  {
    for (const Module::GME::MultitrackPluginDescription* it = Module::GME::MULTITRACK_PLUGINS; it != boost::end(Module::GME::MULTITRACK_PLUGINS); ++it)
    {
      const Module::GME::MultitrackPluginDescription& desc = *it;

      const Formats::Multitrack::Decoder::Ptr multi = desc.CreateMultitrackDecoder();
      const uint_t caps = desc.Desc.ChiptuneCaps;
      const Formats::Chiptune::Decoder::Ptr decoder = desc.CreateChiptuneDecoder(multi);
      const Module::Factory::Ptr factory = MakePtr<Module::GME::MultitrackFactory>(desc.Desc, multi);
      const PlayerPlugin::Ptr plugin = CreatePlayerPlugin(desc.Desc.Id, caps, decoder, factory);
      registrator.RegisterPlugin(plugin);
    }
  }
  
  void RegisterSingletrackGMEPlugins(PlayerPluginsRegistrator& registrator)
  {
    for (const Module::GME::SingletrackPluginDescription* it = Module::GME::SINGLETRACK_PLUGINS; it != boost::end(Module::GME::SINGLETRACK_PLUGINS); ++it)
    {
      const Module::GME::SingletrackPluginDescription& desc = *it;

      const uint_t caps = desc.Desc.ChiptuneCaps;
      const Formats::Chiptune::Decoder::Ptr decoder = desc.CreateChiptuneDecoder();
      const Module::Factory::Ptr factory = MakePtr<Module::GME::SingletrackFactory>(desc.Desc, decoder);
      const PlayerPlugin::Ptr plugin = CreatePlayerPlugin(desc.Desc.Id, caps, decoder, factory);
      registrator.RegisterPlugin(plugin);
    }
  }

  void RegisterGMEPlugins(PlayerPluginsRegistrator& registrator)
  {
    RegisterMultitrackGMEPlugins(registrator);
    RegisterSingletrackGMEPlugins(registrator);
  }
}
