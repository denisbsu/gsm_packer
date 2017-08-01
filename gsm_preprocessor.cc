#include "gsm_preprocessor.h"
#include "preprocessor_delegate.h"
#include "decoder/sch.h"
#include "string.h"
#include "stdio.h"
#include "math.h"

struct burst
{
	unsigned ts:3;
	unsigned fn:22;
	__uint128_t frame_data:116;
	unsigned stealing:1;
	unsigned encrypted:1;
	unsigned reserved:1;
	void set_bit(int i) {
		frame_data = frame_data | (static_cast<__uint128_t>(1) << i);
	}
	bool read_bit(int i) {
		return frame_data & (static_cast<__uint128_t>(1) << i);
	}
	void fill(int timeslot, int framenumber, unsigned char *buffer, bool stealing_bit = 0, bool encrypted_bit = 0, bool reserved_bit = 0) {
		ts = timeslot;
		fn = framenumber;
		frame_data = 0;
		for (int i = 0; i < 58; ++i) {
			if (buffer[i]) {
				set_bit(i);
			}
		}
		
		for (int i = 0; i < 58; ++i) {
			if (buffer[i+58+26]) {
				set_bit(i+58);
			}
		}
		
	}
	void print_burst() {
		printf("TS: %i FN: %i\n", ts, fn);
		for (int i = 0; i < 116; ++i) {
			if (read_bit(i)) {
				printf("1");
			} else {
				printf("0");
			}
		}
		printf("\n");
	}
};

unsigned char RNTABLE[114] = {
    48, 98, 63, 1, 36, 95, 78, 102, 94, 73, \
    0, 64, 25, 81, 76, 59, 124, 23, 104, 100, \
    101, 47, 118, 85, 18, 56, 96, 86, 54, 2, \
    80, 34, 127, 13, 6, 89, 57, 103, 12, 74, \
    55, 111, 75, 38, 109, 71, 112, 29, 11, 88, \
    87, 19, 3, 68, 110, 26, 33, 31, 8, 45, \
    82, 58, 40, 107, 32, 5, 106, 92, 62, 67, \
    77, 108, 122, 37, 60, 66, 121, 42, 51, 126, \
    117, 114, 4, 90, 43, 52, 53, 113, 120, 72, \
    16, 49, 7, 79, 119, 61, 22, 84, 9, 97, \
    91, 15, 21, 24, 46, 39, 93, 105, 65, 70, \
    125, 99, 17, 123 \
};

/*
 * Slow Frequency Hopping (SFH)
 * Return MAI - Mobile Allocation Index into MA
 */
int calculate_ma_sfh(int maio, int hsn, int n, int t1, int t2, int t3, int fn)
{
		printf("%d, %d, %d -> %d\n", t1, t2, t3, fn);
    int mai = 0;
    int s = 0;
    int nbin = floor(log2(n) + 1);

    if (hsn == 0)
        mai = (fn + maio) % n;
    else {
        int t1r = t1 % 64;
        int m = t2 + RNTABLE[(hsn ^ t1r) + t3];
        int mprim = m % (1 << nbin);
        int tprim = t3 % (1 << nbin);

        if (mprim < n)
            s = mprim;
        else
            s = (mprim + tprim) % n;

        mai = (s + maio) % n;
    }

    //DCOUT("MAI: " << mai);

    return (mai);
}

/*
 * Slow Frequency Hopping (SFH)
 * Calculate the index into the CA - Cell allocation
 * Maybe this should be a static table for the sake of speed
 */
/*
int calculate_ca_sfh(std::vector<unsigned char> ma, int maio, int hsn, int t1, int t2, int t3, int fn)
{
  unsigned char data;
  int cai_offset = 0;
  int mai_offset = 0;
  int found = 0;

  int mai = calculate_ma_sfh(maio, hsn, d_narfcn, t1, t2, t3, fn);
  std::vector<unsigned char>::iterator it;

  for(it = ma.begin(); it < ma.end(); it++)
  {
      data = (unsigned char)(*it);

      for (int i = 0; i < 8; i++)
      {
        if (data & 0x01)
        {
          if (mai_offset == mai)
          {
            found = 1;
            break;
          }
          mai_offset++;
        }

        cai_offset++;

        data >>= 1;
      }

      if (found)
        break;
  }

  //DCOUT("CAI: " << cai_offset)

  return (cai_offset);
}
*/


static FILE * sink;
static burst the_burst;

gsm_preprocessor::gsm_preprocessor() : fn(0), ts(0){
	GS_new(&d_gs_ctx);
	d_gs_ctx.ts_ctx[ts].type = TST_FCCH_SCH_BCCH_CCCH_SDCCH4;
	sink = fopen ("/Users/denisbsu/trash/gsm_log.txt" , "wb");
};

gsm_preprocessor::~gsm_preprocessor(){
	fclose(sink);
};

preprocessor_delegate *delegate;	
int empty_mask[] = {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1};
int full_mask[] =  {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

void gsm_preprocessor::enable_all_seq() {
	if (!delegate) {
		return;
	}
	delegate -> set_mask(full_mask, 12);
}
void gsm_preprocessor::enable_one_seq(int num) {
	if (!delegate) {
		return;
	}
	empty_mask[2 + num] = 1;
	delegate -> set_mask(empty_mask, 12);
	empty_mask[2 + num] = 0;
}

void gsm_preprocessor::set_fn(int t1, int t2, int t3, int _ts) {
	int true_fn = (51 * 26 * t1) + (51 * (((t3 + 26) - t2) % 26)) + t3;
	if (true_fn != fn) {
		printf("Unexpected fn: %d (vs %d)\n", true_fn, fn);
		fn = true_fn;
	} else {
		//printf("Alles gut\n");
	}
	ts = _ts;
}

void gsm_preprocessor::print_packet(unsigned char *packet) {
	printf("TS%d %d ", ts, fn);
	for (int i = 0; i < 142; ++i) {
		printf("%d", packet[i+3]);
	}
	printf("\n");
}
void gsm_preprocessor::prepare_frame(unsigned char *frame, char *type) {
	ts = (ts + 1) % 8;
	if (!ts) fn++;
	if (strncmp(type, (char *)"FREQ", 4) == 0) {
		if (ts) {
			printf("Forced sync\n");
		}
		ts = 0;
	}
			//print_packet(frame);


	if (strncmp(type, (char *)"SYNC", 4) == 0) {
		int t1, t2, t3, d_ncc, d_bcc;
		if (decode_sch(&frame[3], &t1, &t2, &t3, &d_ncc, &d_bcc) == 0) {
			//printf("bcc: %d ncc: %d t1: %d t2: %d t3: %d\n", d_bcc, d_ncc, t1, t2, t3);
			set_fn(t1, t2, t3, 0);
			enable_one_seq(d_bcc);
		} else {
			//printf("SYNC decoding failed");
			enable_all_seq();
		}
	}

	if (strncmp(type, (char *)"TRAIN", 5) == 0) {
		if (ts == 0) {
			int t3 = fn % 51;
			if (t3 % 10 > 1) {
				t3 = t3 - 2 * (t3 / 10);
				t3 = t3 % 4;
	//				int k = calculate_ma_sfh(0, 16, 2, fn / (51*26), fn % 26, fn % 51, fn);
	//printf("K = %d\n", k);


		//print_packet(frame);
				if (!GS_process(&d_gs_ctx, 0, NORMAL, &frame[3], fn, t3 == 2)) {
				//printf(".");
				} else {
				//printf("!");
				}
			}
		}
		the_burst.fill(ts, fn, &frame[3]);
		//the_burst.print_burst();
		fwrite(&the_burst, sizeof(burst), 1, sink);
	}

}