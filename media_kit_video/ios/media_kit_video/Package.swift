// swift-tools-version: 5.9
import PackageDescription
let package = Package(
    name: "media_kit_video",
    platforms: [.iOS("13.0")],
    products: [.library(name: "media-kit-video", targets: ["media_kit_video"])],
    dependencies: [.package(name: "MediaKitMpv", path: "../../common/mpv")],
    targets: [.target(
        name: "media_kit_video",
        dependencies: [.product(name: "Mpv", package: "MediaKitMpv")],
        sources: ["plugin"],
        resources: [.process("PrivacyInfo.xcprivacy")]
    )]
)
