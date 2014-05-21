/*
  lsquaredc.c

  Copyright (C) 2014 Jan Rychter
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:
  
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
  
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include "lsquaredc.h"

#define DEVICE_NAME_LENGTH 11   /* example: "/dev/i2c-1" + the terminating 0 */

static uint32_t check_i2c_functionality(handle) {
  unsigned long funcs;
  if(ioctl(handle, I2C_FUNCS, &funcs) < 0) {
    return 0;
  }
  return (uint32_t)(funcs & I2C_FUNC_I2C);
}


/*
   Opens an I2C device. The supplied bus number corresponds to Linux I2C bus numbering: e.g. for "/dev/i2c-1" use bus
   number 1. Also checks if the device actually supports I2C. Returns the handle which should subsequently be used with
   i2c_send_sequence(), or a negative number in case of an error.
*/
int i2c_open(uint8_t bus) {
  char device_name[DEVICE_NAME_LENGTH];
  int handle;
  if(bus > 9) return -1;        /* sanity check */
  snprintf(device_name, DEVICE_NAME_LENGTH, "/dev/i2c-%d", bus);
  if((handle = open(device_name, O_RDWR)) < 0) return handle;
  if(!check_i2c_functionality(handle)) return -1;
  return handle;
}


/*
   The Linux I2C ioctl interface annoyingly requires an *array* of struct i2c_msg pointers, instead of a pointer to a
   linked list. This means that we have to go through the sequence once just to count how many messages there will be,
   before we allocate memory for message buffers.
*/
static uint32_t count_segments(uint16_t *sequence, uint32_t sequence_length) {
  uint32_t number_of_segments = 1; /* there is always at least one segment */
  uint32_t i;

  for(i = 1; i < sequence_length; i++) {
    if(sequence[i] == I2C_RESTART) number_of_segments++;
  }
  return number_of_segments;
}

/* These are here for readability and correspond to bit 0 of the address byte. */
#define WRITING 0
#define READING 1


/*
  Sends a command/data sequence that can include restarts, writes and reads. Every transmission begins with a START,
  and ends with a STOP so you do not have to specify that.

  sequence is the I2C operation sequence that should be performed. It can include any number of writes, restarts and
  reads. Note that the sequence is composed of uint16_t, not uint8_t. This is because we have to support out-of-band
  signalling of I2C_RESTART and I2C_READ operations, while still passing through 8-bit data.

  sequence_length is the number of sequence elements (not bytes). Sequences of arbitrary length are supported, but
  there is an upper limit on the number of segments (restarts): no more than 42. The minimum sequence length is
  (rather obviously) 2.

  received_data should point to a buffer that can hold as many bytes as there are I2C_READ operations in the
  sequence. If there are no reads, 0 can be passed, as this parameter will not be used.
*/
int i2c_send_sequence(int handle, uint16_t *sequence, uint32_t sequence_length, uint8_t *received_data) {
  struct i2c_rdwr_ioctl_data message_sequence;
  uint32_t number_of_segments = count_segments(sequence, sequence_length);
  struct i2c_msg *messages = malloc(number_of_segments * sizeof(struct i2c_msg));
  struct i2c_msg *current_message = messages;
  /* msg_buf needs to hold all *bytes written* in the entire sequence. Since it is difficult to estimate that number
     without processing the sequence, we make an upper-bound guess: sequence_length. Yes, this is inefficient, but
     optimizing this doesn't seem to be worth the effort. */
  uint8_t *msg_buf = malloc(sequence_length); /* certainly no more than that */
  uint8_t *msg_cur_buf_ptr = msg_buf;
  uint8_t *msg_cur_buf_base;
  uint32_t msg_cur_buf_size;
  uint8_t address;
  uint8_t rw;
  uint32_t i;
  int result = -1;

  if(sequence_length < 2) goto i2c_send_sequence_cleanup;
  if((number_of_segments > I2C_RDRW_IOCTL_MAX_MSGS)) goto i2c_send_sequence_cleanup;

  address = sequence[0];        /* the first byte is always an address */
  rw = address & 1;
  msg_cur_buf_size = 0;
  msg_cur_buf_base = msg_cur_buf_ptr;
  i = 1;

  while(i < sequence_length) {
    if(sequence[i] != I2C_RESTART) {
      /* if we are writing, the only thing in the sequence are bytes to be written */
      if(rw == WRITING) *msg_cur_buf_ptr++ = (uint8_t)(sequence[i]);
      /* for reads, there is nothing to be done, as the only possible thing in the sequence is I2C_READ */
      msg_cur_buf_size++;
    }

    if((sequence[i] == I2C_RESTART) || (i == (sequence_length - 1))) {
      /* segment is complete, fill out the message structure */
      current_message->addr = address >> 1; /* Linux uses 7-bit addresses */
      current_message->flags = rw ? I2C_M_RD : 0;
      current_message->len = msg_cur_buf_size;
      /* buf needs to point to either the buffer that will receive data, or buffer that holds bytes to be written */
      current_message->buf = rw ? received_data : msg_cur_buf_base;
      current_message++;

      if(rw == READING) received_data += msg_cur_buf_size;

      /* do we have another transaction coming? */
      if(i < (sequence_length - 2)) { /* every I2C transaction is at least two bytes long */
        address = sequence[++i];
        rw = address & 1;
        msg_cur_buf_size = 0;
        msg_cur_buf_base = msg_cur_buf_ptr;
      }
    }
    i++;
  }

  message_sequence.msgs = messages;
  message_sequence.nmsgs = number_of_segments;

  result = ioctl(handle, I2C_RDWR, (unsigned long)(&message_sequence));

 i2c_send_sequence_cleanup:
  free(msg_buf);
  free(messages);

  return result;
}


/* This function is just a cosmetic wrapper, added for consistency. */
int i2c_close(int handle) {
  return close(handle);
}
