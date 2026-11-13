import Foundation
import CoreGraphics

public protocol FlutterTexture: AnyObject {}
public protocol FlutterTextureRegistry {
  func register(_ texture: FlutterTexture) -> Int64
  func unregisterTexture(_ id: Int64)
  func textureFrameAvailable(_ id: Int64)
}
public protocol ResizableTextureProtocol: FlutterTexture {
  func dispose()
  func resize(_ size: CGSize)
  func render(_ size: CGSize)
}

// Keep the real worker, VideoOutput initialization, and registration/disposal
// order. Only the platform textures and Flutter registry are replaced.
final class Control {
  static let lock = NSLock()
  static var gate: DispatchSemaphore?
  static var entered = false
  static var fail = false
  static var disposals = 0
  static func locked<T>(_ body: () -> T) -> T {
    lock.lock()
    defer { lock.unlock() }
    return body()
  }
}
final class TextureSW: ResizableTextureProtocol {
  init?(handle: OpaquePointer, updateCallback: @escaping () -> Void) {
    let gate = Control.locked { () -> DispatchSemaphore? in
      Control.entered = true
      return Control.gate
    }
    gate?.wait()
    if Control.locked({ Control.fail }) { return nil }
  }
  func dispose() { Control.locked { Control.disposals += 1 } }
  func resize(_ size: CGSize) {}
  func render(_ size: CGSize) {}
}
final class TextureHW: ResizableTextureProtocol {
  init?(handle: OpaquePointer, updateCallback: @escaping () -> Void) { return nil }
  func dispose() {}
  func resize(_ size: CGSize) {}
  func render(_ size: CGSize) {}
}
final class SafeResizableTexture: ResizableTextureProtocol {
  let child: ResizableTextureProtocol
  init(_ child: ResizableTextureProtocol) { self.child = child }
  func dispose() { child.dispose() }
  func resize(_ size: CGSize) { child.resize(size) }
  func render(_ size: CGSize) { child.render(size) }
}
struct Params { let rotate: Int64 = 0; let dw: Int64 = 0; let dh: Int64 = 0 }
enum MPVHelpers { static func getVideoOutParams(_ handle: OpaquePointer) -> Params { Params() } }
final class Registry: FlutterTextureRegistry {
  var registrations = 0
  var unregistrations = 0
  func register(_ texture: FlutterTexture) -> Int64 {
    precondition(Thread.isMainThread)
    registrations += 1
    return Int64(registrations)
  }
  func unregisterTexture(_ id: Int64) {
    precondition(Thread.isMainThread)
    unregistrations += 1
  }
  func textureFrameAvailable(_ id: Int64) { precondition(Thread.isMainThread) }
}

func check(_ condition: @autoclosure () -> Bool, _ message: String) {
  guard condition() else { fatalError(message) }
}
func pump(until condition: () -> Bool) {
  let deadline = Date().addingTimeInterval(3)
  while !condition() && Date() < deadline {
    _ = RunLoop.current.run(mode: .default, before: Date().addingTimeInterval(0.005))
  }
  check(condition(), "Timed out waiting for worker/platform callback")
}

@main enum Tests {
  static func main() {
    let renderGate = VideoOutputRenderGate()
    let rendering = DispatchSemaphore(value: 0)
    let releaseRender = DispatchSemaphore(value: 0)
    let disabling = DispatchSemaphore(value: 0)
    let disabled = DispatchSemaphore(value: 0)
    DispatchQueue.global().async {
      renderGate.runIfEnabled {
        rendering.signal()
        releaseRender.wait()
      }
    }
    check(rendering.wait(timeout: .now() + 2) == .success, "Rendering should enter the open gate")
    DispatchQueue.global().async {
      disabling.signal()
      renderGate.setEnabled(false)
      disabled.signal()
    }
    check(disabling.wait(timeout: .now() + 2) == .success, "Disable should start")
    check(disabled.wait(timeout: .now() + 0.02) == .timedOut, "Disable must wait for in-flight GL work")
    releaseRender.signal()
    check(disabled.wait(timeout: .now() + 2) == .success, "Disable should finish when GL work drains")
    check(!renderGate.runIfEnabled { fatalError("GL must not begin after disable completes") }, "Disabled gate rejects GL")
    renderGate.setEnabled(true)
    check(renderGate.runIfEnabled {}, "Foreground activation should reopen the gate")
    print("PASS rendering gate drains in-flight work and blocks new background GL")

    let configuration = VideoOutputConfiguration(width: 640, height: 360, enableHardwareAcceleration: false)
    let registry = Registry()
    let gate = DispatchSemaphore(value: 0)
    Control.locked { Control.gate = gate }
    var ready: Bool?
    var resized = false
    var disposed = false
    let output = VideoOutput(handle: 1, configuration: configuration, registry: registry,
      textureUpdateCallback: { _, _ in resized = true }, initializationCompletion: {
        check(Thread.isMainThread, "Initialization completion must use platform thread")
        check(resized, "Texture ID must be emitted before successful readiness")
        ready = $0
      })
    pump { Control.locked { Control.entered } }
    check(ready == nil && registry.registrations == 0, "Do not register/complete before context initialization")
    output.dispose { disposed = true }
    check(!disposed, "Disposal must wait for queued initialization")
    gate.signal()
    pump { disposed }
    check(ready == true && registry.registrations == 1 && registry.unregistrations == 1,
          "Initialization and disposal must each finish once in order")
    check(Control.locked { Control.disposals } == 1, "Render context must be disposed once")
    print("PASS real VideoOutput waits for initialization and drains queued disposal")

    Control.locked { Control.gate = nil; Control.fail = true }
    var failedReady: Bool?
    var failedDisposed = false
    let failed = VideoOutput(handle: 2, configuration: configuration, registry: registry,
      textureUpdateCallback: { _, _ in fatalError("Failed context cannot publish a texture") },
      initializationCompletion: { failedReady = $0 })
    pump { failedReady != nil }
    check(failedReady == false && registry.registrations == 1, "Creation failure is recoverable and registers no texture")
    failed.dispose { failedDisposed = true }
    pump { failedDisposed }
    check(registry.unregistrations == 1, "Failed output has no registered texture to unregister")
    print("PASS real VideoOutput reports context failure and remains disposable")
  }
}
