// swift-tools-version: 5.9
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let libmpvTargets = [
    "Avcodec",
    "Avfilter",
    "Avformat",
    "Avutil",
    "Mbedcrypto",
    "Mbedtls",
    "Mbedx509",
    "Mpv",
    "Swresample",
    "Swscale"
]

let libmpvArtifactBase = "https://github.com/media-kit/libmpv-darwin-build/releases/download/v0.7.0/libmpv-xcframeworks_v0.7.0_ios-universal-audio-default"
let libmpvChecksums = [
    "Avcodec": "fc72195ca32ed023a6230e4e558323fdb14d8e779b7c68cf97ed56ac6d00aebb",
    "Avfilter": "e4d71789a98a9f0d60d83de15b7facf6bcf5914f25a6614e8b956296899a99d7",
    "Avformat": "4abb7dd215203a22caaabb215b0b2f5e10fd1fe0c8a1ae874908eeea8859eb1e",
    "Avutil": "63573305de494575ca29b4bfe328997c0e844132d9e4b6bf8c22ffd4f3cf6273",
    "Mbedcrypto": "b016619250601bac48c1b628f958d352d58716bf74fcb6168d7c0ef7b1a78e4d",
    "Mbedtls": "457264fdcdaea78ff643c5d3fa184876f955fe202471b7f2b856fb698794269e",
    "Mbedx509": "1cde5fa0f6d5308d3a012ce4925551aaad0449e63af82de2ee509970645e96e6",
    "Mpv": "0c052ddcb71ab19bd1ae2bbc74514bbfad2ef71eb7b41468c7e71ee9cf7dd2dd",
    "Swresample": "5ddbeb5a57a59e87d3b62f33d6340fbcdc35a3dc972064d53c87b9cabf97b4fc",
    "Swscale": "302507bbd858e5fc84a88a441ae4b14d577a5a6bd8119ed2e2367468fc9a9dbb"
]

let package = Package(
    name: "media_kit_libs_ios_audio",
    platforms: [
        .iOS("13.0")
    ],
    products: [
        .library(name: "media_kit_libs_ios_audio", targets: ["media_kit_libs_ios_audio"] + libmpvTargets)
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
            name: "media_kit_libs_ios_audio",
            dependencies: libmpvTargets.map { framework in .target(name: framework) },
            resources: [
                .process("PrivacyInfo.xcprivacy")
            ]
        )
    ]
)
