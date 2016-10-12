include version

PLATFORM=$(shell uname)
NCURSES=$(pkg-config --libs --cflags ncurses)
# -D_DARWIN_C_SOURCE -I/opt/local/include -L/opt/local/lib -lncurses

UTF8_SUPPORT=yes
DESTDIR=
PREFIX=/opt/local
CONFIG_FILE=$(DESTDIR)$(PREFIX)/etc/multitail.conf

CC?=gcc
CFLAGS+=--std=c99 -Wall -Wextra -Wno-unused-parameter -funsigned-char -O3
CPPFLAGS+=-I$(PREFIX)/include -D$(PLATFORM) -DVERSION=\"$(VERSION)\" -DCONFIG_FILE=\"$(CONFIG_FILE)\" -D_FORTIFY_SOURCE=2

# build dependency files while compile (*.d)
CPPFLAGS+= -MMD -MP

DEBUG:=-g -D_DEBUG #-pg -W -pedantic # -pg #-fprofile-arcs

ifeq ($(PLATFORM),Darwin)
LDFLAGS+=-L$(PREFIX)/lib -lcurses -lpanel -lutil -lm
CFLAGS+=-DUTF8_SUPPORT
else
ifeq ($(UTF8_SUPPORT),yes)
LDFLAGS+=-lpanelw -lncursesw -lutil -lm
CFLAGS+=-DUTF8_SUPPORT
else
LDFLAGS+=-lpanel -lncurses -lutil -lm
endif
endif

OBJS:=utils.o mt.o error.o my_pty.o term.o scrollback.o help.o mem.o cv.o selbox.o stripstring.o color.o misc.o ui.o exec.o diff.o config.o cmdline.o globals.o history.o clipboard.o
DEPENDS:= $(OBJS:%.o=%.d)


.PHONY: all check install uninstall clean distclean package
all: multitail

pcredemo: LDFLAGS+=-lpcre

multitail: $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o multitail

ccmultitail: $(OBJS)
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
	mkdir -p $(DESTDIR)$(PREFIX)/etc/multitail/
	cp multitail.conf $(CONFIG_FILE).new
	cp conversion-scripts/* $(DESTDIR)$(PREFIX)/etc/multitail/
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
	rm -f $(OBJS) multitail core gmon.out *.da ccmultitail pcredemo

package: clean
	# source package
	rm -rf multitail-$(VERSION)*
	mkdir multitail-$(VERSION)
	cp -a conversion-scripts *.conf *.c *.h multitail.1 manual*.html Makefile makefile.* INSTALL license.txt readme.txt thanks.txt version multitail-$(VERSION)
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

### cppcheck: unusedFunction check can't be used with '-j' option. Disabling unusedFunction check.
check:
	#XXX TBD to use cppechk --check-config $(CPPFLAGS) -I/usr/include
	cppcheck --std=c99 --verbose --force --enable=all --inconclusive --template=gcc \
		'--suppress=variableScope' --xml --xml-version=2 . 2> cppcheck.xml
	cppcheck-htmlreport --file=cppcheck.xml --report-dir=cppcheck
	make clean
	-scan-build make

coverity:
	make clean
	rm -rf cov-int
	CC=gcc cov-build --dir cov-int make all
	tar vczf ~/site/coverity/multitail.tgz README cov-int/
	putsite -q
	/home/folkert/.coverity-mt.sh

distclean: clean
	rm -rf cov-int cppcheck cppcheck.xml *.d *~ tags

# include dependency files for any other rule:
ifneq ($(filter-out clean distclean,$(MAKECMDGOALS)),)
-include $(DEPENDS)
endif

