/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "DexMethodHandle.h"
#include "DexAnnotation.h"
#include "DexIdx.h"

#include <sstream>

DexMethodHandle::DexMethodHandle(MethodHandleType type,
                                 DexMethodRef* methodref) {
  always_assert_log(isInvokeType(type),
                    "MethodHandleType %d invalid to use with methodref");
  m_type = type;
  m_methodref = methodref;
}

DexMethodHandle::DexMethodHandle(MethodHandleType type, DexFieldRef* fieldref) {
  always_assert_log(!isInvokeType(type),
                    "MethodHandleType %d invalid to use with fieldref");
  m_type = type;
  m_fieldref = fieldref;
}

DexMethodRef* DexMethodHandle::methodref() const {
  always_assert_log(isInvokeType(m_type),
                    "MethodHandleType %d invalid to use with methodref");
  return m_methodref;
}

DexFieldRef* DexMethodHandle::fieldref() const {
  always_assert_log(!isInvokeType(m_type),
                    "MethodHandleType %d invalid to use with fieldref");
  return m_fieldref;
}

bool DexMethodHandle::isInvokeType(MethodHandleType type) {
  switch (type) {
  case METHOD_HANDLE_TYPE_INVOKE_STATIC:
  case METHOD_HANDLE_TYPE_INVOKE_INSTANCE:
  case METHOD_HANDLE_TYPE_INVOKE_CONSTRUCTOR:
  case METHOD_HANDLE_TYPE_INVOKE_DIRECT:
  case METHOD_HANDLE_TYPE_INVOKE_INTERFACE:
    return true;
  default:
    return false;
  }
}
