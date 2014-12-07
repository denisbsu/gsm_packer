#include "buffer_delegate_impl.h"
#include "cyclic_buffer.h"
#include "stdlib.h"
#include "stdio.h"
#include "gsm_constants.h"

int compare_by_mod(int a, int b, int mod) {
  a = a % mod;
  a = a > 0 ? a : mod - a;
  b = b % mod;
  b = b > 0 ? b : mod - b;
  if (a == b) {
    return 0;
  }
  int delta = abs(a - b);
  delta = mod - delta > delta ? delta : mod - delta;
  if ((a + delta) % mod == b) {
    return -1;
  }
  return 1;
}

bool buffer_delegate_impl::adjust_barier(int expected_offset, int offset, cyclic_buffer *buffer) {
  int expected_beginning = (packet_len + offset - expected_offset) % packet_len;
  if (buffer -> barier != expected_beginning) {
    printf("Set barier (%d) to %d\n", buffer -> barier, expected_beginning);
  }
  if (compare_by_mod(buffer -> barier, expected_beginning, packet_len) < 0) {
    buffer -> barier = expected_beginning;
    printf("Will check next time\n");
    return false;
  }
  buffer -> barier = expected_beginning;
  return true;
}

void buffer_delegate_impl::copy_and_print_buffer(cyclic_buffer *buffer) {
  int *internal_buffer = buffer -> copy_packet(buffer -> barier);
  internal_buffer[0] = internal_buffer[1] = internal_buffer[2] = 0;
  int state = 0;
  for (int j = 0; j < packet_len; ++j) {
    if (internal_buffer[j]) state = !state;
    printf("%d", internal_buffer[j]);
  }
  printf("\n");
  delete [] internal_buffer;  
}

bool buffer_delegate_impl::check_seq(int *seq, int len, int seq_offset, cyclic_buffer *buffer, char* name) {
    if (!seq) {
      copy_and_print_buffer(buffer);
      printf("Print as is");
      return true;
    }
    int offset = buffer -> find_offset_for_sequence(seq, len);
    if (offset >= 0) {
        if (adjust_barier(seq_offset, offset, buffer)){
          copy_and_print_buffer(buffer);
        } else {
          return true;
        }
      printf("Found %s on offset %d (barier %d)\n", name, offset, buffer -> barier);
      return true;
    }
    return false;
}

void buffer_delegate_impl::process(cyclic_buffer *buffer) {
	//printf("Processing\n");
	for (int i = 0; i < patterns_num; ++i) {
		if (check_seq(patterns[i], patterns_lens[i], patterns_offsets[i], buffer, patterns_names[i])) {
			return;
		}
	}
};

buffer_delegate_impl::buffer_delegate_impl(int len) : packet_len(len) {};
buffer_delegate_impl::~buffer_delegate_impl(){
	for (int i = 0; i < patterns_num; ++i) {
		delete [] patterns[i];
		delete [] patterns_names[i];
	}
	if (patterns_num > 0) {
		delete [] patterns;
		delete [] patterns_lens;
		delete [] patterns_offsets;
		delete [] patterns_lens;
	}
};
