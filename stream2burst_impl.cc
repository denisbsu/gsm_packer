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
    {}

    /*
     * Our virtual destructor.
     */
    stream2burst_impl::~stream2burst_impl()
    {
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

        const int packet_len = 156;
        const int short_sync_len = 25;
        const int short_sync_offset = 62;
        static decoder_state state = FIND_FCC;
        static int zeroes = 0;
        int new_item = 0;
        static int counter = 0;
        static int diff_state = 0;
        static int TS = 0;
        static int packet_buffer[packet_len]; 
        static int short_sync[short_sync_len] = {1,1,1,0,1,0,0,0,0,1,1,0,1,0,0,1,1,1,1,0,1,0,0,0,0};
        for (int i = 0; i < input_len; ++i) {
          new_item = !in[i];
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
                    printf("%d", state);
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

