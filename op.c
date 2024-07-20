#include <string.h>

#include "Headers/array.h"
#include "Headers/op.h"

/*Counts the digits in an int*/
unsigned int count(unsigned int i)
{
  unsigned int ret = 1;


  while (i /= 10)
    ret++;
  return ret;
}

/*Divide a list*/
struct Array *split_arr(struct Array *l, size_t separator1, size_t separator2)
{
  size_t i, j;
  struct Array *res = allocArray();


  initArray(res, l->size);
  for (i = separator1, j = 0; i < separator2; i++, j++) {
    res->array[j] = l->array[i];
    insertArray(res);
  }
  return res;
}

/*Merges two lists*/
struct Array *merge(struct Array *l1, struct Array *l2)
{
  size_t i;
  uint8_t size = l1->size + l2->size;
  struct Array *l = allocArray();


  initArray(l, size);
  for (i = 0; i < l1->used; i++) {
    l->array[i] = l1->array[i];
    insertArray(l);
  }
  for (i = 1; i < l2->used; i++) {
    l->array[l1->used + i] = l2->array[i];
    insertArray(l);
  }
  return l;
}

struct Array *reverse_arr(struct Array *l)
{
  size_t i, j;
  struct Array *res = allocArray();


  initArray(res, l->used);
  for (i = 0, j = l->used; i < l->used; i++, j--) {
    res->array[i] = l->array[j - 1];
    insertArray(res);
  }
  return res;
}

struct Array *copy_arr(struct Array *l1, struct Array *l2)
{
  size_t i;


  initArray(l2, l1->used);
  for (i = 0; i < l1->used; i++) {
    l2[i] = l1[i];
    insertArray(l2);
  }
  return l2;
}

struct Array *pop_arr(struct Array *l)
{
  size_t i;


  for (i = 0; i < l->used; i++)
    l[i] = l[i + 1];
  l->array[l->used] = 0;
  l->used -= 1;
  return l;
}

/* Add two numbers in a GF(2^8) finite field */
uint8_t gf_add(uint8_t x, uint8_t y)
{
  return x ^ y;
}

/* Subtract two numbers in a GF(2^8) finite field */
uint8_t gf_sub(uint8_t x, uint8_t y)
{
  return x ^ y;
}

/* Multiply two numbers in a GF(2^8) finite field */
uint8_t gf_mul(uint8_t x, uint8_t y, struct gf_tables *gf_table)
{
  if (x == 0 || y == 0)
    return 0;
  return gf_table->gf_exp->array[gf_table->gf_log->array[x] + gf_table->gf_log->array[y]];
}

/* Divide two numbers in a GF(2^8) finite field */
uint8_t gf_div(uint8_t x, uint8_t y, struct gf_tables *gf_table)
{
  if (y == 0) {
    fprintf(stderr, "Division by zero! Aborting...\n");
    exit(EXIT_FAILURE);
  }
  if (x == 0)
    return 0;
  return gf_table->gf_exp->
    array[(gf_table->gf_log->array[x] + 255 - gf_table->gf_log->array[y]) % 255];
}

/* Computes the power of a number in a GF(2^8) finite field */
uint8_t gf_pow(uint8_t x, uint16_t power, struct gf_tables *gf_table)
{
  return gf_table->gf_exp->array[(gf_table->gf_log->array[x] * power) % 255];
}

/* Computes the inverse of a number in a GF(2^8) finite field */
uint8_t gf_inverse(uint8_t x, struct gf_tables *gf_table)
{
  return gf_table->gf_exp->array[255 - gf_table->gf_log->array[x]];
}

/*Precompute the logarithm and anti-log tables for faster computation later, using the provided primitive polynomial.*/
struct gf_tables *init_tables()
{
  int i;
  uint32_t x, prim;
  struct gf_tables *gf_table = malloc(sizeof(struct gf_tables));
  struct Array *gf_expp = allocArray();
  struct Array *gf_logg = allocArray();


  initArray(gf_expp, 512);      // Init the exponent table
  initArray(gf_logg, 256);      // Init the log table

  x = 1;
  prim = 0x11d;

  for (i = 0; i < 256; i++) {
    gf_expp->array[i] = x;
    insertArray(gf_expp);
    gf_logg->array[x] = i;
    insertArray(gf_logg);
    x <<= 1;
    if (x & 0x100)
      x ^= prim;
  }
  for (i = 255; i < 512; i++) {
    gf_expp->array[i] = gf_expp->array[i - 255];
    insertArray(gf_expp);
  }
  //set the tables to the struct
  gf_table->gf_exp = gf_expp;
  gf_table->gf_log = gf_logg;

  return gf_table;
}

/* Multiplies a polynomial by a scalar in a GF(2^8) finite field */
struct Array *gf_poly_scale(struct Array *p, uint8_t x, struct gf_tables *gf_table)
{
  size_t i;
  size_t len = p->used;
  struct Array *res = allocArray();


  initZArray(res, len);
  for (i = 0; i < len; i++) {
    uint8_t result = gf_mul(p->array[i], x, gf_table);


    res->array[i] = result;
    insertArray(res);
  }
  return res;
}

/* Adds two polynomials in a GF(2^8) finite field */
struct Array *gf_poly_add(struct Array *p, struct Array *q)
{
  size_t i;
  size_t len = p->used >= q->used ? p->used : q->used;
  struct Array *res = allocArray();


  initZArray(res, len);
  for (i = 0; i < p->used; i++) {
    res->array[i + len - p->used] = p->array[i];
  }
  for (i = 0; i < q->used; i++) {
    res->array[i + len - q->used] ^= q->array[i];
  }
  res->used = len;
  return res;
}

/* Multiplies two polynomials in a GF(2^8) finite field */
struct Array *gf_poly_mul(struct Array *p, struct Array *q, struct gf_tables *gf_table)
{
  size_t i, j;
  struct Array *res = allocArray();


  initZArray(res, (p->used + q->used));
  for (j = 0; j < q->used; j++) {
    for (i = 0; i < p->used; i++) {
      res->array[i + j] = gf_add(res->array[i + j], gf_mul(p->array[i], q->array[j], gf_table));
    }
  }
  res->used = q->used + p->used - 1;
  return res;
}

/*Evaluates a polynomial in GF(2^p) given the value for x. This is based on Horner's scheme for maximum efficiency.*/
uint8_t gf_poly_eval(struct Array *p, uint8_t x, struct gf_tables *gf_table)
{
  size_t i;
  uint8_t y = p->array[0];


  for (i = 1; i < p->used; i++)
    y = gf_mul(y, x, gf_table) ^ p->array[i];
  return y;
}

/*Fast polynomial division by using Extended Synthetic Division and optimized for GF(2^p) computations.*/
struct Tuple *gf_poly_div(struct Array *dividend, struct Array *divisor,
                          struct gf_tables *gf_table)
{
  size_t i;
  struct Tuple *result = malloc(sizeof(struct Tuple));
  size_t length = dividend->used;
  size_t separator = divisor->used - 1;
  struct Array *msg_out = allocArray();
  struct Array *msg_out2 = allocArray();
  struct Array *msg_out3 = allocArray();


  initArray(msg_out, length);
  initArray(msg_out2, length);
  initArray(msg_out3, length);
  memmove(msg_out->array, dividend->array, dividend->used);

  for (i = 0; i < dividend->used - divisor->used + 1; i++) {
    size_t j;
    uint8_t coef = msg_out->array[i];


    if (coef != 0) {
      for (j = 1; j < divisor->used; j++) {
        msg_out->array[i + j] ^= gf_mul(divisor->array[j], coef, gf_table);
      }
    }
  }
  msg_out->used = divisor->used + dividend->used - 1;

  memmove(msg_out2->array, msg_out->array, (msg_out->used - separator));
  msg_out2->used = msg_out->used - separator;
  msg_out3->array = msg_out->array + (msg_out->used - separator);
  msg_out3->used = separator;
  result->x = msg_out2;
  result->y = msg_out3;

  return result;

}
