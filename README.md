# XXFWilson23 PS4 14.02 rainbow ShellUI text

This branch builds a volatile ShellUI module for PS4 firmware 14.02. It changes
native PS4 UI2 label text through the managed PUI `TextColor` property; it is
not a web-page animation and it does not modify `goldhen.bin`.

## Artifacts

1. `shellui/bin/xxfwilson23_rainbow_shellui.prx` is loaded into `SceShellUI`.
2. `payload/rainbow_loader.bin` embeds that exact PRX, stages it as
   `/user/data/xxfwilson23_rainbow_shellui.prx`, and asks GoldHEN SDK command 6
   to load it into `SceShellUI`.

Both components are volatile and disappear after a full PS4 reboot. The module
is never added to GoldHEN's persistent plugin list.

## Runtime validation and safety

The implementation uses the live 14.02 metadata captured from this console. It
requires these exact managed contracts before making any code change:

- `Sce.PlayStation.PUI.UI2.Label.OnPrepareRender()` is an instance method
  declared by `Label` and returns `void`;
- `Label.TextColor` has an instance setter accepting exactly one `UIColor`;
- `UIColor.FromByte(byte, byte, byte, byte)` is static and returns `UIColor`.

The color palette is created through `mono_runtime_invoke` and held by pinned
Mono GC handles. The module installs one five-byte relative detour on the
verified AOT `Label.OnPrepareRender` entry. Before installing it, HDE64 decodes
whole instructions and rejects relative control flow, RIP-relative operands,
returns, existing detours, or a target outside signed 32-bit jump range. The
original bytes and installed jump are read back through syscall 108. Any failed
check stops without enabling the effect.

The hook runs on ShellUI's own render thread, invokes the managed setter with a
pinned boxed `UIColor`, then calls the original method through a checked
trampoline. The live detour is intentionally not removed at runtime; rebooting
the PS4 is the supported way to unload it.

This validation greatly reduces risk, but live code detouring is inherently
experimental and can still panic the console. The log is written to
`/user/data/xxfwilson23-rainbow/rainbow-1402.log`.

## Build

GitHub Actions builds both stages and verifies that `rainbow_loader.bin`
contains exactly one byte-for-byte copy of the PRX produced in the same job.
For a local Linux build:

```sh
export OO_PS4_TOOLCHAIN=/path/to/OpenOrbis/PS4Toolchain
export GOLDHEN_SDK=/path/to/GoldHEN_Plugins_SDK
make -C "$GOLDHEN_SDK"
make
```

The payload stage also requires `PS4SDK` from
`Scene-Collective/ps4-payload-sdk`.

## Use

1. Load GoldHEN and enable its BinLoader server.
2. Send `rainbow_loader.bin` to port 9090 once.
3. Wait for `XXFWilson23: rainbow ShellUI text enabled`.
4. Fully reboot the PS4 to remove the module.

Do not inject the loader twice on the same boot. See
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for source-level attribution.
User-facing notifications are branded `XXFWilson23`.
