# Autobuild

How LumaVAULT's nightly build (bigguy, 23:00 to 06:00) builds this repository. The form is in lumavault-server
`docs/AUTOBUILD.md`.

## gfamigawget2

Nothing to build on main yet: main holds the readme, and the work is on a branch. The nightly lists this entry as
never. When main has a build, give this section a script (a file in this repository), `frequency: nightly`, and what
to keep.

- frequency: never
- kind: app
- notes: nothing on main to build yet
