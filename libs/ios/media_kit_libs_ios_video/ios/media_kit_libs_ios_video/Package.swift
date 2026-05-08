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

let libmpvArtifactBase = "https://github.com/media-kit/libmpv-darwin-build/releases/download/v0.7.0/libmpv-xcframeworks_v0.7.0_ios-universal-video-default"
let libmpvChecksums = [
    "Ass": "152cebec157ab9a8c0ea7c69709586455174654bb992b6e6a6603bbe3d315b2b",
    "Avcodec": "a6baa1a31800d2b589613d767a0725fe1c76b3e9b4530403e26f2aa80ab99af7",
    "Avfilter": "93724550b2613d656a9f26be5dcacf5bcead491cf41f1b1da368fa938053c1f2",
    "Avformat": "d7ebc5cf298f3a680253a4370259b0b04056197f216aa2f499f764871482e192",
    "Avutil": "eb6854669f86977eca3aee104372188a48ef4ca7a1628e1d5843108b97b0fcf9",
    "Dav1d": "e3af3deac054737be3adc5505ce73b81d9a865f5d7c3521ef947158af2307246",
    "Freetype": "64299703e582c1a7b0954ee513a0a53edaea0807e6afb0b73d71f9441017c156",
    "Fribidi": "67532b36791dbb227ba6f526ea96a62c19cf32da1c1661e17c3055e087c705d2",
    "Harfbuzz": "8d585274490514ba3832935db718a49e1f3dc7d03470f9a3a8880c1b4f30678e",
    "Mbedcrypto": "b016619250601bac48c1b628f958d352d58716bf74fcb6168d7c0ef7b1a78e4d",
    "Mbedtls": "457264fdcdaea78ff643c5d3fa184876f955fe202471b7f2b856fb698794269e",
    "Mbedx509": "1cde5fa0f6d5308d3a012ce4925551aaad0449e63af82de2ee509970645e96e6",
    "Mpv": "9956f3360a16f4284b8d2aaecf7169c46fb5f427fd0e144dc4a959bac16b8f10",
    "Png16": "07a73635dac96817136d595c09fe9854590cd0436c24093f60ee50c75705cb43",
    "Swresample": "8361e29e94e8ab82b7a7763a51db9c65dcbb23e06fdb2c79f8874bde6a13ff23",
    "Swscale": "2eaf89744d0b08e948b5637e643e12f87f6f67bea8eb06db01c012206ba2ba08",
    "Uchardet": "82f0117f1dd69cb2b57aaaf3d561a15af21584d9f4b6dc5b821e06468dec09fe",
    "Xml2": "370b24ec59970288f81281a6ace234632630b357963b18e9891eb98bca5cc164"
]

let package = Package(
    name: "media_kit_libs_ios_video",
    platforms: [
        .iOS("13.0")
    ],
    products: [
        .library(name: "media_kit_libs_ios_video", targets: ["media_kit_libs_ios_video"] + libmpvTargets)
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
