@TestOn('vm')
library;

import 'dart:io';

import 'package:code_assets/code_assets.dart';
import 'package:path/path.dart' as p;
import 'package:test/test.dart';

import '../../hook/src/ohos.dart';

void main() {
  late Directory root;
  late Directory sdk;

  setUp(() async {
    root = await Directory.systemTemp.createTemp('media-kit-ohos-');
    sdk = Directory(p.join(root.path, 'default', 'openharmony', 'native'));
    final toolchain = File(
      p.join(sdk.path, 'build/cmake/ohos.toolchain.cmake'),
    );
    await toolchain.parent.create(recursive: true);
    await toolchain.writeAsString('');
  });
  tearDown(() => root.delete(recursive: true));

  test('Flutter-OH Linux compatibility input selects the OHOS SDK', () {
    final config = OhosBuildConfig.detect(
      targetOS: OS.linux,
      architecture: Architecture.arm64,
      compiler: sdk.uri.resolve('llvm/bin/clang'),
      environment: {},
    );
    expect(config?.nativeSdk, sdk.path);
  });

  test('an OHOS environment does not turn a normal Linux build into OHOS', () {
    expect(
      OhosBuildConfig.detect(
        targetOS: OS.linux,
        architecture: Architecture.arm64,
        compiler: Uri.file('/usr/bin/clang'),
        environment: {'HOS_SDK_HOME': root.path},
      ),
      isNull,
    );
  });

  test('explicit OHOS target accepts the command-line SDK layout', () {
    expect(
      OhosBuildConfig.detect(
        targetOS: OS.linux,
        architecture: Architecture.arm64,
        explicitlyOhos: true,
        environment: {'HOS_SDK_HOME': root.path},
      )?.nativeSdk,
      sdk.path,
    );
  });

  test('a future OHOS target name is recognized without the Linux shim', () {
    expect(
      OhosBuildConfig.detect(
        targetOS: OS.fromString('ohos'),
        architecture: Architecture.arm64,
        sdkPath: sdk.path,
        environment: {},
      )?.nativeSdk,
      sdk.path,
    );
  });

  test('explicit OHOS fails on other host targets and missing SDKs', () {
    expect(
      () => OhosBuildConfig.detect(
        targetOS: OS.macOS,
        architecture: Architecture.arm64,
        explicitlyOhos: true,
        sdkPath: sdk.path,
      ),
      throwsUnsupportedError,
    );
    expect(
      () => OhosBuildConfig.detect(
        targetOS: OS.linux,
        architecture: Architecture.arm64,
        explicitlyOhos: true,
        environment: {},
      ),
      throwsStateError,
    );
  });

  test('OHOS architectures without a bundled mpv fail explicitly', () {
    expect(
      () => OhosBuildConfig.detect(
        targetOS: OS.linux,
        architecture: Architecture.x64,
        compiler: sdk.uri.resolve('llvm/bin/clang'),
        environment: {},
      ),
      throwsUnsupportedError,
    );
  });
}
