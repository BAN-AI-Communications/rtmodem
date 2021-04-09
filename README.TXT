Date:     22 Nov 82 23:33:34-EST (Mon)
From:     Bob Hoffman
To:       Dave Fylstra
Subject:  MODEM protocol implementation for RT11

I read your note on net.micro.cpm concerning RT11 tools and thought
I'd give you what I have:  I wrote this to run on my LSI-11 at home
using the DECUS C compiler.  The code should be relatively
self-explanatory given an understanding of the MODEM protocol itself.
The reason for using a global (dladdr) for the device address is that
the device address is hard-coded into the binary in exactly one
place -- the global itself.  This makes it easy to patch the binary
if you should move the .SAV file to another system that has its
modem port at a different address [I find myself doing this often].
I have used it at 1200 baud and it seems to work just fine.  If you
have any questions or comments about it, please don't hesitate to ask.


	Good luck,
	---Bob.
		...mcnc!idis!pitt!hoffman
