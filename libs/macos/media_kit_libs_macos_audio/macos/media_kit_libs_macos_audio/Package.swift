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

let libmpvArtifactBase = "https://github.com/media-kit/libmpv-darwin-build/releases/download/v0.7.0/libmpv-xcframeworks_v0.7.0_macos-universal-audio-full"
let libmpvChecksums = [
    "Avcodec": "1372bdc0fa7f4ded6b565d29023aba08dd0ed9d6b1916bd2f03409c6370f4e1d",
    "Avfilter": "8c471be58b680ae3ac60ffef3ea166447adcd40f660b051a6e4f59aecdd45893",
    "Avformat": "3bb73535b43a1cc992777c3b017eb5a84bf4d0ae437c9250ff8511a161617a46",
    "Avutil": "3d65e0b2bd65cad4c8a639194398753329dc657c2f418a9c7c662ec8947d76d4",
    "Mbedcrypto": "1107eef2c02409840b5f51f9333e527e027aef26f2246af5831f489d50eb6ba0",
    "Mbedtls": "f9e8b88cf029b1d8c3093fb3c3150ebc8815f05b7ca259950a1c05e31c7ac532",
    "Mbedx509": "8697ad3585f41fb8b8806efaabdd6ff1fa617c7b5a05c13aa86152902ae12917",
    "Mpv": "6f5d7fe8b31235d28d8a5fb97b801f0cfc3f19ccad7ac2cb9100f699aafb95e3",
    "Swresample": "87c53bd728d0406624c782497a0ef971d28226a84f67997c1891db8bc6c2cc12",
    "Swscale": "ea7786b057662edc505696bd9dbbb8bc48feadd869eee1d8eaeab8b2da15d9ed"
]

let package = Package(
    name: "media_kit_libs_macos_audio",
    platforms: [
        .macOS("10.15")
    ],
    products: [
        .library(name: "media_kit_libs_macos_audio", targets: ["media_kit_libs_macos_audio"] + libmpvTargets)
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
            name: "media_kit_libs_macos_audio",
            dependencies: libmpvTargets.map { framework in .target(name: framework) },
            resources: [
                .process("PrivacyInfo.xcprivacy")
            ]
        )
    ]
)
