/// This file is a part of media_kit (https://github.com/media-kit/media-kit).
///
/// Copyright © 2021 & onwards, Hitesh Kumar Saini <saini123hitesh@gmail.com>.
/// All rights reserved.
/// Use of this source code is governed by MIT license that can be found in the LICENSE file.

import 'dart:ffi';
import 'dart:io';

import 'package:test/test.dart';

import 'package:media_kit/ffi/ffi.dart';
import 'package:media_kit/src/player/native/core/native_library.dart';

void main() {
  test(
    'native-library-ensure-initialized',
    () {
      expect(
        NativeLibrary.ensureInitialized,
        returnsNormally,
      );
    },
  );
  test(
    'native-library-path',
    () {
      expect(
        () {
          final library = NativeLibrary.path;
          print(library);
        },
        returnsNormally,
      );
    },
  );

  test('explicit libmpv path restores the C numeric locale on Linux', () {
    NativeLibrary.ensureInitialized();
    final path = NativeLibrary.path;
    final setLocale = DynamicLibrary.process()
        .lookupFunction<
          Pointer<Utf8> Function(Int32, Pointer<Utf8>),
          Pointer<Utf8> Function(int, Pointer<Utf8>)
        >('setlocale');
    const lcNumeric = 1; // LC_NUMERIC on Linux.
    final previous = setLocale(
      lcNumeric,
      nullptr,
    ).toDartString().toNativeUtf8();
    final utf8 = 'C.UTF-8'.toNativeUtf8();
    try {
      if (setLocale(lcNumeric, utf8) == nullptr) {
        markTestSkipped('C.UTF-8 locale unavailable');
        return;
      }
      NativeLibrary.ensureInitialized(libmpv: path);
      expect(setLocale(lcNumeric, nullptr).toDartString(), 'C');
    } finally {
      setLocale(lcNumeric, previous);
      malloc.free(previous);
      malloc.free(utf8);
    }
  }, skip: !Platform.isLinux);
}
