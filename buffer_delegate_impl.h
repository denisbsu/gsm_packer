#include "buffer_delegate.h"
#include "preprocessor_delegate.h"

class buffer_delegate_impl : public buffer_delegate , public preprocessor_delegate {
private:
  bool adjust_barier(int expected_offset, int offset, cyclic_buffer *buffer);
  void copy_and_print_buffer(cyclic_buffer *buffer, char *type);
  bool check_seq(int *seq, int len, int offset, cyclic_buffer *buffer, char* name = (char *)"UNKNOWN");
  unsigned char *internal_buffer;
public:
  int packet_len;
  int patterns_num;
  int **patterns;
  int *patterns_lens;
  char **patterns_names;
  int *patterns_offsets;
  int *patterns_mask;
  void (*callback)(unsigned char *, char *);
  buffer_delegate_impl(int len);
  ~buffer_delegate_impl();
  void process(cyclic_buffer *buffer);
  void set_mask(int *mask, int mask_len);
};
