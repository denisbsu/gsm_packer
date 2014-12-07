#include "cyclic_buffer.h"
#include "buffer_delegate.h"
#include "stdlib.h"

int cyclic_buffer::increment_cursor(int cursor) {
  return cursor = (cursor + 1) % buffer_len;
}

bool cyclic_buffer::check_sequence_on_position(int *seq, int len, int pos) {
  for (int i = 0; i < len; ++i, pos = increment_cursor(pos)) {
    if (seq[i] != buffer[pos]) {
      return false;
    }
  }
  return true;
}

cyclic_buffer::cyclic_buffer(int len):cursor(0), barier(0), delegate(NULL) {
  buffer_len = len;
  buffer = new int[buffer_len];
  for (int i = 0; i < buffer_len; ++i) {
    buffer[i] = 0;
  };
};
void cyclic_buffer::add_item(int item) {
  buffer[cursor] = item;
  cursor = increment_cursor(cursor);
  if (cursor == barier && delegate) {
    delegate -> process(this);
  }
}
int *cyclic_buffer::copy_packet(int offset) {
  int *result = new int[buffer_len];
  offset = offset % buffer_len;
  offset = offset < 0 ? buffer_len + offset : offset;
  for (int counter = 0; counter < buffer_len; counter++, offset = increment_cursor(offset)) {
    result[counter] = buffer[offset];
  }
  return result;
}
int cyclic_buffer::find_offset_for_sequence (int *seq, int len) {
  for (int offset = 0; offset < buffer_len; ++offset) {
    if (check_sequence_on_position(seq, len, offset)) {
      return offset;
    }
  }
  return -1;
}

cyclic_buffer::~cyclic_buffer() {
  delete [] buffer;
};