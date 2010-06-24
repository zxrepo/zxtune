/*
Abstract:
  Implicit plugins list

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

#ifndef __CORE_PLUGINS_IMPLICITS_LIST_H_DEFINED__
#define __CORE_PLUGINS_IMPLICITS_LIST_H_DEFINED__

namespace ZXTune
{
  //forward declaration
  class PluginsEnumerator;
  
  void RegisterHobetaConvertor(PluginsEnumerator& enumerator);
  void RegisterHrust1xConvertor(PluginsEnumerator& enumerator);
  void RegisterHrust2xConvertor(PluginsEnumerator& enumerator);
  void RegisterFDIConvertor(PluginsEnumerator& enumerator);
  void RegisterDSQConvertor(PluginsEnumerator& enumerator);

  void RegisterImplicitPlugins(PluginsEnumerator& enumerator)
  {
    RegisterHobetaConvertor(enumerator);
    RegisterHrust1xConvertor(enumerator);
    RegisterHrust2xConvertor(enumerator);
    RegisterFDIConvertor(enumerator);
    RegisterDSQConvertor(enumerator);
  }
}

#endif //__CORE_PLUGINS_IMPLICITS_LIST_H_DEFINED__
