/*
Abstract:
  AYM-based modules support

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

#pragma once
#ifndef CORE_PLUGINS_PLAYERS_AYM_BASE_H_DEFINED
#define CORE_PLUGINS_PLAYERS_AYM_BASE_H_DEFINED

//local includes
#include "aym_chiptune.h"
#include "core/plugins/players/renderer.h"
//library includes
#include <core/module_holder.h>
#include <sound/render_params.h>

namespace Module
{
  namespace AYM
  {
    class Holder : public Module::Holder
    {
    public:
      typedef boost::shared_ptr<const Holder> Ptr;


      virtual Renderer::Ptr CreateRenderer(Parameters::Accessor::Ptr params, Devices::AYM::Device::Ptr chip) const = 0;
      virtual AYM::Chiptune::Ptr GetChiptune() const = 0;
    };

    Holder::Ptr CreateHolder(Chiptune::Ptr chiptune);

    Analyzer::Ptr CreateAnalyzer(Devices::AYM::Device::Ptr device);

    Renderer::Ptr CreateRenderer(Sound::RenderParameters::Ptr params, AYM::DataIterator::Ptr iterator, Devices::AYM::Device::Ptr device);
    Renderer::Ptr CreateRenderer(const Holder& holder, Parameters::Accessor::Ptr params, Sound::Receiver::Ptr target);
  }
}

#endif //CORE_PLUGINS_PLAYERS_AYM_BASE_H_DEFINED
