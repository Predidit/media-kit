// swift-tools-version: 5.9
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let libmpvTargets = [
    "Ass",
    "Avcodec",
    "Avfilter",
    "Avformat",
    "Avutil",
    "Dav1d",
    "Freetype",
    "Fribidi",
    "Harfbuzz",
    "Mbedcrypto",
    "Mbedtls",
    "Mbedx509",
    "Mpv",
    "Png16",
    "Swresample",
    "Swscale",
    "Uchardet",
    "Xml2"
]

let libmpvArtifactBase = "https://github.com/media-kit/libmpv-darwin-build/releases/download/v0.7.0/libmpv-xcframeworks_v0.7.0_macos-universal-video-default"
let libmpvChecksums = [
    "Ass": "fd5e0767d9c5646e4be90555f716baf129f4572d5ca63cd7e7402629a5dfd4df",
    "Avcodec": "5c7556bcae6c6236e8b2ce13b5cea57efe82117b5254d5d718b6e2e1215ebfce",
    "Avfilter": "1a601279068c88268d6402cc1438cc77db96015fe4580550a495303dd523dde5",
    "Avformat": "ebcf285d7a991c20bdb79f74e99ab8a43177387c1a408b063b1e34f706586f7d",
    "Avutil": "ca43c5ef524c62fec0dba34326b4c4e41f6ecd6dd0a4560ace34e5f05840f3aa",
    "Dav1d": "7a7d9d8683f54ac0a60d58cfa6a8e3d8111892ae76e2837c5f1a86327688e09e",
    "Freetype": "babdeee15b17fd79ecd11686421a8c7307e6bf54ede4c0780523449c74c9a514",
    "Fribidi": "1c71675f70baa0e55e63736b9ba3bc1de26fbe206f6a07dd1ed4ac87d2379225",
    "Harfbuzz": "e1676afce4985bda71d79991c4ac422faacf601274e1a2d569bb323bd7eeb419",
    "Mbedcrypto": "1107eef2c02409840b5f51f9333e527e027aef26f2246af5831f489d50eb6ba0",
    "Mbedtls": "f9e8b88cf029b1d8c3093fb3c3150ebc8815f05b7ca259950a1c05e31c7ac532",
    "Mbedx509": "8697ad3585f41fb8b8806efaabdd6ff1fa617c7b5a05c13aa86152902ae12917",
    "Mpv": "e127db446f863d18f188736baafce9f12511696c7b2bbaf4ddf3a50b502a03b1",
    "Png16": "7d7298dd45acafdbbbfbef062a70c9f39209fb612bf5c3997f5542d97adee62e",
    "Swresample": "99023ac41623fba61f9145ddf74bb6660fae974740e28012367e4335643b4cb8",
    "Swscale": "587da3b1fdf67b55de3993ccd648d5e61f19fdd567a8772e740483867c4f5f09",
    "Uchardet": "312a6958e337571af3002c5eb8a2f856321f6625ddcb3a966d54984cff14e350",
    "Xml2": "ed2c252c95e5f0ab2fd74c8d769c614f3b6d93821f349c481fff05047b864ab8"
]

let package = Package(
    name: "media_kit_libs_macos_video",
    platforms: [
        .macOS("10.15")
    ],
    products: [
        .library(name: "media_kit_libs_macos_video", targets: ["media_kit_libs_macos_video"] + libmpvTargets)
    ],
    dependencies: [],
    targets: libmpvTargets.map { framework in
        .binaryTarget(
            name: framework,
            url: "\(libmpvArtifactBase)_\(framework).zip",
            checksum: libmpvChecksums[framework]!
        )
    } + [
        .target(
            name: "media_kit_libs_macos_video",
            dependencies: libmpvTargets.map { framework in .target(name: framework) },
            resources: [
                .process("PrivacyInfo.xcprivacy")
            ]
        )
    ]
)
