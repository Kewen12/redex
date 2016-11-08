/**
 * Copyright (c) 2016-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

package redex;

import static org.fest.assertions.api.Assertions.*;
import java.lang.reflect.*;

import org.junit.Test;

class Parameterized<T> {}

class ExtendsParameterized extends Parameterized<RenameClassesTest> {}

public class RenameClassesTest {

  @Test
  public void testIAmRenamed() {
    assertThat(this.getClass().getName().startsWith("X.")).isTrue();
  }

  @Test
  public void testSignatureAnnoRewrite() throws Exception {
    ParameterizedType ptype =
        (ParameterizedType)ExtendsParameterized.class.getGenericSuperclass();
    assertThat(ptype.getActualTypeArguments()[0]).isEqualTo(this.getClass());
  }

}
