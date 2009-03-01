AM_CFLAGS= $(libofa_CFLAGS) $(libsndfile_CFLAGS) \
	   @AFPRINT_CFLAGS@ -DGITHEAD=\"$(GITHEAD)\"
bin_PROGRAMS= afprint
afprint_SOURCES= main.c
afprint_LDADD= $(libofa_LIBS) $(libsndfile_LIBS)
