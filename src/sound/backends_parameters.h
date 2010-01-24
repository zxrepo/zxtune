/**
*
* @file      sound/backends_parameters.h
* @brief     Backends parameters names
* @version   $Id$
* @author    (C) Vitamin/CAIG/2001
*
**/

#ifndef __SOUND_BACKENDS_PARAMETERS_H_DEFINED__
#define __SOUND_BACKENDS_PARAMETERS_H_DEFINED__

#include <parameters.h>

namespace Parameters
{
  namespace ZXTune
  {
    namespace Sound
    {
      //! @brief Backend-specific parameters namespace
      namespace Backends
      {
        //! @brief Parameters#ZXTune#Sound#Backends namespace prefix
        const Char PREFIX[] =
        {
          'z','x','t','u','n','e','.','s','o','u','n','d','.','b','a','c','k','e','n','d','s','.','\0'
        };
        
        //! @brief %Wav backend parameters namespace
        namespace Wav
        {
          //! @brief Output filename template
          //! @see core/module_attrs.h for possibly supported field names
          const Char FILENAME[] =
          {
            'z','x','t','u','n','e','.','s','o','u','n','d','.','b','a','c','k','e','n','d','s','.','w','a','v','.','f','i','l','e','n','a','m','e','\0'
          };
        }

        //! @brief %Win32 backend parameters namespace
        namespace Win32
        {
          //@{
          //! @name Output device parameter
          //! @brief 0-based output device index
          
          //! Default value
          const IntType DEVICE_DEFAULT = -1;
          //! Parameter name
          const char DEVICE[] =
          {
            'z','x','t','u','n','e','.','s','o','u','n','d','.','b','a','c','k','e','n','d','s','.','w','i','n','3','2','.','d','e','v','i','c','e','\0'
          };
          //@}
          
          //@{
          //! @name Buffers count parameter
          
          //! Default value
          const IntType BUFFERS_DEFAULT = 3;
          //! Parameters name
          const char BUFFERS[] =
          {
            'z','x','t','u','n','e','.','s','o','u','n','d','.','b','a','c','k','e','n','d','s','.','w','i','n','3','2','.','b','u','f','f','e','r','s','\0'
          };
          //@}
        }

        //! @brief %OSS backend parameters
        namespace OSS
        {
          //@{
          //! @name Output device filename
          
          //! Default value
          const Char DEVICE_DEFAULT[] = {'/', 'd', 'e', 'v', '/', 'd', 's', 'p', '\0'};
          //! Parameter name
          const Char DEVICE[] =
          {
            'z','x','t','u','n','e','.','s','o','u','n','d','.','b','a','c','k','e','n','d','s','.','o','s','s','.','d','e','v','i','c','e','\0'
          };
          //@}
          
          //@{
          //! @name Mixer device filename
          
          //! Default value
          const Char MIXER_DEFAULT[] = {'/', 'd', 'e', 'v', '/', 'm', 'i', 'x', 'e', 'r', '\0'};
          //! Parameter name
          const Char MIXER[] =
          {
            'z','x','t','u','n','e','.','s','o','u','n','d','.','b','a','c','k','e','n','d','s','.','o','s','s','.','m','i','x','e','r','\0'
          };
        }
      }
    }
  }
}
#endif //__SOUND_BACKENDS_PARAMETERS_H_DEFINED__
