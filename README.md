# WFQueue (Multi-Producer/Single-Consumer) in C++

Setup [vcpkg](https://vcpkg.io/en/) on the build machine, and ensure that VCPKG_ROOT is available in the PATH environment variable.
Details of how to do this can be found at steps 1 and 2 in this [getting started doc](https://learn.microsoft.com/en-gb/vcpkg/get_started/get-started).
Ensure the `vcpkg` executable is available in your `PATH`.

Configure CMake, which will install and build dependencies via vcpkg.
Additionally, since I use `NeoVim`, I export the `compile_commands.json` to the build directory to for use with `clangd`:

```bash
cmake --preset=vcpkg -DCMAKE_EXPORT_COMPILE_COMMANDS=1
```

Build the test via CMake:

```bash
cmake --build build
```

Run the tests:

```bash
./build/test_wait_free_queue
```

Build documentation into the docs folder:

```bash
doxygen Doxyfile
```

You can also build and test in a Docker containter.

Build the container:

```bash
docker build -t wf-queue-test .
```

Run the tests in the container:

```bash
docker run --rm wf-queue-test
```