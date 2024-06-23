#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../util/measure.h"

const size_t PAGE_SIZE = 4 * 1024;

struct Matrix {
  // Data is stowed in row-major order.
  double* data;
  size_t width;
  size_t height;
};

Matrix MakeMatrix(size_t width, size_t height) {
  const size_t n = width * height;
  double* const data = (double*)aligned_alloc(PAGE_SIZE, n * sizeof(double));
  for (size_t row = 0; row < 1024; row++) {
    for (size_t col = 0; col < 1024; col++) {
      data[row * 1024 + col] = 1.0 + (row * 1024 + col) / (1024. * 1024.);
    }
  }
  return { data, width, height };
}

double Checksum(Matrix m) {
  double sum = 0;
  for (size_t i = 0; i < m.width * m.height; ++i) {
    sum += m.data[i];
  }
  return sum;
}

// c = a*b
//
// Returns the number of seconds taken.
double SimpleMatMul(Matrix a, Matrix b, Matrix c) {
  assert(a.width == b.height);
  assert(a.height == c.height);
  assert(b.width == c.width);

  clock_t start = clock();
  for (size_t row = 0; row < a.height; ++row) {
    for (size_t col = 0; col < b.width; ++col) {
      double sum = 0;
      for (size_t k = 0; k < a.width; ++k) {
        sum += a.data[row * a.width + k] * b.data[k * b.width + col];
      }
      c.data[row * c.width + col] = sum;
    }
  }
  clock_t stop = clock();
  return (stop - start) * 1. / CLOCKS_PER_SEC;
}

// c = a*b
//
// Returns the number of seconds taken.
double SimpleMatMulColumnwise(Matrix a, Matrix b, Matrix c) {
  assert(a.width == b.height);
  assert(a.height == c.height);
  assert(b.width == c.width);

  clock_t start = clock();
  for (size_t col = 0; col < b.width; ++col) {
    for (size_t row = 0; row < a.height; ++row) {
      double sum = 0;
      for (size_t k = 0; k < a.width; ++k) {
        sum += a.data[row * a.width + k] * b.data[k * b.width + col];
      }
      c.data[row * c.width + col] = sum;
    }
  }
  clock_t stop = clock();
  return (stop - start) * 1. / CLOCKS_PER_SEC;
}

// Returns the transpose of the given matrix.
// Involves a malloc.
Matrix Transposed(Matrix a) {
  const size_t n = a.width * a.height;
  double* const data = (double*)aligned_alloc(PAGE_SIZE, n * sizeof(double));
  Matrix ret = {data, a.height, a.width};
  for (size_t y = 0; y < a.height; y++) {
    for (size_t x = 0; x < a.width; x++) {
      double val = a.data[y * a.width + x];
      ret.data[x * a.height + y] = val;
    }
  }
  return ret;
}

// Returns the transpose of the given matrix.
// Involves a malloc.
//
// Only works on matrices with 8-divisible width and height.
Matrix Transposed8x8(Matrix a) {
  assert(a.height % 8 == 0);
  assert(a.width % 8 == 0);

  double* const data = (double*)aligned_alloc(PAGE_SIZE, a.width * a.height * sizeof(double));
  Matrix ret = {data, a.height, a.width};

  const size_t block_w = a.width / 8;
  const size_t block_h = a.height / 8;
  for (size_t a_block_y = 0; a_block_y < block_h; a_block_y++) {
    for (size_t a_block_x = 0; a_block_x < block_w; a_block_x++) {
      for (size_t row = 0; row < 8; ++row) {
        for (size_t col = 0; col < 8; ++col) {
          const size_t a_x = a_block_x * 8 + col;
          const size_t a_y = a_block_y * 8 + row;
          const double val = a.data[a_y * a.width + a_x];
          const size_t ret_x = a_y;
          const size_t ret_y = a_x;
          ret.data[ret_y * ret.width + ret_x] = val;
        }
      }
    }
  }
  return ret;
}

// Returns the transpose of the given matrix.
// Involves a malloc.
//
// Only works on matrices with 8-divisible width and height.
Matrix Transposed8x8Flipped(Matrix a) {
  assert(a.height % 8 == 0);
  assert(a.width % 8 == 0);

  double* const data = (double*)aligned_alloc(PAGE_SIZE, a.width * a.height * sizeof(double));
  Matrix ret = {data, a.height, a.width};

  const size_t block_w = a.width / 8;
  const size_t block_h = a.height / 8;
  for (size_t a_block_y = 0; a_block_y < block_h; a_block_y++) {
    for (size_t a_block_x = 0; a_block_x < block_w; a_block_x++) {
      for (size_t col = 0; col < 8; ++col) {
        for (size_t row = 0; row < 8; ++row) {
          const size_t a_x = a_block_x * 8 + col;
          const size_t a_y = a_block_y * 8 + row;
          const double val = a.data[a_y * a.width + a_x];
          const size_t ret_x = a_y;
          const size_t ret_y = a_x;
          ret.data[ret_y * ret.width + ret_x] = val;
        }
      }
    }
  }
  return ret;
}

// c = a*b
//
// Returns the number of seconds taken.
double TransposeMatMul(Matrix a, Matrix b, Matrix* c) {
  assert(a.width == b.height);
  assert(a.height == c->height);
  assert(b.width == c->width);

  clock_t start = clock();
  Matrix bb = Transposed8x8Flipped(b);
  Matrix cc = {c->data, c->height, c->width};
  for (size_t col = 0; col < b.width; ++col) {
    for (size_t row = 0; row < a.height; ++row) {
      double sum = 0;
      for (size_t k = 0; k < a.width; ++k) {
        sum += a.data[row * a.width + k] * bb.data[col * bb.width + k];
      }
      cc.data[row * cc.width + col] = sum;
    }
  }
  Matrix ccc = Transposed8x8Flipped(cc);
  free(c->data);
  c->data = ccc.data;
  free(bb.data);

  clock_t stop = clock();
  return (stop - start) * 1. / CLOCKS_PER_SEC;
}

// Map from row-major to 32x32block-major.
Matrix BlockRemap(Matrix a) {
  assert(a.height % 32 == 0);
  assert(a.width % 32 == 0);

  double* const data = (double*)aligned_alloc(PAGE_SIZE, a.width * a.height * sizeof(double));
  Matrix ret = {data, a.height, a.width};

  const size_t block_w = a.width  / 32;
  const size_t block_h = a.height / 32;
  const size_t block_size = 32 * 32;
  for (size_t a_block_y = 0; a_block_y < block_h; ++a_block_y) {
    for (size_t a_block_x = 0; a_block_x < block_w; ++a_block_x) {
      const size_t ret_block = a_block_y * block_w + a_block_x;
      double* const block_data = ret.data + ret_block * block_size;

      for (size_t row = 0; row < 32; ++row) {
        const size_t a_y = a_block_y * 32 + row;
        double* const a_row = a.data + a_y * a.width;
        for (size_t col = 0; col < 32; ++col) {
          const size_t a_x = a_block_x * 32 + col;
          block_data[row * 32 + col] = a_row[a_x];
        }
      }
    }
  }

  return ret;
}

// Map from 32x32block-major to row-major.
Matrix BlockUnmap(Matrix a) {
  assert(a.height % 32 == 0);
  assert(a.width % 32 == 0);

  double* const data = (double*)aligned_alloc(PAGE_SIZE, a.width * a.height * sizeof(double));
  Matrix ret = {data, a.height, a.width};

  const size_t block_w = a.width  / 32;
  const size_t block_h = a.height / 32;
  const size_t block_size = 32 * 32;

  for (size_t block_y = 0; block_y < block_h; ++block_y) {
    for (size_t block_x = 0; block_x < block_w; ++block_x) {
      const size_t block_i = block_y * block_w + block_x;
      double* const a_block = a.data + block_i * block_size;
      for (size_t row = 0; row < 32; ++row) {
        for (size_t col = 0; col < 32; ++col) {
          const size_t ret_x = block_x * 32 + col;
          const size_t ret_y = block_y * 32 + row;
          ret.data[ret_y * ret.width + ret_x] = a_block[row * 32 + col];
        }
      }
    }
  }

  return ret;
}

// c = a*b
//
// Returns the number of seconds taken.
double BlockRemapMatMul(Matrix a, Matrix b, Matrix* c) {
  assert(a.width == b.height);
  assert(a.height == c->height);
  assert(b.width == c->width);

  clock_t start = clock();

  Matrix aa = BlockRemap(a);
  Matrix bb = BlockRemap(b);
  Matrix cc = {c->data, c->height, c->width};
  // for (int i = 0; i < cc.height * cc.width; ++i) cc.data[i] = 0.0;
  memset(cc.data, 0, cc.height * cc.width * sizeof(double));

  constexpr size_t BLOCK_DIM = 32;
  constexpr size_t BLOCK_AREA = BLOCK_DIM * BLOCK_DIM;
  const size_t c_block_w = cc.width  / BLOCK_DIM;
  const size_t c_block_h = cc.height / BLOCK_DIM;

  for (size_t block_row = 0; block_row < cc.height / BLOCK_DIM; ++block_row) {
    for (size_t block_col = 0; block_col < cc.width / BLOCK_DIM; ++block_col) {
      double* const cc_block = cc.data + BLOCK_AREA * (block_row * c_block_w + block_col);

      // Produce cc_block.
      for (size_t k = 0; k < a.width / BLOCK_DIM; ++k) {
        // Pseudocode:
        //   const Block aa_block = aa.blocks[block_row * c_block_w + k];
        //   const Block bb_block = aa.blocks[k         * c_block_w + block_col];
        //   cc.block += aa_block * bb_block;
        double* const aa_block = aa.data + BLOCK_AREA * (block_row * c_block_w + k);
        double* const bb_block = bb.data + BLOCK_AREA * (k         * c_block_w + block_col);
        // Multiply these two 32x32 row-major matrices into cc_block.
        for (size_t row = 0; row < 32; ++row) {
          for (size_t col = 0; col < 32; ++col) {
            double sum = 0.0;
            sum += aa_block[row * 32 +  0] * bb_block[ 0 * 32 + col];
            sum += aa_block[row * 32 +  1] * bb_block[ 1 * 32 + col];
            sum += aa_block[row * 32 +  2] * bb_block[ 2 * 32 + col];
            sum += aa_block[row * 32 +  3] * bb_block[ 3 * 32 + col];
            sum += aa_block[row * 32 +  4] * bb_block[ 4 * 32 + col];
            sum += aa_block[row * 32 +  5] * bb_block[ 5 * 32 + col];
            sum += aa_block[row * 32 +  6] * bb_block[ 6 * 32 + col];
            sum += aa_block[row * 32 +  7] * bb_block[ 7 * 32 + col];
            sum += aa_block[row * 32 +  8] * bb_block[ 8 * 32 + col];
            sum += aa_block[row * 32 +  9] * bb_block[ 9 * 32 + col];
            sum += aa_block[row * 32 + 10] * bb_block[10 * 32 + col];
            sum += aa_block[row * 32 + 11] * bb_block[11 * 32 + col];
            sum += aa_block[row * 32 + 12] * bb_block[12 * 32 + col];
            sum += aa_block[row * 32 + 13] * bb_block[13 * 32 + col];
            sum += aa_block[row * 32 + 14] * bb_block[14 * 32 + col];
            sum += aa_block[row * 32 + 15] * bb_block[15 * 32 + col];
            sum += aa_block[row * 32 + 16] * bb_block[16 * 32 + col];
            sum += aa_block[row * 32 + 17] * bb_block[17 * 32 + col];
            sum += aa_block[row * 32 + 18] * bb_block[18 * 32 + col];
            sum += aa_block[row * 32 + 19] * bb_block[19 * 32 + col];
            sum += aa_block[row * 32 + 20] * bb_block[20 * 32 + col];
            sum += aa_block[row * 32 + 21] * bb_block[21 * 32 + col];
            sum += aa_block[row * 32 + 22] * bb_block[22 * 32 + col];
            sum += aa_block[row * 32 + 23] * bb_block[23 * 32 + col];
            sum += aa_block[row * 32 + 24] * bb_block[24 * 32 + col];
            sum += aa_block[row * 32 + 25] * bb_block[25 * 32 + col];
            sum += aa_block[row * 32 + 26] * bb_block[26 * 32 + col];
            sum += aa_block[row * 32 + 27] * bb_block[27 * 32 + col];
            sum += aa_block[row * 32 + 28] * bb_block[28 * 32 + col];
            sum += aa_block[row * 32 + 29] * bb_block[29 * 32 + col];
            sum += aa_block[row * 32 + 30] * bb_block[30 * 32 + col];
            sum += aa_block[row * 32 + 31] * bb_block[31 * 32 + col];
            cc_block[row * 32 + col] += sum;
          }
        }
      }
    }
  }

  Matrix ccc = BlockUnmap(cc);
  free(c->data);
  c->data = ccc.data;
  free(aa.data);
  free(bb.data);

  clock_t stop = clock();
  return (stop - start) * 1. / CLOCKS_PER_SEC;
}

int main(int argc, char** argv) {
  Matrix a = MakeMatrix(1024, 1024);
  Matrix b = MakeMatrix(1024, 1024);
  Matrix c = MakeMatrix(1024, 1024);

  bool verbose = (argc == 2) && (strcmp(argv[1], "-v") == 0);
  if (verbose) {
    fprintf(
      stdout,
      "pre: checksums a[%f] b[%f] c[%f]\n",
      Checksum(a), Checksum(b), Checksum(c)
    );
    fprintf(stdout, "pre: sizeof(Matrix.data)=%lu\n", sizeof(a.data[0]) * a.width * a.height);
    fprintf(stdout, "\n");
  }

  #if defined SIMPLE || defined ALL
  {
    CleanCache();
    double secs = SimpleMatMul(a, b, c);
    double checksum = Checksum(c);
    fprintf(stdout, "SimpleMatMul:           %f seconds, sum = %f\n", secs, checksum);
  }
  #endif

  #if defined SIMPLE_COLUMNWISE || defined ALL
  {
    CleanCache();
    double secs = SimpleMatMulColumnwise(a, b, c);
    double checksum = Checksum(c);
    fprintf(stdout, "SimpleMatMulColumnwise: %f seconds, sum = %f\n", secs, checksum);
  }
  #endif

  #if defined JUST_TRANSPOSE || defined ALL
  {
    for (int i = 0; i < 100; i++) {
      CleanCache();
      clock_t start = clock();
      auto cc = Transposed(c);
      auto ccc = Transposed(cc);
      clock_t stop = clock();
      double secs = (stop - start) * 1. / CLOCKS_PER_SEC;
      double checksum = Checksum(ccc);
      free(cc.data);
      free(ccc.data);
      fprintf(stdout, "Transposed x2: %f seconds, sum = %f\n", secs, checksum);
    }
  }
  #endif

  #if defined JUST_TRANSPOSE_88 || defined ALL
  {
    for (int i = 0; i < 100; i++) {
      CleanCache();
      clock_t start = clock();
      auto cc = Transposed8x8(c);
      auto ccc = Transposed8x8(cc);
      clock_t stop = clock();
      double secs = (stop - start) * 1. / CLOCKS_PER_SEC;
      double checksum = Checksum(ccc);
      free(cc.data);
      free(ccc.data);
      fprintf(stdout, "Transposed x2: %f seconds, sum = %f\n", secs, checksum);
    }
  }
  #endif

  #if defined JUST_TRANSPOSE_88_2 || defined ALL
  {
    for (int i = 0; i < 100; i++) {
      CleanCache();
      clock_t start = clock();
      auto cc = Transposed8x8Flipped(c);
      auto ccc = Transposed8x8Flipped(cc);
      clock_t stop = clock();
      double secs = (stop - start) * 1. / CLOCKS_PER_SEC;
      double checksum = Checksum(ccc);
      free(cc.data);
      free(ccc.data);
      fprintf(stdout, "Transposed x2: %f seconds, sum = %f\n", secs, checksum);
    }
  }
  #endif

  #if defined TRANSPOSE || defined ALL
  {
    CleanCache();
    double secs = TransposeMatMul(a, b, &c);
    double checksum = Checksum(c);
    fprintf(stdout, "TransposeMatMul: %f seconds, sum = %f\n", secs, checksum);
  }
  #endif

  #if defined JUST_BLOCK_REMAP || defined ALL
  {
    CleanCache();
    clock_t start = clock();
    auto cc = BlockRemap(c);
    auto ccc = BlockUnmap(c);
    clock_t stop = clock();
    double secs = (stop - start) * 1. / CLOCKS_PER_SEC;
    fprintf(stdout, "JustBlockRemap -- %f secs\n", secs);
    for (size_t i = 0; i < c.width * c.height; ++i) {
      assert(c.data[i] = ccc.data[i]);
    }
    free(cc.data);
    free(ccc.data);
  }
  #endif

  #if defined BLOCK_REMAP || defined ALL
  {
    CleanCache();
    double secs = BlockRemapMatMul(a, b, &c);
    double checksum = Checksum(c);
    fprintf(stdout, "BlockRemapMatMul: %f seconds, sum = %f\n", secs, checksum);
  }
  #endif
}
