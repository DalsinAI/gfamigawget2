# Autobuild

How LumaVAULT's nightly build (bigguy, 23:00 to 06:00) builds this repository. The form is in lumavault-server
`docs/AUTOBUILD.md`.

## gfopenwget

The GFOpenUp part `Parts/wget2`: `C:wget2`, `LIBS:regex.library`, and the manual and licences in `SYS:Docs/Wget2`.
The PC tests (`tests/host-wget2.sh`) run first, with real downloads from local HTTP and HTTPS servers, and the
build stops if they fail. `build-nightly.sh` takes the open9 stove from `$STOVES`, because the nightly's own
`STOVE` is the plain os32-gcc16 one.

script: build-nightly.sh
frequency: nightly
kind: part
keep: out/Parts/wget2
minutes: 90
