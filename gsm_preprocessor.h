#include "decoder/gsmstack.h"

class preprocessor_delegate;

class gsm_preprocessor
{
private:
	void enable_all_seq();
	void enable_one_seq(int num);
	void set_fn(int t1, int t2, int t3, int ts);
	void print_packet(unsigned char *packet);
	GS_CTX d_gs_ctx;
public:
	gsm_preprocessor();
	virtual ~gsm_preprocessor();
	preprocessor_delegate *delegate;
	int fn;
	int ts;

	void prepare_frame(unsigned char *frame, char *type);

	/* data */
};