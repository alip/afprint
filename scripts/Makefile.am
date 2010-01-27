CLEANFILES= puidlookup
SUBDIRS= .
bin_SCRIPTS= ofatag puidlookup

puidlookup: puidlookup.in
	$(SED) -e "s:@MYVERSION@:$(VERSION):" -e "s:@MYGITHEAD@:$(GIT_HEAD):" $< > $@
	chmod +x puidlookup
