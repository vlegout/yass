
AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/schedulers/cplex/include -Wno-deprecated -Wunused

AM_LDFLAGS = \
	-module -avoid-version -lm \
	$(top_builddir)/libyass/libyass.la

plugindir = $(libdir)/yass

plugin_LTLIBRARIES = \
	bf.la \
	csf.la \
	csfd.la \
	csfp50.la \
	csfdp50.la \
	csfp80.la \
	csfdp80.la \
	csfn50.la \
	csfn80.la \
	edf.la \
	fcfs.la \
	fork.la \
	gangedf.la \
	gedf.la \
	llf.la \
	pedf.la \
	pf.la \
	rm.la \
	run.la \
	sp.la \
	uedf.la

if ENABLE_CPLEX

plugin_LTLIBRARIES += \
	izl.la \
	lpdpm1.la \
	lpdpm2.la \
	lpdpmmc1.la \
	lpdpmmc2.la \
	lpdpmmc3.la \
	lpdpmmc4.la \
	lpdpmmc5.la

endif

bf_la_SOURCES = bf.c

csf_la_SOURCES = csf.c csf_common.c

csfd_la_SOURCES = csfd.c csf_common.c

csfp50_la_SOURCES = csf_p_50.c csf_common.c

csfdp50_la_SOURCES = csfd_p_50.c csf_common.c

csfp80_la_SOURCES = csf_p_80.c csf_common.c

csfdp80_la_SOURCES = csfd_p_80.c csf_common.c

csfn50_la_SOURCES = csf_n_50.c csf_common.c

csfn80_la_SOURCES = csf_n_80.c csf_common.c

edf_la_SOURCES = edf.c

fcfs_la_SOURCES = fcfs.c

fork_la_SOURCES = fork.c

gangedf_la_SOURCES = gang-edf.c

gedf_la_SOURCES = gedf.c

llf_la_SOURCES = llf.c

if ENABLE_CPLEX

izl_la_SOURCES = izl.cpp lpdpm.cpp lpdpm_online.cpp
izl_la_LIBADD = -L$(top_srcdir)/schedulers/cplex/$(CPLEX)/ -lilocplex -lconcert -lcplex

lpdpm1_la_SOURCES = lpdpm1.cpp lpdpm.cpp lpdpm_online.cpp
lpdpm1_la_LIBADD = -L$(top_srcdir)/schedulers/cplex/$(CPLEX)/ -lilocplex -lconcert -lcplex

lpdpm2_la_SOURCES = lpdpm2.cpp lpdpm.cpp lpdpm_online.cpp
lpdpm2_la_LIBADD = -L$(top_srcdir)/schedulers/cplex/$(CPLEX)/ -lilocplex -lconcert -lcplex

lpdpmmc1_la_SOURCES = lpdpmmc1.cpp lpdpm.cpp lpdpm_online.cpp
lpdpmmc1_la_LIBADD = -L$(top_srcdir)/schedulers/cplex/$(CPLEX)/ -lilocplex -lconcert -lcplex

lpdpmmc2_la_SOURCES = lpdpmmc2.cpp lpdpm.cpp lpdpm_online.cpp
lpdpmmc2_la_LIBADD = -L$(top_srcdir)/schedulers/cplex/$(CPLEX)/ -lilocplex -lconcert -lcplex

lpdpmmc3_la_SOURCES = lpdpmmc3.cpp lpdpm.cpp lpdpm_online.cpp
lpdpmmc3_la_LIBADD = -L$(top_srcdir)/schedulers/cplex/$(CPLEX)/ -lilocplex -lconcert -lcplex

lpdpmmc4_la_SOURCES = lpdpmmc4.cpp lpdpm.cpp lpdpm_online.cpp
lpdpmmc4_la_LIBADD = -L$(top_srcdir)/schedulers/cplex/$(CPLEX)/ -lilocplex -lconcert -lcplex

lpdpmmc5_la_SOURCES = lpdpmmc5.cpp lpdpm.cpp lpdpm_online.cpp
lpdpmmc5_la_LIBADD = -L$(top_srcdir)/schedulers/cplex/$(CPLEX)/ -lilocplex -lconcert -lcplex

endif

pedf_la_SOURCES = pedf.c

pf_la_SOURCES = pf.c

rm_la_SOURCES = rm.c

run_la_SOURCES = run.c

sp_la_SOURCES = sp.c

uedf_la_SOURCES = uedf.c

EXTRA_DIST = lpdpm.hpp csf.h
