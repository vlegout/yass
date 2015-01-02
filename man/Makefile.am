
if ENABLE_MANPAGES

yass.1: yass.txt
	a2x -f manpage -D ${top_builddir}/man $<

yass-draw.1: yass-draw.txt
	a2x -f manpage -D ${top_builddir}/man $<

yass.interface.5: interface.txt
	a2x -f manpage -D ${top_builddir}/man $<

yass.configuration.5: configuration.txt
	a2x -f manpage -D ${top_builddir}/man $<

man1_MANS = \
	yass.1 \
	yass-draw.1

man5_MANS = \
	yass.configuration.5 \
	yass.interface.5

EXTRA_DIST = \
	configuration.txt \
	interface.txt \
	yass.txt \
	yass-draw.txt

CLEANFILES = $(man1_MANS) $(man5_MANS)

endif
