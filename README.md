DeforaOS Browser
================

About Browser
-------------

Browser is a file manager and image browser.

It can be extended through plug-ins, which are then displayed in the left pane
of the file management windows.

Browser is part of the DeforaOS Project, found at https://www.defora.org/.


Compiling Browser
-----------------

Browser depends on the following components:

 * Gtk+ 2 or 3
 * DeforaOS libDesktop
 * an implementation of `make`
 * gettext (libintl) for translations
 * DocBook-XSL for the manual pages
 * GTK-Doc for the API documentation

With GCC, this should then be enough to compile and install Browser:

    $ make install

To install (or package) Browser in a different location:

    $ make PREFIX="/another/prefix" install

Browser also supports `DESTDIR`, to be installed in a staging directory; for
instance:

    $ make DESTDIR="/staging/directory" PREFIX="/another/prefix" install


Documentation
-------------

Manual pages for each of the executables installed are available in the `doc`
folder. They are written in the DocBook-XML format, and need libxslt and
DocBook-XSL to be installed for conversion to the HTML or man file format.

Likewise, the API reference for Browser (plug-ins) is available in the
`doc/gtkdoc` folder, and is generated using gtk-doc.

Distributing Browser
--------------------

DeforaOS Browser is subject to the terms of the 2-clause BSD license. Please
see the `COPYING` file for more information.
