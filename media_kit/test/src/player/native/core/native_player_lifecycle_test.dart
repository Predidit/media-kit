import 'dart:io';
import 'dart:typed_data';

import 'package:image/image.dart' as image;
import 'package:media_kit/media_kit.dart';
import 'package:test/test.dart';

void main() {
  late Directory directory;
  late String source;
  late String picture;
  setUpAll(() async {
    MediaKit.ensureInitialized();
    NativePlayer.test = true;
    directory = await Directory.systemTemp.createTemp('media_kit_lifecycle_');
    source = '${directory.path}/silence.wav';
    // 200 ms PCM, generated locally: no network or external test media.
    final data = ByteData(44 + 19200);
    void text(int offset, String value) {
      for (var i = 0; i < value.length; i++) {
        data.setUint8(offset + i, value.codeUnitAt(i));
      }
    }

    text(0, 'RIFF');
    data.setUint32(4, data.lengthInBytes - 8, Endian.little);
    text(8, 'WAVEfmt ');
    data.setUint32(16, 16, Endian.little);
    data.setUint16(20, 1, Endian.little);
    data.setUint16(22, 1, Endian.little);
    data.setUint32(24, 48000, Endian.little);
    data.setUint32(28, 96000, Endian.little);
    data.setUint16(32, 2, Endian.little);
    data.setUint16(34, 16, Endian.little);
    text(36, 'data');
    data.setUint32(40, 19200, Endian.little);
    await File(source).writeAsBytes(data.buffer.asUint8List());
    picture = '${directory.path}/frame.bmp';
    final frame = image.Image(width: 32, height: 32);
    image.fill(frame, color: image.ColorRgb8(200, 40, 80));
    await File(picture).writeAsBytes(image.encodeBmp(frame));
  });
  tearDownAll(() => directory.delete(recursive: true));

  for (final asynchronous in [true, false]) {
    test('playback and repeated disposal (async=$asynchronous)', () async {
      for (var i = 0; i < 3; i++) {
        final player = Player(
          configuration: PlayerConfiguration(async: asynchronous),
        );
        final native = player.platform! as NativePlayer;
        final completed = player.stream.completed.firstWhere((value) => value);
        await player.open(Media(source));
        await completed.timeout(const Duration(seconds: 10));
        await player.stop();
        final first = native.dispose();
        expect(identical(first, native.dispose()), isTrue);
        await first.timeout(const Duration(seconds: 10));
        expect(native.ctx.address, 0);
        await expectLater(native.command(['stop']), throwsA(anything));
      }
    });
  }

  test('dispose while initialization is pending', () async {
    final player = Player();
    await player.dispose().timeout(const Duration(seconds: 10));
    expect((player.platform! as NativePlayer).ctx.address, 0);
  });

  test('screenshots own their pixels across player destruction', () async {
    final player = Player();
    addTearDown(player.dispose);
    final native = player.platform! as NativePlayer;
    final loaded = player.stream.width.firstWhere((width) => width == 32);
    await player.open(Media(picture));
    await loaded.timeout(const Duration(seconds: 10));
    await player.pause();
    final pixels = await player.screenshot(format: null);
    expect(pixels, isNotNull);
    final expected = List<int>.of(pixels!);
    expect(await player.safeScreenshot(format: null), orderedEquals(expected));
    final jpeg = await player.screenshot();
    expect(image.decodeJpg(jpeg!)!.width, 32);
    // Encoding continues in another isolate after the mpv frame is released.
    final pngFuture = native.screenshot(
      format: 'image/png',
      synchronized: false,
    );
    await native.dispose();
    final png = await pngFuture;
    expect(image.decodePng(png!)!.width, 32);
    expect(pixels, orderedEquals(expected));
  });
}
