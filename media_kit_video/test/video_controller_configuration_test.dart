import 'package:flutter_test/flutter_test.dart';
import 'package:media_kit_video/src/video_controller/platform_video_controller.dart';

void main() {
  test('Windows D3D11 adapter defaults to the existing Auto path', () {
    const configuration = VideoControllerConfiguration();

    expect(configuration.windowsD3D11AdapterId, isNull);
  });

  test('Windows D3D11 adapter ID is preserved by copyWith', () {
    const configuration = VideoControllerConfiguration(
      windowsD3D11AdapterId: 'adapter-id',
    );

    expect(
      configuration.copyWith(width: 640).windowsD3D11AdapterId,
      'adapter-id',
    );
  });
}
