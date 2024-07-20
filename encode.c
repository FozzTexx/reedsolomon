#include <string.h>

#include "Headers/array.h"
#include "Headers/op.h"

struct Array *array2(uint8_t a, uint8_t b)
{
  struct Array *p = allocArray();


  initArray(p, 3);
  p->array[0] = a;
  p->array[1] = b;
  p->used = 2;
  return p;
}

/*Generate an irreducible generator polynomial (necessary to encode a message into Reed-Solomon)*/
struct Array *rs_generator_poly(uint8_t nsym, struct gf_tables *gf_table)
{
  uint8_t i;
  struct Array *g = allocArray();


  initArray(g, nsym);
  g->array[0] = 1;
  insertArray(g);
  for (i = 0; i < nsym; i++)
    g = gf_poly_mul(g, array2(1, gf_pow(2, i, gf_table)), gf_table);
  return g;
}

/*Reed-Solomon main encoding function*/
struct Array *rs_encode_msg(struct Array *msg_in, uint8_t nsym, struct gf_tables *gf_table)
{
  size_t i, len_gen;
  struct Array *gen = allocArray();
  struct Array *msg_out = allocArray();


  if (msg_in->used + nsym > 255) {
    fprintf(stderr, "Message too long, %u is the size when 255 is the max",
            msg_in->used + nsym);
    exit(EXIT_FAILURE);
  }
  len_gen = nsym * 2;
  initArray(gen, len_gen);

  gen = rs_generator_poly(nsym, gf_table);
  initZArray(msg_out, msg_in->used + gen->used - 1);
  memmove(msg_out->array, msg_in->array, msg_in->used);

  for (i = 0; i < msg_in->used; i++) {
    size_t j;
    uint8_t coef = msg_out->array[i];


    if (coef != 0) {
      for (j = 1; j < gen->used; j++) {
        msg_out->array[i + j] ^= gf_mul(gen->array[j], coef, gf_table);
      }
    }
  }
  msg_out->used = gen->used + msg_in->used - 1;

  memmove(msg_out->array, msg_in->array, msg_in->used);
  freeArray(gen);
  return msg_out;
}
