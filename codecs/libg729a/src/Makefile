#
# Build Library from Reference components
#
#
ARFLAGS=cr
CFLAGS+=-Wall -O3 -Dsingle -march=$(shell uname -m) -ffast-math -funroll-loops -fomit-frame-pointer -Werror
#CFLAGS+=-Wall -O3 -march=$(shell uname -m) -funroll-loops -fomit-frame-pointer -Werror
CFLAGS+=-DINLINE_FUNCS
LIB=libg729ab.a
OBJS=acelp_ca.o cod_ld8a.o \
    filter.o gainpred.o lpca.o lpcfunca.o \
    lspgetq.o p_parity.o pitch_a.o pre_proc.o \
    pred_lt3.o qua_gain.o qua_lsp.o tab_ld8a.o util.o \
    taming.o cor_func.o vad.o tab_dtx.o dtx.o \
    qsidgain.o qsidlsf.o calcexc.o dec_sid.o \
    de_acelp.o dec_gain.o dec_lag3.o dec_ld8a.o \
    lspdec.o post_pro.o postfila.o \
    g729ab.o
     
RANLIB=ranlib

all: $(LIB)

$(LIB): $(OBJS)
	$(AR) $(ARFLAGS) $(LIB) $(OBJS)
	$(RANLIB) $(LIB)

clean: 
	rm -f $(LIB) $(OBJS)
