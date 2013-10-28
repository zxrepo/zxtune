/*
Abstract:
  Module container and related interface

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

#pragma once
#ifndef CORE_MODULE_OPEN_H_DEFINED
#define CORE_MODULE_OPEN_H_DEFINED

//library includes
#include <core/data_location.h>
#include <core/module_holder.h>

namespace Module
{
  //! @param location Source data location
  //! @throw Error if no object detected
  Holder::Ptr Open(ZXTune::DataLocation::Ptr location);

  Holder::Ptr Open(const Binary::Container& data);
}

#endif //CORE_MODULE_OPEN_H_DEFINED
