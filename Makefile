include version

UTF8_SUPPORT=yes
DESTDIR=
PREFIX=/usr
CONFIG_FILE=$(DESTDIR)/etc/multitail.conf

CC?=gcc
DEBUG=-g -O2 -Wall # -D_DEBUG # -pg #  -D_DEBUG  #-pg -W -pedantic # -pg #-fprofile-arcs
ifeq ($(UTF8_SUPPORT),yes)
LDFLAGS+=-lpanelw -lncursesw -lutil -lm
CFLAGS+=-funsigned-char -D`uname` -DVERSION=\"$(VERSION)\" -DCONFIG_FILE=\"$(CONFIG_FILE)\" -DUTF8_SUPPORT -D_FORTIFY_SOURCE=2
else
LDFLAGS+=-lpanel -lncurses -lutil -lm
CFLAGS+=-funsigned-char -D`uname` -DVERSION=\"$(VERSION)\" -DCONFIG_FILE=\"$(CONFIG_FILE)\" -D_FORTIFY_SOURCE=2
endif

OBJS=utils.o mt.o error.o my_pty.o term.o scrollback.o help.o mem.o cv.o selbox.o stripstring.o color.o misc.o ui.o exec.o diff.o config.o cmdline.o globals.o history.o xclip.o

all: multitail

multitail: $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o multitail

multitail_ccmalloc: $(OBJS)
	ccmalloc --no-wrapper $(CC) -Wall -W $(OBJS) $(LDFLAGS) -o ccmultitail

install: multitail
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp multitail $(DESTDIR)$(PREFIX)/bin
	mkdir -p $(DESTDIR)$(PREFIX)/share/man/man1
	cp multitail.1 $(DESTDIR)$(PREFIX)/share/man/man1/multitail.1
	mkdir -p $(DESTDIR)$(PREFIX)/share/doc/multitail-$(VERSION)
	cp *.txt INSTALL manual*.html $(DESTDIR)$(PREFIX)/share/doc/multitail-$(VERSION)
	#
	### COPIED multitail.conf.new, YOU NEED TO REPLACE THE multitail.conf
	### YOURSELF WITH THE NEW FILE
	#
	mkdir -p $(DESTDIR)/etc/multitail/
	cp multitail.conf $(CONFIG_FILE).new
	cp conversion-scripts/* $(DESTDIR)/etc/multitail/
#rm -f $(DESTDIR)$(PREFIX)/share/man/man1/multitail.1.gz
#gzip -9 $(DESTDIR)$(PREFIX)/share/man/man1/multitail.1
	#
	# There's a mailinglist!
	# Send an e-mail to minimalist@vanheusden.com with in the subject
	# 'subscribe multitail' to subscribe.
	#
	# you might want to run 'make thanks' now :-)
	# http://www.vanheusden.com/wishlist.php
	#
	# How do YOU use multitail? Please send me an e-mail so that I can
	# update the examples page.

uninstall: clean
	rm -f $(DESTDIR)$(PREFIX)/bin/multitail
	rm -f $(DESTDIR)$(PREFIX)/share/man/man1/multitail.1.gz
#	rm -f $(CONFIG_FILE)
	rm -rf $(DESTDIR)$(PREFIX)/share/doc/multitail-$(VERSION)

clean:
	rm -f $(OBJS) multitail core gmon.out *.da ccmultitail

package: clean
	# source package
	rm -rf multitail-$(VERSION)*
	mkdir multitail-$(VERSION)
	cp conversion-scripts/* *.conf *.c *.h multitail.1 manual*.html Makefile makefile.* Changes INSTALL license.txt readme.txt thanks.txt version multitail-$(VERSION)
	tar czf multitail-$(VERSION).tgz multitail-$(VERSION)
	rm -rf multitail-$(VERSION)

thanks:
	echo Automatic thank you e-mail for multitail $(VERSION) on a `uname -a` | mail -s "multitail $(VERSION)" folkert@vanheusden.com
	echo Is your company using MultiTail and you would like to be
	echo mentioned on http://www.vanheusden.com/multitail/usedby.html ?
	echo Then please send me a logo -not too big- and a link and I will
	echo add it to that page.
	echo
	echo Oh, blatant plug: http://keetweej.vanheusden.com/wishlist.html

check:
	cppcheck -v --force -j 3 --enable=all --inconclusive -I. . 2> err.txt
	#
	make clean
	scan-build make

coverity:
	make clean
	rm -rf cov-int
	CC=gcc cov-build --dir cov-int make all
	tar vczf ~/site/coverity/multitail.tgz README cov-int/
	putsite -q
	/home/folkert/.coverity-mt.sh
