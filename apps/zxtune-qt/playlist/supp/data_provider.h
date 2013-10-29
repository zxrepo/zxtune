/**
* 
* @file
*
* @brief Playlist data provider interface
*
* @author vitamin.caig@gmail.com
*
**/

#pragma once

//local includes
#include "data.h"

namespace Playlist
{
  namespace Item
  {
    class DetectParameters
    {
    public:
      virtual ~DetectParameters() {}

      virtual Parameters::Container::Ptr CreateInitialAdjustedParameters() const = 0;
      virtual void ProcessItem(Data::Ptr item) = 0;
      virtual void ShowProgress(unsigned progress) = 0;
      virtual void ShowMessage(const String& message) = 0;
    };

    class DataProvider
    {
    public:
      typedef boost::shared_ptr<const DataProvider> Ptr;

      virtual ~DataProvider() {}

      virtual void DetectModules(const String& path, DetectParameters& detectParams) const = 0;

      virtual void OpenModule(const String& path, DetectParameters& detectParams) const = 0;

      static Ptr Create(Parameters::Accessor::Ptr parameters);
    };
  }
}
