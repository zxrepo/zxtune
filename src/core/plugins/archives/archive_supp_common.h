/*
Abstract:
  Common archive plugins support

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

#pragma once
#ifndef __CORE_PLUGINS_ARCHIVE_SUPP_COMMON_H_DEFINED__
#define __CORE_PLUGINS_ARCHIVE_SUPP_COMMON_H_DEFINED__

//local includes
#include "core/plugins/plugins_types.h"
//library includes
#include <formats/packed.h>

namespace ZXTune
{
  ArchivePlugin::Ptr CreateArchivePlugin(const String& id, uint_t caps, Formats::Packed::Decoder::Ptr decoder);
}

#endif //__CORE_PLUGINS_ARCHIVE_SUPP_COMMON_H_DEFINED__
