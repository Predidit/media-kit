import 'package:flutter/material.dart';
import 'package:media_kit/media_kit.dart';
import 'package:media_kit_video/media_kit_video.dart';

import '../common/globals.dart';
import '../common/sources/sources.dart';

class AndroidSurfaceTextureScreen extends StatefulWidget {
  const AndroidSurfaceTextureScreen({super.key});

  @override
  State<AndroidSurfaceTextureScreen> createState() =>
      _AndroidSurfaceTextureScreenState();
}

class _AndroidSurfaceTextureScreenState
    extends State<AndroidSurfaceTextureScreen> {
  late final Player player = Player();
  late final VideoController controller = VideoController(
    player,
    configuration: configuration.value.copyWith(
      enableAndroidSurfaceProducer: false,
    ),
  );

  @override
  void initState() {
    super.initState();
    player.open(Media(sources[0]));
    player.stream.error.listen((error) => debugPrint(error));
  }

  @override
  void dispose() {
    player.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final horizontal =
        MediaQuery.of(context).size.width > MediaQuery.of(context).size.height;
    return Scaffold(
      appBar: AppBar(
        title: const Text('Android SurfaceTexture'),
      ),
      body: SizedBox.expand(
        child: horizontal
            ? Center(
                child: AspectRatio(
                  aspectRatio: 16.0 / 9.0,
                  child: Card(
                    clipBehavior: Clip.antiAlias,
                    margin: const EdgeInsets.all(32.0),
                    child: Video(
                      controller: controller,
                    ),
                  ),
                ),
              )
            : ListView(
                children: [
                  Video(
                    controller: controller,
                    width: MediaQuery.of(context).size.width,
                    height: MediaQuery.of(context).size.width * 9.0 / 16.0,
                  ),
                  const ListTile(
                    title: Text(
                      'Single video using Android SurfaceTexture',
                      style: TextStyle(fontSize: 14.0),
                    ),
                    subtitle: Text(
                      'enableAndroidSurfaceProducer: false',
                    ),
                  ),
                ],
              ),
      ),
    );
  }
}
