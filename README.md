nbu-export
==========

nbu-export is a utility to export messages, memos, calender items and contacts
from Nokia NBU backups.

nbu-export runs on the BSDs, Linux, macOS and Cygwin.

This example will give you an idea of how it works:

	$ ./nbu-export
	usage: nbu-export backup [directory]
	$ ./nbu-export backup.nbu export
	$ find export -type f | sort
	export/calendar.ics
	export/contacts.vcf
	export/memos/memo-1.txt
	export/memos/memo-2.txt
	export/memos/memo-3.txt
	export/messages/predefdrafts.vmg
	export/messages/predefinbox.vmg
	export/messages/predefsent.vmg
	export/mms/predefinbox/1.mms
	export/mms/predefinbox/2.mms
	export/mms/predefinbox/3.mms

Building
--------

To build nbu-export, you will need a C compiler and `make`.

On OpenBSD, run:

	git clone https://github.com/tbvdm/nbu-export.git
	cd nbu-export
	make

On other systems, run:

	git clone https://github.com/tbvdm/nbu-export.git
	cd nbu-export
	git checkout portable
	make

Acknowledgement
---------------

Reading the [NbuExplorer][1] source was very helpful in understanding the NBU
file format. NbuExplorer was written by Petr Vilem. Many thanks to him!

[1]: https://sourceforge.net/projects/nbuexplorer/
