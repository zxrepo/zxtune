/**
*
* @file      io/api.h
* @brief     End-client API declaration
* @version   $Id$
* @author    (C) Vitamin/CAIG/2001
*
**/

#pragma once
#ifndef IO_API_H_DEFINED
#define IO_API_H_DEFINED

//library includes
#include <binary/container.h>
#include <io/identifier.h>

//forward declarations
class Error;
namespace Parameters
{
  class Accessor;
}

namespace Log
{
  class ProgressCallback;
}

namespace IO
{
  //! @brief Resolve uri to identifier object
  //! @param uri Full data identifier
  //! @throw Error if failed to resolve
  Identifier::Ptr ResolveUri(const String& uri);

  //! @brief Performs opening specified uri
  //! @param path External data identifier
  //! @param params %Parameters accessor to setup providers' work
  //! @param cb Callback for long-time controllable operations
  //! @throw Error if failed to open
  Binary::Container::Ptr OpenData(const String& path, const Parameters::Accessor& params, Log::ProgressCallback& cb);
}

#endif //IO_API_H_DEFINED
