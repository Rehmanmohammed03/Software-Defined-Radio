## Building a project for debugging using `cmake`

These instructions explain how to build in `Debug` mode, rebuild the executables, and use `gdb` for debugging.

### Set the build type to `Debug`

To configure the build with the `Debug` type, change to the `build` sub-folder and run the following command:

```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
```

This will configure the build system to use `Debug` instead of `Release`. The `Debug` build includes symbols necessary for debugging, disables optimization, and enables additional runtime checks.

If you need to switch back to `Release`, run:

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release
```

### Rebuild the project

Once the configuration is updated, you need to rebuild the project by running:

```bash
make
```

### Use `gdb` to debug

To debug any of the generated executables using `gdb`, follow these steps:

Navigate to the build directory and run `gdb` with the executable you want to debug. For example, to debug `project` (this applies to any of the executables), use:

```bash
gdb ./project
```

Once inside `gdb`, you can set breakpoints at specific functions or lines. For example:

```bash
break main
```

Start the program by typing:

```bash
run
```

Use the following `gdb` commands to inspect and control program execution:

- `step`: Step into a function call.
- `next`: Execute the next line of code.
- `print <variable>`: Print the value of a variable.
- `backtrace`: Display the call stack.
- `continue`: Resume execution until the next breakpoint.

To exit `gdb`, type:

```bash
quit
```

### References

If you need good external references to `gdb`, it is suggested that you watch the first 4 minutes of this [third-party video tutorial](https://www.youtube.com/watch?v=bWH-nL7v5F4) to see how to step through your code in `gdb`.

For another quick overview, refer to this [textual tutorial](http://www.cs.toronto.edu/~krueger/csc209h/tut/gdb_tutorial.html).

Once more comfortable with `gdb`, refer to this short list of [gdb commands](https://www.tutorialspoint.com/gnu_debugger/gdb_commands.htm) and the [official documentation](https://www.gnu.org/software/gdb/documentation/) for further clarification.
