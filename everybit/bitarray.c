/**
 * Copyright (c) 2012 MIT License by 6.172 Staff
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 **/

// Implements the ADT specified in bitarray.h as a packed array of bits; a bit
// array containing bit_sz bits will consume roughly bit_sz/8 bytes of
// memory.


#include "./bitarray.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include <sys/types.h>


// ********************************* Types **********************************

// Concrete data type representing an array of bits.
struct bitarray {
  // The number of bits represented by this bit array.
  // Need not be divisible by 8.
  size_t bit_sz;

  // The underlying memory buffer that stores the bits in
  // packed form (8 per byte).
  char *buf;
};


// ******************** Prototypes for static functions *********************

// Rotates a subarray left by an arbitrary number of bits.
//
// bit_offset is the index of the start of the subarray
// bit_length is the length of the subarray, in bits
// bit_left_amount is the number of places to rotate the
//                    subarray left
//
// The subarray spans the half-open interval
// [bit_offset, bit_offset + bit_length)
// That is, the start is inclusive, but the end is exclusive.
static void bitarray_rotate_left(bitarray_t *const bitarray,
                                 const size_t bit_offset,
                                 const size_t bit_length,
                                 const size_t bit_left_amount);

// Rotates a subarray left by one bit.
//
// bit_offset is the index of the start of the subarray
// bit_length is the length of the subarray, in bits
//
// The subarray spans the half-open interval
// [bit_offset, bit_offset + bit_length)
// That is, the start is inclusive, but the end is exclusive.
static void bitarray_rotate_left_one(bitarray_t *const bitarray,
                                     const size_t bit_offset,
                                     const size_t bit_length);

// Portable modulo operation that supports negative dividends.
//
// Many programming languages define modulo in a manner incompatible with its
// widely-accepted mathematical definition.
// http://stackoverflow.com/questions/1907565/c-python-different-behaviour-of-the-modulo-operation
// provides details; in particular, C's modulo
// operator (which the standard calls a "remainder" operator) yields a result
// signed identically to the dividend e.g., -1 % 10 yields -1.
// This is obviously unacceptable for a function which returns size_t, so we
// define our own.
//
// n is the dividend and m is the divisor
//
// Returns a positive integer r = n (mod m), in the range
// 0 <= r < m.
static size_t modulo(const ssize_t n, const size_t m);

// Produces a mask which, when ANDed with a byte, retains only the
// bit_index th byte.
//
// Example: bitmask(5) produces the byte 0b00100000.
//
// (Note that here the index is counted from right
// to left, which is different from how we represent bitarrays in the
// tests.  This function is only used by bitarray_get and bitarray_set,
// however, so as long as you always use bitarray_get and bitarray_set
// to access bits in your bitarray, this reverse representation should
// not matter.
static char bitmask(const size_t bit_index);


// ******************************* Functions ********************************

bitarray_t *bitarray_new(const size_t bit_sz) {
  // Allocate an underlying buffer of ceil(bit_sz/8) bytes.
  char *const buf = calloc(1, bit_sz / 8 + ((bit_sz % 8 == 0) ? 0 : 1));
  if (buf == NULL) {
    return NULL;
  }

  // Allocate space for the struct.
  bitarray_t *const bitarray = malloc(sizeof(struct bitarray));
  if (bitarray == NULL) {
    free(buf);
    return NULL;
  }

  bitarray->buf = buf;
  bitarray->bit_sz = bit_sz;
  return bitarray;
}

void bitarray_free(bitarray_t *const bitarray) {
  if (bitarray == NULL) {
    return;
  }
  free(bitarray->buf);
  bitarray->buf = NULL;
  free(bitarray);
}

size_t bitarray_get_bit_sz(const bitarray_t *const bitarray) {
  return bitarray->bit_sz;
}

bool bitarray_get(const bitarray_t *const bitarray, const size_t bit_index) {
  assert(bit_index < bitarray->bit_sz);

  // We're storing bits in packed form, 8 per byte.  So to get the nth
  // bit, we want to look at the (n mod 8)th bit of the (floor(n/8)th)
  // byte.
  //
  // In C, integer division is floored explicitly, so we can just do it to
  // get the byte; we then bitwise-and the byte with an appropriate mask
  // to produce either a zero byte (if the bit was 0) or a nonzero byte
  // (if it wasn't).  Finally, we convert that to a boolean.
  return (bitarray->buf[bit_index / 8] & bitmask(bit_index)) ?
             true : false;
}

void bitarray_set(bitarray_t *const bitarray,
                  const size_t bit_index,
                  const bool value) {
  assert(bit_index < bitarray->bit_sz);

  // We're storing bits in packed form, 8 per byte.  So to set the nth
  // bit, we want to set the (n mod 8)th bit of the (floor(n/8)th) byte.
  //
  // In C, integer division is floored explicitly, so we can just do it to
  // get the byte; we then bitwise-and the byte with an appropriate mask
  // to clear out the bit we're about to set.  We bitwise-or the result
  // with a byte that has either a 1 or a 0 in the correct place.
  bitarray->buf[bit_index / 8] =
      (bitarray->buf[bit_index / 8] & ~bitmask(bit_index)) |
           (value ? bitmask(bit_index) : 0);
}

ssize_t get_shift(const size_t bit_length, const ssize_t bit_right_amount) {
  ssize_t result = modulo(bit_right_amount, bit_length);
  if(result < 0){
    result = result + bit_length;
  }
  return result;
}

size_t get_char_index(const size_t bit_offset){
  return bit_offset / 8;
}

size_t get_end_char_index(const size_t bit_offset, const size_t bit_length){
  return get_char_index(bit_offset + bit_length - 1);
}

size_t get_end_bit(const size_t bit_offset, const size_t bit_length){
  return (8 - (bit_offset + bit_length)%8) % 8;
}

static const char end_mask[] = {
  0xFF, 0xFE, 0xFC, 0xF8, 0xF0, 0xE0, 0xC0, 0x80
};

static const char begin_mask[] = {
  0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F
};

static const char left_mask[] = {
  0xFF, 0x7F, 0x3F, 0x1F, 0x0F, 0x07, 0x03, 0x01, 0x00
};

static const char right_mask[] = {
  0xFF, 0xFE, 0xFC, 0xF8, 0xF0, 0xE0, 0xC0, 0x80, 0x00
};

char clean_left(char c, const size_t bit_count){
  return c & left_mask[bit_count];
}

char clean_right(char c, const size_t bit_count){
  return c & right_mask[bit_count];
}

void rotate_single(bitarray_t *const bitarray, const size_t charIndex,
                    const size_t beginBit,
                    const size_t endBit,
                    const ssize_t shiftBit){
  unsigned char c = bitarray->buf[charIndex];
  printf("c: %02x\n", c);
  printf("shift: %zu\n", shiftBit);
  printf("beginBit: %zu\n", beginBit);
  printf("endBit: %zu\n", endBit);

  // 2, 4, 2
  // case: 11010111
  // case: 00000010, 0, 6, 1

  // 1

  char c1 = clean_left(c, endBit);
  printf("c1: %02x\n", c1);
  c1 = clean_right(c1, beginBit + shiftBit);
  printf("c1: %02x\n", c1);
  c1 = c1 >> shiftBit;
  printf("c1: %02x\n", c1);

  unsigned char c2 = clean_left(c, (8 - (beginBit + shiftBit)));
  printf("c2: %02x\n", c2);
  c2 = clean_right(c2, beginBit);
  printf("c2: %02x\n", c2);

  size_t left_shift = ((8 - beginBit - shiftBit) - endBit);
  printf("left shift: %zu\n", left_shift);
  c2 = ((unsigned char) ((unsigned char) c2 << left_shift)) % 256;
  printf("c2: %02x\n", c2);

  char c3 = clean_left(c, 8 - beginBit);
  printf("c3: %02x\n", c3);

  char c4 = clean_right(c, 8 - endBit);
  printf("c4: %02x\n", c4);

  c = c1 | c2 | c3 | c4;

  printf("c: %02x\n", c);
  bitarray->buf[charIndex] = c;
}



void bitarray_rotate(bitarray_t *const bitarray,
                     const size_t bit_offset,
                     const size_t bit_length,
                     const ssize_t bit_right_amount) {

  if(bit_offset + bit_length > bitarray->bit_sz){
    return;
  }

  if(bit_length == 0 || bit_length == 1){
    return;
  }

  const ssize_t shift = get_shift(bit_length, bit_right_amount);
  if(shift == 0){
    return;
  }

  const ssize_t shiftByte = shift / 8;
  const ssize_t shiftBit = shift %8;

  const size_t beginCharIndex = get_char_index(bit_offset);
  const size_t endCharIndex = get_char_index(bit_offset + bit_length - 1);

  const size_t beginBit = bit_offset%8;
  const size_t endBit = get_end_bit(bit_offset, bit_length);

  if(beginCharIndex == endCharIndex){
    rotate_single(bitarray, beginCharIndex, beginBit, endBit, shiftBit);
  }

}

void bitarray_rotate_old(bitarray_t *const bitarray,
                     const size_t bit_offset,
                     const size_t bit_length,
                     const ssize_t bit_right_amount) {
  assert(bit_offset + bit_length <= bitarray->bit_sz);

  if (bit_length == 0) {
    return;
  }

  // Convert a rotate left or right to a left rotate only, and eliminate
  // multiple full rotations.
  bitarray_rotate_left(bitarray, bit_offset, bit_length,
           modulo(-bit_right_amount, bit_length));
}

static void bitarray_rotate_left(bitarray_t *const bitarray,
                                 const size_t bit_offset,
                                 const size_t bit_length,
                                 const size_t bit_left_amount) {
  for (size_t i = 0; i < bit_left_amount; i++) {
    bitarray_rotate_left_one(bitarray, bit_offset, bit_length);
  }
}

static void bitarray_rotate_left_one(bitarray_t *const bitarray,
                                     const size_t bit_offset,
                                     const size_t bit_length) {
  // Grab the first bit in the range, shift everything left by one, and
  // then stick the first bit at the end.
  const bool first_bit = bitarray_get(bitarray, bit_offset);
  size_t i;
  for (i = bit_offset; i + 1 < bit_offset + bit_length; i++) {
    bitarray_set(bitarray, i, bitarray_get(bitarray, i + 1));
  }
  bitarray_set(bitarray, i, first_bit);
}

static size_t modulo(const ssize_t n, const size_t m) {
  const ssize_t signed_m = (ssize_t)m;
  assert(signed_m > 0);
  const ssize_t result = ((n % signed_m) + signed_m) % signed_m;
  assert(result >= 0);
  return (size_t)result;
}

void myassert(char * msg, bool result){
  if(!result){
    printf("%s Failed!\n", msg);
  } else {
    printf("%s Passed!\n", msg);
  }
}

void test_mod(){
  myassert("mod1", modulo(16, 8) == 0);
  myassert("mod2", modulo(0, 8) == 0);
}

void test_get_shift(){
  myassert("getShift1", get_shift(2, 10) == 0);
  myassert("getShift2", get_shift(5, 1) == 1);
  myassert("getShift3", get_shift(5, -1) == 4);
  myassert("getShift4", get_shift(5, -7) == 3);
}

void print_bit_array(bitarray_t* ba){
  size_t bit_sz = ba->bit_sz;
  printf("bit_sz = %zu\n", bit_sz);
  size_t length = bit_sz / 8 + ((bit_sz % 8 == 0) ? 0 : 1);
  printf("length = %zu\n", length);
  for(int i=0;i<length;i++){
    printf("%02x\n", (unsigned char) ba->buf[i]);
  }
}

void test_bitarray(){
  bitarray_t* ba1 = bitarray_new(16);
  bitarray_set(ba1, 1, 1);

  myassert("bitarray1", ((unsigned char) ba1->buf[0]) == 2);
  myassert("bitarray2", ((unsigned char) ba1->buf[1]) == 0);

  bitarray_set(ba1, 14, 1);
  myassert("bitarray3", ((unsigned char) ba1->buf[1]) == 64);
}

void test_bitarray_rotate(){
  bitarray_t* ba1 = bitarray_new(7);
  bitarray_set(ba1, 1, 1);

  bitarray_rotate(ba1, 0, 0, 0);
  myassert("rotate1", ba1->buf[0] == 2);

  bitarray_rotate(ba1, 0, 8, 1);
  myassert("rotate2", ba1->buf[0] == 2);

  bitarray_rotate(ba1, 0, 7, 0);
  myassert("rotate3", ba1->buf[0] == 2);

  bitarray_rotate(ba1, 0, 1, 1);
  myassert("rotate4", ba1->buf[0] == 2);

  bitarray_rotate(ba1, 0, 7, 1);
  myassert("rotate5", ba1->buf[0] == 1);

  //00 000011
  //00 100001
  bitarray_t* ba2 = bitarray_new(6);
  bitarray_set(ba2, 0, 1);
  bitarray_set(ba2, 1, 1);
  bitarray_rotate(ba2, 0, 6, 1);
  myassert("rotate6", ba2->buf[0] == 33);

  // 01 0101 10
  // 01 1010 10
  bitarray_t* ba3 = bitarray_new(8);
  ba3->buf[0] = 0x56;
  bitarray_rotate(ba3, 2, 4, 1);
  myassert("rotate7", ((unsigned char) ba3->buf[0]) == 0x6A);

  // 010101 10
  // 101010 10
  bitarray_t* ba4 = bitarray_new(8);
  ba4->buf[0] = 0x56;
  bitarray_rotate(ba4, 2, 6, 1);
  myassert("rotate7", ((unsigned char) ba4->buf[0]) == 0xAA);

  // 010101 01
  // 101010 10
  bitarray_t* ba5 = bitarray_new(16);
  ba5->buf[1] = 0x55;

  bitarray_rotate(ba5, 8, 8, 11);
  myassert("rotate8", ((unsigned char) ba5->buf[1]) == 0xAA);

  // 010101 01
  // 101010 10
  bitarray_t* ba6 = bitarray_new(16);
  ba6->buf[1] = 0x55;

  bitarray_rotate(ba6, 8, 8, -11);
  myassert("rotate8", ((unsigned char) ba6->buf[1]) == 0xAA);

}

void test_get_end_char_index(){
  myassert("get_end_char1", get_end_char_index(15, 1) == 1);
  myassert("get_end_char2", get_end_char_index(15, 2) == 2);
  myassert("get_end_char3", get_end_char_index(15, 9) == 2);
  myassert("get_end_char4", get_end_char_index(15, 10) == 3);
}

void test_get_char_index(){
  myassert("get_begin_char1", get_char_index(7) == 0);
  myassert("get_begin_char2", get_char_index(12) == 1);
  myassert("get_begin_char3", get_char_index(15) == 1);
  myassert("get_begin_char3", get_char_index(16) == 2);
}

void test_get_end_bit(){
  myassert("test_get_end_bit1", get_end_bit(0, 6) == 2);
  myassert("test_get_end_bit2", get_end_bit(0, 9) == 7);
  myassert("test_get_end_bit3", get_end_bit(0, 18) == 6);

  myassert("test_get_end_bit4", get_end_bit(1, 9) == 6);
}

void test_clean_left(){
  char c = 0xFF;
  myassert("clean_left1", ((unsigned char) clean_left(c, 1)) == 0x7F);
  myassert("clean_left2", ((unsigned char) clean_left(c, 6)) == 0x03);
}

void test_clean_right(){
  char c = 0xFF;
  myassert("clean_right1", ((unsigned char) clean_right(c, 1)) == 0xFE);
  myassert("clean_right1", ((unsigned char) clean_right(c, 6)) == 0xC0);
}

static char bitmask(const size_t bit_index) {
  return 1 << (bit_index % 8);
}

void run_unit(){
  test_mod();
  test_get_shift();
  test_bitarray();
  test_bitarray_rotate();
  test_get_char_index();
  test_get_end_char_index();
  test_get_end_bit();
  test_clean_left();
  test_clean_right();
}
