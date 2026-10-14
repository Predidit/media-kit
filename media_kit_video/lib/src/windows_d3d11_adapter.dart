/// A D3D11 hardware adapter available to the current Windows process.
///
/// [id] is an opaque DXGI adapter ID used to resolve this adapter against the
/// current enumeration. It may become stale after hardware or driver changes;
/// if so, video output falls back to Auto and reports a
/// [WindowsVideoOutputError]. Do not assume the ID will remain valid forever.
class WindowsD3D11Adapter {
  /// Opaque DXGI adapter ID. Do not parse this value.
  final String id;

  /// Adapter description reported by DXGI.
  final String name;

  /// Dedicated video memory, in MiB.
  final int dedicatedVideoMemoryMb;

  /// Creates an adapter description.
  const WindowsD3D11Adapter({
    required this.id,
    required this.name,
    required this.dedicatedVideoMemoryMb,
  });
}

/// A renderer error reported by the Windows native video output.
class WindowsVideoOutputError {
  /// Native player handle associated with the failed video output.
  final int playerHandle;

  /// Human-readable error description.
  final String message;

  /// Creates a video output error.
  const WindowsVideoOutputError({
    required this.playerHandle,
    required this.message,
  });
}
