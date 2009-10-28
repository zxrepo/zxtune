/*
Abstract:
  Error subsystem implementation

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

#include <error.h>

#include <text/tools.h>

// implementation of error's core used to keep data
struct Error::Meta
{
  Meta() : Location(), Code(), Text()
  {
  }
  
  Meta(LocationRef loc, CodeType code, const String& txt)
    : Location(loc), Code(code), Text(txt)
  {
    assert(Code);
  }
  Meta(LocationRef loc, CodeType code)
    : Location(loc), Code(code)
  {
    assert(Code);
  }
  Error::Location Location;
  Error::CodeType Code;
  String Text;
  MetaPtr Suberror;

  // static destructor to release error in allocate place (workaround against multiple runtimes)
  static void Delete(Meta* obj)
  {
    delete obj;
  }
};


Error::Error() : ErrorMeta(new Meta(), Meta::Delete)
{
}

Error::Error(LocationRef loc, CodeType code)
  : ErrorMeta(new Meta(loc, code), Meta::Delete)
{
}

Error::Error(LocationRef loc, CodeType code, const String& txt)
  : ErrorMeta(new Meta(loc, code, txt), Meta::Delete)
{
}

Error& Error::AddSuberror(const Error& e)
{
  //do not add/add to 'success' error
  if (e.GetCode() && GetCode())
  {
    MetaPtr ptr = ErrorMeta;
    while (ptr->Suberror)
    {
      ptr = ptr->Suberror;
    }
    ptr->Suberror = e.ErrorMeta;
  }
  return *this;
}

Error Error::FindSuberror(CodeType code) const
{
  MetaPtr ptr = ErrorMeta;
  while (ptr && ptr->Code != code)
  {
    ptr = ptr->Suberror;
  }
  return ptr ? Error(ptr) : Error();
}

void Error::WalkSuberrors(const boost::function<void(unsigned, LocationRef, CodeType, const String&)>& callback) const
{
  MetaPtr ptr = ErrorMeta;
  for (unsigned level = 0; ptr; ++level, ptr = ptr->Suberror)
  {
    const Meta& Meta(*ptr);
    callback(level, Meta.Location, Meta.Code, Meta.Text);
  }
}

const String& Error::GetText() const
{
  return ErrorMeta->Text;
}

Error::CodeType Error::GetCode() const
{
  return ErrorMeta->Code;
}

Error::operator Error::CodeType () const
{
  return ErrorMeta->Code;
}

bool Error::operator ! () const
{
  return ErrorMeta->Code == 0;
}

String Error::AttributesToString(LocationRef loc, CodeType code, const String& text)
{
  return (Formatter(TEXT_ERROR_DEFAULT_FORMAT) % text % code % LocationToString(loc)).str();
}

String Error::LocationToString(Error::LocationRef loc)
{
#ifdef NDEBUG
  return (Formatter(TEXT_ERROR_LOCATION_FORMAT) % loc).str();
#else
  return (Formatter(TEXT_ERROR_LOCATION_FORMAT_DEBUG) % loc.Tag % loc.File % loc.Line % loc.Function).str();
#endif
}
