/**
*
* @file      sound/filter.h
* @brief     Defenition of filtering-related functionality
* @version   $Id$
* @author    (C) Vitamin/CAIG/2001
*
**/

#ifndef __SOUND_FILTER_H_DEFINED__
#define __SOUND_FILTER_H_DEFINED__

#include <sound/receiver.h>

//forward declarations
class Error;

namespace ZXTune
{
  namespace Sound
  {
    //! @brief %Filter interface
    class Filter : public Converter
    {
    public:
      //! @brief Pointer type
      typedef boost::shared_ptr<Filter> Ptr;
      
      //! @brief Switching filter to bandpass mode
      //! @param freq Working sound frequency in Hz
      //! @param lowCutoff Low cutoff edge in Hz
      //! @param highCutoff High cutoff edge in Hz
      //! @return Error() in case of success
      virtual Error SetBandpassParameters(unsigned freq, unsigned lowCutoff, unsigned highCutoff) = 0;
    };
    
    //! @brief Creating FIR-filter instance
    //! @param order Filter order
    //! @param filter Reference to result value
    //! @return Error() in case of success
    Error CreateFIRFilter(unsigned order, Filter::Ptr& filter);
  }
}

#endif //__FILTER_H_DEFINED__
