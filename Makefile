.PHONY: all install

all:

install: ~/bin/re

~/bin/re: re
	ln -s $(CURDIR)/re ~/bin/re
