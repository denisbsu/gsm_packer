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
#include "cyclic_buffer.h"
#include "buffer_delegate_impl.h"


const int packet_len = 156;


cyclic_buffer *test_buffer;










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
      int *encoded_sync;
      int encoded_sync_len;
      int **encoded_train;
      int *freq_sync;
      int freq_sync_len;


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
      test_buffer = new cyclic_buffer(TS_BITS);
      buffer_delegate_impl *delegate = new buffer_delegate_impl(TS_BITS);
      delegate->patterns_num = TRAIN_SEQ_NUM + 3;

      delegate->patterns = new int*[delegate->patterns_num];
      delegate->patterns_lens = new int[delegate->patterns_num];
      delegate->patterns_names = new char*[delegate->patterns_num];
      delegate->patterns_offsets = new int[delegate->patterns_num];

      delegate->patterns[0] = freq_sync;
      delegate->patterns_lens[0] = freq_sync_len;
      delegate->patterns_names[0] = (char *)"FREQ";
      delegate->patterns_offsets[0] = 1;

      delegate->patterns[1] = encoded_sync;
      delegate->patterns_lens[1] = encoded_sync_len;
      delegate->patterns_names[1] = (char *)"SYNC";
      delegate->patterns_offsets[1] = 43;


      for (int i = 0; i < TRAIN_SEQ_NUM; ++i) {
        delegate->patterns[i + 2] = encoded_train[i];
        delegate->patterns_lens[i + 2] = N_TRAIN_BITS - 1;
        delegate->patterns_names[i + 2] = (char *)"TRAIN";
        delegate->patterns_offsets[i + 2] = 62;
      }

      delegate->patterns[TRAIN_SEQ_NUM + 2] = NULL;
      delegate->patterns_lens[TRAIN_SEQ_NUM + 2] = 0;
      delegate->patterns_names[TRAIN_SEQ_NUM + 2] = (char *)"UNKNOWN";
      delegate->patterns_offsets[TRAIN_SEQ_NUM + 2] = 0;


      test_buffer->delegate = delegate;

    }

    /*
     * Our virtual destructor.
     */
    stream2burst_impl::~stream2burst_impl()
    {
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

