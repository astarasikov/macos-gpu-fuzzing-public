# Introduction
This code demonstrates how to corrupt GPU buffers (data/code)
on macOS on both X86 (which uses IOAccelResource) and
ARM (which uses IOGPUResource).

See examples in `corrupt_gpumem`.

It works by preloading libraries with `DYLD_INSERT_LIBRARIES` and running
an app, in this case a compute one.
