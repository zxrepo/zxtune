/*
Abstract:
  File provider functions

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

#pragma once
#ifndef IO_FILE_PROVIDER_H_DEFINED
#define IO_FILE_PROVIDER_H_DEFINED

//library includes
#include <binary/data.h>
#include <binary/output_stream.h>

namespace Parameters
{
  class Accessor;
}

namespace IO
{
  Binary::Data::Ptr OpenLocalFile(const String& path, std::size_t mmapThreshold);

  enum OverwriteMode
  {
    STOP_IF_EXISTS,
    OVERWRITE_EXISTING,
    RENAME_NEW
  };

  class FileCreatingParameters
  {
  public:
    virtual ~FileCreatingParameters() {}

    virtual OverwriteMode Overwrite() const = 0;
    virtual bool CreateDirectories() const = 0;
    virtual bool SanitizeNames() const = 0;
  };

  Binary::SeekableOutputStream::Ptr CreateLocalFile(const String& path, const FileCreatingParameters& params);
}

#endif //IO_FILE_PROVIDER_H_DEFINED