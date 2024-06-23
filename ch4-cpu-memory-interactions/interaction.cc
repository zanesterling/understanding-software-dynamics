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
}
