Any security related issues should be reported by following the instructions here:
https://01.org/security

Please check the following Android documentation related to HWC2 API:
https://android.googlesource.com/platform/hardware/libhardware/+/master/include/hardware/hwcomposer2.h  

## Contributing

Here are some guidelines to follow for contributing to IA-Hardware-Composer.

### Documentation

In order to facilitate newcomers to this open source project, we encourage
documentation on any newly added features, methods, and classes via doxygen.

To install doxygen on ubuntu simply run:

```
sudo apt install doxygen
```

Once installed, you should provide documentation to any new features you add to
the project or modify/create documentation for any big changes in methods,
classes, etc that you change.

Here is a documentation example on a method.
```
/**
* Add two integers and return the result.
*
* @param a the first integer to be added.
* @param b the second integer to be added.
* @return The result of the addition of a and b.
*/
int add(int a, int b){
  int add;
  add = a + b;
  return add;
}
```

For more information please visit:
https://www.stack.nl/~dimitri/doxygen/manual/docblocks.html

You may check your documentation change by running

```
doxygen Doxyfile
```

This will have generated an html folder with an index.html file inside.
