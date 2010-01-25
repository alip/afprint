DEFS += -DGIT_HEAD=\"$(GIT_HEAD)\"
AM_CFLAGS = @AFPRINT_CFLAGS@ $(libofa_CFLAGS) $(libsndfile_CFLAGS)
bin_PROGRAMS = afprint
afprint_SOURCES = main.c
afprint_LDADD = $(libofa_LIBS) $(libsndfile_LIBS)
