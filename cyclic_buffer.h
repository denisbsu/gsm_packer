class buffer_delegate;

class cyclic_buffer
{
private:
  int increment_cursor(int cursor);
  bool check_sequence_on_position(int *seq, int len, int pos);
public:
  int buffer_len;
  int *buffer;
  int cursor;
  int barier;
  buffer_delegate *delegate;

  cyclic_buffer(int len);
  void add_item(int item);
  int *copy_packet(int offset = 0);
  int find_offset_for_sequence (int *seq, int len);
  virtual ~cyclic_buffer();
};