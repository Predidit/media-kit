import 'dart:io';
import 'dart:typed_data';

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';
import 'package:media_kit/media_kit.dart';
import 'package:media_kit_video/media_kit_video.dart';

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();
  MediaKit.ensureInitialized();

  for (final hardware in [false, true]) {
    testWidgets(
      'bundled mpv renders and disposes repeatedly (hardware=$hardware)',
      (tester) async {
        final directory = await Directory.systemTemp.createTemp(
          'media-kit-video-',
        );
        final file = File('${directory.path}/frame.bmp');
        // A local 32x32 BGR frame, independent of network/codec test fixtures.
        final data = ByteData(54 + 32 * 32 * 3);
        data.setUint16(0, 0x4d42, Endian.little);
        data.setUint32(2, data.lengthInBytes, Endian.little);
        data.setUint32(10, 54, Endian.little);
        data.setUint32(14, 40, Endian.little);
        data.setInt32(18, 32, Endian.little);
        data.setInt32(22, 32, Endian.little);
        data.setUint16(26, 1, Endian.little);
        data.setUint16(28, 24, Endian.little);
        for (var offset = 54; offset < data.lengthInBytes; offset += 3) {
          data.setUint8(offset, 80);
          data.setUint8(offset + 1, 40);
          data.setUint8(offset + 2, 200);
        }
        await file.writeAsBytes(data.buffer.asUint8List());
        try {
          for (var iteration = 0; iteration < 4; iteration++) {
            final player = Player();
            final controller = VideoController(
              player,
              configuration: VideoControllerConfiguration(
                enableHardwareAcceleration: hardware,
              ),
            );
            try {
              await tester.pumpWidget(
                MaterialApp(
                  home: Video(
                    controller: controller,
                    controls: NoVideoControls,
                  ),
                ),
              );
              await tester.runAsync(() async {
                final loaded = player.stream.width.firstWhere(
                  (width) => width == 32,
                );
                await player.open(Media(file.path));
                await loaded.timeout(const Duration(seconds: 15));
                await controller.waitUntilFirstFrameRendered.timeout(
                  const Duration(seconds: 15),
                );
                expect(controller.id.value, isNotNull);
              });
            } finally {
              await tester.pumpWidget(const SizedBox.shrink());
              await tester.runAsync(
                () => player.dispose().timeout(const Duration(seconds: 15)),
              );
            }
            expect((player.platform! as NativePlayer).ctx.address, 0);
          }
        } finally {
          await directory.delete(recursive: true);
        }
      },
    );
  }
}
