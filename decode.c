#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "Headers/array.h"
#include "Headers/op.h"

/*Given the received codeword msg and the number of error correcting symbols (nsym), computes the
 syndromes polynomial. Mathematically, it's essentially equivalent to a Fourrier Transform
 (Chien search being the inverse).*/

struct Array *rs_calc_syndromes(struct Array *msg, uint8_t nsym, struct gf_tables *gf_table)
{
  uint8_t i;
  struct Array *synd = allocArray();
  struct Array *res = allocArray();


  initZArray(synd, nsym + 1);
  initZArray(res, nsym + 2);

  for (i = 0; i < nsym; i++) {
    synd->array[i] = gf_poly_eval(msg, gf_pow(2, i, gf_table), gf_table);
    insertArray(synd);
  }
  insertArray(res);
  for (i = 1; i < nsym + 1; i++) {
    res->array[i] = synd->array[i - 1]; //pad with one 0 for mathematical precision
    insertArray(res);
  }
  //(else we can end up with weird calculations sometimes)
  freeArray(synd);
  return res;
}

/*Returns true if the message + ecc has no error of false otherwise (may not always catch a wrong
 decoding or a wrong message,particularly if there are too many errors, but it usually does)*/
bool rs_check(struct Array *msg, uint8_t nsym, struct gf_tables *gf_table)
{
  uint8_t i;
  bool res;
  struct Array *synd = allocArray();


  initArray(synd, nsym + 1);
  synd = rs_calc_syndromes(msg, nsym, gf_table);
  for (i = 0, res = true; i < nsym + 1; i++)
    res &= synd->array[i] == 0;
  freeArray(synd);
  return res;
}

struct Array *rs_find_errdata_locator(struct Array *e_pos, struct gf_tables *gf_table)
{
  size_t i;
  struct Array *e_loc = allocArray();
  struct Array *one = allocArray();


  initZArray(e_loc, e_pos->used + 1);
  e_loc->array[0] = 1;
  insertArray(e_loc);
  initZArray(one, 2);
  one->array[0] = 1;
  insertArray(one);
  for (i = 0; i < e_pos->used; i++) {
    uint8_t pow_two = gf_pow(2, e_pos->array[i], gf_table);
    struct Array *add;
    struct Array *arr = allocArray();


    initArray(arr, 2);
    arr->array[0] = pow_two;
    insertArray(arr);
    arr->array[1] = 0;
    insertArray(arr);
    add = gf_poly_add(one, arr);
    e_loc = gf_poly_mul(e_loc, add, gf_table);
    freeArray(arr);
  }
  freeArray(one);
  return e_loc;
}

/*Compute the error (or erasures if you supply sigma=erasures locator polynomial, or errata) evaluator
 polynomial Omega from the syndrome and the error/erasures/errata locator Sigma*/
struct Array *rs_find_error_evaluator(struct Array *synd, struct Array *err_loc, uint8_t nsym,
                                      struct gf_tables *gf_table)
{
  size_t len;
  struct Array *res = allocArray();


  initArray(res, nsym + 2);
  res = gf_poly_mul(synd, err_loc, gf_table);
  len = res->used - (nsym + 1);
  memmove(res->array, res->array + len, len);
  res->used = res->used - len;
  return res;
}

/*Forney algorithm, computes the values (error magnitude) to correct the input message.*/
struct Array *rs_correct_errdata(struct Array *msg_in, struct Array *synd,
                                 struct Array *err_pos, struct gf_tables *gf_table)
{
  size_t i, t;
  uint8_t nsym;
  size_t len = msg_in->size;
  struct Array *coef_pos = allocArray();
  struct Array *err_loc = allocArray();
  struct Array *err_eval = allocArray();
  struct Array *X = allocArray();
  struct Array *E = allocArray();
  struct Array *rev_synd = reverse_arr(synd);


  initArray(coef_pos, err_pos->used + 1);

  for (i = 0; i < err_pos->used; i++) {
    coef_pos->array[i] = len - 1 - err_pos->array[i];
    insertArray(coef_pos);
  }

  initZArray(E, len);

  initArray(err_loc, coef_pos->used + 1);
  err_loc = rs_find_errdata_locator(coef_pos, gf_table);

  nsym = err_loc->used - 1;
  err_eval = rs_find_error_evaluator(rev_synd, err_loc, nsym, gf_table);
  err_eval = reverse_arr(err_eval);
  initArray(X, coef_pos->used);

  for (t = 0; t < coef_pos->used; t++) {
    uint8_t q = coef_pos->array[t];


    X->array[t] = gf_pow(2, q, gf_table);
    insertArray(X);
  }

  E->used = len;
  for (i = 0; i < X->used; i++) {
    size_t j, k;
    uint8_t y, err_loc_prime, magnitude;
    struct Array *err_loc_prime_tmp = allocArray();
    uint8_t Xi_inv = gf_inverse(X->array[i], gf_table);


    initArray(err_loc_prime_tmp, X->used + 1);
    for (j = 0; j < X->used; j++) {
      if (j != i) {
        err_loc_prime_tmp->array[err_loc_prime_tmp->used] =
          gf_sub(1, gf_mul(Xi_inv, X->array[j], gf_table));
        insertArray(err_loc_prime_tmp);
      }
    }
    err_loc_prime = 1;
    for (k = 0; k < err_loc_prime_tmp->used; k++)
      err_loc_prime = gf_mul(err_loc_prime, err_loc_prime_tmp->array[k], gf_table);

    y = gf_poly_eval(reverse_arr(err_eval), Xi_inv, gf_table);
    y = gf_mul(gf_pow(X->array[i], 1, gf_table), y, gf_table);
    magnitude = gf_div(y, err_loc_prime, gf_table);
    E->array[err_pos->array[i]] = magnitude;
    freeArray(err_loc_prime_tmp);
  }
  msg_in = gf_poly_add(msg_in, E);
  freeArray(coef_pos);
  freeArray(err_loc);
  freeArray(rev_synd);
  freeArray(err_eval);
  return msg_in;
}

struct Array *rs_find_error_locator(struct Array *synd, uint8_t nsym, uint8_t erase_count,
                                    struct gf_tables *gf_table)
{
  int i;
  size_t errs;
  size_t synd_shift = 0;
  struct Array *err_loc = allocArray();
  struct Array *old_loc = allocArray();


  initZArray(err_loc, 10);
  initZArray(old_loc, 10);
  err_loc->array[0] = 1;
  insertArray(err_loc);
  old_loc->array[0] = 1;
  insertArray(old_loc);

  if (synd->used > nsym)
    synd_shift = synd->used - nsym;

  for (i = 0; i < nsym - erase_count; i++) {
    size_t j;
    size_t K = i + synd_shift;
    uint8_t delta = synd->array[K];


    for (j = 1; j < err_loc->used; j++) {
      delta ^= gf_mul(err_loc->array[err_loc->used - (j + 1)], synd->array[K - j], gf_table);
    }
    old_loc->array[old_loc->used + 1] = 0;
    insertArray(old_loc);
    if (delta != 0) {
      struct Array *scale = allocArray();


      if (old_loc->used > err_loc->used) {
        struct Array *new_loc = allocArray();


        new_loc = gf_poly_scale(old_loc, delta, gf_table);
        old_loc = gf_poly_scale(err_loc, gf_inverse(delta, gf_table), gf_table);
	while (err_loc->size < new_loc->used)
	  insertArray(err_loc);
	memmove(err_loc->array, new_loc->array, err_loc->used);
        err_loc->used = new_loc->used;
      }

      scale = gf_poly_scale(old_loc, delta, gf_table);
      err_loc = gf_poly_add(err_loc, scale);
    }
  }
  while (err_loc->used && err_loc->array[0] == 0)
    err_loc = pop_arr(err_loc);
  errs = err_loc->used - 1;
  if (errs - erase_count * 2 + erase_count > nsym) {
    fprintf(stderr, "Too many errors to correct");
    exit(EXIT_FAILURE);
  }
  freeArray(old_loc);
  return err_loc;
}

struct Array *rs_find_errors(struct Array *err_loc, size_t nmess, struct gf_tables *gf_table)   //nmess is len(msg_in)
{
  size_t i;
  size_t errs = err_loc->used - 1;
  size_t counter = 0;
  struct Array *err_pos = allocArray();


  initArray(err_pos, errs);
  for (i = 0; i < nmess; i++) {
    if (gf_poly_eval(err_loc, gf_pow(2, i, gf_table), gf_table) == 0) {
      err_pos->array[counter] = nmess - 1 - i;
      insertArray(err_pos);
      counter++;
    }
  }
  if (err_pos->used != errs) {
    fprintf(stderr,
            "Too many (or few) errors found by Chien Search"
	    " for the errdata locator polynomial!\n");
    exit(EXIT_FAILURE);
  }
  return err_pos;
}

struct Array *rs_forney_syndromes(struct Array *synd, struct Array *pos, uint8_t nmess,
                                  struct gf_tables *gf_table)
{
  size_t i;
  struct Array *erase_pos_reversed = allocArray();
  struct Array *fsynd = allocArray();


  initArray(erase_pos_reversed, pos->used);
  for (i = 0; i < pos->used; i++) {
    erase_pos_reversed->array[i] = nmess - 1 - pos->array[i];
  }
  erase_pos_reversed->used = pos->used;

  initArray(fsynd, synd->used);
  memmove(fsynd->array, synd->array + 1, synd->used - 1);
  fsynd->used = synd->used - 1;
  for (i = 0; i < pos->used; i++) {
    size_t j;
    uint8_t x = gf_pow(2, erase_pos_reversed->array[i], gf_table);


    for (j = 0; j < fsynd->used - 1; j++)
      fsynd->array[j] = gf_mul(fsynd->array[j], x, gf_table) ^ fsynd->array[j + 1];
  }
  freeArray(erase_pos_reversed);
  return fsynd;
}

struct Array *rs_correct_msg(struct Array *msg_in, uint8_t nsym, struct Array *erase_pos,
                             struct gf_tables *gf_table)
{
  size_t i;
  uint8_t max;
  struct Array *synd, *err_loc, *err_pos;
  struct Array *msg_out = allocArray();


  if (msg_in->used > 255) {
    fprintf(stderr, "Message is too long ");
    exit(EXIT_FAILURE);
  }

  initArray(msg_out, msg_in->used);
  memmove(msg_out->array, msg_in->array, msg_in->used);

  synd = rs_calc_syndromes(msg_in, nsym, gf_table);
  max = synd->array[0];
  for (i = 0; i < synd->used; i++) {
    max = synd->array[i] > max ? synd->array[i] : max;
  }
  if (max == 0) {       //No errors
    return msg_out;
  }
  //struct Array *fsynd = rs_forney_syndromes(synd, erase_pos, msg_out->used, gf_table);
  err_loc = rs_find_error_locator(synd, nsym, 0, gf_table);
  err_pos = rs_find_errors(reverse_arr(err_loc), msg_in->used, gf_table);
  if (err_pos == NULL) {
    fprintf(stderr, "Could not locate error");
    exit(EXIT_FAILURE);
  }
  msg_out = rs_correct_errdata(msg_in, synd, err_pos, gf_table);
  synd = rs_calc_syndromes(msg_out, nsym, gf_table);
  max = synd->array[0];
  for (i = 0; i < synd->used; i++) {
    max = synd->array[i] > max ? synd->array[i] : max;
  }
  if (max > 0) {        //Couldn't correct
    fprintf(stderr, "Could not correct message");
    exit(EXIT_FAILURE);
  }
  freeArray(synd);
  freeArray(err_loc);
  freeArray(err_pos);
  return msg_out;
}
