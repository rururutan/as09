# as68/as09

as68/as09 are 6800/6801/6803/6809 assembler on UNIX.

It outputs Motrola 'S format' object code.

To compile, set CFLAGS and DESTDIR in makefile appropriately.

## Usage

> as09 src_file [-O] [-v] [-s] [-o [obj_file]] [-l [lst_file]]

or

> as68 src_file [-X] [-v] [-s] [-o [obj_file]] [-l [lst_file]]

## Options

    -O		optimize long branches to short (6809 only)
    -l [file]	output assembler listing
    -o [file]	output object code
    -s		output symbol table to the list file
    -v		turn on verbose flag
    -X		enable 6801 operation codes (6800 only)

listing file name can be explicitly specified with '-l' option
or defaults to src_file.l.

object file name can be explicitly specified with '-o' option
or defaults to src_file.o.

## License

COPYRIGHT (c) 1981, 1987 Masataka Ohta, Hiroshi Tezuka

No rights reserved. Everyone is permitted to do anything
on this program including copying, transplanting,
debugging, and modifying.
