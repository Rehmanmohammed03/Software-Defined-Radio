## Getting started with the workflow

To get started, or when adding new files to the project, change to the `build` sub-folder and run:

`cmake ..`

By default, the auto-generated `Makefile` will create three executables whose dependencies are defined in [`CMakeLists.txt`](../CMakeLists.txt). All you need to do is run `make` in the `build` sub-folder. The build is done in `Release` mode by default.

The `project` executable runs your mission code and is the primary executable for the project. For this lab, you will focus on the other two executables: `test_primitives`, which runs all enabled unit tests, and `bench_primitives`, which runs all registered benchmarks. To run only a subset of benchmarks, use the `--benchmark_filter` flag. For example, to benchmark only functions with "DFT" in their names, run:

`./bench_primitives --benchmark_filter=DFT.*`

For more details on Google's test framework, visit:

[https://github.com/google/googletest/blob/main/docs/index.md](https://github.com/google/googletest/blob/main/docs/index.md)

For more details on Google's benchmark framework, visit:

[https://github.com/google/benchmark/blob/main/docs/user_guide.md](https://github.com/google/benchmark/blob/main/docs/user_guide.md)

### Generate a single executable

By default, for the last lab, you should generate all three executables because the focus is on learning how to optimize DSP primitives via benchmarking and unit testing incrementally. However, during the project phase of the course, when the DSP primitives have been optimized, if you wish to build only one executable instead of all three (for example, only the `project` executable), change to the `build` sub-folder and type:

```bash
rm -rf CMakeCache.txt CMakeFiles *.cmake Makefile 

rm -f project test_primitives bench_primitives 

cmake .. -DTARGET_TO_BUILD=project

make
```

The same line of reasoning can be used for any of the three executables.

To build a single executable in debug mode, add `-DCMAKE_BUILD_TYPE=Debug` to the `cmake` command. For more on debugging, refer to [cmake-build-debug.md](cmake-build-debug.md).
