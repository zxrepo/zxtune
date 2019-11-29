/**
* 
* @file
*
* @brief  Zdata containers support
*
* @author vitamin.caig@gmail.com
*
**/

#pragma once

//library includes
#include <binary/view.h>
#include <core/data_location.h>

namespace ZXTune
{
  DataLocation::Ptr BuildZdataContainer(Binary::View content);
}
