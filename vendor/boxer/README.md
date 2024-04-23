# Boxer 🥊

## Introduction

Boxer is a simple library that allows for easy cross-platform creation of message boxes / alerts / what have you.

## Example

macOS:

![macOS](https://user-images.githubusercontent.com/1409522/213894782-72c37b24-bdb3-4b29-a847-cbff7748b1fe.png)

Windows:

![Windows](https://user-images.githubusercontent.com/1409522/213894790-55cf2be8-bcc0-4867-95e0-7741993f07eb.png)

Linux:

![Linux](https://user-images.githubusercontent.com/1409522/213894798-1bb1c279-5190-4108-b49c-08a28c7dfc29.png)

## Language

Boxer is written in C++, though it has a C branch available as well.

## Compiling Boxer

Boxer is set up to be built with CMake.

To generate a static library, execute CMake with the root of the repo as the source directory. Additionally, the example program can be built by enabling the BOXER_BUILD_EXAMPLES option.

On Linux, Boxer requires the gtk+-3.0 package.

## Including Boxer

Wherever you want to use Boxer, just include the header:

```c++
#include <boxer/boxer.h>
```

## Linking Against Boxer

### Static

If Boxer was built statically, just link against the generated static library.

### CMake

To compile Boxer along with another application using CMake, first add the Boxer subdirectory:

```cmake
add_subdirectory("path/to/Boxer")
```

Then link against the Boxer library:

```cmake
target_link_libraries(<target> <INTERFACE|PUBLIC|PRIVATE> Boxer)
```

## Using Boxer

To create a message box using Boxer, call the 'show' method in the 'boxer' namespace and provide a message and title:

```c++
boxer::show("Simple message boxes are very easy to create.", "Simple Example");
```

A style / set of buttons may also be specified, and the user's selection can be determined from the function's return value:

```c++
boxer::Selection sel = boxer::show("Make a choice:", "Decision", boxer::Style::Warning, boxer::Buttons::YesNo);
```

Calls to 'show' are blocking - execution of your program will not continue until the user dismisses the message box.

### Encoding

Boxer accepts strings encoded in UTF-8:

```c++
boxer::show(u8"Boxer accepts UTF-8 strings. 💯", u8"Unicode 👍");
```

On Windows, `UNICODE` needs to be defined when compiling Boxer to enable UTF-8 support:

```cmake
if (WIN32)
   target_compile_definitions(Boxer PRIVATE UNICODE)
endif (WIN32)
```
