/*
Abstract:
  Sdl api gate interface

Last changed:
  $Id$

  This file is automatically generated. Do not edit!
*/

#ifndef SOUND_BACKENDS_SDL_API_H_DEFINED
#define SOUND_BACKENDS_SDL_API_H_DEFINED

//platform-dependent includes
#include <SDL/SDL.h>
//boost includes
#include <boost/shared_ptr.hpp>

namespace Sound
{
  namespace Sdl
  {
    class Api
    {
    public:
      typedef boost::shared_ptr<Api> Ptr;
      virtual ~Api() {}

      
      virtual char* SDL_GetError(void) = 0;
      virtual const SDL_version* SDL_Linked_Version(void) = 0;
      virtual int SDL_Init(Uint32 flags) = 0;
      virtual int SDL_InitSubSystem(Uint32 flags) = 0;
      virtual void SDL_QuitSubSystem(Uint32 flags) = 0;
      virtual Uint32 SDL_WasInit(Uint32 flags) = 0;
      virtual void SDL_Quit(void) = 0;
      virtual int SDL_OpenAudio(SDL_AudioSpec *desired, SDL_AudioSpec *obtained) = 0;
      virtual void SDL_PauseAudio(int pause_on) = 0;
      virtual void SDL_CloseAudio(void) = 0;
    };

    //throw exception in case of error
    Api::Ptr LoadDynamicApi();

  }
}

#endif //SOUND_BACKENDS_SDL_API_H_DEFINED
