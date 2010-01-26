CLEANFILES= puidlookup
SUBDIRS= .
bin_SCRIPTS= puidlookup

puidlookup: puidlookup.in
	$(SED) -e "s:@AFPRINT_VERSION@:$(VERSION):" $< > $@
	chmod +x puidlookup
