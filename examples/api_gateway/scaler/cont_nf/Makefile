#                    openNetVM
#      https://github.com/sdnfv/openNetVM
#
# BSD LICENSE
#
# Copyright(c)
#          2015-2020 George Washington University
#          2015-2020 University of California Riverside
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
# Redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in
# the documentation and/or other materials provided with the
# distribution.
# The name of the author may not be used to endorse or promote
# products derived from this software without specific prior
# written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# include lwip properly
LWIPDIR=lwip-linux/lwip-2.0.2
LWIPINC=-I$(LWIPDIR)/src/include -I$(LWIPDIR)/src -I$(LWIPDIR)/test/unit 

CC=gcc
CFLAGS=-I. -Wall -Wextra -O0 -g3 $(LWIPINC)
DEPS=cont_nf.h

# %.o: lwip_stack.c cont_nf.c $(DEPS)
# 	$(CC) -c -o $@ $< $(CFLAGS)

cont_nf: lwip_stack.o cont_nf.o
	$(CC) $(CFLAGS) -o $@ lwip_stack.o cont_nf.o 

lwip_stack.o: lwip_stack.c $(DEPS)
	$(CC) $(CFLAGS) -c lwip_stack.c $(DEPS)

cont_nf.o: cont_nf.c $(DEPS)
	$(CC) $(CFLAGS) -c cont_nf.c $(DEPS)
# conf_nf: lwip_stack.o cont_nf.o $(DEPS)
# 	$(CC) 

.PHONY: clean

clean:
	rm -f *.o cont_nf *.gch
# LWIPDIR=lwip-linux/lwip-2.0.2
# LWIPINC=-I$(LWIPDIR)/src/include -I$(LWIPDIR)/src -I$(LWIPDIR)/test/unit 

# CC=gcc
# CFLAGS=-I. -Wall -Wextra -O0 -g3 $(LWIPINC)
# DEPS= cont_nf.h

# # binary name
# APP = cont_nf

# %.o: lwip_stack.c cont_nf.c $(DEPS)
# 	$(CC) -c -o $@ $< $(CFLAGS)

# cont: cont_nf.o
# 	$(CC) -o $(APP) cont_nf.o

# .PHONY: clean

# clean:
# 	rm -f $(APP) *.o