import 'dart:async';
import 'dart:ffi';

import 'package:media_kit/ffi/ffi.dart';
import 'package:media_kit/generated/libmpv/bindings.dart';
import 'package:media_kit/src/player/native/core/native_event_loop.dart';
import 'package:media_kit/src/player/native/core/native_library.dart';
import 'package:media_kit/src/player/native/core/native_property.dart';
import 'package:test/test.dart';

void main() {
  late DynamicLibrary library;
  setUpAll(() {
    NativeLibrary.ensureInitialized();
    library = DynamicLibrary.open(NativeLibrary.path);
  });

  test('observes native events and awaits actual destruction', () async {
    final observed = Completer<bool>();
    final loop = await NativeEventLoop.create(library, (event) async {
      if (event.ref.event_id == mpv_event_id.MPV_EVENT_PROPERTY_CHANGE) {
        final property = event.ref.data.cast<mpv_event_property>().ref;
        if (property.name.cast<Utf8>().toDartString() == 'pause' &&
            !observed.isCompleted) {
          observed.complete(property.data.cast<Int32>().value != 0);
        }
      }
    });
    addTearDown(loop.dispose);
    final name = 'pause'.toNativeUtf8();
    try {
      expect(
        MPV(library).mpv_observe_property(
          loop.handle,
          1,
          name.cast(),
          mpv_format.MPV_FORMAT_FLAG,
        ),
        0,
      );
      await observed.future.timeout(const Duration(seconds: 10));
    } finally {
      calloc.free(name);
    }
    final first = loop.dispose();
    expect(identical(first, loop.dispose()), isTrue);
    await first;
  });

  test('repeated create/dispose keeps instances independent', () async {
    for (var i = 0; i < 30; i++) {
      final first = await NativeEventLoop.create(library, (_) async {});
      final second = await NativeEventLoop.create(library, (_) async {});
      await Future.wait([first.dispose(), second.dispose()]);
    }
  });

  test(
    'unavailable audio, subtitle and timing properties have no event payload',
    () async {
      final expected = {
        'audio-params': mpv_format.MPV_FORMAT_NODE,
        'video-params': mpv_format.MPV_FORMAT_NODE,
        'sub-text': mpv_format.MPV_FORMAT_NODE,
        'time-pos': mpv_format.MPV_FORMAT_DOUBLE,
        'duration': mpv_format.MPV_FORMAT_DOUBLE,
        'audio-bitrate': mpv_format.MPV_FORMAT_DOUBLE,
      };
      final remaining = expected.keys.toSet();
      final received = Completer<void>();
      final loop = await NativeEventLoop.create(library, (event) async {
        if (event.ref.event_id != mpv_event_id.MPV_EVENT_PROPERTY_CHANGE) {
          return;
        }
        final property = event.ref.data.cast<mpv_event_property>().ref;
        final name = property.name.cast<Utf8>().toDartString();
        if (remaining.remove(name)) {
          expect(property.format, mpv_format.MPV_FORMAT_NONE, reason: name);
          expect(property.data, nullptr, reason: name);
          if (remaining.isEmpty) received.complete();
        }
      }, options: {'config': 'no', 'vo': 'null', 'ao': 'null'});
      addTearDown(loop.dispose);
      final mpv = MPV(library);
      expect(readNativeString(mpv, loop.handle, 'path'), isNull);
      expect(readNativeInt64(mpv, loop.handle, 'time-pos'), isNull);
      expect(
        readNativeNode<Object>(mpv, loop.handle, 'audio-params', (_) {
          fail('Unavailable audio parameters must not be parsed');
        }),
        isNull,
      );
      for (final entry in expected.entries) {
        final name = entry.key.toNativeUtf8();
        try {
          expect(
            mpv.mpv_observe_property(loop.handle, 1, name.cast(), entry.value),
            0,
          );
        } finally {
          calloc.free(name);
        }
      }
      await received.future.timeout(const Duration(seconds: 10));
    },
  );
}
