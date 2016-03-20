#!/bin/sh
echo '<html xmlns="http://www.w3.org/1999/xhtml"><body>
' > Readme.html
cat Readme.md >> Readme.html
echo '
</body></html>
' >> Readme.html
xsltproc scripts/xhtml_to_docbook.xsl Readme.html > Readme.docbook
