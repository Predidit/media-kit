import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:image/image.dart' as image;
import 'package:integration_test/integration_test.dart';
import 'package:media_kit/media_kit.dart';
import 'package:media_kit_video/media_kit_video.dart';

void main() {
  // Native textures and VideoController initialization request frames between
  // explicit pumps. Keep those frames flowing throughout the integration test.
  IntegrationTestWidgetsFlutterBinding.ensureInitialized().framePolicy =
      LiveTestWidgetsFlutterBindingFramePolicy.fullyLive;
  MediaKit.ensureInitialized();

  for (final hardware in [false, true]) {
    testWidgets(
      'bundled mpv renders and disposes repeatedly (hardware=$hardware)',
      (tester) async {
        final directory = await Directory.systemTemp.createTemp(
          'media-kit-video-',
        );
        final file = File('${directory.path}/frame.png');
        // Generate a local frame in a format supported by every mpv bundle.
        final frame = image.Image(width: 32, height: 32);
        image.fill(frame, color: image.ColorRgb8(200, 40, 80));
        await file.writeAsBytes(image.encodePng(frame));
        try {
          for (var iteration = 0; iteration < 10; iteration++) {
            final player = Player(configuration: const PlayerConfiguration(logLevel: MPVLogLevel.debug));
            final logSubscription = player.stream.log.listen((log) => print('MPV: $log'));
            final controller = VideoController(
              player,
              configuration: VideoControllerConfiguration(
                enableHardwareAcceleration: hardware,
              ),
            );
            try {
              await tester
                  .pumpWidget(
                    MaterialApp(
                      home: Video(
                        controller: controller,
                        controls: NoVideoControls,
                      ),
                    ),
                  )
                  .timeout(const Duration(seconds: 15));
              await tester.runAsync(() async {
                final loaded = player.stream.width.firstWhere(
                  (width) => width == 32,
                );
                await player
                    .open(Media(file.path))
                    .timeout(const Duration(seconds: 15));
                await loaded.timeout(const Duration(seconds: 15));
                await controller.waitUntilFirstFrameRendered.timeout(
                  const Duration(seconds: 15),
                );
                expect(controller.id.value, isNotNull);
              });
            } finally {
              await tester
                  .pumpWidget(const SizedBox.shrink())
                  .timeout(const Duration(seconds: 15));
              await tester.runAsync(
                () => player.dispose().timeout(const Duration(seconds: 15)),
              );
            }
            await logSubscription.cancel();
            expect((player.platform! as NativePlayer).ctx.address, 0);
          }
        } finally {
          await directory.delete(recursive: true);
        }
      },
      timeout: const Timeout(Duration(minutes: 2)),
    );
  }
}
