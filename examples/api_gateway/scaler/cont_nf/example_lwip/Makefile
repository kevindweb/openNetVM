C_OBJS=
ASM_OBJS=
COMPONENT=SSL_nf.o
INTERFACES=
DEPENDENCIES=
IF_LIB=./net_stack.o
ADDITIONAL_LIBS=-lcobj_format -lcos_kernel_api $(POSIX_HYPERCALL_LIB) -lcos_defkernel_api -laxtls

pre: first all
first:
	$(info |     [CP]	copying network stack from lwip)
	@cp $(LWIPOBJ) .

include ../../Makefile.subsubdir
MANDITORY_LIB=simple_stklib_NF.o
