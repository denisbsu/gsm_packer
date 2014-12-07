/* -*- c++ -*- */
/* 
 * Copyright 2014 <+YOU OR YOUR COMPANY+>.
 * 
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include "stream2burst_impl.h"
#include "gsm_constants.h"


const int packet_len = 156;


class cyclic_buffer;
class buffer_delegate {
private:
  bool adjust_barier(int expected_offset, int offset, cyclic_buffer *buffer);
  void copy_and_print_buffer(cyclic_buffer *buffer);
  bool check_seq(int *seq, int len, int offset, cyclic_buffer *buffer, char* name = (char *)"UNKNOWN");
public:
  buffer_delegate(){};
  ~buffer_delegate(){};
  void process(cyclic_buffer *buffer);
};
int *encoded_sync;
int encoded_sync_len;
int **encoded_train;
int *freq_sync;
int freq_sync_len;
cyclic_buffer *test_buffer;


class cyclic_buffer
{
private:
  int increment_cursor(int cursor) {
    return cursor = (cursor + 1) % packet_len;
  }
  int decrement_cursor() {
    cursor--;
    if (cursor < 0)
      cursor = packet_len - 1;
    return cursor;
  }
  bool check_sequence_on_position(int *seq, int len, int pos) {
    for (int i = 0; i < len; ++i, pos = increment_cursor(pos)) {
      if (seq[i] != buffer[pos]) {
        return false;
      }
    }
    return true;
  }
public:
  int *buffer;
  int cursor;
  int barier;
  buffer_delegate *delegate;

  cyclic_buffer():cursor(0), barier(0), delegate(NULL) {
    buffer = new int[packet_len];
    for (int i = 0; i < packet_len; ++i) {
      buffer[i] = 0;
    };
  };
  void add_item(int item) {
    buffer[cursor] = item;
    cursor = increment_cursor(cursor);
    if (cursor == barier && delegate) {
      delegate -> process(this);
    }
  }
  int *copy_packet(int offset = 0) {
    int *result = new int[packet_len];
    offset = offset % packet_len;
    offset = offset < 0 ? packet_len + offset : offset;
    for (int counter = 0; counter < packet_len; counter++, offset = increment_cursor(offset)) {
      result[counter] = buffer[offset];
    }
    return result;
  }
  int find_offset_for_sequence (int *seq, int len) {
    for (int offset = 0; offset < packet_len; ++offset) {
      if (check_sequence_on_position(seq, len, offset)) {
        return offset;
      }
    }
    return -1;
  }

  virtual ~cyclic_buffer() {
    delete [] buffer;
  };
};


const int true_train_offset = 62;
const int true_sync_offset = 43;
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

bool buffer_delegate::adjust_barier(int expected_offset, int offset, cyclic_buffer *buffer) {
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

void buffer_delegate::copy_and_print_buffer(cyclic_buffer *buffer) {
  int *internal_buffer = buffer -> copy_packet(buffer -> barier);
  internal_buffer[0]=internal_buffer[1]=internal_buffer[2]=0;
  int state = 0;
  for (int j = 0; j < packet_len; ++j) {
    if (internal_buffer[j]) state = !state;
    printf("%d", internal_buffer[j]);
  }
  printf("\n");
  delete [] internal_buffer;  
}

bool buffer_delegate::check_seq(int *seq, int len, int seq_offset, cyclic_buffer *buffer, char* name) {
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

  void buffer_delegate::process(cyclic_buffer *buffer) {
    printf("Processing\n");
    if (check_seq(freq_sync, freq_sync_len, 1, buffer, (char *)"FREQ")) {
      return;
    }
    if (check_seq(encoded_sync, encoded_sync_len, true_sync_offset, buffer, (char *)"SYNC")) {
      return;
    }
    for (int i = 0; i < TRAIN_SEQ_NUM; ++i) {
      if (check_seq(encoded_train[i], N_TRAIN_BITS - 1, true_train_offset, buffer, (char *)"TRAIN")) {
        return;
      }
    }
    check_seq(NULL, 0, 0, buffer, (char*)"UNKNOWN");
  };





namespace gr {
  namespace gsm_packer {

    stream2burst::sptr
    stream2burst::make()
    {
      return gnuradio::get_initial_sptr
        (new stream2burst_impl());
    }

    /*
     * The private constructor
     */
    stream2burst_impl::stream2burst_impl()
      : gr::block("stream2burst",
              gr::io_signature::make(1, 1, sizeof(char)),
              gr::io_signature::make(1, 1, sizeof(char) * 142))
    {
      encoded_sync_len = sizeof(SYNC_BITS)/sizeof(SYNC_BITS[0]) - 1;
      encoded_sync = new int[encoded_sync_len];
      for (int i = 0; i < encoded_sync_len; ++i) {
        encoded_sync[i] = SYNC_BITS[i] != SYNC_BITS[i + 1];
      }

      encoded_train = new int*[TRAIN_SEQ_NUM];
      for (int i = 0; i < TRAIN_SEQ_NUM; ++i) {
        encoded_train[i] = new int[N_TRAIN_BITS - 1];
        for (int j = 0; j < N_TRAIN_BITS - 1; ++j) {
          encoded_train[i][j] = train_seq[i][j] != train_seq[i][j + 1];
        }
      }

      freq_sync_len = 147;
      freq_sync = new int[freq_sync_len];
      for (int i = 0; i < freq_sync_len; ++i) {
        freq_sync[i] = 0;
      }
      test_buffer = new cyclic_buffer();
      test_buffer -> delegate = new buffer_delegate();

    }

    /*
     * Our virtual destructor.
     */
    stream2burst_impl::~stream2burst_impl()
    {
      delete [] encoded_sync;
      for (int i = 0; i < TRAIN_SEQ_NUM; ++i) {
        delete [] encoded_train[i];
      }
      delete [] encoded_train;
      delete [] freq_sync;
      delete test_buffer -> delegate;
      delete test_buffer;
    }

    void
    stream2burst_impl::forecast (int noutput_items, gr_vector_int &ninput_items_required)
    {
        /* <+forecast+> e.g. ninput_items_required[0] = noutput_items */
    }

enum decoder_state {
  FIND_FCC,
  DECODING

};


static int packet_cursor = 0;
static int packet_buffer[packet_len];

    int
    stream2burst_impl::general_work (int noutput_items,
                       gr_vector_int &ninput_items,
                       gr_vector_const_void_star &input_items,
                       gr_vector_void_star &output_items)
    {
        const char *in = (const char *) input_items[0];
        int input_len = ninput_items[0];
        if (!input_len) {
          return 0;
        }
        
        printf("%d\n", input_len);
        /*
        for (int i = 0; i < input_len; ++i) {
          printf("%d ", in[i]);
        }
        printf("\n");
        */
        std::vector<char> *out = (std::vector<char> *) output_items[0];

        const int short_sync_len = 25;
        const int short_sync_offset = 62;
        static decoder_state state = FIND_FCC;
        static int zeroes = 0;
        int new_item = 0;
        static int counter = 0;
        static int diff_state = 0;
        static int TS = 0;
        static int short_sync[short_sync_len] = {1,1,1,0,1,0,0,0,0,1,1,0,1,0,0,1,1,1,1,0,1,0,0,0,0};
        for (int i = 0; i < input_len; ++i) {
          new_item = !in[i];
          test_buffer -> add_item(new_item);


          switch (state) {

            case FIND_FCC:
              if (new_item) {
                //if (zeroes > 100) printf("zeroes: %d\n", zeroes);
                zeroes = 0;
              }
              else {
                zeroes++;
                if (zeroes == 147) {
                  i = i + 8;
                  state = DECODING;
                  zeroes = 0;
                  TS = 1;
                }
              }
            break;

            case DECODING:
              if (counter == 0) {
                if (!(in[i] == 0 && in[i+1] == 1 && in[i+2] == 1)) {
                  //printf("Adjusting sync: (%d%d%d)\n", in[i], in[i+1], in[i+2]);
/*                  printf("Trying forward: ");
                  if ((in[i+1] == 0 && in[i+2] == 1 && in[i+3] == 1))
                  {
                    printf("OK\n");
                    break;
                  }
                  printf("failed\n");*/
                  //printf("Trying backward: ");
                  if (in[i-1] == 0 && in[i] == 1 && in[i+1] == 1)
                  {
                    //printf("OK\n");
                    i = i-2;
                    break;
                  }
                  //printf("failed\n");
                  //printf("Going forward.\n");
                  break;

                }
              }
              if (new_item == 0) {
                zeroes++;
              }
              /*if (counter > 2 && counter < 145) {
                if (new_item) diff_state = !diff_state;
                printf("%d", diff_state);
              }
              */
              packet_buffer[counter] = new_item;
              counter++;
              if (counter == packet_len) {
                //printf("\n");
                counter = 0;
                diff_state = 0;
                int offset = 0;
                for (int j = 0; j < packet_len - short_sync_len; ++j) {
                  bool found = true;
                  for (int k = 0; k < short_sync_len && found; ++k) {
                    found = packet_buffer[j+k] == short_sync[k];
                  }
                  if (found)
                  {
                    //printf("offset: %d\n", j);
                    offset = j;
                  }
                }

                int delta = 0;
                if (offset) {
                  delta = offset - short_sync_offset;
                  if (delta) {
                    if (delta > -4 && delta < 10) {
                      //printf("easy recovery by %d\n", delta);
                      i = i + delta;
                    }
                    else {
                      int cursor = i - packet_len + delta + 3;
                      if (delta < 0 && cursor >= 0) {
                        int value = delta + 3;
                        int cursor2 = 0;
                        for (;value <= 0; ++value, ++cursor, ++cursor2) {
                          packet_buffer[cursor2] = !in[cursor];
                          //printf("writing %d\n", temp);
                        }
                        //printf("hard recovery by %d\n", delta);
                        i = i + delta;
                        delta = -3;
                      }
                      else {
                        printf("Frame is lost (%d)\n", delta);
                      }
                    } 
                  }
                }


                if (zeroes > 140 && TS != 0) {
                  if (i >= packet_len) i = i - packet_len;
                  printf("seems wrong TS, reseted\n");
                  state = FIND_FCC;
                  TS = 0;
                  break;
                }
                printf("TS%d ", TS);
                int state = 0;
                packet_buffer[0] = 0;
                for (int j = delta; j < packet_len + delta; ++j) {
                  if (j < 0 || j >= packet_len) {
                    printf("0"); state = 0;
                  } else {
                    if (packet_buffer[j]) state = !state;
                    //printf("%d", state);
                    printf("%d", packet_buffer[j]);
                  }
                }
                printf("\n");
                TS++;
                zeroes = 0;
                if (TS == 8) TS = 0;
              }
              
          }
        }

        // Do <+signal processing+>
        // Tell runtime system how many input items we consumed on
        // each input stream.
        consume_each (input_len);

        // Tell runtime system how many output items we produced.
        return noutput_items;
    }

  } /* namespace gsm_packer */
} /* namespace gr */

