CC := clang
AS := as
LD := ld
STRIP := strip

CFLAGS := -O2 -mavx2 -ffreestanding -nostdlib -fno-pie -fno-builtin \
          -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables \
          -mno-red-zone -Wall -Wextra -Werror

LDFLAGS := -static -nostdlib -no-pie -z norelro -z noexecstack --build-id=none

all: linx_core_engine

linx_entry.o: linx_entry.s
	$(AS) --64 $< -o $@

linx_core_engine.o: linx_core_engine.c
	$(CC) $(CFLAGS) -c $< -o $@

linx_core_engine: linx_entry.o linx_core_engine.o
	$(LD) $(LDFLAGS) $^ -o $@
	$(STRIP) -s -R .comment -R .note -R .eh_frame -R .eh_frame_hdr $@

test: linx_core_engine
	taskset -c 0 ./linx_core_engine

clean:
	rm -f *.o linx_core_engine
