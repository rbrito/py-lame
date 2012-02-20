# Python bindings for `libmp3lame`

## Using the module

These bindings of Python for the LAME library are in an embrionary stage at
the moment.

There is no documentation at the moment, but you can, to partially remedy
the situation, use the sample application `slame` ("Simple LAME) to know how
to use these bindings.

This module is a thin Python wrapper around the C implementation of LAME,
but not all of the functions in C are implemented (this is subject to
change, feel free to send comments about it).

To restate, this is

    ******************
    *                *
    * ALPHA VERSION! *
    *                *
    ******************

Don't expect much yet.

## Building and installing

Build module:

	./setup.py build

Install module:

	./setup.py install

## Authors

* Alexander Leidinger (Alexander@Leidinger.net)
* Kyle VanderBeek (kylev@users.sf.net)

## License

This module is licensed under a BSD license.

----
$Id$
