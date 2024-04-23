# libbitcoin-sqlite-task
This repository contains a small SQLite program in C++ for the stretch goal (libbitcoin organization, Summer of Bitcoin programme) created in 2024.

## Build and run
For compilation, run the following commands for creating the build directory:

```
mkdir build; cd build
```

Run cmake for configuration:

```
cmake ..
```

Compile with make by using:

```
make
```

Run the application with:
```
./app
```

## Table scheme
For the small database table scheme, the example from the official website of the [Visual Paradigm](https://www.visual-paradigm.com/features/database-design-with-erd-tools/) program was chosen. The *Staff* table structure is as follows:

![table_scheme.png](doc/table_scheme.png)

The examples of the table records are taken from the same source.
