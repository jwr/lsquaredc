/*
  example.c

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
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "lsquaredc.h"

/*
  An example of how to use the lsquaredc library. The code below talks to a SFH7773 light/proximity sensor. It opens the
  I2C bus (number 1, so /dev/i2c-1), issues two init sequences, and then performs a part number query in a repeated
  start I2C transaction, reading one byte from the device.
*/
int main(void) {
  uint16_t init_sequence1[] = {0x70, 0x80, 3};
  uint16_t init_sequence2[] = {0x70, 0x81, 3};
  uint16_t pn_query[] = {0x70, 0x8a, I2C_RESTART, 0x71, I2C_READ};
  uint8_t status;
  int i2c_handle;
  int result;

  i2c_handle = i2c_open(1);

  printf("Opened bus, result=%d\n", i2c_handle);
  result = i2c_send_sequence(i2c_handle, init_sequence1, 3, 0);
  printf("Sequence processed, result=%d\n", result);
  result = i2c_send_sequence(i2c_handle, init_sequence2, 3, 0);
  printf("Sequence processed, result=%d\n", result);
  result = i2c_send_sequence(i2c_handle, pn_query, 5, &status);
  printf("Sequence processed, result=%d\n", result);
  printf("Status=%d\n", (int)(status));

  i2c_close(i2c_handle);

  return 0;
}
