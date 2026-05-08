// swift-tools-version: 5.9
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let package = Package(
    name: "media_kit_video",
    platforms: [
        .macOS("10.15")
    ],
    products: [
        .library(name: "media-kit-video", targets: ["media_kit_video"])
    ],
    dependencies: [],
    targets: [
        .target(
            name: "media_kit_video",
            dependencies: [],
            sources: ["plugin"],
            resources: [
                .process("PrivacyInfo.xcprivacy")
            ]
        )
    ]
)
