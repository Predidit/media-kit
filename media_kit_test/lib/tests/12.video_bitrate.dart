import 'package:flutter/material.dart';
import 'package:media_kit/media_kit.dart';
import 'package:media_kit_video/media_kit_video.dart';

import '../common/globals.dart';
import '../common/sources/sources.dart';
import '../common/widgets.dart';

class VideoBitrateScreen extends StatefulWidget {
  const VideoBitrateScreen({super.key});

  @override
  State<VideoBitrateScreen> createState() => _VideoBitrateScreenState();
}

class _VideoBitrateScreenState extends State<VideoBitrateScreen> {
  late final Player player = Player();
  late final VideoController controller = VideoController(
    player,
    configuration: configuration.value,
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

  List<Widget> get items => [
        _BitrateTile(
          title: 'Audio bitrate',
          initialData: player.state.audioBitrate,
          stream: player.stream.audioBitrate,
        ),
        _BitrateTile(
          title: 'Video bitrate',
          initialData: player.state.videoBitrate,
          stream: player.stream.videoBitrate,
        ),
        const Divider(height: 1.0),
        for (int i = 0; i < sources.length; i++)
          ListTile(
            title: Text(
              'Video $i',
              style: const TextStyle(
                fontSize: 14.0,
              ),
              maxLines: 1,
              overflow: TextOverflow.ellipsis,
            ),
            onTap: () {
              player.open(Media(sources[i]));
            },
          ),
      ];

  @override
  Widget build(BuildContext context) {
    final horizontal =
        MediaQuery.of(context).size.width > MediaQuery.of(context).size.height;
    return Scaffold(
      appBar: AppBar(
        title: const Text('package:media_kit'),
      ),
      floatingActionButton: Row(
        mainAxisSize: MainAxisSize.min,
        mainAxisAlignment: MainAxisAlignment.end,
        crossAxisAlignment: CrossAxisAlignment.center,
        children: [
          FloatingActionButton(
            heroTag: 'file',
            tooltip: 'Open [File]',
            onPressed: () => showFilePicker(context, player),
            child: const Icon(Icons.file_open),
          ),
          const SizedBox(width: 16.0),
          FloatingActionButton(
            heroTag: 'uri',
            tooltip: 'Open [Uri]',
            onPressed: () => showURIPicker(context, player),
            child: const Icon(Icons.link),
          ),
        ],
      ),
      body: SizedBox.expand(
        child: horizontal
            ? Row(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  Expanded(
                    flex: 3,
                    child: Container(
                      alignment: Alignment.center,
                      child: Column(
                        mainAxisSize: MainAxisSize.min,
                        children: [
                          Expanded(
                            child: Card(
                              clipBehavior: Clip.antiAlias,
                              margin: const EdgeInsets.all(32.0),
                              child: Video(
                                controller: controller,
                              ),
                            ),
                          ),
                          const SizedBox(height: 32.0),
                        ],
                      ),
                    ),
                  ),
                  const VerticalDivider(width: 1.0, thickness: 1.0),
                  Expanded(
                    flex: 1,
                    child: ListView(
                      children: items,
                    ),
                  ),
                ],
              )
            : ListView(
                children: [
                  Video(
                    controller: controller,
                    width: MediaQuery.of(context).size.width,
                    height: MediaQuery.of(context).size.width * 9.0 / 16.0,
                  ),
                  ...items,
                ],
              ),
      ),
    );
  }
}

class _BitrateTile extends StatelessWidget {
  final String title;
  final double? initialData;
  final Stream<double?> stream;

  const _BitrateTile({
    required this.title,
    required this.initialData,
    required this.stream,
  });

  @override
  Widget build(BuildContext context) {
    return StreamBuilder<double?>(
      initialData: initialData,
      stream: stream,
      builder: (context, snapshot) {
        return ListTile(
          title: Text(
            title,
            style: const TextStyle(fontSize: 14.0),
          ),
          subtitle: Text(
            _formatBitrate(snapshot.data),
            style: const TextStyle(fontSize: 20.0),
          ),
        );
      },
    );
  }

  String _formatBitrate(double? value) {
    if (value == null || !value.isFinite || value <= 0.0) {
      return 'N/A';
    }
    if (value >= 1000000.0) {
      return '${(value / 1000000.0).toStringAsFixed(2)} Mbps';
    }
    if (value >= 1000.0) {
      return '${(value / 1000.0).toStringAsFixed(0)} Kbps';
    }
    return '${value.toStringAsFixed(0)} bps';
  }
}
