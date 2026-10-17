import 'dart:io';

import 'package:code_assets/code_assets.dart';
import 'package:hooks/hooks.dart';
import 'package:path/path.dart' as p;

import 'process.dart';

/// Flutter-OH currently reports Linux with an OHOS compiler to hosted hooks.
/// Identify the target SDK instead of assuming that Linux binaries will work.
final class OhosBuildConfig {
  OhosBuildConfig._(this.nativeSdk);

  final String nativeSdk;

  File get sharedCxxRuntime {
    final file = File(
      p.join(
        nativeSdk,
        'llvm',
        'lib',
        'aarch64-linux-ohos',
        'libc++_shared.so',
      ),
    );
    if (!file.existsSync()) {
      throw StateError('OHOS libmpv requires the SDK runtime: ${file.path}');
    }
    return file;
  }

  static OhosBuildConfig? detect({
    required OS targetOS,
    required Architecture architecture,
    Uri? compiler,
    String? sdkPath,
    bool explicitlyOhos = false,
    Map<String, String>? environment,
  }) {
    if (explicitlyOhos && targetOS != OS.linux && targetOS.name != 'ohos') {
      throw UnsupportedError('An explicit OHOS target requires Flutter-OH');
    }
    final compilerSdk = compiler == null
        ? null
        : _findNativeSdk(
            p.dirname(p.dirname(p.dirname(compiler.toFilePath()))),
          );
    if (!explicitlyOhos &&
        targetOS.name != 'ohos' &&
        !(targetOS == OS.linux && compilerSdk != null)) {
      return null;
    }
    if (architecture != Architecture.arm64) {
      throw UnsupportedError('media-kit currently bundles OHOS arm64 only');
    }
    final env = environment ?? Platform.environment;
    for (final candidate in [
      ?sdkPath,
      ?compilerSdk,
      ?env['OHOS_NDK_HOME'],
      ?env['HOS_SDK_HOME'],
    ]) {
      final sdk = _findNativeSdk(candidate);
      if (sdk != null) return OhosBuildConfig._(sdk);
    }
    throw StateError(
      'OpenHarmony native SDK not found. Set OHOS_NDK_HOME or '
      'hooks.user_defines.media_kit.ohos-sdk to the native SDK directory.',
    );
  }

  static String? _findNativeSdk(String root) {
    for (final candidate in [
      root,
      p.join(root, 'native'),
      p.join(root, 'openharmony', 'native'),
      p.join(root, 'default', 'openharmony', 'native'),
    ]) {
      if (File(
        p.join(candidate, 'build', 'cmake', 'ohos.toolchain.cmake'),
      ).existsSync()) {
        return p.normalize(p.absolute(candidate));
      }
    }
    return null;
  }

  /// CBuilder's Linux target supplies a GNU triple; compile this bridge with
  /// the OHOS triple and sysroot while keeping Flutter-OH's asset protocol.
  Future<void> buildBridge(BuildInput input, BuildOutputBuilder output) async {
    final suffix = Platform.isWindows ? '.exe' : '';
    final include = File(
      Platform.resolvedExecutable,
    ).parent.parent.uri.resolve('include/');
    final sources = [
      'src/native_event_loop.c',
      'src/native_library.c',
    ].map((source) => input.packageRoot.resolve(source)).toList();
    final library = input.outputDirectory.resolve('libmedia_kit_event_loop.so');
    await Directory.fromUri(input.outputDirectory).create(recursive: true);
    await runNativeTool(p.join(nativeSdk, 'llvm', 'bin', 'clang$suffix'), [
      '--target=aarch64-linux-ohos',
      '--sysroot=${p.join(nativeSdk, 'sysroot')}',
      '-shared',
      '-fPIC',
      '-O2',
      '-Wl,-z,defs',
      '-Wl,-soname,libmedia_kit_event_loop.so',
      '-I${include.toFilePath()}',
      ...sources.map((source) => source.toFilePath()),
      '-o',
      library.toFilePath(),
      '-ldl',
    ]);
    output.assets.code.add(
      CodeAsset(
        package: input.packageName,
        name: 'src/player/native/core/native_event_loop_bindings.dart',
        linkMode: DynamicLoadingBundled(),
        file: library,
      ),
    );
    output.dependencies.addAll([
      ...sources,
      include.resolve('dart_native_api.h'),
      include.resolve('dart_api.h'),
      File(p.join(nativeSdk, 'oh-uni-package.json')).uri,
    ]);
  }
}
