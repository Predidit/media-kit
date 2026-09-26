// Copyright (c) 2026 Predidit. MIT license.

// ignore_for_file: non_constant_identifier_names

import 'dart:ffi';

import 'package:media_kit/ffi/ffi.dart';
import 'package:media_kit/generated/libmpv/bindings.dart';
import 'package:media_kit/src/player/native/core/native_property.dart';
import 'package:test/test.dart';

final class _PropertyMPV implements MPV {
  int status = 0;
  int nodeFormat = mpv_format.MPV_FORMAT_NONE;
  int integer = 42;
  String? string;
  String? lastProperty;
  int nodeFrees = 0;
  int stringFrees = 0;

  @override
  int mpv_get_property(
    Pointer<mpv_handle> handle,
    Pointer<Int8> name,
    int format,
    Pointer<Void> data,
  ) {
    lastProperty = name.cast<Utf8>().toDartString();
    // libmpv leaves the output untouched on failure.
    if (status < 0) return status;
    if (format == mpv_format.MPV_FORMAT_NODE) {
      data.cast<mpv_node>().ref.format = nodeFormat;
    } else if (format == mpv_format.MPV_FORMAT_INT64) {
      data.cast<Int64>().value = integer;
    } else {
      fail('Unexpected property format: $format');
    }
    return status;
  }

  @override
  void mpv_free_node_contents(Pointer<mpv_node> node) {
    nodeFrees++;
  }

  @override
  Pointer<Int8> mpv_get_property_string(
    Pointer<mpv_handle> handle,
    Pointer<Int8> name,
  ) {
    lastProperty = name.cast<Utf8>().toDartString();
    return string?.toNativeUtf8().cast() ?? nullptr;
  }

  @override
  void mpv_free(Pointer<Void> value) {
    stringFrees++;
    calloc.free(value);
  }

  @override
  dynamic noSuchMethod(Invocation invocation) => super.noSuchMethod(invocation);
}

void main() {
  test('failed node queries neither inspect nor free the output', () {
    final mpv = _PropertyMPV();
    for (final error in [-3, -8, -9, -10, -11]) {
      mpv.status = error;
      expect(
        readNativeNode<Object>(mpv, nullptr, 'decoder-list', (_) {
          fail('A failed query must not reach the node parser');
        }),
        isNull,
      );
    }
    expect(mpv.lastProperty, 'decoder-list');
    expect(mpv.nodeFrees, 0);
  });

  test('successful nodes are released for every format and parser errors', () {
    final mpv = _PropertyMPV()..status = 1;
    for (final format in [
      mpv_format.MPV_FORMAT_NONE,
      mpv_format.MPV_FORMAT_NODE_MAP,
      mpv_format.MPV_FORMAT_NODE_ARRAY,
      mpv_format.MPV_FORMAT_STRING,
    ]) {
      mpv.nodeFormat = format;
      expect(
        readNativeNode(mpv, nullptr, 'params', (node) => node.format),
        format,
      );
    }
    expect(mpv.nodeFrees, 4);
    expect(
      () => readNativeNode<Object>(mpv, nullptr, 'params', (_) {
        throw StateError('copy failed');
      }),
      throwsStateError,
    );
    expect(mpv.nodeFrees, 5);
  });

  test(
    'failed scalar queries cannot turn zeroed output into a valid value',
    () {
      final mpv = _PropertyMPV()..status = -10;
      expect(readNativeInt64(mpv, nullptr, 'playlist-pos'), isNull);
      mpv.status = 0;
      expect(readNativeInt64(mpv, nullptr, 'playlist-pos'), 42);
      expect(mpv.lastProperty, 'playlist-pos');
    },
  );

  test('string queries handle absence and return an owned Dart copy', () {
    final mpv = _PropertyMPV();
    expect(readNativeString(mpv, nullptr, 'path'), isNull);
    expect(mpv.stringFrees, 0);
    mpv.string = '音频路径';
    final value = readNativeString(mpv, nullptr, 'path');
    expect(mpv.lastProperty, 'path');
    expect(mpv.stringFrees, 1);
    expect(value, '音频路径');
  });

  test(
    'parameter maps reject absent and non-map nodes before reading the union',
    () {
      expect(NativeNodeMap(nullptr).get<int>('dw'), isNull);
      using((arena) {
        final node = arena<mpv_node>();
        node.ref.u.int64 =
            1; // Not a valid list pointer; must never be followed.
        for (final format in [
          mpv_format.MPV_FORMAT_NONE,
          mpv_format.MPV_FORMAT_INT64,
          mpv_format.MPV_FORMAT_STRING,
          mpv_format.MPV_FORMAT_NODE_ARRAY,
        ]) {
          node.ref.format = format;
          expect(NativeNodeMap(node).get<int>('samplerate'), isNull);
        }
        node.ref.format = mpv_format.MPV_FORMAT_NODE_MAP;
        node.ref.u.list = nullptr;
        expect(NativeNodeMap(node).get<int>('dw'), isNull);
        node.ref.u.list = arena<mpv_node_list>();
        expect(NativeNodeMap(node).get<int>('dw'), isNull);
      });
    },
  );

  test(
    'audio and video fields respect tags and survive native memory release',
    () {
      final copy = using((arena) {
        final node = arena<mpv_node>();
        node.ref.format = mpv_format.MPV_FORMAT_NODE_MAP;
        final list = node.ref.u.list = arena<mpv_node_list>();
        const keys = ['format', 'samplerate', 'aspect', 'dw', 'ignored'];
        list.ref.num = keys.length;
        list.ref.keys = arena<Pointer<Int8>>(keys.length);
        list.ref.values = arena<mpv_node>(keys.length);
        for (var i = 0; i < keys.length; ++i) {
          list.ref.keys[i] = keys[i].toNativeUtf8(allocator: arena).cast();
        }
        final values = list.ref.values;
        values[0].format = mpv_format.MPV_FORMAT_STRING;
        values[0].u.string = 'floatp'.toNativeUtf8(allocator: arena).cast();
        values[1].format = mpv_format.MPV_FORMAT_INT64;
        values[1].u.int64 = 48000;
        values[2].format = mpv_format.MPV_FORMAT_DOUBLE;
        values[2].u.double_ = 16 / 9;
        values[3].format = mpv_format.MPV_FORMAT_STRING;
        values[3].u.string = '1920'.toNativeUtf8(allocator: arena).cast();
        values[4].format = mpv_format.MPV_FORMAT_NONE;
        values[4].u.int64 = 1;
        return NativeNodeMap(node);
      });
      expect(copy.get<String>('format'), 'floatp');
      expect(copy.get<int>('samplerate'), 48000);
      expect(copy.get<String>('samplerate'), isNull);
      expect(copy.get<double>('aspect'), 16 / 9);
      expect(copy.get<int>('dw'), isNull);
      expect(copy.get<Object>('ignored'), isNull);
      expect(copy.get<Object>('missing'), isNull);
    },
  );
}
