#include <tools.h>
#include <formatter.h>
#include <string_helpers.h>
#include <io/provider.h>
#include <core/player.h>
#include <core/plugin.h>
#include <core/error_codes.h>
#include <iostream>

#include <boost/bind.hpp>

using namespace ZXTune;

#define FILE_TAG 25CBBADB

namespace
{
  const unsigned PSG_SIZE = 24;
  const unsigned SKIP_SIZE = 8;
  const unsigned HOBETA_HDR_SIZE = 17;

  const unsigned SKIP1_OFFSET = 0;
  const unsigned HOBETA1_OFFSET = SKIP_SIZE;
  const unsigned PSG1_OFFSET = HOBETA1_OFFSET + HOBETA_HDR_SIZE;
  const unsigned HOBETA2_OFFSET = PSG1_OFFSET + PSG_SIZE;
  const unsigned SKIP2_OFFSET = HOBETA2_OFFSET + HOBETA_HDR_SIZE;
  const unsigned PSG2_OFFSET = SKIP2_OFFSET + SKIP_SIZE;
  const unsigned SKIP3_OFFSET = PSG2_OFFSET + PSG_SIZE;

  const unsigned HOB_CRC1 = 257 * (unsigned('f') + 'i' + 'l' + 'e' + 'n' + 'a' + 'm' + 'e' + 'e' + 'x' + 't' + 1 + PSG_SIZE) + 105;
  const unsigned HOB_CRC2 = 257 * (unsigned('f') + 'i' + 'l' + 'e' + 'n' + 'a' + 'm' + 'e' + 'e' + 'x' + 't' + 1 + PSG_SIZE + SKIP_SIZE * 2) + 105;
  const unsigned char PSG_DUMP[] = {
    //skip size=8
    0, 0, 0, 0, 0, 0, 0, 0,
    //hobeta header size=17 crc=105+257*Ea[0..14]=
    'f', 'i', 'l', 'e', 'n', 'a', 'm', 'e', 'e', 'x', 't', PSG_SIZE, 0,  0,  1,  HOB_CRC1 & 0xff, HOB_CRC1 >> 8,
    //offset size=24 crc=1720746500
    'P', 'S', 'G', 0x1a, 1, 50, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0xff,
    0, 0, 1, 1, //0x100
    7, 0xfe,
    0xfd,//end
    //hobeta header size=17
    'f', 'i', 'l', 'e', 'n', 'a', 'm', 'e', 'e', 'x', 't', PSG_SIZE + 2 * SKIP_SIZE, 0,  0,  1,  HOB_CRC2 & 0xff, HOB_CRC2 >> 8,
    //skip size=8
    0, 0, 0, 0, 0, 0, 0, 0,
    //size=24 crc=799865041
    'P', 'S', 'G', 0x1a, 1, 50, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0xff,
    0, 0xff,
    6, 10,
    7, ~(1 | 8),
    0xfd,
    
    0, 0, 0, 0, 0, 0, 0, 0//skip
  };
    
  class FixedContainer : public IO::DataContainer
  {
  public:
    FixedContainer(const uint8_t* data, std::size_t size)
      : Ptr(data), Len(size)
    {
    }
    
    virtual std::size_t Size() const
    {
      return Len;
    }
    virtual const void* Data() const
    {
      return Ptr;
    }
    virtual Ptr GetSubcontainer(std::size_t offset, std::size_t size) const
    {
      assert(offset + size <= Len);
      return DataContainer::Ptr(new FixedContainer(Ptr + offset, size));
    }
  private:
    const uint8_t* const Ptr;
    const std::size_t Len;
  };
  
  inline IO::DataContainer::Ptr CreateContainer(std::size_t offset = 0)
  {
    return IO::DataContainer::Ptr(new FixedContainer(PSG_DUMP + offset, ArraySize(PSG_DUMP) - offset));
  }
  
  void ErrOuter(unsigned /*level*/, Error::LocationRef loc, Error::CodeType code, const String& text)
  {
    std::cout << Error::AttributesToString(loc, code, text);;
  }
  
  void ShowPluginInfo(const PluginInformation& info)
  {
    std::cout <<
      " Plugin:\n"
      "  Id: " << info.Id << "\n"
      "  Descr: " << info.Description << "\n"
      "  Vers: " << info.Version << "\n"
      "  Caps: 0x" << std::hex << info.Capabilities << std::dec << "\n";
  }

  class OutVisitor : public boost::static_visitor<String>
  {
  public:
    String operator()(const Parameters::DataType& dmp) const
    {
      OutStringStream str;
      str << "<array of size " << dmp.size() << ">";
      return str.str();
    }
    template<class T>
    String operator()(const T& var) const
    {
      OutStringStream str;
      str << var;
      return str.str();
    }
  };

  void OutProp(const Parameters::Map::value_type& val)
  {
    std::cout << "  " << val.first << ": " << boost::apply_visitor(OutVisitor(), val.second) << "\n";
  }
  
  void ShowModuleInfo(const Module::Information& info)
  {
    std::cout << " Module:\n"
      "  Stat.Position: " << info.Statistic.Position << "\n"
      "  Stat.Pattern: " << info.Statistic.Pattern << "\n"
      "  Stat.Line: " << info.Statistic.Line << "\n"
      "  Stat.Frame: " << info.Statistic.Frame << "\n"
      "  Stat.Tempo: " << info.Statistic.Tempo << "\n"
      "  Stat.Channels: " << info.Statistic.Channels << "\n"
      "  Loop (p/f): " << info.LoopPosition << '/' << info.LoopFrame << "\n"
      "  Channels: " << info.PhysicalChannels << "\n";
    std::for_each(info.Properties.begin(), info.Properties.end(), OutProp);
  }
  
  bool PluginFilter(const PluginInformation& info)
  {
    return info.Id == "PSG";
  }

  bool NoHobeta(const PluginInformation& info)
  {
    return info.Id == "Hobeta";
  }

  void PluginLogger(const String& str)
  {
    std::cout << " >" << str << std::endl;
  }
  
  Error PluginCallback(Module::Holder::Ptr holder, unsigned& count, const bool& detailed)
  {
    PluginInformation plugInfo;
    holder->GetPlayerInfo(plugInfo);
    std::cout << " Plugin: " << plugInfo.Id << std::endl;
    if (detailed)
    {
      Module::Information modInfo;
      holder->GetModuleInformation(modInfo);
      ShowModuleInfo(modInfo);
    }
    ++count;
    return Error();
  }
  
  void TestPlayers(unsigned players, unsigned expected, const String& txt, unsigned line)
  {
    if (players == expected)
    {
      std::cout << "Passed test for " << txt << std::endl;
    }
    else
    {
      std::cout << "Failed test for " << txt << " at line " << line << std::endl;
      std::cout << "Found " << players << " players while " << expected << " expected" << std::endl;
      throw Error(THIS_LINE, 1);
    }
  }
  
  void TestError(const Error& err, Error::CodeType code, const String& txt, unsigned line)
  {
    if (err == code)
    {
      std::cout << "Passed test for " << txt << std::endl;
    }
    else
    {
      std::cout << "Failed test for " << txt << " at line " << line << std::endl;
    }
    err.WalkSuberrors(ErrOuter);
  }
}

int main()
{
  try
  {
    unsigned count = 0;
    bool detailed = true;
    
    std::vector<PluginInformation> plugins;
    EnumeratePlugins(plugins);
    std::cout << "Supported plugins:" << std::endl;
    std::for_each(plugins.begin(), plugins.end(), ShowPluginInfo);
    
    Parameters::Map commonParams;
    DetectParameters detectParams;
    
    TestError(DetectModules(commonParams, detectParams, CreateContainer(PSG1_OFFSET), String()), Module::ERROR_INVALID_PARAMETERS, "no callback", __LINE__);
    
    detectParams.Callback = boost::bind(&PluginCallback, _1, boost::ref(count), boost::cref(detailed));
    detectParams.Logger = PluginLogger;
    
    TestError(DetectModules(commonParams, detectParams, IO::DataContainer::Ptr(), String()), Module::ERROR_INVALID_PARAMETERS, "no data", __LINE__);
    
    
    TestError(DetectModules(commonParams, detectParams, CreateContainer(), "invalid_path"), Module::ERROR_FIND_SUBMODULE, "invalid path", __LINE__);
    count = 0;
    ThrowIfError(DetectModules(commonParams, detectParams, CreateContainer(PSG2_OFFSET), String()));
    TestPlayers(count, 1, "simple opening", __LINE__);
    detectParams.Filter = PluginFilter;
    count = 0;
    ThrowIfError(DetectModules(commonParams, detectParams, CreateContainer(PSG1_OFFSET), String()));
    TestPlayers(count, 0, "filtered opening", __LINE__);

    //testing without hobeta
    detailed = false;
    
    detectParams.Filter = NoHobeta;
    ThrowIfError(DetectModules(commonParams, detectParams, CreateContainer(), String()));
    TestPlayers(count, 2, "raw scanning opening (no implicit)", __LINE__);
    count = 0;
    ThrowIfError(DetectModules(commonParams, detectParams, CreateContainer(PSG1_OFFSET), String()));
    TestPlayers(count, 2, "raw scanning starting from begin (no implicit)", __LINE__);
    count = 0;
    ThrowIfError(DetectModules(commonParams, detectParams, CreateContainer(), (Formatter("%1%.raw") % PSG2_OFFSET).str()));
    TestPlayers(count, 1, "detect from startpoint (no implicit)", __LINE__);
    count = 0;
    ThrowIfError(DetectModules(commonParams, detectParams, CreateContainer(), "1.raw"));
    TestPlayers(count, 0, "detect from invalid offset (no implicit)", __LINE__);

    detectParams.Filter = 0;
    count = 0;
    //
    ThrowIfError(DetectModules(commonParams, detectParams, CreateContainer(), String()));
    TestPlayers(count, 2, "raw scanning opening", __LINE__);
    count = 0;
    ThrowIfError(DetectModules(commonParams, detectParams, CreateContainer(PSG1_OFFSET), String()));
    TestPlayers(count, 2, "raw scanning starting from begin", __LINE__);
    count = 0;
    ThrowIfError(DetectModules(commonParams, detectParams, CreateContainer(), (Formatter("%1%.raw") % PSG2_OFFSET).str()));
    TestPlayers(count, 1, "detect from startpoint", __LINE__);
    count = 0;
    ThrowIfError(DetectModules(commonParams, detectParams, CreateContainer(), "1.raw"));
    TestPlayers(count, 0, "detect from invalid offset", __LINE__);
  }
  catch (const Error& e)
  {
    e.WalkSuberrors(ErrOuter);
  }
}
