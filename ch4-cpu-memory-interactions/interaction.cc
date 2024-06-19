#include <assert.h>
#include <time.h>
#include <x86intrin.h>

const size_t PAGE_SIZE = 4 * 1024;

struct Matrix {
  // Data is stowed in row-major order.
  int64_t* data;
  size_t width;
  size_t height;
};

Matrix MakeMatrix(size_t width, size_t height) {
  const size_t n = width * height;
  int64_t* const data = (int64_t*)aligned_alloc(PAGE_SIZE, n * sizeof(int64_t));
  // TODO: Initialize the values.
  return { data, width, height };
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
      int64_t sum = 0;
      for (size_t k = 0; k < a.width; ++k) {
        sum += a.data[row * a.width + k] * b.data[k * b.width + col];
      }
      c.data[row * c.width + col] = sum;
    }
  }
  clock_t stop = clock();
  return (stop - start) * 1. / CLOCKS_PER_SEC;
}

int64_t Checksum(Matrix m) {
  int64_t sum = 0;
  for (size_t i = 0; i < m.width * m.height; ++i) {
    sum += m.data[i];
  }
  return sum;
}

int main(int argc, char** argv) {
  Matrix a = MakeMatrix(1024, 1024);
  Matrix b = MakeMatrix(1024, 1024);
  Matrix c = MakeMatrix(1024, 1024);

  double secs = SimpleMatMul(a, b, c);
  int64_t checksum = Checksum(c);
  fprintf(stdout, "SimpleMatMul: %f seconds, sum = %ld\n", secs, checksum);
}
