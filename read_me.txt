ALGOL68G - Algol 68 Genie (Version 1.18.0)
------------------------------------------

Algol68G is a nearly fully featured implementation of Algol 68 as defined by 
the Revised Report.

Algol68G's front-end parses an entire Algol 68 source program. If parsing is 
successful, the syntax tree (that serves as an intermediate program 
representation) is interpreted by Algol68G's back-end. The interpreter performs
many runtime checks. 

Author of Algol68G is Marcel van der Veer <algol68g@xs4all.nl>.
Web pages for Algol68G are at <http://www.xs4all.nl/~jmvdveer>.

Algol68G is free software, you can redistribute it and/or modify it under the 
terms of the GNU General Public License. This software is distributed in the 
hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied 
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. The author of 
this software does not accept responsibility to anyone for the consequences of 
using it or for whether it serves any particular purpose or works at all. See 
the GNU General Public License for more details. The GNU General Public License 
does not permit this software to be redistributed in proprietary programs.


BRIEF INSTALLATION GUIDE
------------------------

If you are a reasonably experienced Linux user, the following list of commands
may suffice to help you on your way.

Enter:

tar -xzf algol68g-1.18.0.tgz
cd algol68g-1.18.0
./configure -O2 --threads
make
make install # if you are superuser, that is, "root" #

If you do not feel confident to follow this short list, follow the detailed 
instructions in "Algol 68 Genie Documentation", a book containing an informal 
introduction to Algol 68, a manual for Algol 68 Genie, and the Algol 68 Revised
Report, which is available from:

http://www.xs4all.nl/~jmvdveer/algol.html


