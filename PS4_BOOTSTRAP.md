# PS4 bootstrap

The bootstrap package validates the OpenOrbis application entry point and
writable-data path before Source engine modules are added to the monolithic
link.

Build and validate the package on macOS:

```sh
./package-ps4-bootstrap.sh
```

The package is written below `build-ps4-bootstrap/package/` with title ID
`KISK00001`. It includes `libc.prx` and `libSceFios2.prx` from the configured
OpenOrbis SDK. Package art and `right.sprx` are copied from the local
`freegnm-examples/videoout-linear` package assets and are not duplicated in
this repository.

Stage the validated package to the configured PS4 FTP server:

```sh
./stage-ps4-bootstrap.sh
```

The defaults are `10.0.1.157:2121` and `/data/pkg`; override `PS4_HOST`,
`PS4_FTP_PORT`, or `PS4_PKG_DIR` when required. Staging does not install or
launch the package.

After installing and launching the package, success is the creation of:

```text
/data/kisak-strike/startup.log
```

The bootstrap-only package writes these bounded markers:

```text
kisak-ps4: bootstrap entered
kisak-ps4: bootstrap-only build
kisak-ps4: launcher not linked
```

`KISAK_PS4_MONOLITHIC=ON` changes the entry point to call
`KisakRegisterStaticModules()` and then `LauncherMain(argc, argv)` directly.
That option must only be enabled once the linked Source libraries provide the
registration hook and uniquely named factory exports.
