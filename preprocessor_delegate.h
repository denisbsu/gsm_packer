class preprocessor_delegate
{
public:
	preprocessor_delegate(){};
	virtual ~preprocessor_delegate(){};
	virtual void set_mask(int *mask, int mask_len) = 0;
};