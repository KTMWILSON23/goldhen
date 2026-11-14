# XXFWilson23 PS4 ShellUI metadata collector

This branch builds an intermediate, read-only metadata collector for PS4
firmware 14.02. It does **not** enable rainbow text. Its only purpose is to
record the live managed PUI/ShellUI API names needed to implement that effect
without guessing or detouring unverified native instructions.

## Delivery design

The build has two parts:

1. `shellui/bin/xxfwilson23_shellui_metadata_collector.prx` attaches to the
   Mono runtime already running inside `SceShellUI` and enumerates relevant
   class, property, and method metadata.
2. `payload/shellui_metadata_loader.bin` embeds that PRX, writes it to
   `/user/data/xxfwilson23_shellui_metadata_collector.prx`, and asks GoldHEN
   SDK syscall 500 to load it into `SceShellUI`. `/user/data` is shared across
   the BinLoader and `SceShellUI` process sandboxes. Before loading, the file is
   set to mode `0777`, matching GoldHEN's own plugin-loader requirement. The
   loader removes that file only if writing, permission setup, or loading fails.
3. The PRX writes bounded evidence to
   `/user/data/xxfwilson23-rainbow/pui-metadata-1402.txt`, including a completion
   status and SHA-256 digest.

The loaded module is volatile and disappears after a PS4 reboot.
Nothing is added to GoldHEN's persistent plugin list, and `goldhen.bin` is not
modified.

The collector installs no hooks, performs no `TextColor` writes, and makes no
UI changes.

## Current validation gate

The delivery path, Mono attachment, and bounded read-only metadata enumeration
are implemented. The actual label-tree walk remains disabled with
`XXF_RAINBOW_METADATA_READY=0` until this capture identifies the exact
root/child APIs. This collector must not be presented as a working rainbow
build.

This gate prevents two known failure modes:

- changing only labels created after the module loads; and
- detouring an AOT prologue without proving its instruction boundaries.

The final implementation will call the `TextColor` setter through
`mono_runtime_invoke` on ShellUI's own thread. The managed `UIColor` value is
passed by address; it is never passed as a native by-value struct.

## Build

GitHub Actions builds both stages and verifies that the final `.bin` contains
the exact PRX bytes produced earlier in the same job. For a local Linux build:

```sh
export OO_PS4_TOOLCHAIN=/path/to/OpenOrbis/PS4Toolchain
export GOLDHEN_SDK=/path/to/GoldHEN_Plugins_SDK
make -C "$GOLDHEN_SDK"
make
```

The payload stage also requires `PS4SDK` from
`Scene-Collective/ps4-payload-sdk`.

## Collector usage

1. Run the full 14.02 GoldHEN build.
2. Send `shellui_metadata_loader.bin` to GoldHEN BinLoader on port 9090.
3. Wait for the `XXFWilson23` collector completion notification.
4. Retrieve `/user/data/xxfwilson23-rainbow/pui-metadata-1402.txt` through GoldHEN
   FTP and verify its footer before using the evidence.
5. Reboot the PS4 to unload the collector.

Run the collector only once per boot. A second injection can load another copy
of the PRX and make both copies compete for the same evidence file.

See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for required source-level
attribution. User-facing notifications are branded `XXFWilson23`.
