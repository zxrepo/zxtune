/**
*
* @file     core/error_codes.h
* @brief    %Error codes for core subsystem
* @version  $Id$
* @author   (C) Vitamin/CAIG/2001
*
**/

#ifndef __CORE_ERROR_CODES_H_DEFINED__
#define __CORE_ERROR_CODES_H_DEFINED__

//common includes
#include <error.h>

namespace ZXTune
{
  //! @brief %Module-related functionality namespace
  namespace Module
  {
    //! @brief Module-related error codes
    enum ErrorCode
    {
      //! Input parameters are invalid
      ERROR_INVALID_PARAMETERS = Error::ModuleCode<'M', 'O', 'D'>::Value,
      //! Invalid module format detected
      ERROR_INVALID_FORMAT,
      //! Attempting to playback module after the end reached
      ERROR_MODULE_END,
      //! Failed to convert module
      ERROR_MODULE_CONVERT,
      //! Failed to find submodule by specified path
      ERROR_FIND_SUBMODULE,
      //! Module detection is canceled
      ERROR_DETECT_CANCELED,
      //! No memory
      ERROR_NO_MEMORY,
      //! @internal Failed to find container plugin
      ERROR_FIND_CONTAINER_PLUGIN,
      //! @internal Failed to find archive plugin
      ERROR_FIND_ARCHIVE_PLUGIN,
      //! @internal Failed to find player plugin
      ERROR_FIND_PLAYER_PLUGIN
    };
  }
}

#endif //__CORE_ERROR_CODES_H_DEFINED__
