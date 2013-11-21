Who?
----
This program was written by mail@vanheusden.com
Check http://www.vanheusden.com/multitail/ for new(er)
versions.


Help?
-----
For help at any time, press F1.


How to compile
--------------
Make sure you have the ncursesw development libraries installed.
Note: ncursesW! This is required because of utf8 support.


Mailinglist
-----------
There's a mailinglist.
Send an e-mail to minimalist@vanheusden.com with in the subject 'subscribe multitail'
to subscribe.


Tips
----
You can also use MultiTail to view logfiles on other hosts!
How?
Like this:
multitail -l "ssh username@host tail -f file"
Q: but then I cannot enter the password!
A1: use authentication via keys
A2: or use "ssh-agent": then you only once have to enter your passphrase (so login once
    to that host manually, and then start MultiTail)


Q & A
-----
Q: the program fails then resizing the terminal-window
A: solution: upgrade ncursesw to version 5.3 (or more recent)

Q: when I use the -l option on some program, I get nothing in the window
A: now that is strange! please tell me what program you're trying to interface to MultiTail.
   please do: any help is appreciated!


It ain't workin'!
-----------------
Please send me an e-mail telling me all details and all. And if the file is
not too big, send it please as well.


For help, suggestions or anything else you can write to: folkert@vanheusden.com
Please consider using PGP. Key-ID is: 1f28d8ae, this key is available on most
public PGP key servers and also here: http://www.vanheusden.com/key.txt
