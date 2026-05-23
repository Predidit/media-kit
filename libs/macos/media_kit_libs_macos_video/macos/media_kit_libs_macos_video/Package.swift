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

let libmpvArtifactBase = "https://github.com/Predidit/libmpv-darwin-build/releases/download/0.6.7/libmpv-xcframeworks_0.6.7_macos-universal-video-default"
let libmpvChecksums = [
    "Ass": "d513ff96f1d43b56ec9ef1334812d83dee51c3ea81b71aa26de2105d6876556c",
    "Avcodec": "f56c4a92108312df069e77bfe2e672842832e3a2e9526a16710519ea8403662f",
    "Avfilter": "cb5a590ae4b2be70dcc80209b760290233b603d9cce968fe0628c40d704309c1",
    "Avformat": "25ea68a6e9fd5b4619ff00b1d967648ee139f047aa62a88042d320afede2ddbc",
    "Avutil": "50e2c8ae79f2baed26951ab1f93386dde7d6f245357d65e0cac8a49200300a5d",
    "Dav1d": "e2ca965b149ba85a67502234b40a68a2335a36e76204360be2f40c4d3e29027d",
    "Freetype": "b33dda0eb19eb61bf2c1cab6ee8598fc59e4be5472a6d4730cf5b546b50e019b",
    "Fribidi": "24b138976d8a315ae1e99fb563454c01727d7d9c2593da6b7ae5975472d4f5ee",
    "Harfbuzz": "98301d6f95d3f915e870677f8ccce8514d86a17863837437aefe92f204ea2731",
    "Mbedcrypto": "a152c81bbf0bf6caba31309db1c9987ed081224641f98694e8780870a71c78a4",
    "Mbedtls": "dac7eb987d97df86fb9400ca5e90e5c17cb86ec63f4d4b77fd552da25a4eb12b",
    "Mbedx509": "8b13a78328574c64490d6addd09136492e4fcf284466ab17d496f26b27d773a4",
    "Mpv": "5be94d380a12ce56d1016d48b46f4cac663178bd010c42389e6c9ec162ee4278",
    "Png16": "7b723cf98825386ec7588c25aacfeb8b25bb7335d1b0f7d2a1caee6950f872c3",
    "Swresample": "01f757ac170fdcae5724e607827cd15a18d3205be91890e1d4fd28f17332d174",
    "Swscale": "f3af7665670ec86912a82e7234de178bbfeb0a2967da204c09a8c64d85f0edc2",
    "Uchardet": "30d1ac3cf0bf74adf99fd891c6b8213ffb8fa697576af743b715aa5193e84527",
    "Xml2": "b76b6e63bf253be48c499c1f6c8f1a7b07b7814fe2015a8597367c83833cb161"
]
let libmpvProductTargets: [String] = ["media_kit_libs_macos_video"] + libmpvTargets

let package = Package(
    name: "media_kit_libs_macos_video",
    platforms: [
        .macOS("10.9")
    ],
    products: [
        .library(name: "media-kit-libs-macos-video", targets: libmpvProductTargets),
        .library(name: "Mpv", targets: ["Mpv"])
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
