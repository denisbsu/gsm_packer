#include "cyclic_buffer.h"
#include "buffer_delegate.h"
#include "stdlib.h"
#include "stdio.h"

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
  printf(item?"1":"0");
  buffer[cursor] = item;
  cursor = increment_cursor(cursor);
  if (cursor == barier && delegate) {
    delegate -> process(this);
  }
}
unsigned char *cyclic_buffer::copy_packet(int offset) {
  unsigned char *result = new unsigned char[buffer_len];
  copy_packet_to_buffer(offset, result, buffer_len);
  return result;
}

int cyclic_buffer::copy_packet_to_buffer(int offset, unsigned char *_buffer, int len) {
  if (len < buffer_len) {
    return -1;
  }
  offset = offset % buffer_len;
  offset = offset < 0 ? buffer_len + offset : offset;
  for (int counter = 0; counter < buffer_len; counter++, offset = increment_cursor(offset)) {
    _buffer[counter] = (unsigned char)buffer[offset];
  }
  return 0;
}

int cyclic_buffer::find_offset_for_sequence (int *seq, int len) {
  int initial = (buffer_len - 8 - len) / 2 - 3;
  if (initial < 0) initial = 0;
  for (int offset = initial; offset < buffer_len + initial; ++offset) {
    if (check_sequence_on_position(seq, len, offset % buffer_len)) {
      return offset;
    }
  }
  return -1;
}

cyclic_buffer::~cyclic_buffer() {
  delete [] buffer;
};