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

let libmpvArtifactBase = "https://github.com/Predidit/libmpv-darwin-build/releases/download/0.6.7/libmpv-xcframeworks_0.6.7_ios-universal-video-default"
let libmpvChecksums = [
    "Ass": "8607605bded29730fbd6c20d1b6988444d330ed7e4b1142b10e7cfaa55f772ec",
    "Avcodec": "a79367de7ebb92b9cc45ada63edc1a218e0299db967dd9a248d4c5dd19ea6191",
    "Avfilter": "2775ca25af51d177ec3a25491f549c50f2b6a18950e85af5cf0520bd18097895",
    "Avformat": "3f7f23642737819c0a33b8d9f3e3106ab7c2089bfd6946d8f8ebd8132747a6b1",
    "Avutil": "f6f7b7ecf8f31eead7396b098e915f6accf4c1b8eb230c6bbb08e77d6bee9beb",
    "Dav1d": "0d4bc8138648d5c47291b5e0ae65110e37e9989d7dd5652e6f5da42b3341ec1c",
    "Freetype": "b7ee3376aff8c4a98f2cb5490db06bfdf4541d3984a6ae5ed7be0dcf8c9fda81",
    "Fribidi": "6dbb29733d4832b2853acfb61390c7e2756f2597ebe37e0f2cc4f67008cd57a9",
    "Harfbuzz": "f6a463b5a33368d4f84da09ba1cf0cf6be8a7120a74746f522a4114ea7187610",
    "Mbedcrypto": "d1693d8053b8025b16e6eb246991afd81139bba4187d29aaee9be8dafa4aa2ec",
    "Mbedtls": "48739aa2fe45d234b51a0f019ea69623e15b2006463a90c7b023cfb3497082a7",
    "Mbedx509": "7c9e490743d56c7b6303e5a069afc3878ed797fab5c73b7055f29dd637b03586",
    "Mpv": "11ab22f45a46b4baa69f87c657d574d4f6c4845daf097fa21f3b82b75770c3e5",
    "Png16": "1692ae51f6c148d80a1c244e631a6f1495c18e80d397139db16b47e2021b6fc6",
    "Swresample": "8bfae5718a74b79dd64956649c92102d19c9ffc2597657607d6138d51f8a23a9",
    "Swscale": "5551a0421843efb55da8a520be240cc9f3a6f46c93572de4f0e7345c91c4171f",
    "Uchardet": "74dca5c489a12f324a7f6b087090670eef9364fec71886f8551a6fd434f435bb",
    "Xml2": "f4cf4ad858d64a291505d7d23d5218586849547541b3694ef979390117f0e1db"
]
let libmpvProductTargets: [String] = ["media_kit_libs_ios_video"] + libmpvTargets

let package = Package(
    name: "media_kit_libs_ios_video",
    platforms: [
        .iOS("9.0")
    ],
    products: [
        .library(name: "media-kit-libs-ios-video", targets: libmpvProductTargets),
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
            name: "media_kit_libs_ios_video",
            dependencies: libmpvTargets.map { framework in .target(name: framework) },
            resources: [
                .process("PrivacyInfo.xcprivacy")
            ]
        )
    ]
)
