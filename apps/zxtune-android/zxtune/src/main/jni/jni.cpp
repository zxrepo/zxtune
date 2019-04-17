/**
* 
* @file
*
* @brief JNI entry points
*
* @author vitamin.caig@gmail.com
*
**/

//local includes
#include "module.h"
//platform includes
#include <jni.h>

const jint JNI_VERSION = JNI_VERSION_1_6;

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* /*reserved*/)
{
  JNIEnv* env;
  if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION) != JNI_OK) {
    return JNI_ERR;
  }

  Module::InitJni(env);

  return JNI_VERSION;
}

JNIEXPORT void JNI_OnUnload(JavaVM* vm, void* /*reserved*/)
{
  JNIEnv* env;
  vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION);

  Module::CleanupJni(env);
}
