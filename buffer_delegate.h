class cyclic_buffer;

class buffer_delegate {
public:
  buffer_delegate(){};
  virtual ~buffer_delegate(){};
  virtual void process(cyclic_buffer *buffer) = 0;
};
