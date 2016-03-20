Instructions on how to build the AFIO docs
==========================================
Niall Douglas

The Boost.AFIO docs are borrowed heavily from those of Boost.Geometry, so all
credit should go there. The following might look Windows specific, but I had
a terrible time matching up the right versions of binaries on POSIX from vendor
provided package repositories, so it's probably actually easier to configure
on Windows crazily enough.

1. Install doxygen, java and python 2.7 such that it runs from PATH.

2. Make a directory in a path containing no spaces somewhere to hold many tools.
Let's say C:\BoostBook or /home/ned/BoostBook.

3. Follow these instructions from http://www.boost.org/doc/libs/1_55_0/doc/html/quickbook/install.html.

WINDOWS ONLY: Get the xsltproc, libxml2, libxslt and the Docbook XSL mentioned
by the document above all into C:\BoostBook. The directory structure would look
like this:

C:\BoostBook\bin\xsltproc.exe
...
C:\BoostBook\xml\docbook-xsl
...

Note that the link on the above page to the Docbook XSL gives you the -ns version
which won't work. You need the -1 version from
http://sourceforge.net/projects/docbook/files/docbook-xsl/1.78.1/

4. OPTIONAL: Compile Boost QuickBook and copy the output binary into BoostBook\bin.

5. If on Boost prior to v1.54, apply the doxygen_xml2qbk.patch in this directory
to Boost. This will add support for producing QuickBook documentation from public
member variables. Compile libs/geometry/doc/src/docutils/tools/doxygen_xml2qbk
using something like:

./b2 libs/geometry/doc/src/docutils/tools/doxygen_xml2qbk

... and copy the output binary into BoostBook\bin.

6. I simply can't get any version past or present of Apache FOP to work with
the present DocBook XSLTs, so go to http://www.renderx.com/download/personal.html,
register for a personal edition of RenderX XEP, and follow the instructions from
http://www.boost.org/doc/libs/1_54_0/doc/html/boostbook/getting/started.html
to configure your user-config.jam with all the right paths but using XEP
instead of FOP.

7. Open a command box and cd to this directory (libs/afio/doc). Do:

set DOXYGEN_XML2QBK="C:\BoostBook\bin\doxygen_xml2qbk.exe"
python make_qbk.py

8. Return to the boost top directory. Do:

cd doc
../b2 ../libs/afio/doc

HTML documentation is output alongside other Boost documentation.

9. For PDF output do 

../b2 ../libs/afio/doc pdf
