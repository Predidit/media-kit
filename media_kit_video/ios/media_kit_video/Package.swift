// swift-tools-version: 5.9
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let package = Package(
    name: "media_kit_video",
    platforms: [
        .iOS("13.0")
    ],
    products: [
        .library(name: "media-kit-video", targets: ["media_kit_video"])
    ],
    dependencies: [
        .package(name: "media_kit_libs_ios_video", path: "../media_kit_libs_ios_video")
    ],
    targets: [
        .target(
            name: "media_kit_video",
            dependencies: [
                .product(name: "Mpv", package: "media_kit_libs_ios_video")
            ],
            sources: ["plugin"],
            resources: [
                .process("PrivacyInfo.xcprivacy")
            ]
        )
    ]
)
