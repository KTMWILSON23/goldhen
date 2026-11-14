# Third-party notices

This project uses the documented GoldHEN SDK command that loads a PRX into a
named process. The two-stage ShellUI loading architecture was independently
validated against the following open-source reference:

- `Zenalios/ps4-thumbgrid-ime`, commit
  `9e4fbc9f39f207ce4f08f1ec635bca5fa5bb90f1`.

The reference is distributed under this license:

> MIT License
>
> Copyright (c) 2025 Zenalios
>
> Permission is hereby granted, free of charge, to any person obtaining a copy
> of this software and associated documentation files (the "Software"), to deal
> in the Software without restriction, including without limitation the rights
> to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
> copies of the Software, and to permit persons to whom the Software is
> furnished to do so, subject to the following conditions:
>
> The above copyright notice and this permission notice shall be included in all
> copies or substantial portions of the Software.
>
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
> IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
> FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
> AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
> LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
> OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
> SOFTWARE.

Builds also fetch pinned revisions of, but do not commit copies of:

- `GoldHEN/GoldHEN_Plugins_SDK`
- `OpenOrbis/OpenOrbis-PS4-Toolchain`
- `Scene-Collective/ps4-payload-sdk`

Those projects remain subject to their own licenses and attribution terms.
No repository-wide license file was found at the pinned
`Scene-Collective/ps4-payload-sdk` revision. At least
`libPS4/source/pfs.c` and `libPS4/source/pkg.c` carry GPL v2 notices; this file
does not infer a single license for every file in that dependency. The build
therefore publishes the loader linker map so the exact linked object set can
be reviewed before redistribution. This collector artifact is not represented
as a final public release. The ShellUI PRX is built separately with OpenOrbis
and is not linked with `libPS4`.
