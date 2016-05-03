# https://gcc.gnu.org/onlinedocs/gcc/

CC := $(CROSS_COMPILE)gcc

# https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html#Optimize-Options
# https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html#Warning-Options
CFLAGS	= -Wall -O0

SRC	=	ioctl.c
OBJ	=	$(SRC:.c=.o)

all:	ioctl_example


ioctl_example:	$(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS) $(LIBS)

# $< - The name of the first prerequisite
# $@ - The file name of the target of the rule
.c.o:
	@echo [Compile] $<
	$(CC) -c $(CFLAGS) $< -o $@
	
.PHONY:	clean
clean:
	@echo "[Clean]"
	rm -f $(OBJ) ioctl_example
	sudo rm -f $(MODULE_DEST_TARGET)/ioctl_example $(MODULE_DEST_IMAGE)/ioctl_example

.PHONY:	install
install: ioctl_example
	@echo "[Install]"
	cp ioctl_example $(MODULE_DEST_TARGET)
	sudo cp ioctl_example $(MODULE_DEST_IMAGE)
