#include <gtest/gtest.h>
#include <hardware/hardware.h>

#include "separate_rects.h"

using namespace separate_rects;

#define RectSet RectSet<TId, TNum>
#define Rect Rect<TNum>
#define IdSet IdSet<TId>
typedef uint64_t TId;
typedef float TNum;

struct SeparateRectTest : public testing::Test {
  bool IsEquality(std::vector<RectSet> &out,
                  std::vector<RectSet> &expected_out) {
    // Test for rects missing from out
    for (size_t i = 0; i < expected_out.size(); i++) {
      RectSet &ex_out = expected_out[i];
      if (std::find(out.begin(), out.end(), ex_out) == out.end()) {
        return false;
      }
    }

    // Test for presence of unexpected rects in out
    for (size_t i = 0; i < out.size(); i++) {
      RectSet &actual_out = out[i];
      if (std::find(expected_out.begin(), expected_out.end(), actual_out) ==
          expected_out.end()) {
        return false;
      }
    }

    return true;
  }
};

TEST_F(SeparateRectTest, test_separate_rect) {
  std::vector<Rect> in;
  std::vector<RectSet> out;
  std::vector<RectSet> expected_out;

  in.push_back({0, 0, 4, 5});
  in.push_back({2, 0, 6, 6});
  in.push_back({4, 0, 8, 5});
  in.push_back({0, 7, 8, 9});

  in.push_back({10, 0, 18, 5});
  in.push_back({12, 0, 16, 5});

  in.push_back({20, 11, 24, 17});
  in.push_back({22, 13, 26, 21});
  in.push_back({32, 33, 36, 37});
  in.push_back({30, 31, 38, 39});

  in.push_back({40, 43, 48, 45});
  in.push_back({44, 41, 46, 47});

  in.push_back({50, 51, 52, 53});
  in.push_back({50, 51, 52, 53});
  in.push_back({50, 51, 52, 53});

  in.push_back({0, 0, 0, 10});
  in.push_back({0, 0, 10, 0});
  in.push_back({10, 0, 0, 10});
  in.push_back({0, 10, 10, 0});

  for (int i = 0; i < 100000; i++) {
    out.clear();
    separate_frects_64(in, &out);
  }

  expected_out.push_back(RectSet(IdSet(0), Rect(0, 0, 2, 5)));
  expected_out.push_back(RectSet(IdSet(1), Rect(2, 5, 6, 6)));
  expected_out.push_back(RectSet(IdSet(1) | 0, Rect(2, 0, 4, 5)));
  expected_out.push_back(RectSet(IdSet(1) | 2, Rect(4, 0, 6, 5)));
  expected_out.push_back(RectSet(IdSet(2), Rect(6, 0, 8, 5)));
  expected_out.push_back(RectSet(IdSet(3), Rect(0, 7, 8, 9)));
  expected_out.push_back(RectSet(IdSet(4), Rect(10, 0, 12, 5)));
  expected_out.push_back(RectSet(IdSet(5) | 4, Rect(12, 0, 16, 5)));
  expected_out.push_back(RectSet(IdSet(4), Rect(16, 0, 18, 5)));
  expected_out.push_back(RectSet(IdSet(6), Rect(20, 11, 22, 17)));
  expected_out.push_back(RectSet(IdSet(6) | 7, Rect(22, 13, 24, 17)));
  expected_out.push_back(RectSet(IdSet(6), Rect(22, 11, 24, 13)));
  expected_out.push_back(RectSet(IdSet(7), Rect(22, 17, 24, 21)));
  expected_out.push_back(RectSet(IdSet(7), Rect(24, 13, 26, 21)));
  expected_out.push_back(RectSet(IdSet(9), Rect(30, 31, 32, 39)));
  expected_out.push_back(RectSet(IdSet(8) | 9, Rect(32, 33, 36, 37)));
  expected_out.push_back(RectSet(IdSet(9), Rect(32, 37, 36, 39)));
  expected_out.push_back(RectSet(IdSet(9), Rect(32, 31, 36, 33)));
  expected_out.push_back(RectSet(IdSet(9), Rect(36, 31, 38, 39)));
  expected_out.push_back(RectSet(IdSet(10), Rect(40, 43, 44, 45)));
  expected_out.push_back(RectSet(IdSet(10) | 11, Rect(44, 43, 46, 45)));
  expected_out.push_back(RectSet(IdSet(11), Rect(44, 41, 46, 43)));
  expected_out.push_back(RectSet(IdSet(11), Rect(44, 45, 46, 47)));
  expected_out.push_back(RectSet(IdSet(10), Rect(46, 43, 48, 45)));
  expected_out.push_back(RectSet(IdSet(12) | 13 | 14, Rect(50, 51, 52, 53)));

  ASSERT_TRUE(IsEquality(out, expected_out));
}
