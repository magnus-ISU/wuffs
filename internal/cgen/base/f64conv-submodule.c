// After editing this file, run "go generate" in the parent directory.

// Copyright 2020 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// ---------------- IEEE 754 Floating Point

#define WUFFS_BASE__PRIVATE_IMPLEMENTATION__HPD__DECIMAL_POINT__RANGE 2047
#define WUFFS_BASE__PRIVATE_IMPLEMENTATION__HPD__DIGITS_PRECISION 800

// WUFFS_BASE__PRIVATE_IMPLEMENTATION__HPD__SHIFT__MAX_INCL is the largest N
// such that ((10 << N) < (1 << 64)).
#define WUFFS_BASE__PRIVATE_IMPLEMENTATION__HPD__SHIFT__MAX_INCL 60

// wuffs_base__private_implementation__high_prec_dec (abbreviated as HPD) is a
// fixed precision floating point decimal number, augmented with ±infinity
// values, but it cannot represent NaN (Not a Number).
//
// "High precision" means that the mantissa holds 800 decimal digits. 800 is
// WUFFS_BASE__PRIVATE_IMPLEMENTATION__HPD__DIGITS_PRECISION.
//
// An HPD isn't for general purpose arithmetic, only for conversions to and
// from IEEE 754 double-precision floating point, where the largest and
// smallest positive, finite values are approximately 1.8e+308 and 4.9e-324.
// HPD exponents above +2047 mean infinity, below -2047 mean zero. The ±2047
// bounds are further away from zero than ±(324 + 800), where 800 and 2047 is
// WUFFS_BASE__PRIVATE_IMPLEMENTATION__HPD__DIGITS_PRECISION and
// WUFFS_BASE__PRIVATE_IMPLEMENTATION__HPD__DECIMAL_POINT__RANGE.
//
// digits[.. num_digits] are the number's digits in big-endian order. The
// uint8_t values are in the range [0 ..= 9], not ['0' ..= '9'], where e.g. '7'
// is the ASCII value 0x37.
//
// decimal_point is the index (within digits) of the decimal point. It may be
// negative or be larger than num_digits, in which case the explicit digits are
// padded with implicit zeroes.
//
// For example, if num_digits is 3 and digits is "\x07\x08\x09":
//   - A decimal_point of -2 means ".00789"
//   - A decimal_point of -1 means ".0789"
//   - A decimal_point of +0 means ".789"
//   - A decimal_point of +1 means "7.89"
//   - A decimal_point of +2 means "78.9"
//   - A decimal_point of +3 means "789."
//   - A decimal_point of +4 means "7890."
//   - A decimal_point of +5 means "78900."
//
// As above, a decimal_point higher than +2047 means that the overall value is
// infinity, lower than -2047 means zero.
//
// negative is a sign bit. An HPD can distinguish positive and negative zero.
//
// truncated is whether there are more than
// WUFFS_BASE__PRIVATE_IMPLEMENTATION__HPD__DIGITS_PRECISION digits, and at
// least one of those extra digits are non-zero. The existence of long-tail
// digits can affect rounding.
//
// The "all fields are zero" value is valid, and represents the number +0.
typedef struct {
  uint32_t num_digits;
  int32_t decimal_point;
  bool negative;
  bool truncated;
  uint8_t digits[WUFFS_BASE__PRIVATE_IMPLEMENTATION__HPD__DIGITS_PRECISION];
} wuffs_base__private_implementation__high_prec_dec;

// wuffs_base__private_implementation__high_prec_dec__trim trims trailing
// zeroes from the h->digits[.. h->num_digits] slice. They have no benefit,
// since we explicitly track h->decimal_point.
//
// Preconditions:
//  - h is non-NULL.
static inline void  //
wuffs_base__private_implementation__high_prec_dec__trim(
    wuffs_base__private_implementation__high_prec_dec* h) {
  while ((h->num_digits > 0) && (h->digits[h->num_digits - 1] == 0)) {
    h->num_digits--;
  }
}

// wuffs_base__private_implementation__high_prec_dec__assign sets h to
// represent the number x.
//
// Preconditions:
//  - h is non-NULL.
static void  //
wuffs_base__private_implementation__high_prec_dec__assign(
    wuffs_base__private_implementation__high_prec_dec* h,
    uint64_t x,
    bool negative) {
  uint32_t n = 0;

  // Set h->digits.
  if (x > 0) {
    // Calculate the digits, working right-to-left. After we determine n (how
    // many digits there are), copy from buf to h->digits.
    //
    // UINT64_MAX, 18446744073709551615, is 20 digits long. It can be faster to
    // copy a constant number of bytes than a variable number (20 instead of
    // n). Make buf large enough (and start writing to it from the middle) so
    // that can we always copy 20 bytes: the slice buf[(20-n) .. (40-n)].
    uint8_t buf[40] = {0};
    uint8_t* ptr = &buf[20];
    do {
      uint64_t remaining = x / 10;
      x -= remaining * 10;
      ptr--;
      *ptr = (uint8_t)x;
      n++;
      x = remaining;
    } while (x > 0);
    memcpy(h->digits, ptr, 20);
  }

  // Set h's other fields.
  h->num_digits = n;
  h->decimal_point = (int32_t)n;
  h->negative = negative;
  h->truncated = false;
  wuffs_base__private_implementation__high_prec_dec__trim(h);
}

static wuffs_base__status  //
wuffs_base__private_implementation__high_prec_dec__parse(
    wuffs_base__private_implementation__high_prec_dec* h,
    wuffs_base__slice_u8 s) {
  if (!h) {
    return wuffs_base__make_status(wuffs_base__error__bad_receiver);
  }
  h->num_digits = 0;
  h->decimal_point = 0;
  h->negative = false;
  h->truncated = false;

  uint8_t* p = s.ptr;
  uint8_t* q = s.ptr + s.len;

  for (; (p < q) && (*p == '_'); p++) {
  }
  if (p >= q) {
    return wuffs_base__make_status(wuffs_base__error__bad_argument);
  }

  // Parse sign.
  do {
    if (*p == '+') {
      p++;
    } else if (*p == '-') {
      h->negative = true;
      p++;
    } else {
      break;
    }
    for (; (p < q) && (*p == '_'); p++) {
    }
  } while (0);

  // Parse digits.
  uint32_t nd = 0;
  int32_t dp = 0;
  bool saw_digits = false;
  bool saw_non_zero_digits = false;
  bool saw_dot = false;
  for (; p < q; p++) {
    if (*p == '_') {
      // No-op.

    } else if ((*p == '.') || (*p == ',')) {
      // As per https://en.wikipedia.org/wiki/Decimal_separator, both '.' or
      // ',' are commonly used. We just parse either, regardless of LOCALE.
      if (saw_dot) {
        return wuffs_base__make_status(wuffs_base__error__bad_argument);
      }
      saw_dot = true;
      dp = (int32_t)nd;

    } else if ('0' == *p) {
      if (!saw_dot && !saw_non_zero_digits && saw_digits) {
        // We don't allow unnecessary leading zeroes: "000123" or "0644".
        return wuffs_base__make_status(wuffs_base__error__bad_argument);
      }
      saw_digits = true;
      if (nd == 0) {
        // Track leading zeroes implicitly.
        dp--;
      } else if (nd <
                 WUFFS_BASE__PRIVATE_IMPLEMENTATION__HPD__DIGITS_PRECISION) {
        h->digits[nd++] = 0;
      } else {
        // Long-tail zeroes are ignored.
      }

    } else if (('0' < *p) && (*p <= '9')) {
      if (!saw_dot && !saw_non_zero_digits && saw_digits) {
        // We don't allow unnecessary leading zeroes: "000123" or "0644".
        return wuffs_base__make_status(wuffs_base__error__bad_argument);
      }
      saw_digits = true;
      saw_non_zero_digits = true;
      if (nd < WUFFS_BASE__PRIVATE_IMPLEMENTATION__HPD__DIGITS_PRECISION) {
        h->digits[nd++] = (uint8_t)(*p - '0');
      } else {
        // Long-tail non-zeroes set the truncated bit.
        h->truncated = true;
      }

    } else {
      break;
    }
  }

  if (!saw_digits) {
    return wuffs_base__make_status(wuffs_base__error__bad_argument);
  }
  if (!saw_dot) {
    dp = (int32_t)nd;
  }

  // Parse exponent.
  if ((p < q) && ((*p == 'E') || (*p == 'e'))) {
    p++;
    for (; (p < q) && (*p == '_'); p++) {
    }
    if (p >= q) {
      return wuffs_base__make_status(wuffs_base__error__bad_argument);
    }

    int32_t exp_sign = +1;
    if (*p == '+') {
      p++;
    } else if (*p == '-') {
      exp_sign = -1;
      p++;
    }

    int32_t exp = 0;
    const int32_t exp_large =
        WUFFS_BASE__PRIVATE_IMPLEMENTATION__HPD__DECIMAL_POINT__RANGE +
        WUFFS_BASE__PRIVATE_IMPLEMENTATION__HPD__DIGITS_PRECISION;
    bool saw_exp_digits = false;
    for (; p < q; p++) {
      if (*p == '_') {
        // No-op.
      } else if (('0' <= *p) && (*p <= '9')) {
        saw_exp_digits = true;
        if (exp < exp_large) {
          exp = (10 * exp) + ((int32_t)(*p - '0'));
        }
      } else {
        break;
      }
    }
    if (!saw_exp_digits) {
      return wuffs_base__make_status(wuffs_base__error__bad_argument);
    }
    dp += exp_sign * exp;
  }

  // Finish.
  if (p != q) {
    return wuffs_base__make_status(wuffs_base__error__bad_argument);
  }
  h->num_digits = nd;
  if (nd == 0) {
    h->decimal_point = 0;
  } else if (dp <
             -WUFFS_BASE__PRIVATE_IMPLEMENTATION__HPD__DECIMAL_POINT__RANGE) {
    h->decimal_point =
        -WUFFS_BASE__PRIVATE_IMPLEMENTATION__HPD__DECIMAL_POINT__RANGE - 1;
  } else if (dp >
             +WUFFS_BASE__PRIVATE_IMPLEMENTATION__HPD__DECIMAL_POINT__RANGE) {
    h->decimal_point =
        +WUFFS_BASE__PRIVATE_IMPLEMENTATION__HPD__DECIMAL_POINT__RANGE + 1;
  } else {
    h->decimal_point = dp;
  }
  wuffs_base__private_implementation__high_prec_dec__trim(h);
  return wuffs_base__make_status(NULL);
}

// --------

// The etc__hpd_left_shift and etc__powers_of_5 tables were printed by
// script/print-hpd-left-shift.go. That script has an optional -comments flag,
// whose output is not copied here, which prints further detail.
//
// These tables are used in
// wuffs_base__private_implementation__high_prec_dec__lshift_num_new_digits.

// wuffs_base__private_implementation__hpd_left_shift[i] encodes the number of
// new digits created after multiplying a positive integer by (1 << i): the
// additional length in the decimal representation. For example, shifting "234"
// by 3 (equivalent to multiplying by 8) will produce "1872". Going from a
// 3-length string to a 4-length string means that 1 new digit was added (and
// existing digits may have changed).
//
// Shifting by i can add either N or N-1 new digits, depending on whether the
// original positive integer compares >= or < to the i'th power of 5 (as 10
// equals 2 * 5). Comparison is lexicographic, not numerical.
//
// For example, shifting by 4 (i.e. multiplying by 16) can add 1 or 2 new
// digits, depending on a lexicographic comparison to (5 ** 4), i.e. "625":
//  - ("1"      << 4) is "16",       which adds 1 new digit.
//  - ("5678"   << 4) is "90848",    which adds 1 new digit.
//  - ("624"    << 4) is "9984",     which adds 1 new digit.
//  - ("62498"  << 4) is "999968",   which adds 1 new digit.
//  - ("625"    << 4) is "10000",    which adds 2 new digits.
//  - ("625001" << 4) is "10000016", which adds 2 new digits.
//  - ("7008"   << 4) is "112128",   which adds 2 new digits.
//  - ("99"     << 4) is "1584",     which adds 2 new digits.
//
// Thus, when i is 4, N is 2 and (5 ** i) is "625". This etc__hpd_left_shift
// array encodes this as:
//  - etc__hpd_left_shift[4] is 0x1006 = (2 << 11) | 0x0006.
//  - etc__hpd_left_shift[5] is 0x1009 = (? << 11) | 0x0009.
// where the ? isn't relevant for i == 4.
//
// The high 5 bits of etc__hpd_left_shift[i] is N, the higher of the two
// possible number of new digits. The low 11 bits are an offset into the
// etc__powers_of_5 array (of length 0x051C, so offsets fit in 11 bits). When i
// is 4, its offset and the next one is 6 and 9, and etc__powers_of_5[6 .. 9]
// is the string "\x06\x02\x05", so the relevant power of 5 is "625".
//
// Thanks to Ken Thompson for the original idea.
static const uint16_t wuffs_base__private_implementation__hpd_left_shift[65] = {
    0x0000, 0x0800, 0x0801, 0x0803, 0x1006, 0x1009, 0x100D, 0x1812, 0x1817,
    0x181D, 0x2024, 0x202B, 0x2033, 0x203C, 0x2846, 0x2850, 0x285B, 0x3067,
    0x3073, 0x3080, 0x388E, 0x389C, 0x38AB, 0x38BB, 0x40CC, 0x40DD, 0x40EF,
    0x4902, 0x4915, 0x4929, 0x513E, 0x5153, 0x5169, 0x5180, 0x5998, 0x59B0,
    0x59C9, 0x61E3, 0x61FD, 0x6218, 0x6A34, 0x6A50, 0x6A6D, 0x6A8B, 0x72AA,
    0x72C9, 0x72E9, 0x7B0A, 0x7B2B, 0x7B4D, 0x8370, 0x8393, 0x83B7, 0x83DC,
    0x8C02, 0x8C28, 0x8C4F, 0x9477, 0x949F, 0x94C8, 0x9CF2, 0x051C, 0x051C,
    0x051C, 0x051C,
};

// wuffs_base__private_implementation__powers_of_5 contains the powers of 5,
// concatenated together: "5", "25", "125", "625", "3125", etc.
static const uint8_t wuffs_base__private_implementation__powers_of_5[0x051C] = {
    5, 2, 5, 1, 2, 5, 6, 2, 5, 3, 1, 2, 5, 1, 5, 6, 2, 5, 7, 8, 1, 2, 5, 3, 9,
    0, 6, 2, 5, 1, 9, 5, 3, 1, 2, 5, 9, 7, 6, 5, 6, 2, 5, 4, 8, 8, 2, 8, 1, 2,
    5, 2, 4, 4, 1, 4, 0, 6, 2, 5, 1, 2, 2, 0, 7, 0, 3, 1, 2, 5, 6, 1, 0, 3, 5,
    1, 5, 6, 2, 5, 3, 0, 5, 1, 7, 5, 7, 8, 1, 2, 5, 1, 5, 2, 5, 8, 7, 8, 9, 0,
    6, 2, 5, 7, 6, 2, 9, 3, 9, 4, 5, 3, 1, 2, 5, 3, 8, 1, 4, 6, 9, 7, 2, 6, 5,
    6, 2, 5, 1, 9, 0, 7, 3, 4, 8, 6, 3, 2, 8, 1, 2, 5, 9, 5, 3, 6, 7, 4, 3, 1,
    6, 4, 0, 6, 2, 5, 4, 7, 6, 8, 3, 7, 1, 5, 8, 2, 0, 3, 1, 2, 5, 2, 3, 8, 4,
    1, 8, 5, 7, 9, 1, 0, 1, 5, 6, 2, 5, 1, 1, 9, 2, 0, 9, 2, 8, 9, 5, 5, 0, 7,
    8, 1, 2, 5, 5, 9, 6, 0, 4, 6, 4, 4, 7, 7, 5, 3, 9, 0, 6, 2, 5, 2, 9, 8, 0,
    2, 3, 2, 2, 3, 8, 7, 6, 9, 5, 3, 1, 2, 5, 1, 4, 9, 0, 1, 1, 6, 1, 1, 9, 3,
    8, 4, 7, 6, 5, 6, 2, 5, 7, 4, 5, 0, 5, 8, 0, 5, 9, 6, 9, 2, 3, 8, 2, 8, 1,
    2, 5, 3, 7, 2, 5, 2, 9, 0, 2, 9, 8, 4, 6, 1, 9, 1, 4, 0, 6, 2, 5, 1, 8, 6,
    2, 6, 4, 5, 1, 4, 9, 2, 3, 0, 9, 5, 7, 0, 3, 1, 2, 5, 9, 3, 1, 3, 2, 2, 5,
    7, 4, 6, 1, 5, 4, 7, 8, 5, 1, 5, 6, 2, 5, 4, 6, 5, 6, 6, 1, 2, 8, 7, 3, 0,
    7, 7, 3, 9, 2, 5, 7, 8, 1, 2, 5, 2, 3, 2, 8, 3, 0, 6, 4, 3, 6, 5, 3, 8, 6,
    9, 6, 2, 8, 9, 0, 6, 2, 5, 1, 1, 6, 4, 1, 5, 3, 2, 1, 8, 2, 6, 9, 3, 4, 8,
    1, 4, 4, 5, 3, 1, 2, 5, 5, 8, 2, 0, 7, 6, 6, 0, 9, 1, 3, 4, 6, 7, 4, 0, 7,
    2, 2, 6, 5, 6, 2, 5, 2, 9, 1, 0, 3, 8, 3, 0, 4, 5, 6, 7, 3, 3, 7, 0, 3, 6,
    1, 3, 2, 8, 1, 2, 5, 1, 4, 5, 5, 1, 9, 1, 5, 2, 2, 8, 3, 6, 6, 8, 5, 1, 8,
    0, 6, 6, 4, 0, 6, 2, 5, 7, 2, 7, 5, 9, 5, 7, 6, 1, 4, 1, 8, 3, 4, 2, 5, 9,
    0, 3, 3, 2, 0, 3, 1, 2, 5, 3, 6, 3, 7, 9, 7, 8, 8, 0, 7, 0, 9, 1, 7, 1, 2,
    9, 5, 1, 6, 6, 0, 1, 5, 6, 2, 5, 1, 8, 1, 8, 9, 8, 9, 4, 0, 3, 5, 4, 5, 8,
    5, 6, 4, 7, 5, 8, 3, 0, 0, 7, 8, 1, 2, 5, 9, 0, 9, 4, 9, 4, 7, 0, 1, 7, 7,
    2, 9, 2, 8, 2, 3, 7, 9, 1, 5, 0, 3, 9, 0, 6, 2, 5, 4, 5, 4, 7, 4, 7, 3, 5,
    0, 8, 8, 6, 4, 6, 4, 1, 1, 8, 9, 5, 7, 5, 1, 9, 5, 3, 1, 2, 5, 2, 2, 7, 3,
    7, 3, 6, 7, 5, 4, 4, 3, 2, 3, 2, 0, 5, 9, 4, 7, 8, 7, 5, 9, 7, 6, 5, 6, 2,
    5, 1, 1, 3, 6, 8, 6, 8, 3, 7, 7, 2, 1, 6, 1, 6, 0, 2, 9, 7, 3, 9, 3, 7, 9,
    8, 8, 2, 8, 1, 2, 5, 5, 6, 8, 4, 3, 4, 1, 8, 8, 6, 0, 8, 0, 8, 0, 1, 4, 8,
    6, 9, 6, 8, 9, 9, 4, 1, 4, 0, 6, 2, 5, 2, 8, 4, 2, 1, 7, 0, 9, 4, 3, 0, 4,
    0, 4, 0, 0, 7, 4, 3, 4, 8, 4, 4, 9, 7, 0, 7, 0, 3, 1, 2, 5, 1, 4, 2, 1, 0,
    8, 5, 4, 7, 1, 5, 2, 0, 2, 0, 0, 3, 7, 1, 7, 4, 2, 2, 4, 8, 5, 3, 5, 1, 5,
    6, 2, 5, 7, 1, 0, 5, 4, 2, 7, 3, 5, 7, 6, 0, 1, 0, 0, 1, 8, 5, 8, 7, 1, 1,
    2, 4, 2, 6, 7, 5, 7, 8, 1, 2, 5, 3, 5, 5, 2, 7, 1, 3, 6, 7, 8, 8, 0, 0, 5,
    0, 0, 9, 2, 9, 3, 5, 5, 6, 2, 1, 3, 3, 7, 8, 9, 0, 6, 2, 5, 1, 7, 7, 6, 3,
    5, 6, 8, 3, 9, 4, 0, 0, 2, 5, 0, 4, 6, 4, 6, 7, 7, 8, 1, 0, 6, 6, 8, 9, 4,
    5, 3, 1, 2, 5, 8, 8, 8, 1, 7, 8, 4, 1, 9, 7, 0, 0, 1, 2, 5, 2, 3, 2, 3, 3,
    8, 9, 0, 5, 3, 3, 4, 4, 7, 2, 6, 5, 6, 2, 5, 4, 4, 4, 0, 8, 9, 2, 0, 9, 8,
    5, 0, 0, 6, 2, 6, 1, 6, 1, 6, 9, 4, 5, 2, 6, 6, 7, 2, 3, 6, 3, 2, 8, 1, 2,
    5, 2, 2, 2, 0, 4, 4, 6, 0, 4, 9, 2, 5, 0, 3, 1, 3, 0, 8, 0, 8, 4, 7, 2, 6,
    3, 3, 3, 6, 1, 8, 1, 6, 4, 0, 6, 2, 5, 1, 1, 1, 0, 2, 2, 3, 0, 2, 4, 6, 2,
    5, 1, 5, 6, 5, 4, 0, 4, 2, 3, 6, 3, 1, 6, 6, 8, 0, 9, 0, 8, 2, 0, 3, 1, 2,
    5, 5, 5, 5, 1, 1, 1, 5, 1, 2, 3, 1, 2, 5, 7, 8, 2, 7, 0, 2, 1, 1, 8, 1, 5,
    8, 3, 4, 0, 4, 5, 4, 1, 0, 1, 5, 6, 2, 5, 2, 7, 7, 5, 5, 5, 7, 5, 6, 1, 5,
    6, 2, 8, 9, 1, 3, 5, 1, 0, 5, 9, 0, 7, 9, 1, 7, 0, 2, 2, 7, 0, 5, 0, 7, 8,
    1, 2, 5, 1, 3, 8, 7, 7, 7, 8, 7, 8, 0, 7, 8, 1, 4, 4, 5, 6, 7, 5, 5, 2, 9,
    5, 3, 9, 5, 8, 5, 1, 1, 3, 5, 2, 5, 3, 9, 0, 6, 2, 5, 6, 9, 3, 8, 8, 9, 3,
    9, 0, 3, 9, 0, 7, 2, 2, 8, 3, 7, 7, 6, 4, 7, 6, 9, 7, 9, 2, 5, 5, 6, 7, 6,
    2, 6, 9, 5, 3, 1, 2, 5, 3, 4, 6, 9, 4, 4, 6, 9, 5, 1, 9, 5, 3, 6, 1, 4, 1,
    8, 8, 8, 2, 3, 8, 4, 8, 9, 6, 2, 7, 8, 3, 8, 1, 3, 4, 7, 6, 5, 6, 2, 5, 1,
    7, 3, 4, 7, 2, 3, 4, 7, 5, 9, 7, 6, 8, 0, 7, 0, 9, 4, 4, 1, 1, 9, 2, 4, 4,
    8, 1, 3, 9, 1, 9, 0, 6, 7, 3, 8, 2, 8, 1, 2, 5, 8, 6, 7, 3, 6, 1, 7, 3, 7,
    9, 8, 8, 4, 0, 3, 5, 4, 7, 2, 0, 5, 9, 6, 2, 2, 4, 0, 6, 9, 5, 9, 5, 3, 3,
    6, 9, 1, 4, 0, 6, 2, 5,
};

// wuffs_base__private_implementation__high_prec_dec__lshift_num_new_digits
// returns the number of additional decimal digits when left-shifting by shift.
//
// See below for preconditions.
static uint32_t  //
wuffs_base__private_implementation__high_prec_dec__lshift_num_new_digits(
    wuffs_base__private_implementation__high_prec_dec* h,
    uint32_t shift) {
  // Masking with 0x3F should be unnecessary (assuming the preconditions) but
  // it's cheap and ensures that we don't overflow the
  // wuffs_base__private_implementation__hpd_left_shift array.
  shift &= 63;

  uint32_t x_a = wuffs_base__private_implementation__hpd_left_shift[shift];
  uint32_t x_b = wuffs_base__private_implementation__hpd_left_shift[shift + 1];
  uint32_t num_new_digits = x_a >> 11;
  uint32_t pow5_a = 0x7FF & x_a;
  uint32_t pow5_b = 0x7FF & x_b;

  const uint8_t* pow5 =
      &wuffs_base__private_implementation__powers_of_5[pow5_a];
  uint32_t i = 0;
  uint32_t n = pow5_b - pow5_a;
  for (; i < n; i++) {
    if (i >= h->num_digits) {
      return num_new_digits - 1;
    } else if (h->digits[i] == pow5[i]) {
      continue;
    } else if (h->digits[i] < pow5[i]) {
      return num_new_digits - 1;
    } else {
      return num_new_digits;
    }
  }
  return num_new_digits;
}

// --------

// wuffs_base__private_implementation__high_prec_dec__rounded_integer returns
// the integral (non-fractional) part of h, provided that it is 18 or fewer
// decimal digits. For 19 or more digits, it returns UINT64_MAX. Note that:
//   - (1 << 53) is    9007199254740992, which has 16 decimal digits.
//   - (1 << 56) is   72057594037927936, which has 17 decimal digits.
//   - (1 << 59) is  576460752303423488, which has 18 decimal digits.
//   - (1 << 63) is 9223372036854775808, which has 19 decimal digits.
// and that IEEE 754 double precision has 52 mantissa bits.
//
// That integral part is rounded-to-even: rounding 7.5 or 8.5 both give 8.
//
// h's negative bit is ignored: rounding -8.6 returns 9.
//
// See below for preconditions.
static uint64_t  //
wuffs_base__private_implementation__high_prec_dec__rounded_integer(
    wuffs_base__private_implementation__high_prec_dec* h) {
  if ((h->num_digits == 0) || (h->decimal_point < 0)) {
    return 0;
  } else if (h->decimal_point > 18) {
    return UINT64_MAX;
  }

  uint32_t dp = (uint32_t)(h->decimal_point);
  uint64_t n = 0;
  uint32_t i = 0;
  for (; i < dp; i++) {
    n = (10 * n) + ((i < h->num_digits) ? h->digits[i] : 0);
  }

  bool round_up = false;
  if (dp < h->num_digits) {
    round_up = h->digits[dp] >= 5;
    if ((h->digits[dp] == 5) && (dp + 1 == h->num_digits)) {
      // We are exactly halfway. If we're truncated, round up, otherwise round
      // to even.
      round_up = h->truncated ||  //
                 ((dp > 0) && (1 & h->digits[dp - 1]));
    }
  }
  if (round_up) {
    n++;
  }

  return n;
}

// wuffs_base__private_implementation__high_prec_dec__small_xshift shifts h's
// number (where 'x' is 'l' or 'r' for left or right) by a small shift value.
//
// Preconditions:
//  - h is non-NULL.
//  - h->decimal_point is "not extreme".
//  - shift is non-zero.
//  - shift is "a small shift".
//
// "Not extreme" means within
// ±WUFFS_BASE__PRIVATE_IMPLEMENTATION__HPD__DECIMAL_POINT__RANGE.
//
// "A small shift" means not more than
// WUFFS_BASE__PRIVATE_IMPLEMENTATION__HPD__SHIFT__MAX_INCL.
//
// wuffs_base__private_implementation__high_prec_dec__rounded_integer and
// wuffs_base__private_implementation__high_prec_dec__lshift_num_new_digits
// have the same preconditions.
//
// wuffs_base__private_implementation__high_prec_dec__lshift keeps the first
// two preconditions but not the last two. Its shift argument is signed and
// does not need to be "small": zero is a no-op, positive means left shift and
// negative means right shift.

static void  //
wuffs_base__private_implementation__high_prec_dec__small_lshift(
    wuffs_base__private_implementation__high_prec_dec* h,
    uint32_t shift) {
  if (h->num_digits == 0) {
    return;
  }
  uint32_t num_new_digits =
      wuffs_base__private_implementation__high_prec_dec__lshift_num_new_digits(
          h, shift);
  uint32_t rx = h->num_digits - 1;                   // Read  index.
  uint32_t wx = h->num_digits - 1 + num_new_digits;  // Write index.
  uint64_t n = 0;

  // Repeat: pick up a digit, put down a digit, right to left.
  while (((int32_t)rx) >= 0) {
    n += ((uint64_t)(h->digits[rx])) << shift;
    uint64_t quo = n / 10;
    uint64_t rem = n - (10 * quo);
    if (wx < WUFFS_BASE__PRIVATE_IMPLEMENTATION__HPD__DIGITS_PRECISION) {
      h->digits[wx] = (uint8_t)rem;
    } else if (rem > 0) {
      h->truncated = true;
    }
    n = quo;
    wx--;
    rx--;
  }

  // Put down leading digits, right to left.
  while (n > 0) {
    uint64_t quo = n / 10;
    uint64_t rem = n - (10 * quo);
    if (wx < WUFFS_BASE__PRIVATE_IMPLEMENTATION__HPD__DIGITS_PRECISION) {
      h->digits[wx] = (uint8_t)rem;
    } else if (rem > 0) {
      h->truncated = true;
    }
    n = quo;
    wx--;
  }

  // Finish.
  h->num_digits += num_new_digits;
  if (h->num_digits >
      WUFFS_BASE__PRIVATE_IMPLEMENTATION__HPD__DIGITS_PRECISION) {
    h->num_digits = WUFFS_BASE__PRIVATE_IMPLEMENTATION__HPD__DIGITS_PRECISION;
  }
  h->decimal_point += (int32_t)num_new_digits;
  wuffs_base__private_implementation__high_prec_dec__trim(h);
}

static void  //
wuffs_base__private_implementation__high_prec_dec__small_rshift(
    wuffs_base__private_implementation__high_prec_dec* h,
    uint32_t shift) {
  uint32_t rx = 0;  // Read  index.
  uint32_t wx = 0;  // Write index.
  uint64_t n = 0;

  // Pick up enough leading digits to cover the first shift.
  while ((n >> shift) == 0) {
    if (rx < h->num_digits) {
      // Read a digit.
      n = (10 * n) + h->digits[rx++];
    } else if (n == 0) {
      // h's number used to be zero and remains zero.
      return;
    } else {
      // Read sufficient implicit trailing zeroes.
      while ((n >> shift) == 0) {
        n = 10 * n;
        rx++;
      }
      break;
    }
  }
  h->decimal_point -= ((int32_t)(rx - 1));
  if (h->decimal_point <
      -WUFFS_BASE__PRIVATE_IMPLEMENTATION__HPD__DECIMAL_POINT__RANGE) {
    // After the shift, h's number is effectively zero.
    h->num_digits = 0;
    h->decimal_point = 0;
    h->negative = false;
    h->truncated = false;
    return;
  }

  // Repeat: pick up a digit, put down a digit, left to right.
  uint64_t mask = (((uint64_t)(1)) << shift) - 1;
  while (rx < h->num_digits) {
    uint8_t new_digit = ((uint8_t)(n >> shift));
    n = (10 * (n & mask)) + h->digits[rx++];
    h->digits[wx++] = new_digit;
  }

  // Put down trailing digits, left to right.
  while (n > 0) {
    uint8_t new_digit = ((uint8_t)(n >> shift));
    n = 10 * (n & mask);
    if (wx < WUFFS_BASE__PRIVATE_IMPLEMENTATION__HPD__DIGITS_PRECISION) {
      h->digits[wx++] = new_digit;
    } else if (new_digit > 0) {
      h->truncated = true;
    }
  }

  // Finish.
  h->num_digits = wx;
  wuffs_base__private_implementation__high_prec_dec__trim(h);
}

static void  //
wuffs_base__private_implementation__high_prec_dec__lshift(
    wuffs_base__private_implementation__high_prec_dec* h,
    int32_t shift) {
  if (shift > 0) {
    while (shift > +WUFFS_BASE__PRIVATE_IMPLEMENTATION__HPD__SHIFT__MAX_INCL) {
      wuffs_base__private_implementation__high_prec_dec__small_lshift(
          h, WUFFS_BASE__PRIVATE_IMPLEMENTATION__HPD__SHIFT__MAX_INCL);
      shift -= WUFFS_BASE__PRIVATE_IMPLEMENTATION__HPD__SHIFT__MAX_INCL;
    }
    wuffs_base__private_implementation__high_prec_dec__small_lshift(
        h, ((uint32_t)(+shift)));
  } else if (shift < 0) {
    while (shift < -WUFFS_BASE__PRIVATE_IMPLEMENTATION__HPD__SHIFT__MAX_INCL) {
      wuffs_base__private_implementation__high_prec_dec__small_rshift(
          h, WUFFS_BASE__PRIVATE_IMPLEMENTATION__HPD__SHIFT__MAX_INCL);
      shift += WUFFS_BASE__PRIVATE_IMPLEMENTATION__HPD__SHIFT__MAX_INCL;
    }
    wuffs_base__private_implementation__high_prec_dec__small_rshift(
        h, ((uint32_t)(-shift)));
  }
}

// --------

// wuffs_base__private_implementation__high_prec_dec__round_etc rounds h's
// number. For those functions that take an n argument, rounding produces at
// most n digits (which is not necessarily at most n decimal places). Negative
// n values are ignored, as well as any n greater than or equal to h's number
// of digits. The etc__round_just_enough function implicitly chooses an n to
// implement WUFFS_BASE__RENDER_NUMBER_FXX__JUST_ENOUGH_PRECISION.
//
// Preconditions:
//  - h is non-NULL.
//  - h->decimal_point is "not extreme".
//
// "Not extreme" means within
// ±WUFFS_BASE__PRIVATE_IMPLEMENTATION__HPD__DECIMAL_POINT__RANGE.

static void  //
wuffs_base__private_implementation__high_prec_dec__round_down(
    wuffs_base__private_implementation__high_prec_dec* h,
    int32_t n) {
  if ((n < 0) || (h->num_digits <= (uint32_t)n)) {
    return;
  }
  h->num_digits = (uint32_t)(n);
  wuffs_base__private_implementation__high_prec_dec__trim(h);
}

static void  //
wuffs_base__private_implementation__high_prec_dec__round_up(
    wuffs_base__private_implementation__high_prec_dec* h,
    int32_t n) {
  if ((n < 0) || (h->num_digits <= (uint32_t)n)) {
    return;
  }

  for (n--; n >= 0; n--) {
    if (h->digits[n] < 9) {
      h->digits[n]++;
      h->num_digits = (uint32_t)(n + 1);
      return;
    }
  }

  // The number is all 9s. Change to a single 1 and adjust the decimal point.
  h->digits[0] = 1;
  h->num_digits = 1;
  h->decimal_point++;
}

static void  //
wuffs_base__private_implementation__high_prec_dec__round_nearest(
    wuffs_base__private_implementation__high_prec_dec* h,
    int32_t n) {
  if ((n < 0) || (h->num_digits <= (uint32_t)n)) {
    return;
  }
  bool up = h->digits[n] >= 5;
  if ((h->digits[n] == 5) && ((n + 1) == ((int32_t)(h->num_digits)))) {
    up = h->truncated ||  //
         ((n > 0) && ((h->digits[n - 1] & 1) != 0));
  }

  if (up) {
    wuffs_base__private_implementation__high_prec_dec__round_up(h, n);
  } else {
    wuffs_base__private_implementation__high_prec_dec__round_down(h, n);
  }
}

static void  //
wuffs_base__private_implementation__high_prec_dec__round_just_enough(
    wuffs_base__private_implementation__high_prec_dec* h,
    int32_t exp2,
    uint64_t mantissa) {
  // The magic numbers 52 and 53 in this function are because IEEE 754 double
  // precision has 52 mantissa bits.
  //
  // Let f be the floating point number represented by exp2 and mantissa (and
  // also the number in h): the number (mantissa * (2 ** (exp2 - 52))).
  //
  // If f is zero or a small integer, we can return early.
  if ((mantissa == 0) ||
      ((exp2 < 53) && (h->decimal_point >= ((int32_t)(h->num_digits))))) {
    return;
  }

  // The smallest normal f has an exp2 of -1022 and a mantissa of (1 << 52).
  // Subnormal numbers have the same exp2 but a smaller mantissa.
  static const int32_t min_incl_normal_exp2 = -1022;
  static const uint64_t min_incl_normal_mantissa = 0x0010000000000000ul;

  // Compute lower and upper bounds such that any number between them (possibly
  // inclusive) will round to f. First, the lower bound. Our number f is:
  //   ((mantissa + 0)         * (2 ** (  exp2 - 52)))
  //
  // The next lowest floating point number is:
  //   ((mantissa - 1)         * (2 ** (  exp2 - 52)))
  // unless (mantissa - 1) drops the (1 << 52) bit and exp2 is not the
  // min_incl_normal_exp2. Either way, call it:
  //   ((l_mantissa)           * (2 ** (l_exp2 - 52)))
  //
  // The lower bound is halfway between them (noting that 52 became 53):
  //   (((2 * l_mantissa) + 1) * (2 ** (l_exp2 - 53)))
  int32_t l_exp2 = exp2;
  uint64_t l_mantissa = mantissa - 1;
  if ((exp2 > min_incl_normal_exp2) && (mantissa <= min_incl_normal_mantissa)) {
    l_exp2 = exp2 - 1;
    l_mantissa = (2 * mantissa) - 1;
  }
  wuffs_base__private_implementation__high_prec_dec lower;
  wuffs_base__private_implementation__high_prec_dec__assign(
      &lower, (2 * l_mantissa) + 1, false);
  wuffs_base__private_implementation__high_prec_dec__lshift(&lower,
                                                            l_exp2 - 53);

  // Next, the upper bound. Our number f is:
  //   ((mantissa + 0)       * (2 ** (exp2 - 52)))
  //
  // The next highest floating point number is:
  //   ((mantissa + 1)       * (2 ** (exp2 - 52)))
  //
  // The upper bound is halfway between them (noting that 52 became 53):
  //   (((2 * mantissa) + 1) * (2 ** (exp2 - 53)))
  wuffs_base__private_implementation__high_prec_dec upper;
  wuffs_base__private_implementation__high_prec_dec__assign(
      &upper, (2 * mantissa) + 1, false);
  wuffs_base__private_implementation__high_prec_dec__lshift(&upper, exp2 - 53);

  // The lower and upper bounds are possible outputs only if the original
  // mantissa is even, so that IEEE round-to-even would round to the original
  // mantissa and not its neighbors.
  bool inclusive = (mantissa & 1) == 0;

  // As we walk the digits, we want to know whether rounding up would fall
  // within the upper bound. This is tracked by upper_delta:
  //  - When -1, the digits of h and upper are the same so far.
  //  - When +0, we saw a difference of 1 between h and upper on a previous
  //    digit and subsequently only 9s for h and 0s for upper. Thus, rounding
  //    up may fall outside of the bound if !inclusive.
  //  - When +1, the difference is greater than 1 and we know that rounding up
  //    falls within the bound.
  //
  // This is a state machine with three states. The numerical value for each
  // state (-1, +0 or +1) isn't important, other than their order.
  int upper_delta = -1;

  // We can now figure out the shortest number of digits required. Walk the
  // digits until h has distinguished itself from lower or upper.
  //
  // The zi and zd variables are indexes and digits, for z in l (lower), h (the
  // number) and u (upper).
  //
  // The lower, h and upper numbers may have their decimal points at different
  // places. In this case, upper is the longest, so we iterate ui starting from
  // 0 and iterate li and hi starting from either 0 or -1.
  int32_t ui = 0;
  for (;; ui++) {
    // Calculate hd, the middle number's digit.
    int32_t hi = ui - upper.decimal_point + h->decimal_point;
    if (hi >= ((int32_t)(h->num_digits))) {
      break;
    }
    uint8_t hd = (((uint32_t)hi) < h->num_digits) ? h->digits[hi] : 0;

    // Calculate ld, the lower bound's digit.
    int32_t li = ui - upper.decimal_point + lower.decimal_point;
    uint8_t ld = (((uint32_t)li) < lower.num_digits) ? lower.digits[li] : 0;

    // We can round down (truncate) if lower has a different digit than h or if
    // lower is inclusive and is exactly the result of rounding down (i.e. we
    // have reached the final digit of lower).
    bool can_round_down =
        (ld != hd) ||  //
        (inclusive && ((li + 1) == ((int32_t)(lower.num_digits))));

    // Calculate ud, the upper bound's digit, and update upper_delta.
    uint8_t ud = (((uint32_t)ui) < upper.num_digits) ? upper.digits[ui] : 0;
    if (upper_delta < 0) {
      if ((hd + 1) < ud) {
        // For example:
        // h     = 12345???
        // upper = 12347???
        upper_delta = +1;
      } else if (hd != ud) {
        // For example:
        // h     = 12345???
        // upper = 12346???
        upper_delta = +0;
      }
    } else if (upper_delta == 0) {
      if ((hd != 9) || (ud != 0)) {
        // For example:
        // h     = 1234598?
        // upper = 1234600?
        upper_delta = +1;
      }
    }

    // We can round up if upper has a different digit than h and either upper
    // is inclusive or upper is bigger than the result of rounding up.
    bool can_round_up =
        (upper_delta > 0) ||    //
        ((upper_delta == 0) &&  //
         (inclusive || ((ui + 1) < ((int32_t)(upper.num_digits)))));

    // If we can round either way, round to nearest. If we can round only one
    // way, do it. If we can't round, continue the loop.
    if (can_round_down) {
      if (can_round_up) {
        wuffs_base__private_implementation__high_prec_dec__round_nearest(
            h, hi + 1);
        return;
      } else {
        wuffs_base__private_implementation__high_prec_dec__round_down(h,
                                                                      hi + 1);
        return;
      }
    } else {
      if (can_round_up) {
        wuffs_base__private_implementation__high_prec_dec__round_up(h, hi + 1);
        return;
      }
    }
  }
}

// --------

// The wuffs_base__private_implementation__etc_powers_of_10 tables were printed
// by script/print-mpb-powers-of-10.go. That script has an optional -detail
// flag, whose output is not copied here, which prints further detail.
//
// These tables are used in
// wuffs_base__private_implementation__medium_prec_bin__assign_from_hpd.

// wuffs_base__private_implementation__powers_of_10 contains truncated
// approximations to the powers of 10, ranging from 1e-326 to 1e+310 inclusive,
// as 637 uint32_t quintuples (128-bit mantissa, 32-bit base-2 exponent biased
// by 0x04BE (which is 1214)). The array size is 637 * 5 = 3185.
//
// For example, the third approximation, for 1e-324, consists of the uint32_t
// quintuple (0x828675B9, 0x52064CAC, 0x5DCE35EA, 0xCF42894A, 0x000A). The
// first four form a little-endian uint128_t value. The last one is an int32_t
// value: -1140. Together, they represent the approximation to 1e-324:
//   0xCF42894A_5DCE35EA_52064CAC_828675B9 * (2 ** (0x000A - 0x04BE))
//
// Similarly, 1e+4 is approximated by the uint64_t quintuple
// (0x00000000, 0x00000000, 0x00000000, 0x9C400000, 0x044C) which means:
//   0x9C400000_00000000_00000000_00000000 * (2 ** (0x044C - 0x04BE))
//
// Similarly, 1e+68 is approximated by the uint64_t quintuple
// (0x63EE4BDD, 0x4CA7AAA8, 0xD4C4FB27, 0xED63A231, 0x0520) which means:
//   0xED63A231_D4C4FB27.4CA7AAA8_63EE4BDD * (2 ** (0x0520 - 0x04BE))
static const uint32_t wuffs_base__private_implementation__powers_of_10[3185] = {
    0xF7604B57, 0x014BB630, 0xFE98746D, 0x84A57695, 0x0004,  // 1e-326
    0x35385E2D, 0x419EA3BD, 0x7E3E9188, 0xA5CED43B, 0x0007,  // 1e-325
    0x828675B9, 0x52064CAC, 0x5DCE35EA, 0xCF42894A, 0x000A,  // 1e-324
    0xD1940993, 0x7343EFEB, 0x7AA0E1B2, 0x818995CE, 0x000E,  // 1e-323
    0xC5F90BF8, 0x1014EBE6, 0x19491A1F, 0xA1EBFB42, 0x0011,  // 1e-322
    0x77774EF6, 0xD41A26E0, 0x9F9B60A6, 0xCA66FA12, 0x0014,  // 1e-321
    0x955522B4, 0x8920B098, 0x478238D0, 0xFD00B897, 0x0017,  // 1e-320
    0x5D5535B0, 0x55B46E5F, 0x8CB16382, 0x9E20735E, 0x001B,  // 1e-319
    0x34AA831D, 0xEB2189F7, 0x2FDDBC62, 0xC5A89036, 0x001E,  // 1e-318
    0x01D523E4, 0xA5E9EC75, 0xBBD52B7B, 0xF712B443, 0x0021,  // 1e-317
    0x2125366E, 0x47B233C9, 0x55653B2D, 0x9A6BB0AA, 0x0025,  // 1e-316
    0x696E840A, 0x999EC0BB, 0xEABE89F8, 0xC1069CD4, 0x0028,  // 1e-315
    0x43CA250D, 0xC00670EA, 0x256E2C76, 0xF148440A, 0x002B,  // 1e-314
    0x6A5E5728, 0x38040692, 0x5764DBCA, 0x96CD2A86, 0x002F,  // 1e-313
    0x04F5ECF2, 0xC6050837, 0xED3E12BC, 0xBC807527, 0x0032,  // 1e-312
    0xC633682E, 0xF7864A44, 0xE88D976B, 0xEBA09271, 0x0035,  // 1e-311
    0xFBE0211D, 0x7AB3EE6A, 0x31587EA3, 0x93445B87, 0x0039,  // 1e-310
    0xBAD82964, 0x5960EA05, 0xFDAE9E4C, 0xB8157268, 0x003C,  // 1e-309
    0x298E33BD, 0x6FB92487, 0x3D1A45DF, 0xE61ACF03, 0x003F,  // 1e-308
    0x79F8E056, 0xA5D3B6D4, 0x06306BAB, 0x8FD0C162, 0x0043,  // 1e-307
    0x9877186C, 0x8F48A489, 0x87BC8696, 0xB3C4F1BA, 0x0046,  // 1e-306
    0xFE94DE87, 0x331ACDAB, 0x29ABA83C, 0xE0B62E29, 0x0049,  // 1e-305
    0x7F1D0B14, 0x9FF0C08B, 0xBA0B4925, 0x8C71DCD9, 0x004D,  // 1e-304
    0x5EE44DD9, 0x07ECF0AE, 0x288E1B6F, 0xAF8E5410, 0x0050,  // 1e-303
    0xF69D6150, 0xC9E82CD9, 0x32B1A24A, 0xDB71E914, 0x0053,  // 1e-302
    0x3A225CD2, 0xBE311C08, 0x9FAF056E, 0x892731AC, 0x0057,  // 1e-301
    0x48AAF406, 0x6DBD630A, 0xC79AC6CA, 0xAB70FE17, 0x005A,  // 1e-300
    0xDAD5B108, 0x092CBBCC, 0xB981787D, 0xD64D3D9D, 0x005D,  // 1e-299
    0x08C58EA5, 0x25BBF560, 0x93F0EB4E, 0x85F04682, 0x0061,  // 1e-298
    0x0AF6F24E, 0xAF2AF2B8, 0x38ED2621, 0xA76C5823, 0x0064,  // 1e-297
    0x0DB4AEE1, 0x1AF5AF66, 0x07286FAA, 0xD1476E2C, 0x0067,  // 1e-296
    0xC890ED4D, 0x50D98D9F, 0x847945CA, 0x82CCA4DB, 0x006B,  // 1e-295
    0xBAB528A0, 0xE50FF107, 0x6597973C, 0xA37FCE12, 0x006E,  // 1e-294
    0xA96272C8, 0x1E53ED49, 0xFEFD7D0C, 0xCC5FC196, 0x0071,  // 1e-293
    0x13BB0F7A, 0x25E8E89C, 0xBEBCDC4F, 0xFF77B1FC, 0x0074,  // 1e-292
    0x8C54E9AC, 0x77B19161, 0xF73609B1, 0x9FAACF3D, 0x0078,  // 1e-291
    0xEF6A2417, 0xD59DF5B9, 0x75038C1D, 0xC795830D, 0x007B,  // 1e-290
    0x6B44AD1D, 0x4B057328, 0xD2446F25, 0xF97AE3D0, 0x007E,  // 1e-289
    0x430AEC32, 0x4EE367F9, 0x836AC577, 0x9BECCE62, 0x0082,  // 1e-288
    0x93CDA73F, 0x229C41F7, 0x244576D5, 0xC2E801FB, 0x0085,  // 1e-287
    0x78C1110F, 0x6B435275, 0xED56D48A, 0xF3A20279, 0x0088,  // 1e-286
    0x6B78AAA9, 0x830A1389, 0x345644D6, 0x9845418C, 0x008C,  // 1e-285
    0xC656D553, 0x23CC986B, 0x416BD60C, 0xBE5691EF, 0x008F,  // 1e-284
    0xB7EC8AA8, 0x2CBFBE86, 0x11C6CB8F, 0xEDEC366B, 0x0092,  // 1e-283
    0x32F3D6A9, 0x7BF7D714, 0xEB1C3F39, 0x94B3A202, 0x0096,  // 1e-282
    0x3FB0CC53, 0xDAF5CCD9, 0xA5E34F07, 0xB9E08A83, 0x0099,  // 1e-281
    0x8F9CFF68, 0xD1B3400F, 0x8F5C22C9, 0xE858AD24, 0x009C,  // 1e-280
    0xB9C21FA1, 0x23100809, 0xD99995BE, 0x91376C36, 0x00A0,  // 1e-279
    0x2832A78A, 0xABD40A0C, 0x8FFFFB2D, 0xB5854744, 0x00A3,  // 1e-278
    0x323F516C, 0x16C90C8F, 0xB3FFF9F9, 0xE2E69915, 0x00A6,  // 1e-277
    0x7F6792E3, 0xAE3DA7D9, 0x907FFC3B, 0x8DD01FAD, 0x00AA,  // 1e-276
    0xDF41779C, 0x99CD11CF, 0xF49FFB4A, 0xB1442798, 0x00AD,  // 1e-275
    0xD711D583, 0x40405643, 0x31C7FA1D, 0xDD95317F, 0x00B0,  // 1e-274
    0x666B2572, 0x482835EA, 0x7F1CFC52, 0x8A7D3EEF, 0x00B4,  // 1e-273
    0x0005EECF, 0xDA324365, 0x5EE43B66, 0xAD1C8EAB, 0x00B7,  // 1e-272
    0x40076A82, 0x90BED43E, 0x369D4A40, 0xD863B256, 0x00BA,  // 1e-271
    0xE804A291, 0x5A7744A6, 0xE2224E68, 0x873E4F75, 0x00BE,  // 1e-270
    0xA205CB36, 0x711515D0, 0x5AAAE202, 0xA90DE353, 0x00C1,  // 1e-269
    0xCA873E03, 0x0D5A5B44, 0x31559A83, 0xD3515C28, 0x00C4,  // 1e-268
    0xFE9486C2, 0xE858790A, 0x1ED58091, 0x8412D999, 0x00C8,  // 1e-267
    0xBE39A872, 0x626E974D, 0x668AE0B6, 0xA5178FFF, 0x00CB,  // 1e-266
    0x2DC8128F, 0xFB0A3D21, 0x402D98E3, 0xCE5D73FF, 0x00CE,  // 1e-265
    0xBC9D0B99, 0x7CE66634, 0x881C7F8E, 0x80FA687F, 0x00D2,  // 1e-264
    0xEBC44E80, 0x1C1FFFC1, 0x6A239F72, 0xA139029F, 0x00D5,  // 1e-263
    0x66B56220, 0xA327FFB2, 0x44AC874E, 0xC9874347, 0x00D8,  // 1e-262
    0x0062BAA8, 0x4BF1FF9F, 0x15D7A922, 0xFBE91419, 0x00DB,  // 1e-261
    0x603DB4A9, 0x6F773FC3, 0xADA6C9B5, 0x9D71AC8F, 0x00DF,  // 1e-260
    0x384D21D3, 0xCB550FB4, 0x99107C22, 0xC4CE17B3, 0x00E2,  // 1e-259
    0x46606A48, 0x7E2A53A1, 0x7F549B2B, 0xF6019DA0, 0x00E5,  // 1e-258
    0xCBFC426D, 0x2EDA7444, 0x4F94E0FB, 0x99C10284, 0x00E9,  // 1e-257
    0xFEFB5308, 0xFA911155, 0x637A1939, 0xC0314325, 0x00EC,  // 1e-256
    0x7EBA27CA, 0x793555AB, 0xBC589F88, 0xF03D93EE, 0x00EF,  // 1e-255
    0x2F3458DE, 0x4BC1558B, 0x35B763B5, 0x96267C75, 0x00F3,  // 1e-254
    0xFB016F16, 0x9EB1AAED, 0x83253CA2, 0xBBB01B92, 0x00F6,  // 1e-253
    0x79C1CADC, 0x465E15A9, 0x23EE8BCB, 0xEA9C2277, 0x00F9,  // 1e-252
    0xEC191EC9, 0x0BFACD89, 0x7675175F, 0x92A1958A, 0x00FD,  // 1e-251
    0x671F667B, 0xCEF980EC, 0x14125D36, 0xB749FAED, 0x0100,  // 1e-250
    0x80E7401A, 0x82B7E127, 0x5916F484, 0xE51C79A8, 0x0103,  // 1e-249
    0xB0908810, 0xD1B2ECB8, 0x37AE58D2, 0x8F31CC09, 0x0107,  // 1e-248
    0xDCB4AA15, 0x861FA7E6, 0x8599EF07, 0xB2FE3F0B, 0x010A,  // 1e-247
    0x93E1D49A, 0x67A791E0, 0x67006AC9, 0xDFBDCECE, 0x010D,  // 1e-246
    0x5C6D24E0, 0xE0C8BB2C, 0x006042BD, 0x8BD6A141, 0x0111,  // 1e-245
    0x73886E18, 0x58FAE9F7, 0x4078536D, 0xAECC4991, 0x0114,  // 1e-244
    0x506A899E, 0xAF39A475, 0x90966848, 0xDA7F5BF5, 0x0117,  // 1e-243
    0x52429603, 0x6D8406C9, 0x7A5E012D, 0x888F9979, 0x011B,  // 1e-242
    0xA6D33B83, 0xC8E5087B, 0xD8F58178, 0xAAB37FD7, 0x011E,  // 1e-241
    0x90880A64, 0xFB1E4A9A, 0xCF32E1D6, 0xD5605FCD, 0x0121,  // 1e-240
    0x9A55067F, 0x5CF2EEA0, 0xA17FCD26, 0x855C3BE0, 0x0125,  // 1e-239
    0xC0EA481E, 0xF42FAA48, 0xC9DFC06F, 0xA6B34AD8, 0x0128,  // 1e-238
    0xF124DA26, 0xF13B94DA, 0xFC57B08B, 0xD0601D8E, 0x012B,  // 1e-237
    0xD6B70858, 0x76C53D08, 0x5DB6CE57, 0x823C1279, 0x012F,  // 1e-236
    0x0C64CA6E, 0x54768C4B, 0xB52481ED, 0xA2CB1717, 0x0132,  // 1e-235
    0xCF7DFD09, 0xA9942F5D, 0xA26DA268, 0xCB7DDCDD, 0x0135,  // 1e-234
    0x435D7C4C, 0xD3F93B35, 0x0B090B02, 0xFE5D5415, 0x0138,  // 1e-233
    0x4A1A6DAF, 0xC47BC501, 0x26E5A6E1, 0x9EFA548D, 0x013C,  // 1e-232
    0x9CA1091B, 0x359AB641, 0x709F109A, 0xC6B8E9B0, 0x013F,  // 1e-231
    0x03C94B62, 0xC30163D2, 0x8CC6D4C0, 0xF867241C, 0x0142,  // 1e-230
    0x425DCF1D, 0x79E0DE63, 0xD7FC44F8, 0x9B407691, 0x0146,  // 1e-229
    0x12F542E4, 0x985915FC, 0x4DFB5636, 0xC2109436, 0x0149,  // 1e-228
    0x17B2939D, 0x3E6F5B7B, 0xE17A2BC4, 0xF294B943, 0x014C,  // 1e-227
    0xEECF9C42, 0xA705992C, 0x6CEC5B5A, 0x979CF3CA, 0x0150,  // 1e-226
    0x2A838353, 0x50C6FF78, 0x08277231, 0xBD8430BD, 0x0153,  // 1e-225
    0x35246428, 0xA4F8BF56, 0x4A314EBD, 0xECE53CEC, 0x0156,  // 1e-224
    0xE136BE99, 0x871B7795, 0xAE5ED136, 0x940F4613, 0x015A,  // 1e-223
    0x59846E3F, 0x28E2557B, 0x99F68584, 0xB9131798, 0x015D,  // 1e-222
    0x2FE589CF, 0x331AEADA, 0xC07426E5, 0xE757DD7E, 0x0160,  // 1e-221
    0x5DEF7621, 0x3FF0D2C8, 0x3848984F, 0x9096EA6F, 0x0164,  // 1e-220
    0x756B53A9, 0x0FED077A, 0x065ABE63, 0xB4BCA50B, 0x0167,  // 1e-219
    0x12C62894, 0xD3E84959, 0xC7F16DFB, 0xE1EBCE4D, 0x016A,  // 1e-218
    0xABBBD95C, 0x64712DD7, 0x9CF6E4BD, 0x8D3360F0, 0x016E,  // 1e-217
    0x96AACFB3, 0xBD8D794D, 0xC4349DEC, 0xB080392C, 0x0171,  // 1e-216
    0xFC5583A0, 0xECF0D7A0, 0xF541C567, 0xDCA04777, 0x0174,  // 1e-215
    0x9DB57244, 0xF41686C4, 0xF9491B60, 0x89E42CAA, 0x0178,  // 1e-214
    0xC522CED5, 0x311C2875, 0xB79B6239, 0xAC5D37D5, 0x017B,  // 1e-213
    0x366B828B, 0x7D633293, 0x25823AC7, 0xD77485CB, 0x017E,  // 1e-212
    0x02033197, 0xAE5DFF9C, 0xF77164BC, 0x86A8D39E, 0x0182,  // 1e-211
    0x0283FDFC, 0xD9F57F83, 0xB54DBDEB, 0xA8530886, 0x0185,  // 1e-210
    0xC324FD7B, 0xD072DF63, 0x62A12D66, 0xD267CAA8, 0x0188,  // 1e-209
    0x59F71E6D, 0x4247CB9E, 0x3DA4BC60, 0x8380DEA9, 0x018C,  // 1e-208
    0xF074E608, 0x52D9BE85, 0x8D0DEB78, 0xA4611653, 0x018F,  // 1e-207
    0x6C921F8B, 0x67902E27, 0x70516656, 0xCD795BE8, 0x0192,  // 1e-206
    0xA3DB53B6, 0x00BA1CD8, 0x4632DFF6, 0x806BD971, 0x0196,  // 1e-205
    0xCCD228A4, 0x80E8A40E, 0x97BF97F3, 0xA086CFCD, 0x0199,  // 1e-204
    0x8006B2CD, 0x6122CD12, 0xFDAF7DF0, 0xC8A883C0, 0x019C,  // 1e-203
    0x20085F81, 0x796B8057, 0x3D1B5D6C, 0xFAD2A4B1, 0x019F,  // 1e-202
    0x74053BB0, 0xCBE33036, 0xC6311A63, 0x9CC3A6EE, 0x01A3,  // 1e-201
    0x11068A9C, 0xBEDBFC44, 0x77BD60FC, 0xC3F490AA, 0x01A6,  // 1e-200
    0x15482D44, 0xEE92FB55, 0x15ACB93B, 0xF4F1B4D5, 0x01A9,  // 1e-199
    0x2D4D1C4A, 0x751BDD15, 0x2D8BF3C5, 0x99171105, 0x01AD,  // 1e-198
    0x78A0635D, 0xD262D45A, 0x78EEF0B6, 0xBF5CD546, 0x01B0,  // 1e-197
    0x16C87C34, 0x86FB8971, 0x172AACE4, 0xEF340A98, 0x01B3,  // 1e-196
    0xAE3D4DA0, 0xD45D35E6, 0x0E7AAC0E, 0x9580869F, 0x01B7,  // 1e-195
    0x59CCA109, 0x89748360, 0xD2195712, 0xBAE0A846, 0x01BA,  // 1e-194
    0x703FC94B, 0x2BD1A438, 0x869FACD7, 0xE998D258, 0x01BD,  // 1e-193
    0x4627DDCF, 0x7B6306A3, 0x5423CC06, 0x91FF8377, 0x01C1,  // 1e-192
    0x17B1D542, 0x1A3BC84C, 0x292CBF08, 0xB67F6455, 0x01C4,  // 1e-191
    0x1D9E4A93, 0x20CABA5F, 0x7377EECA, 0xE41F3D6A, 0x01C7,  // 1e-190
    0x7282EE9C, 0x547EB47B, 0x882AF53E, 0x8E938662, 0x01CB,  // 1e-189
    0x4F23AA43, 0xE99E619A, 0x2A35B28D, 0xB23867FB, 0x01CE,  // 1e-188
    0xE2EC94D4, 0x6405FA00, 0xF4C31F31, 0xDEC681F9, 0x01D1,  // 1e-187
    0x8DD3DD04, 0xDE83BC40, 0x38F9F37E, 0x8B3C113C, 0x01D5,  // 1e-186
    0xB148D445, 0x9624AB50, 0x4738705E, 0xAE0B158B, 0x01D8,  // 1e-185
    0xDD9B0957, 0x3BADD624, 0x19068C76, 0xD98DDAEE, 0x01DB,  // 1e-184
    0x0A80E5D6, 0xE54CA5D7, 0xCFA417C9, 0x87F8A8D4, 0x01DF,  // 1e-183
    0xCD211F4C, 0x5E9FCF4C, 0x038D1DBC, 0xA9F6D30A, 0x01E2,  // 1e-182
    0x0069671F, 0x7647C320, 0x8470652B, 0xD47487CC, 0x01E5,  // 1e-181
    0x0041E073, 0x29ECD9F4, 0xD2C63F3B, 0x84C8D4DF, 0x01E9,  // 1e-180
    0x00525890, 0xF4681071, 0xC777CF09, 0xA5FB0A17, 0x01EC,  // 1e-179
    0x4066EEB4, 0x7182148D, 0xB955C2CC, 0xCF79CC9D, 0x01EF,  // 1e-178
    0x48405530, 0xC6F14CD8, 0x93D599BF, 0x81AC1FE2, 0x01F3,  // 1e-177
    0x5A506A7C, 0xB8ADA00E, 0x38CB002F, 0xA21727DB, 0x01F6,  // 1e-176
    0xF0E4851C, 0xA6D90811, 0x06FDC03B, 0xCA9CF1D2, 0x01F9,  // 1e-175
    0x6D1DA663, 0x908F4A16, 0x88BD304A, 0xFD442E46, 0x01FC,  // 1e-174
    0x043287FE, 0x9A598E4E, 0x15763E2E, 0x9E4A9CEC, 0x0200,  // 1e-173
    0x853F29FD, 0x40EFF1E1, 0x1AD3CDBA, 0xC5DD4427, 0x0203,  // 1e-172
    0xE68EF47C, 0xD12BEE59, 0xE188C128, 0xF7549530, 0x0206,  // 1e-171
    0x301958CE, 0x82BB74F8, 0x8CF578B9, 0x9A94DD3E, 0x020A,  // 1e-170
    0x3C1FAF01, 0xE36A5236, 0x3032D6E7, 0xC13A148E, 0x020D,  // 1e-169
    0xCB279AC1, 0xDC44E6C3, 0xBC3F8CA1, 0xF18899B1, 0x0210,  // 1e-168
    0x5EF8C0B9, 0x29AB103A, 0x15A7B7E5, 0x96F5600F, 0x0214,  // 1e-167
    0xF6B6F0E7, 0x7415D448, 0xDB11A5DE, 0xBCB2B812, 0x0217,  // 1e-166
    0x3464AD21, 0x111B495B, 0x91D60F56, 0xEBDF6617, 0x021A,  // 1e-165
    0x00BEEC34, 0xCAB10DD9, 0xBB25C995, 0x936B9FCE, 0x021E,  // 1e-164
    0x40EEA742, 0x3D5D514F, 0x69EF3BFB, 0xB84687C2, 0x0221,  // 1e-163
    0x112A5112, 0x0CB4A5A3, 0x046B0AFA, 0xE65829B3, 0x0224,  // 1e-162
    0xEABA72AB, 0x47F0E785, 0xE2C2E6DC, 0x8FF71A0F, 0x0228,  // 1e-161
    0x65690F56, 0x59ED2167, 0xDB73A093, 0xB3F4E093, 0x022B,  // 1e-160
    0x3EC3532C, 0x306869C1, 0xD25088B8, 0xE0F218B8, 0x022E,  // 1e-159
    0xC73A13FB, 0x1E414218, 0x83725573, 0x8C974F73, 0x0232,  // 1e-158
    0xF90898FA, 0xE5D1929E, 0x644EEACF, 0xAFBD2350, 0x0235,  // 1e-157
    0xB74ABF39, 0xDF45F746, 0x7D62A583, 0xDBAC6C24, 0x0238,  // 1e-156
    0x328EB783, 0x6B8BBA8C, 0xCE5DA772, 0x894BC396, 0x023C,  // 1e-155
    0x3F326564, 0x066EA92F, 0x81F5114F, 0xAB9EB47C, 0x023F,  // 1e-154
    0x0EFEFEBD, 0xC80A537B, 0xA27255A2, 0xD686619B, 0x0242,  // 1e-153
    0xE95F5F36, 0xBD06742C, 0x45877585, 0x8613FD01, 0x0246,  // 1e-152
    0x23B73704, 0x2C481138, 0x96E952E7, 0xA798FC41, 0x0249,  // 1e-151
    0x2CA504C5, 0xF75A1586, 0xFCA3A7A0, 0xD17F3B51, 0x024C,  // 1e-150
    0xDBE722FB, 0x9A984D73, 0x3DE648C4, 0x82EF8513, 0x0250,  // 1e-149
    0xD2E0EBBA, 0xC13E60D0, 0x0D5FDAF5, 0xA3AB6658, 0x0253,  // 1e-148
    0x079926A8, 0x318DF905, 0x10B7D1B3, 0xCC963FEE, 0x0256,  // 1e-147
    0x497F7052, 0xFDF17746, 0x94E5C61F, 0xFFBBCFE9, 0x0259,  // 1e-146
    0xEDEFA633, 0xFEB6EA8B, 0xFD0F9BD3, 0x9FD561F1, 0x025D,  // 1e-145
    0xE96B8FC0, 0xFE64A52E, 0x7C5382C8, 0xC7CABA6E, 0x0260,  // 1e-144
    0xA3C673B0, 0x3DFDCE7A, 0x1B68637B, 0xF9BD690A, 0x0263,  // 1e-143
    0xA65C084E, 0x06BEA10C, 0x51213E2D, 0x9C1661A6, 0x0267,  // 1e-142
    0xCFF30A62, 0x486E494F, 0xE5698DB8, 0xC31BFA0F, 0x026A,  // 1e-141
    0xC3EFCCFA, 0x5A89DBA3, 0xDEC3F126, 0xF3E2F893, 0x026D,  // 1e-140
    0x5A75E01C, 0xF8962946, 0x6B3A76B7, 0x986DDB5C, 0x0271,  // 1e-139
    0xF1135823, 0xF6BBB397, 0x86091465, 0xBE895233, 0x0274,  // 1e-138
    0xED582E2C, 0x746AA07D, 0x678B597F, 0xEE2BA6C0, 0x0277,  // 1e-137
    0xB4571CDC, 0xA8C2A44E, 0x40B717EF, 0x94DB4838, 0x027B,  // 1e-136
    0x616CE413, 0x92F34D62, 0x50E4DDEB, 0xBA121A46, 0x027E,  // 1e-135
    0xF9C81D17, 0x77B020BA, 0xE51E1566, 0xE896A0D7, 0x0281,  // 1e-134
    0xDC1D122E, 0x0ACE1474, 0xEF32CD60, 0x915E2486, 0x0285,  // 1e-133
    0x132456BA, 0x0D819992, 0xAAFF80B8, 0xB5B5ADA8, 0x0288,  // 1e-132
    0x97ED6C69, 0x10E1FFF6, 0xD5BF60E6, 0xE3231912, 0x028B,  // 1e-131
    0x1EF463C1, 0xCA8D3FFA, 0xC5979C8F, 0x8DF5EFAB, 0x028F,  // 1e-130
    0xA6B17CB2, 0xBD308FF8, 0xB6FD83B3, 0xB1736B96, 0x0292,  // 1e-129
    0xD05DDBDE, 0xAC7CB3F6, 0x64BCE4A0, 0xDDD0467C, 0x0295,  // 1e-128
    0x423AA96B, 0x6BCDF07A, 0xBEF60EE4, 0x8AA22C0D, 0x0299,  // 1e-127
    0xD2C953C6, 0x86C16C98, 0x2EB3929D, 0xAD4AB711, 0x029C,  // 1e-126
    0x077BA8B7, 0xE871C7BF, 0x7A607744, 0xD89D64D5, 0x029F,  // 1e-125
    0x64AD4972, 0x11471CD7, 0x6C7C4A8B, 0x87625F05, 0x02A3,  // 1e-124
    0x3DD89BCF, 0xD598E40D, 0xC79B5D2D, 0xA93AF6C6, 0x02A6,  // 1e-123
    0x8D4EC2C3, 0x4AFF1D10, 0x79823479, 0xD389B478, 0x02A9,  // 1e-122
    0x585139BA, 0xCEDF722A, 0x4BF160CB, 0x843610CB, 0x02AD,  // 1e-121
    0xEE658828, 0xC2974EB4, 0x1EEDB8FE, 0xA54394FE, 0x02B0,  // 1e-120
    0x29FEEA32, 0x733D2262, 0xA6A9273E, 0xCE947A3D, 0x02B3,  // 1e-119
    0x5A3F525F, 0x0806357D, 0x8829B887, 0x811CCC66, 0x02B7,  // 1e-118
    0xB0CF26F7, 0xCA07C2DC, 0x2A3426A8, 0xA163FF80, 0x02BA,  // 1e-117
    0xDD02F0B5, 0xFC89B393, 0x34C13052, 0xC9BCFF60, 0x02BD,  // 1e-116
    0xD443ACE2, 0xBBAC2078, 0x41F17C67, 0xFC2C3F38, 0x02C0,  // 1e-115
    0x84AA4C0D, 0xD54B944B, 0x2936EDC0, 0x9D9BA783, 0x02C4,  // 1e-114
    0x65D4DF11, 0x0A9E795E, 0xF384A931, 0xC5029163, 0x02C7,  // 1e-113
    0xFF4A16D5, 0x4D4617B5, 0xF065D37D, 0xF64335BC, 0x02CA,  // 1e-112
    0xBF8E4E45, 0x504BCED1, 0x163FA42E, 0x99EA0196, 0x02CE,  // 1e-111
    0x2F71E1D6, 0xE45EC286, 0x9BCF8D39, 0xC06481FB, 0x02D1,  // 1e-110
    0xBB4E5A4C, 0x5D767327, 0x82C37088, 0xF07DA27A, 0x02D4,  // 1e-109
    0xD510F86F, 0x3A6A07F8, 0x91BA2655, 0x964E858C, 0x02D8,  // 1e-108
    0x0A55368B, 0x890489F7, 0xB628AFEA, 0xBBE226EF, 0x02DB,  // 1e-107
    0xCCEA842E, 0x2B45AC74, 0xA3B2DBE5, 0xEADAB0AB, 0x02DE,  // 1e-106
    0x0012929D, 0x3B0B8BC9, 0x464FC96F, 0x92C8AE6B, 0x02E2,  // 1e-105
    0x40173744, 0x09CE6EBB, 0x17E3BBCB, 0xB77ADA06, 0x02E5,  // 1e-104
    0x101D0515, 0xCC420A6A, 0x9DDCAABD, 0xE5599087, 0x02E8,  // 1e-103
    0x4A12232D, 0x9FA94682, 0xC2A9EAB6, 0x8F57FA54, 0x02EC,  // 1e-102
    0xDC96ABF9, 0x47939822, 0xF3546564, 0xB32DF8E9, 0x02EF,  // 1e-101
    0x93BC56F7, 0x59787E2B, 0x70297EBD, 0xDFF97724, 0x02F2,  // 1e-100
    0x3C55B65A, 0x57EB4EDB, 0xC619EF36, 0x8BFBEA76, 0x02F6,  // 1e-99
    0x0B6B23F1, 0xEDE62292, 0x77A06B03, 0xAEFAE514, 0x02F9,  // 1e-98
    0x8E45ECED, 0xE95FAB36, 0x958885C4, 0xDAB99E59, 0x02FC,  // 1e-97
    0x18EBB414, 0x11DBCB02, 0xFD75539B, 0x88B402F7, 0x0300,  // 1e-96
    0x9F26A119, 0xD652BDC2, 0xFCD2A881, 0xAAE103B5, 0x0303,  // 1e-95
    0x46F0495F, 0x4BE76D33, 0x7C0752A2, 0xD59944A3, 0x0306,  // 1e-94
    0x0C562DDB, 0x6F70A440, 0x2D8493A5, 0x857FCAE6, 0x030A,  // 1e-93
    0x0F6BB952, 0xCB4CCD50, 0xB8E5B88E, 0xA6DFBD9F, 0x030D,  // 1e-92
    0x1346A7A7, 0x7E2000A4, 0xA71F26B2, 0xD097AD07, 0x0310,  // 1e-91
    0x8C0C28C8, 0x8ED40066, 0xC873782F, 0x825ECC24, 0x0314,  // 1e-90
    0x2F0F32FA, 0x72890080, 0xFA90563B, 0xA2F67F2D, 0x0317,  // 1e-89
    0x3AD2FFB9, 0x4F2B40A0, 0x79346BCA, 0xCBB41EF9, 0x031A,  // 1e-88
    0x4987BFA8, 0xE2F610C8, 0xD78186BC, 0xFEA126B7, 0x031D,  // 1e-87
    0x2DF4D7C9, 0x0DD9CA7D, 0xE6B0F436, 0x9F24B832, 0x0321,  // 1e-86
    0x79720DBB, 0x91503D1C, 0xA05D3143, 0xC6EDE63F, 0x0324,  // 1e-85
    0x97CE912A, 0x75A44C63, 0x88747D94, 0xF8A95FCF, 0x0327,  // 1e-84
    0x3EE11ABA, 0xC986AFBE, 0xB548CE7C, 0x9B69DBE1, 0x032B,  // 1e-83
    0xCE996168, 0xFBE85BAD, 0x229B021B, 0xC24452DA, 0x032E,  // 1e-82
    0x423FB9C3, 0xFAE27299, 0xAB41C2A2, 0xF2D56790, 0x0331,  // 1e-81
    0xC967D41A, 0xDCCD879F, 0x6B0919A5, 0x97C560BA, 0x0335,  // 1e-80
    0xBBC1C920, 0x5400E987, 0x05CB600F, 0xBDB6B8E9, 0x0338,  // 1e-79
    0xAAB23B68, 0x290123E9, 0x473E3813, 0xED246723, 0x033B,  // 1e-78
    0x0AAF6521, 0xF9A0B672, 0x0C86E30B, 0x9436C076, 0x033F,  // 1e-77
    0x8D5B3E69, 0xF808E40E, 0x8FA89BCE, 0xB9447093, 0x0342,  // 1e-76
    0x30B20E04, 0xB60B1D12, 0x7392C2C2, 0xE7958CB8, 0x0345,  // 1e-75
    0x5E6F48C2, 0xB1C6F22B, 0x483BB9B9, 0x90BD77F3, 0x0349,  // 1e-74
    0x360B1AF3, 0x1E38AEB6, 0x1A4AA828, 0xB4ECD5F0, 0x034C,  // 1e-73
    0xC38DE1B0, 0x25C6DA63, 0x20DD5232, 0xE2280B6C, 0x034F,  // 1e-72
    0x5A38AD0E, 0x579C487E, 0x948A535F, 0x8D590723, 0x0353,  // 1e-71
    0xF0C6D851, 0x2D835A9D, 0x79ACE837, 0xB0AF48EC, 0x0356,  // 1e-70
    0x6CF88E65, 0xF8E43145, 0x98182244, 0xDCDB1B27, 0x0359,  // 1e-69
    0x641B58FF, 0x1B8E9ECB, 0xBF0F156B, 0x8A08F0F8, 0x035D,  // 1e-68
    0x3D222F3F, 0xE272467E, 0xEED2DAC5, 0xAC8B2D36, 0x0360,  // 1e-67
    0xCC6ABB0F, 0x5B0ED81D, 0xAA879177, 0xD7ADF884, 0x0363,  // 1e-66
    0x9FC2B4E9, 0x98E94712, 0xEA94BAEA, 0x86CCBB52, 0x0367,  // 1e-65
    0x47B36224, 0x3F2398D7, 0xA539E9A5, 0xA87FEA27, 0x036A,  // 1e-64
    0x19A03AAD, 0x8EEC7F0D, 0x8E88640E, 0xD29FE4B1, 0x036D,  // 1e-63
    0x300424AC, 0x1953CF68, 0xF9153E89, 0x83A3EEEE, 0x0371,  // 1e-62
    0x3C052DD7, 0x5FA8C342, 0xB75A8E2B, 0xA48CEAAA, 0x0374,  // 1e-61
    0xCB06794D, 0x3792F412, 0x653131B6, 0xCDB02555, 0x0377,  // 1e-60
    0xBEE40BD0, 0xE2BBD88B, 0x5F3EBF11, 0x808E1755, 0x037B,  // 1e-59
    0xAE9D0EC4, 0x5B6ACEAE, 0xB70E6ED6, 0xA0B19D2A, 0x037E,  // 1e-58
    0x5A445275, 0xF245825A, 0x64D20A8B, 0xC8DE0475, 0x0381,  // 1e-57
    0xF0D56712, 0xEED6E2F0, 0xBE068D2E, 0xFB158592, 0x0384,  // 1e-56
    0x9685606B, 0x55464DD6, 0xB6C4183D, 0x9CED737B, 0x0388,  // 1e-55
    0x3C26B886, 0xAA97E14C, 0xA4751E4C, 0xC428D05A, 0x038B,  // 1e-54
    0x4B3066A8, 0xD53DD99F, 0x4D9265DF, 0xF5330471, 0x038E,  // 1e-53
    0x8EFE4029, 0xE546A803, 0xD07B7FAB, 0x993FE2C6, 0x0392,  // 1e-52
    0x72BDD033, 0xDE985204, 0x849A5F96, 0xBF8FDB78, 0x0395,  // 1e-51
    0x8F6D4440, 0x963E6685, 0xA5C0F77C, 0xEF73D256, 0x0398,  // 1e-50
    0x79A44AA8, 0xDDE70013, 0x27989AAD, 0x95A86376, 0x039C,  // 1e-49
    0x580D5D52, 0x5560C018, 0xB17EC159, 0xBB127C53, 0x039F,  // 1e-48
    0x6E10B4A6, 0xAAB8F01E, 0x9DDE71AF, 0xE9D71B68, 0x03A2,  // 1e-47
    0x04CA70E8, 0xCAB39613, 0x62AB070D, 0x92267121, 0x03A6,  // 1e-46
    0xC5FD0D22, 0x3D607B97, 0xBB55C8D1, 0xB6B00D69, 0x03A9,  // 1e-45
    0xB77C506A, 0x8CB89A7D, 0x2A2B3B05, 0xE45C10C4, 0x03AC,  // 1e-44
    0x92ADB242, 0x77F3608E, 0x9A5B04E3, 0x8EB98A7A, 0x03B0,  // 1e-43
    0x37591ED3, 0x55F038B2, 0x40F1C61C, 0xB267ED19, 0x03B3,  // 1e-42
    0xC52F6688, 0x6B6C46DE, 0x912E37A3, 0xDF01E85F, 0x03B6,  // 1e-41
    0x3B3DA015, 0x2323AC4B, 0xBABCE2C6, 0x8B61313B, 0x03BA,  // 1e-40
    0x0A0D081A, 0xABEC975E, 0xA96C1B77, 0xAE397D8A, 0x03BD,  // 1e-39
    0x8C904A21, 0x96E7BD35, 0x53C72255, 0xD9C7DCED, 0x03C0,  // 1e-38
    0x77DA2E54, 0x7E50D641, 0x545C7575, 0x881CEA14, 0x03C4,  // 1e-37
    0xD5D0B9E9, 0xDDE50BD1, 0x697392D2, 0xAA242499, 0x03C7,  // 1e-36
    0x4B44E864, 0x955E4EC6, 0xC3D07787, 0xD4AD2DBF, 0x03CA,  // 1e-35
    0xEF0B113E, 0xBD5AF13B, 0xDA624AB4, 0x84EC3C97, 0x03CE,  // 1e-34
    0xEACDD58E, 0xECB1AD8A, 0xD0FADD61, 0xA6274BBD, 0x03D1,  // 1e-33
    0xA5814AF2, 0x67DE18ED, 0x453994BA, 0xCFB11EAD, 0x03D4,  // 1e-32
    0x8770CED7, 0x80EACF94, 0x4B43FCF4, 0x81CEB32C, 0x03D8,  // 1e-31
    0xA94D028D, 0xA1258379, 0x5E14FC31, 0xA2425FF7, 0x03DB,  // 1e-30
    0x13A04330, 0x096EE458, 0x359A3B3E, 0xCAD2F7F5, 0x03DE,  // 1e-29
    0x188853FC, 0x8BCA9D6E, 0x8300CA0D, 0xFD87B5F2, 0x03E1,  // 1e-28
    0xCF55347D, 0x775EA264, 0x91E07E48, 0x9E74D1B7, 0x03E5,  // 1e-27
    0x032A819D, 0x95364AFE, 0x76589DDA, 0xC6120625, 0x03E8,  // 1e-26
    0x83F52204, 0x3A83DDBD, 0xD3EEC551, 0xF79687AE, 0x03EB,  // 1e-25
    0x72793542, 0xC4926A96, 0x44753B52, 0x9ABE14CD, 0x03EF,  // 1e-24
    0x0F178293, 0x75B7053C, 0x95928A27, 0xC16D9A00, 0x03F2,  // 1e-23
    0x12DD6338, 0x5324C68B, 0xBAF72CB1, 0xF1C90080, 0x03F5,  // 1e-22
    0xEBCA5E03, 0xD3F6FC16, 0x74DA7BEE, 0x971DA050, 0x03F9,  // 1e-21
    0xA6BCF584, 0x88F4BB1C, 0x92111AEA, 0xBCE50864, 0x03FC,  // 1e-20
    0xD06C32E5, 0x2B31E9E3, 0xB69561A5, 0xEC1E4A7D, 0x03FF,  // 1e-19
    0x62439FCF, 0x3AFF322E, 0x921D5D07, 0x9392EE8E, 0x0403,  // 1e-18
    0xFAD487C2, 0x09BEFEB9, 0x36A4B449, 0xB877AA32, 0x0406,  // 1e-17
    0x7989A9B3, 0x4C2EBE68, 0xC44DE15B, 0xE69594BE, 0x0409,  // 1e-16
    0x4BF60A10, 0x0F9D3701, 0x3AB0ACD9, 0x901D7CF7, 0x040D,  // 1e-15
    0x9EF38C94, 0x538484C1, 0x095CD80F, 0xB424DC35, 0x0410,  // 1e-14
    0x06B06FB9, 0x2865A5F2, 0x4BB40E13, 0xE12E1342, 0x0413,  // 1e-13
    0x442E45D3, 0xF93F87B7, 0x6F5088CB, 0x8CBCCC09, 0x0417,  // 1e-12
    0x1539D748, 0xF78F69A5, 0xCB24AAFE, 0xAFEBFF0B, 0x041A,  // 1e-11
    0x5A884D1B, 0xB573440E, 0xBDEDD5BE, 0xDBE6FECE, 0x041D,  // 1e-10
    0xF8953030, 0x31680A88, 0x36B4A597, 0x89705F41, 0x0421,  // 1e-9
    0x36BA7C3D, 0xFDC20D2B, 0x8461CEFC, 0xABCC7711, 0x0424,  // 1e-8
    0x04691B4C, 0x3D329076, 0xE57A42BC, 0xD6BF94D5, 0x0427,  // 1e-7
    0xC2C1B10F, 0xA63F9A49, 0xAF6C69B5, 0x8637BD05, 0x042B,  // 1e-6
    0x33721D53, 0x0FCF80DC, 0x1B478423, 0xA7C5AC47, 0x042E,  // 1e-5
    0x404EA4A8, 0xD3C36113, 0xE219652B, 0xD1B71758, 0x0431,  // 1e-4
    0x083126E9, 0x645A1CAC, 0x8D4FDF3B, 0x83126E97, 0x0435,  // 1e-3
    0x0A3D70A3, 0x3D70A3D7, 0x70A3D70A, 0xA3D70A3D, 0x0438,  // 1e-2
    0xCCCCCCCC, 0xCCCCCCCC, 0xCCCCCCCC, 0xCCCCCCCC, 0x043B,  // 1e-1
    0x00000000, 0x00000000, 0x00000000, 0x80000000, 0x043F,  // 1e0
    0x00000000, 0x00000000, 0x00000000, 0xA0000000, 0x0442,  // 1e1
    0x00000000, 0x00000000, 0x00000000, 0xC8000000, 0x0445,  // 1e2
    0x00000000, 0x00000000, 0x00000000, 0xFA000000, 0x0448,  // 1e3
    0x00000000, 0x00000000, 0x00000000, 0x9C400000, 0x044C,  // 1e4
    0x00000000, 0x00000000, 0x00000000, 0xC3500000, 0x044F,  // 1e5
    0x00000000, 0x00000000, 0x00000000, 0xF4240000, 0x0452,  // 1e6
    0x00000000, 0x00000000, 0x00000000, 0x98968000, 0x0456,  // 1e7
    0x00000000, 0x00000000, 0x00000000, 0xBEBC2000, 0x0459,  // 1e8
    0x00000000, 0x00000000, 0x00000000, 0xEE6B2800, 0x045C,  // 1e9
    0x00000000, 0x00000000, 0x00000000, 0x9502F900, 0x0460,  // 1e10
    0x00000000, 0x00000000, 0x00000000, 0xBA43B740, 0x0463,  // 1e11
    0x00000000, 0x00000000, 0x00000000, 0xE8D4A510, 0x0466,  // 1e12
    0x00000000, 0x00000000, 0x00000000, 0x9184E72A, 0x046A,  // 1e13
    0x00000000, 0x00000000, 0x80000000, 0xB5E620F4, 0x046D,  // 1e14
    0x00000000, 0x00000000, 0xA0000000, 0xE35FA931, 0x0470,  // 1e15
    0x00000000, 0x00000000, 0x04000000, 0x8E1BC9BF, 0x0474,  // 1e16
    0x00000000, 0x00000000, 0xC5000000, 0xB1A2BC2E, 0x0477,  // 1e17
    0x00000000, 0x00000000, 0x76400000, 0xDE0B6B3A, 0x047A,  // 1e18
    0x00000000, 0x00000000, 0x89E80000, 0x8AC72304, 0x047E,  // 1e19
    0x00000000, 0x00000000, 0xAC620000, 0xAD78EBC5, 0x0481,  // 1e20
    0x00000000, 0x00000000, 0x177A8000, 0xD8D726B7, 0x0484,  // 1e21
    0x00000000, 0x00000000, 0x6EAC9000, 0x87867832, 0x0488,  // 1e22
    0x00000000, 0x00000000, 0x0A57B400, 0xA968163F, 0x048B,  // 1e23
    0x00000000, 0x00000000, 0xCCEDA100, 0xD3C21BCE, 0x048E,  // 1e24
    0x00000000, 0x00000000, 0x401484A0, 0x84595161, 0x0492,  // 1e25
    0x00000000, 0x00000000, 0x9019A5C8, 0xA56FA5B9, 0x0495,  // 1e26
    0x00000000, 0x00000000, 0xF4200F3A, 0xCECB8F27, 0x0498,  // 1e27
    0x00000000, 0x40000000, 0xF8940984, 0x813F3978, 0x049C,  // 1e28
    0x00000000, 0x50000000, 0x36B90BE5, 0xA18F07D7, 0x049F,  // 1e29
    0x00000000, 0xA4000000, 0x04674EDE, 0xC9F2C9CD, 0x04A2,  // 1e30
    0x00000000, 0x4D000000, 0x45812296, 0xFC6F7C40, 0x04A5,  // 1e31
    0x00000000, 0xF0200000, 0x2B70B59D, 0x9DC5ADA8, 0x04A9,  // 1e32
    0x00000000, 0x6C280000, 0x364CE305, 0xC5371912, 0x04AC,  // 1e33
    0x00000000, 0xC7320000, 0xC3E01BC6, 0xF684DF56, 0x04AF,  // 1e34
    0x00000000, 0x3C7F4000, 0x3A6C115C, 0x9A130B96, 0x04B3,  // 1e35
    0x00000000, 0x4B9F1000, 0xC90715B3, 0xC097CE7B, 0x04B6,  // 1e36
    0x00000000, 0x1E86D400, 0xBB48DB20, 0xF0BDC21A, 0x04B9,  // 1e37
    0x00000000, 0x13144480, 0xB50D88F4, 0x96769950, 0x04BD,  // 1e38
    0x00000000, 0x17D955A0, 0xE250EB31, 0xBC143FA4, 0x04C0,  // 1e39
    0x00000000, 0x5DCFAB08, 0x1AE525FD, 0xEB194F8E, 0x04C3,  // 1e40
    0x00000000, 0x5AA1CAE5, 0xD0CF37BE, 0x92EFD1B8, 0x04C7,  // 1e41
    0x40000000, 0xF14A3D9E, 0x050305AD, 0xB7ABC627, 0x04CA,  // 1e42
    0xD0000000, 0x6D9CCD05, 0xC643C719, 0xE596B7B0, 0x04CD,  // 1e43
    0xA2000000, 0xE4820023, 0x7BEA5C6F, 0x8F7E32CE, 0x04D1,  // 1e44
    0x8A800000, 0xDDA2802C, 0x1AE4F38B, 0xB35DBF82, 0x04D4,  // 1e45
    0xAD200000, 0xD50B2037, 0xA19E306E, 0xE0352F62, 0x04D7,  // 1e46
    0xCC340000, 0x4526F422, 0xA502DE45, 0x8C213D9D, 0x04DB,  // 1e47
    0x7F410000, 0x9670B12B, 0x0E4395D6, 0xAF298D05, 0x04DE,  // 1e48
    0x5F114000, 0x3C0CDD76, 0x51D47B4C, 0xDAF3F046, 0x04E1,  // 1e49
    0xFB6AC800, 0xA5880A69, 0xF324CD0F, 0x88D8762B, 0x04E5,  // 1e50
    0x7A457A00, 0x8EEA0D04, 0xEFEE0053, 0xAB0E93B6, 0x04E8,  // 1e51
    0x98D6D880, 0x72A49045, 0xABE98068, 0xD5D238A4, 0x04EB,  // 1e52
    0x7F864750, 0x47A6DA2B, 0xEB71F041, 0x85A36366, 0x04EF,  // 1e53
    0x5F67D924, 0x999090B6, 0xA64E6C51, 0xA70C3C40, 0x04F2,  // 1e54
    0xF741CF6D, 0xFFF4B4E3, 0xCFE20765, 0xD0CF4B50, 0x04F5,  // 1e55
    0x7A8921A4, 0xBFF8F10E, 0x81ED449F, 0x82818F12, 0x04F9,  // 1e56
    0x192B6A0D, 0xAFF72D52, 0x226895C7, 0xA321F2D7, 0x04FC,  // 1e57
    0x9F764490, 0x9BF4F8A6, 0xEB02BB39, 0xCBEA6F8C, 0x04FF,  // 1e58
    0x4753D5B4, 0x02F236D0, 0x25C36A08, 0xFEE50B70, 0x0502,  // 1e59
    0x2C946590, 0x01D76242, 0x179A2245, 0x9F4F2726, 0x0506,  // 1e60
    0xB7B97EF5, 0x424D3AD2, 0x9D80AAD6, 0xC722F0EF, 0x0509,  // 1e61
    0x65A7DEB2, 0xD2E08987, 0x84E0D58B, 0xF8EBAD2B, 0x050C,  // 1e62
    0x9F88EB2F, 0x63CC55F4, 0x330C8577, 0x9B934C3B, 0x0510,  // 1e63
    0xC76B25FB, 0x3CBF6B71, 0xFFCFA6D5, 0xC2781F49, 0x0513,  // 1e64
    0x3945EF7A, 0x8BEF464E, 0x7FC3908A, 0xF316271C, 0x0516,  // 1e65
    0xE3CBB5AC, 0x97758BF0, 0xCFDA3A56, 0x97EDD871, 0x051A,  // 1e66
    0x1CBEA317, 0x3D52EEED, 0x43D0C8EC, 0xBDE94E8E, 0x051D,  // 1e67
    0x63EE4BDD, 0x4CA7AAA8, 0xD4C4FB27, 0xED63A231, 0x0520,  // 1e68
    0x3E74EF6A, 0x8FE8CAA9, 0x24FB1CF8, 0x945E455F, 0x0524,  // 1e69
    0x8E122B44, 0xB3E2FD53, 0xEE39E436, 0xB975D6B6, 0x0527,  // 1e70
    0x7196B616, 0x60DBBCA8, 0xA9C85D44, 0xE7D34C64, 0x052A,  // 1e71
    0x46FE31CD, 0xBC8955E9, 0xEA1D3A4A, 0x90E40FBE, 0x052E,  // 1e72
    0x98BDBE41, 0x6BABAB63, 0xA4A488DD, 0xB51D13AE, 0x0531,  // 1e73
    0x7EED2DD1, 0xC696963C, 0x4DCDAB14, 0xE264589A, 0x0534,  // 1e74
    0xCF543CA2, 0xFC1E1DE5, 0x70A08AEC, 0x8D7EB760, 0x0538,  // 1e75
    0x43294BCB, 0x3B25A55F, 0x8CC8ADA8, 0xB0DE6538, 0x053B,  // 1e76
    0x13F39EBE, 0x49EF0EB7, 0xAFFAD912, 0xDD15FE86, 0x053E,  // 1e77
    0x6C784337, 0x6E356932, 0x2DFCC7AB, 0x8A2DBF14, 0x0542,  // 1e78
    0x07965404, 0x49C2C37F, 0x397BF996, 0xACB92ED9, 0x0545,  // 1e79
    0xC97BE906, 0xDC33745E, 0x87DAF7FB, 0xD7E77A8F, 0x0548,  // 1e80
    0x3DED71A3, 0x69A028BB, 0xB4E8DAFD, 0x86F0AC99, 0x054C,  // 1e81
    0x0D68CE0C, 0xC40832EA, 0x222311BC, 0xA8ACD7C0, 0x054F,  // 1e82
    0x90C30190, 0xF50A3FA4, 0x2AABD62B, 0xD2D80DB0, 0x0552,  // 1e83
    0xDA79E0FA, 0x792667C6, 0x1AAB65DB, 0x83C7088E, 0x0556,  // 1e84
    0x91185938, 0x577001B8, 0xA1563F52, 0xA4B8CAB1, 0x0559,  // 1e85
    0xB55E6F86, 0xED4C0226, 0x09ABCF26, 0xCDE6FD5E, 0x055C,  // 1e86
    0x315B05B4, 0x544F8158, 0xC60B6178, 0x80B05E5A, 0x0560,  // 1e87
    0x3DB1C721, 0x696361AE, 0x778E39D6, 0xA0DC75F1, 0x0563,  // 1e88
    0xCD1E38E9, 0x03BC3A19, 0xD571C84C, 0xC913936D, 0x0566,  // 1e89
    0x4065C723, 0x04AB48A0, 0x4ACE3A5F, 0xFB587849, 0x0569,  // 1e90
    0x283F9C76, 0x62EB0D64, 0xCEC0E47B, 0x9D174B2D, 0x056D,  // 1e91
    0x324F8394, 0x3BA5D0BD, 0x42711D9A, 0xC45D1DF9, 0x0570,  // 1e92
    0x7EE36479, 0xCA8F44EC, 0x930D6500, 0xF5746577, 0x0573,  // 1e93
    0xCF4E1ECB, 0x7E998B13, 0xBBE85F20, 0x9968BF6A, 0x0577,  // 1e94
    0xC321A67E, 0x9E3FEDD8, 0x6AE276E8, 0xBFC2EF45, 0x057A,  // 1e95
    0xF3EA101E, 0xC5CFE94E, 0xC59B14A2, 0xEFB3AB16, 0x057D,  // 1e96
    0x58724A12, 0xBBA1F1D1, 0x3B80ECE5, 0x95D04AEE, 0x0581,  // 1e97
    0xAE8EDC97, 0x2A8A6E45, 0xCA61281F, 0xBB445DA9, 0x0584,  // 1e98
    0x1A3293BD, 0xF52D09D7, 0x3CF97226, 0xEA157514, 0x0587,  // 1e99
    0x705F9C56, 0x593C2626, 0xA61BE758, 0x924D692C, 0x058B,  // 1e100
    0x0C77836C, 0x6F8B2FB0, 0xCFA2E12E, 0xB6E0C377, 0x058E,  // 1e101
    0x0F956447, 0x0B6DFB9C, 0xC38B997A, 0xE498F455, 0x0591,  // 1e102
    0x89BD5EAC, 0x4724BD41, 0x9A373FEC, 0x8EDF98B5, 0x0595,  // 1e103
    0xEC2CB657, 0x58EDEC91, 0x00C50FE7, 0xB2977EE3, 0x0598,  // 1e104
    0x6737E3ED, 0x2F2967B6, 0xC0F653E1, 0xDF3D5E9B, 0x059B,  // 1e105
    0x0082EE74, 0xBD79E0D2, 0x5899F46C, 0x8B865B21, 0x059F,  // 1e106
    0x80A3AA11, 0xECD85906, 0xAEC07187, 0xAE67F1E9, 0x05A2,  // 1e107
    0x20CC9495, 0xE80E6F48, 0x1A708DE9, 0xDA01EE64, 0x05A5,  // 1e108
    0x147FDCDD, 0x3109058D, 0x908658B2, 0x884134FE, 0x05A9,  // 1e109
    0x599FD415, 0xBD4B46F0, 0x34A7EEDE, 0xAA51823E, 0x05AC,  // 1e110
    0x7007C91A, 0x6C9E18AC, 0xC1D1EA96, 0xD4E5E2CD, 0x05AF,  // 1e111
    0xC604DDB0, 0x03E2CF6B, 0x9923329E, 0x850FADC0, 0x05B3,  // 1e112
    0xB786151C, 0x84DB8346, 0xBF6BFF45, 0xA6539930, 0x05B6,  // 1e113
    0x65679A63, 0xE6126418, 0xEF46FF16, 0xCFE87F7C, 0x05B9,  // 1e114
    0x3F60C07E, 0x4FCB7E8F, 0x158C5F6E, 0x81F14FAE, 0x05BD,  // 1e115
    0x0F38F09D, 0xE3BE5E33, 0x9AEF7749, 0xA26DA399, 0x05C0,  // 1e116
    0xD3072CC5, 0x5CADF5BF, 0x01AB551C, 0xCB090C80, 0x05C3,  // 1e117
    0xC7C8F7F6, 0x73D9732F, 0x02162A63, 0xFDCB4FA0, 0x05C6,  // 1e118
    0xDCDD9AFA, 0x2867E7FD, 0x014DDA7E, 0x9E9F11C4, 0x05CA,  // 1e119
    0x541501B8, 0xB281E1FD, 0x01A1511D, 0xC646D635, 0x05CD,  // 1e120
    0xA91A4226, 0x1F225A7C, 0x4209A565, 0xF7D88BC2, 0x05D0,  // 1e121
    0xE9B06958, 0x3375788D, 0x6946075F, 0x9AE75759, 0x05D4,  // 1e122
    0x641C83AE, 0x0052D6B1, 0xC3978937, 0xC1A12D2F, 0x05D7,  // 1e123
    0xBD23A49A, 0xC0678C5D, 0xB47D6B84, 0xF209787B, 0x05DA,  // 1e124
    0x963646E0, 0xF840B7BA, 0x50CE6332, 0x9745EB4D, 0x05DE,  // 1e125
    0x3BC3D898, 0xB650E5A9, 0xA501FBFF, 0xBD176620, 0x05E1,  // 1e126
    0x8AB4CEBE, 0xA3E51F13, 0xCE427AFF, 0xEC5D3FA8, 0x05E4,  // 1e127
    0x36B10137, 0xC66F336C, 0x80E98CDF, 0x93BA47C9, 0x05E8,  // 1e128
    0x445D4184, 0xB80B0047, 0xE123F017, 0xB8A8D9BB, 0x05EB,  // 1e129
    0x157491E5, 0xA60DC059, 0xD96CEC1D, 0xE6D3102A, 0x05EE,  // 1e130
    0xAD68DB2F, 0x87C89837, 0xC7E41392, 0x9043EA1A, 0x05F2,  // 1e131
    0x98C311FB, 0x29BABE45, 0x79DD1877, 0xB454E4A1, 0x05F5,  // 1e132
    0xFEF3D67A, 0xF4296DD6, 0xD8545E94, 0xE16A1DC9, 0x05F8,  // 1e133
    0x5F58660C, 0x1899E4A6, 0x2734BB1D, 0x8CE2529E, 0x05FC,  // 1e134
    0xF72E7F8F, 0x5EC05DCF, 0xB101E9E4, 0xB01AE745, 0x05FF,  // 1e135
    0xF4FA1F73, 0x76707543, 0x1D42645D, 0xDC21A117, 0x0602,  // 1e136
    0x791C53A8, 0x6A06494A, 0x72497EBA, 0x899504AE, 0x0606,  // 1e137
    0x17636892, 0x0487DB9D, 0x0EDBDE69, 0xABFA45DA, 0x0609,  // 1e138
    0x5D3C42B6, 0x45A9D284, 0x9292D603, 0xD6F8D750, 0x060C,  // 1e139
    0xBA45A9B2, 0x0B8A2392, 0x5B9BC5C2, 0x865B8692, 0x0610,  // 1e140
    0x68D7141E, 0x8E6CAC77, 0xF282B732, 0xA7F26836, 0x0613,  // 1e141
    0x430CD926, 0x3207D795, 0xAF2364FF, 0xD1EF0244, 0x0616,  // 1e142
    0x49E807B8, 0x7F44E6BD, 0xED761F1F, 0x8335616A, 0x061A,  // 1e143
    0x9C6209A6, 0x5F16206C, 0xA8D3A6E7, 0xA402B9C5, 0x061D,  // 1e144
    0xC37A8C0F, 0x36DBA887, 0x130890A1, 0xCD036837, 0x0620,  // 1e145
    0xDA2C9789, 0xC2494954, 0x6BE55A64, 0x80222122, 0x0624,  // 1e146
    0x10B7BD6C, 0xF2DB9BAA, 0x06DEB0FD, 0xA02AA96B, 0x0627,  // 1e147
    0x94E5ACC7, 0x6F928294, 0xC8965D3D, 0xC83553C5, 0x062A,  // 1e148
    0xBA1F17F9, 0xCB772339, 0x3ABBF48C, 0xFA42A8B7, 0x062D,  // 1e149
    0x14536EFB, 0xFF2A7604, 0x84B578D7, 0x9C69A972, 0x0631,  // 1e150
    0x19684ABA, 0xFEF51385, 0x25E2D70D, 0xC38413CF, 0x0634,  // 1e151
    0x5FC25D69, 0x7EB25866, 0xEF5B8CD1, 0xF46518C2, 0x0637,  // 1e152
    0xFBD97A61, 0xEF2F773F, 0xD5993802, 0x98BF2F79, 0x063B,  // 1e153
    0xFACFD8FA, 0xAAFB550F, 0x4AFF8603, 0xBEEEFB58, 0x063E,  // 1e154
    0xF983CF38, 0x95BA2A53, 0x5DBF6784, 0xEEAABA2E, 0x0641,  // 1e155
    0x7BF26183, 0xDD945A74, 0xFA97A0B2, 0x952AB45C, 0x0645,  // 1e156
    0x9AEEF9E4, 0x94F97111, 0x393D88DF, 0xBA756174, 0x0648,  // 1e157
    0x01AAB85D, 0x7A37CD56, 0x478CEB17, 0xE912B9D1, 0x064B,  // 1e158
    0xC10AB33A, 0xAC62E055, 0xCCB812EE, 0x91ABB422, 0x064F,  // 1e159
    0x314D6009, 0x577B986B, 0x7FE617AA, 0xB616A12B, 0x0652,  // 1e160
    0xFDA0B80B, 0xED5A7E85, 0x5FDF9D94, 0xE39C4976, 0x0655,  // 1e161
    0xBE847307, 0x14588F13, 0xFBEBC27D, 0x8E41ADE9, 0x0659,  // 1e162
    0xAE258FC8, 0x596EB2D8, 0x7AE6B31C, 0xB1D21964, 0x065C,  // 1e163
    0xD9AEF3BB, 0x6FCA5F8E, 0x99A05FE3, 0xDE469FBD, 0x065F,  // 1e164
    0x480D5854, 0x25DE7BB9, 0x80043BEE, 0x8AEC23D6, 0x0663,  // 1e165
    0x9A10AE6A, 0xAF561AA7, 0x20054AE9, 0xADA72CCC, 0x0666,  // 1e166
    0x8094DA04, 0x1B2BA151, 0x28069DA4, 0xD910F7FF, 0x0669,  // 1e167
    0xF05D0842, 0x90FB44D2, 0x79042286, 0x87AA9AFF, 0x066D,  // 1e168
    0xAC744A53, 0x353A1607, 0x57452B28, 0xA99541BF, 0x0670,  // 1e169
    0x97915CE8, 0x42889B89, 0x2D1675F2, 0xD3FA922F, 0x0673,  // 1e170
    0xFEBADA11, 0x69956135, 0x7C2E09B7, 0x847C9B5D, 0x0677,  // 1e171
    0x7E699095, 0x43FAB983, 0xDB398C25, 0xA59BC234, 0x067A,  // 1e172
    0x5E03F4BB, 0x94F967E4, 0x1207EF2E, 0xCF02B2C2, 0x067D,  // 1e173
    0xBAC278F5, 0x1D1BE0EE, 0x4B44F57D, 0x8161AFB9, 0x0681,  // 1e174
    0x69731732, 0x6462D92A, 0x9E1632DC, 0xA1BA1BA7, 0x0684,  // 1e175
    0x03CFDCFE, 0x7D7B8F75, 0x859BBF93, 0xCA28A291, 0x0687,  // 1e176
    0x44C3D43E, 0x5CDA7352, 0xE702AF78, 0xFCB2CB35, 0x068A,  // 1e177
    0x6AFA64A7, 0x3A088813, 0xB061ADAB, 0x9DEFBF01, 0x068E,  // 1e178
    0x45B8FDD0, 0x088AAA18, 0x1C7A1916, 0xC56BAEC2, 0x0691,  // 1e179
    0x57273D45, 0x8AAD549E, 0xA3989F5B, 0xF6C69A72, 0x0694,  // 1e180
    0xF678864B, 0x36AC54E2, 0xA63F6399, 0x9A3C2087, 0x0698,  // 1e181
    0xB416A7DD, 0x84576A1B, 0x8FCF3C7F, 0xC0CB28A9, 0x069B,  // 1e182
    0xA11C51D5, 0x656D44A2, 0xF3C30B9F, 0xF0FDF2D3, 0x069E,  // 1e183
    0xA4B1B325, 0x9F644AE5, 0x7859E743, 0x969EB7C4, 0x06A2,  // 1e184
    0x0DDE1FEE, 0x873D5D9F, 0x96706114, 0xBC4665B5, 0x06A5,  // 1e185
    0xD155A7EA, 0xA90CB506, 0xFC0C7959, 0xEB57FF22, 0x06A8,  // 1e186
    0x42D588F2, 0x09A7F124, 0xDD87CBD8, 0x9316FF75, 0x06AC,  // 1e187
    0x538AEB2F, 0x0C11ED6D, 0x54E9BECE, 0xB7DCBF53, 0x06AF,  // 1e188
    0xA86DA5FA, 0x8F1668C8, 0x2A242E81, 0xE5D3EF28, 0x06B2,  // 1e189
    0x694487BC, 0xF96E017D, 0x1A569D10, 0x8FA47579, 0x06B6,  // 1e190
    0xC395A9AC, 0x37C981DC, 0x60EC4455, 0xB38D92D7, 0x06B9,  // 1e191
    0xF47B1417, 0x85BBE253, 0x3927556A, 0xE070F78D, 0x06BC,  // 1e192
    0x78CCEC8E, 0x93956D74, 0x43B89562, 0x8C469AB8, 0x06C0,  // 1e193
    0x970027B2, 0x387AC8D1, 0x54A6BABB, 0xAF584166, 0x06C3,  // 1e194
    0xFCC0319E, 0x06997B05, 0xE9D0696A, 0xDB2E51BF, 0x06C6,  // 1e195
    0xBDF81F03, 0x441FECE3, 0xF22241E2, 0x88FCF317, 0x06CA,  // 1e196
    0xAD7626C3, 0xD527E81C, 0xEEAAD25A, 0xAB3C2FDD, 0x06CD,  // 1e197
    0xD8D3B074, 0x8A71E223, 0x6A5586F1, 0xD60B3BD5, 0x06D0,  // 1e198
    0x67844E49, 0xF6872D56, 0x62757456, 0x85C70565, 0x06D4,  // 1e199
    0x016561DB, 0xB428F8AC, 0xBB12D16C, 0xA738C6BE, 0x06D7,  // 1e200
    0x01BEBA52, 0xE13336D7, 0x69D785C7, 0xD106F86E, 0x06DA,  // 1e201
    0x61173473, 0xECC00246, 0x0226B39C, 0x82A45B45, 0x06DE,  // 1e202
    0xF95D0190, 0x27F002D7, 0x42B06084, 0xA34D7216, 0x06E1,  // 1e203
    0xF7B441F4, 0x31EC038D, 0xD35C78A5, 0xCC20CE9B, 0x06E4,  // 1e204
    0x75A15271, 0x7E670471, 0xC83396CE, 0xFF290242, 0x06E7,  // 1e205
    0xE984D386, 0x0F0062C6, 0xBD203E41, 0x9F79A169, 0x06EB,  // 1e206
    0xA3E60868, 0x52C07B78, 0x2C684DD1, 0xC75809C4, 0x06EE,  // 1e207
    0xCCDF8A82, 0xA7709A56, 0x37826145, 0xF92E0C35, 0x06F1,  // 1e208
    0x400BB691, 0x88A66076, 0x42B17CCB, 0x9BBCC7A1, 0x06F5,  // 1e209
    0xD00EA435, 0x6ACFF893, 0x935DDBFE, 0xC2ABF989, 0x06F8,  // 1e210
    0xC4124D43, 0x0583F6B8, 0xF83552FE, 0xF356F7EB, 0x06FB,  // 1e211
    0x7A8B704A, 0xC3727A33, 0x7B2153DE, 0x98165AF3, 0x06FF,  // 1e212
    0x592E4C5C, 0x744F18C0, 0x59E9A8D6, 0xBE1BF1B0, 0x0702,  // 1e213
    0x6F79DF73, 0x1162DEF0, 0x7064130C, 0xEDA2EE1C, 0x0705,  // 1e214
    0x45AC2BA8, 0x8ADDCB56, 0xC63E8BE7, 0x9485D4D1, 0x0709,  // 1e215
    0xD7173692, 0x6D953E2B, 0x37CE2EE1, 0xB9A74A06, 0x070C,  // 1e216
    0xCCDD0437, 0xC8FA8DB6, 0xC5C1BA99, 0xE8111C87, 0x070F,  // 1e217
    0x400A22A2, 0x1D9C9892, 0xDB9914A0, 0x910AB1D4, 0x0713,  // 1e218
    0xD00CAB4B, 0x2503BEB6, 0x127F59C8, 0xB54D5E4A, 0x0716,  // 1e219
    0x840FD61D, 0x2E44AE64, 0x971F303A, 0xE2A0B5DC, 0x0719,  // 1e220
    0xD289E5D2, 0x5CEAECFE, 0xDE737E24, 0x8DA471A9, 0x071D,  // 1e221
    0x872C5F47, 0x7425A83E, 0x56105DAD, 0xB10D8E14, 0x0720,  // 1e222
    0x28F77719, 0xD12F124E, 0x6B947518, 0xDD50F199, 0x0723,  // 1e223
    0xD99AAA6F, 0x82BD6B70, 0xE33CC92F, 0x8A5296FF, 0x0727,  // 1e224
    0x1001550B, 0x636CC64D, 0xDC0BFB7B, 0xACE73CBF, 0x072A,  // 1e225
    0x5401AA4E, 0x3C47F7E0, 0xD30EFA5A, 0xD8210BEF, 0x072D,  // 1e226
    0x34810A71, 0x65ACFAEC, 0xE3E95C78, 0x8714A775, 0x0731,  // 1e227
    0x41A14D0D, 0x7F1839A7, 0x5CE3B396, 0xA8D9D153, 0x0734,  // 1e228
    0x1209A050, 0x1EDE4811, 0x341CA07C, 0xD31045A8, 0x0737,  // 1e229
    0xAB460432, 0x934AED0A, 0x2091E44D, 0x83EA2B89, 0x073B,  // 1e230
    0x5617853F, 0xF81DA84D, 0x68B65D60, 0xA4E4B66B, 0x073E,  // 1e231
    0xAB9D668E, 0x36251260, 0x42E3F4B9, 0xCE1DE406, 0x0741,  // 1e232
    0x6B426019, 0xC1D72B7C, 0xE9CE78F3, 0x80D2AE83, 0x0745,  // 1e233
    0x8612F81F, 0xB24CF65B, 0xE4421730, 0xA1075A24, 0x0748,  // 1e234
    0x6797B627, 0xDEE033F2, 0x1D529CFC, 0xC94930AE, 0x074B,  // 1e235
    0x017DA3B1, 0x169840EF, 0xA4A7443C, 0xFB9B7CD9, 0x074E,  // 1e236
    0x60EE864E, 0x8E1F2895, 0x06E88AA5, 0x9D412E08, 0x0752,  // 1e237
    0xB92A27E2, 0xF1A6F2BA, 0x08A2AD4E, 0xC491798A, 0x0755,  // 1e238
    0x6774B1DB, 0xAE10AF69, 0x8ACB58A2, 0xF5B5D7EC, 0x0758,  // 1e239
    0xE0A8EF29, 0xACCA6DA1, 0xD6BF1765, 0x9991A6F3, 0x075C,  // 1e240
    0x58D32AF3, 0x17FD090A, 0xCC6EDD3F, 0xBFF610B0, 0x075F,  // 1e241
    0xEF07F5B0, 0xDDFC4B4C, 0xFF8A948E, 0xEFF394DC, 0x0762,  // 1e242
    0x1564F98E, 0x4ABDAF10, 0x1FB69CD9, 0x95F83D0A, 0x0766,  // 1e243
    0x1ABE37F1, 0x9D6D1AD4, 0xA7A4440F, 0xBB764C4C, 0x0769,  // 1e244
    0x216DC5ED, 0x84C86189, 0xD18D5513, 0xEA53DF5F, 0x076C,  // 1e245
    0xB4E49BB4, 0x32FD3CF5, 0xE2F8552C, 0x92746B9B, 0x0770,  // 1e246
    0x221DC2A1, 0x3FBC8C33, 0xDBB66A77, 0xB7118682, 0x0773,  // 1e247
    0xEAA5334A, 0x0FABAF3F, 0x92A40515, 0xE4D5E823, 0x0776,  // 1e248
    0xF2A7400E, 0x29CB4D87, 0x3BA6832D, 0x8F05B116, 0x077A,  // 1e249
    0xEF511012, 0x743E20E9, 0xCA9023F8, 0xB2C71D5B, 0x077D,  // 1e250
    0x6B255416, 0x914DA924, 0xBD342CF6, 0xDF78E4B2, 0x0780,  // 1e251
    0xC2F7548E, 0x1AD089B6, 0xB6409C1A, 0x8BAB8EEF, 0x0784,  // 1e252
    0x73B529B1, 0xA184AC24, 0xA3D0C320, 0xAE9672AB, 0x0787,  // 1e253
    0x90A2741E, 0xC9E5D72D, 0x8CC4F3E8, 0xDA3C0F56, 0x078A,  // 1e254
    0x7A658892, 0x7E2FA67C, 0x17FB1871, 0x88658996, 0x078E,  // 1e255
    0x98FEEAB7, 0xDDBB901B, 0x9DF9DE8D, 0xAA7EEBFB, 0x0791,  // 1e256
    0x7F3EA565, 0x552A7422, 0x85785631, 0xD51EA6FA, 0x0794,  // 1e257
    0x8F87275F, 0xD53A8895, 0x936B35DE, 0x8533285C, 0x0798,  // 1e258
    0xF368F137, 0x8A892ABA, 0xB8460356, 0xA67FF273, 0x079B,  // 1e259
    0xB0432D85, 0x2D2B7569, 0xA657842C, 0xD01FEF10, 0x079E,  // 1e260
    0x0E29FC73, 0x9C3B2962, 0x67F6B29B, 0x8213F56A, 0x07A2,  // 1e261
    0x91B47B8F, 0x8349F3BA, 0x01F45F42, 0xA298F2C5, 0x07A5,  // 1e262
    0x36219A73, 0x241C70A9, 0x42717713, 0xCB3F2F76, 0x07A8,  // 1e263
    0x83AA0110, 0xED238CD3, 0xD30DD4D7, 0xFE0EFB53, 0x07AB,  // 1e264
    0x324A40AA, 0xF4363804, 0x63E8A506, 0x9EC95D14, 0x07AF,  // 1e265
    0x3EDCD0D5, 0xB143C605, 0x7CE2CE48, 0xC67BB459, 0x07B2,  // 1e266
    0x8E94050A, 0xDD94B786, 0xDC1B81DA, 0xF81AA16F, 0x07B5,  // 1e267
    0x191C8326, 0xCA7CF2B4, 0xE9913128, 0x9B10A4E5, 0x07B9,  // 1e268
    0x1F63A3F0, 0xFD1C2F61, 0x63F57D72, 0xC1D4CE1F, 0x07BC,  // 1e269
    0x673C8CEC, 0xBC633B39, 0x3CF2DCCF, 0xF24A01A7, 0x07BF,  // 1e270
    0xE085D813, 0xD5BE0503, 0x8617CA01, 0x976E4108, 0x07C3,  // 1e271
    0xD8A74E18, 0x4B2D8644, 0xA79DBC82, 0xBD49D14A, 0x07C6,  // 1e272
    0x0ED1219E, 0xDDF8E7D6, 0x51852BA2, 0xEC9C459D, 0x07C9,  // 1e273
    0xC942B503, 0xCABB90E5, 0x52F33B45, 0x93E1AB82, 0x07CD,  // 1e274
    0x3B936243, 0x3D6A751F, 0xE7B00A17, 0xB8DA1662, 0x07D0,  // 1e275
    0x0A783AD4, 0x0CC51267, 0xA19C0C9D, 0xE7109BFB, 0x07D3,  // 1e276
    0x668B24C5, 0x27FB2B80, 0x450187E2, 0x906A617D, 0x07D7,  // 1e277
    0x802DEDF6, 0xB1F9F660, 0x9641E9DA, 0xB484F9DC, 0x07DA,  // 1e278
    0xA0396973, 0x5E7873F8, 0xBBD26451, 0xE1A63853, 0x07DD,  // 1e279
    0x6423E1E8, 0xDB0B487B, 0x55637EB2, 0x8D07E334, 0x07E1,  // 1e280
    0x3D2CDA62, 0x91CE1A9A, 0x6ABC5E5F, 0xB049DC01, 0x07E4,  // 1e281
    0xCC7810FB, 0x7641A140, 0xC56B75F7, 0xDC5C5301, 0x07E7,  // 1e282
    0x7FCB0A9D, 0xA9E904C8, 0x1B6329BA, 0x89B9B3E1, 0x07EB,  // 1e283
    0x9FBDCD44, 0x546345FA, 0x623BF429, 0xAC2820D9, 0x07EE,  // 1e284
    0x47AD4095, 0xA97C1779, 0xBACAF133, 0xD732290F, 0x07F1,  // 1e285
    0xCCCC485D, 0x49ED8EAB, 0xD4BED6C0, 0x867F59A9, 0x07F5,  // 1e286
    0xBFFF5A74, 0x5C68F256, 0x49EE8C70, 0xA81F3014, 0x07F8,  // 1e287
    0x6FFF3111, 0x73832EEC, 0x5C6A2F8C, 0xD226FC19, 0x07FB,  // 1e288
    0xC5FF7EAB, 0xC831FD53, 0xD9C25DB7, 0x83585D8F, 0x07FF,  // 1e289
    0xB77F5E55, 0xBA3E7CA8, 0xD032F525, 0xA42E74F3, 0x0802,  // 1e290
    0xE55F35EB, 0x28CE1BD2, 0xC43FB26F, 0xCD3A1230, 0x0805,  // 1e291
    0xCF5B81B3, 0x7980D163, 0x7AA7CF85, 0x80444B5E, 0x0809,  // 1e292
    0xC332621F, 0xD7E105BC, 0x1951C366, 0xA0555E36, 0x080C,  // 1e293
    0xF3FEFAA7, 0x8DD9472B, 0x9FA63440, 0xC86AB5C3, 0x080F,  // 1e294
    0xF0FEB951, 0xB14F98F6, 0x878FC150, 0xFA856334, 0x0812,  // 1e295
    0x569F33D3, 0x6ED1BF9A, 0xD4B9D8D2, 0x9C935E00, 0x0816,  // 1e296
    0xEC4700C8, 0x0A862F80, 0x09E84F07, 0xC3B83581, 0x0819,  // 1e297
    0x2758C0FA, 0xCD27BB61, 0x4C6262C8, 0xF4A642E1, 0x081C,  // 1e298
    0xB897789C, 0x8038D51C, 0xCFBD7DBD, 0x98E7E9CC, 0x0820,  // 1e299
    0xE6BD56C3, 0xE0470A63, 0x03ACDD2C, 0xBF21E440, 0x0823,  // 1e300
    0xE06CAC74, 0x1858CCFC, 0x04981478, 0xEEEA5D50, 0x0826,  // 1e301
    0x0C43EBC8, 0x0F37801E, 0x02DF0CCB, 0x95527A52, 0x082A,  // 1e302
    0x8F54E6BA, 0xD3056025, 0x8396CFFD, 0xBAA718E6, 0x082D,  // 1e303
    0xF32A2069, 0x47C6B82E, 0x247C83FD, 0xE950DF20, 0x0830,  // 1e304
    0x57FA5441, 0x4CDC331D, 0x16CDD27E, 0x91D28B74, 0x0834,  // 1e305
    0xADF8E952, 0xE0133FE4, 0x1C81471D, 0xB6472E51, 0x0837,  // 1e306
    0xD97723A6, 0x58180FDD, 0x63A198E5, 0xE3D8F9E5, 0x083A,  // 1e307
    0xA7EA7648, 0x570F09EA, 0x5E44FF8F, 0x8E679C2F, 0x083E,  // 1e308
    0x51E513DA, 0x2CD2CC65, 0x35D63F73, 0xB201833B, 0x0841,  // 1e309
    0xA65E58D1, 0xF8077F7E, 0x034BCF4F, 0xDE81E40A, 0x0844,  // 1e310
};

// wuffs_base__private_implementation__f64_powers_of_10 holds powers of 10 that
// can be exactly represented by a float64 (what C calls a double).
static const double wuffs_base__private_implementation__f64_powers_of_10[23] = {
    1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,  1e8,  1e9,  1e10, 1e11,
    1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22,
};

// --------

// wuffs_base__private_implementation__medium_prec_bin (abbreviated as MPB) is
// a fixed precision floating point binary number. Unlike IEEE 754 Floating
// Point, it cannot represent infinity or NaN (Not a Number).
//
// "Medium precision" means that the mantissa holds 64 binary digits, a little
// more than "double precision", and sizeof(MPB) > sizeof(double). 64 is
// obviously the number of bits in a uint64_t.
//
// An MPB isn't for general purpose arithmetic, only for conversions to and
// from IEEE 754 double-precision floating point.
//
// There is no implicit mantissa bit. The mantissa field is zero if and only if
// the overall floating point value is ±0. An MPB is normalized if the mantissa
// is zero or its high bit (the 1<<63 bit) is set.
//
// There is no negative bit. An MPB can only represent non-negative numbers.
//
// The "all fields are zero" value is valid, and represents the number +0.
//
// This is the "Do It Yourself Floating Point" data structure from Loitsch,
// "Printing Floating-Point Numbers Quickly and Accurately with Integers"
// (https://www.cs.tufts.edu/~nr/cs257/archive/florian-loitsch/printf.pdf).
//
// Florian Loitsch is also the primary contributor to
// https://github.com/google/double-conversion
typedef struct {
  uint64_t mantissa;
  int32_t exp2;
} wuffs_base__private_implementation__medium_prec_bin;

static uint32_t  //
wuffs_base__private_implementation__medium_prec_bin__normalize(
    wuffs_base__private_implementation__medium_prec_bin* m) {
  if (m->mantissa == 0) {
    return 0;
  }
  uint32_t shift = wuffs_base__count_leading_zeroes_u64(m->mantissa);
  m->mantissa <<= shift;
  m->exp2 -= (int32_t)shift;
  return shift;
}

// wuffs_base__private_implementation__medium_prec_bin__mul_pow_10 sets m to be
// (m * pow), where pow comes from an etc__powers_of_10 triple starting at p.
//
// The result is rounded, but not necessarily normalized.
//
// Preconditions:
//  - m is non-NULL.
//  - m->mantissa is non-zero.
//  - m->mantissa's high bit is set (i.e. m is normalized).
//
// The etc__powers_of_10 triple is already normalized.
static void  //
wuffs_base__private_implementation__medium_prec_bin__mul_pow_10(
    wuffs_base__private_implementation__medium_prec_bin* m,
    const uint32_t* p) {
  uint64_t p_mantissa = ((uint64_t)p[2]) | (((uint64_t)p[3]) << 32);
  int32_t p_exp2 = (int32_t)p[4];

  wuffs_base__multiply_u64__output o =
      wuffs_base__multiply_u64(m->mantissa, p_mantissa);
  // Round the mantissa up. It cannot overflow because the maximum possible
  // value of o.hi is 0xFFFFFFFFFFFFFFFE.
  m->mantissa = o.hi + (o.lo >> 63);
  m->exp2 = m->exp2 + p_exp2 + 128 - 1214;
}

// wuffs_base__private_implementation__medium_prec_bin__as_f64 converts m to a
// double (what C calls a double-precision float64).
//
// Preconditions:
//  - m is non-NULL.
//  - m->mantissa is non-zero.
//  - m->mantissa's high bit is set (i.e. m is normalized).
static double  //
wuffs_base__private_implementation__medium_prec_bin__as_f64(
    const wuffs_base__private_implementation__medium_prec_bin* m,
    bool negative) {
  uint64_t mantissa64 = m->mantissa;
  // An mpb's mantissa has the implicit (binary) decimal point at the right
  // hand end of the mantissa's explicit digits. A double-precision's mantissa
  // has that decimal point near the left hand end. There's also an explicit
  // versus implicit leading 1 bit (binary digit). Together, the difference in
  // semantics corresponds to adding 63.
  int32_t exp2 = m->exp2 + 63;

  // Ensure that exp2 is at least -1022, the minimum double-precision exponent
  // for normal (as opposed to subnormal) numbers.
  if (-1022 > exp2) {
    uint32_t n = (uint32_t)(-1022 - exp2);
    mantissa64 >>= n;
    exp2 += (int32_t)n;
  }

  // Extract the (1 + 52) bits from the 64-bit mantissa64. 52 is the number of
  // explicit mantissa bits in a double-precision f64.
  //
  // Before, we have 64 bits and due to normalization, the high bit 'H' is 1.
  // 63        55        47       etc     15        7
  // H210_9876_5432_1098_7654_etc_etc_etc_5432_1098_7654_3210
  // ++++_++++_++++_++++_++++_etc_etc_etc_++++_+..._...._....  Kept bits.
  // ...._...._...H_2109_8765_etc_etc_etc_6543_2109_8765_4321  After shifting.
  // After, we have 53 bits (and bit #52 is this 'H' bit).
  uint64_t mantissa53 = mantissa64 >> 11;

  // Round up if the old bit #10 (the highest bit dropped by shifting) was set.
  // We also fix any overflow from rounding up.
  if (mantissa64 & 1024) {
    mantissa53++;
    if ((mantissa53 >> 53) != 0) {
      mantissa53 >>= 1;
      exp2++;
    }
  }

  // Handle double-precision infinity (a nominal exponent of 1024) and
  // subnormals (an exponent of -1023 and no implicit mantissa bit, bit #52).
  if (exp2 >= 1024) {
    mantissa53 = 0;
    exp2 = 1024;
  } else if ((mantissa53 >> 52) == 0) {
    exp2 = -1023;
  }

  // Pack the bits and return.
  const int32_t f64_bias = -1023;
  uint64_t exp2_bits =
      (uint64_t)((exp2 - f64_bias) & 0x07FF);           // (1 << 11) - 1.
  uint64_t bits = (mantissa53 & 0x000FFFFFFFFFFFFF) |   // (1 << 52) - 1.
                  (exp2_bits << 52) |                   //
                  (negative ? 0x8000000000000000 : 0);  // (1 << 63).
  return wuffs_base__ieee_754_bit_representation__to_f64(bits);
}

// wuffs_base__private_implementation__medium_prec_bin__parse_number_f64
// converts from an HPD to a double, using an MPB as scratch space. It returns
// a NULL status.repr if there is no ambiguity in the truncation or rounding to
// a float64 (an IEEE 754 double-precision floating point value).
//
// It may modify m even if it returns a non-NULL status.repr.
static wuffs_base__result_f64  //
wuffs_base__private_implementation__medium_prec_bin__parse_number_f64(
    wuffs_base__private_implementation__medium_prec_bin* m,
    const wuffs_base__private_implementation__high_prec_dec* h,
    bool skip_fast_path_for_tests) {
  do {
    // m->mantissa is a uint64_t, which is an integer approximation to a
    // rational value - h's underlying digits after m's normalization. This
    // error is an upper bound on the difference between the approximate and
    // actual value.
    //
    // The DiyFpStrtod function in https://github.com/google/double-conversion
    // uses a finer grain (1/8th of the ULP, Unit in the Last Place) when
    // tracking error. This implementation is coarser (1 ULP) but simpler.
    //
    // It is an error in the "numerical approximation" sense, not in the
    // typical programming sense (as in "bad input" or "a result type").
    uint64_t error = 0;

    // Convert up to 19 decimal digits (in h->digits) to 64 binary digits (in
    // m->mantissa): (1e19 < (1<<64)) and ((1<<64) < 1e20). If we have more
    // than 19 digits, we're truncating (with error).
    uint32_t i;
    uint32_t i_end = h->num_digits;
    if (i_end > 19) {
      i_end = 19;
      error = 1;
    }
    uint64_t mantissa = 0;
    for (i = 0; i < i_end; i++) {
      mantissa = (10 * mantissa) + h->digits[i];
    }
    m->mantissa = mantissa;
    m->exp2 = 0;

    // Check that exp10 lies in the etc__powers_of_10 range (637 triples).
    int32_t exp10 = h->decimal_point - ((int32_t)(i_end));
    if ((exp10 < -326) || (+310 < exp10)) {
      goto fail;
    }

    // Try a fast path, if float64 math would be exact.
    //
    // 15 is such that 1e15 can be losslessly represented in a float64
    // mantissa: (1e15 < (1<<53)) and ((1<<53) < 1e16).
    //
    // 22 is the maximum valid index for the
    // wuffs_base__private_implementation__f64_powers_of_10 array.
    do {
      if (skip_fast_path_for_tests || ((mantissa >> 52) != 0)) {
        break;
      }
      double d = (double)mantissa;

      if (exp10 == 0) {
        wuffs_base__result_f64 ret;
        ret.status.repr = NULL;
        ret.value = h->negative ? -d : +d;
        return ret;

      } else if (exp10 > 0) {
        if (exp10 > 22) {
          if (exp10 > (15 + 22)) {
            break;
          }
          // If exp10 is in the range 23 ..= 37, try moving a few of the zeroes
          // from the exponent to the mantissa. If we're still under 1e15, we
          // haven't truncated any mantissa bits.
          d *= wuffs_base__private_implementation__f64_powers_of_10[exp10 - 22];
          exp10 = 22;
          if (d >= 1e15) {
            break;
          }
        }
        d *= wuffs_base__private_implementation__f64_powers_of_10[exp10];
        wuffs_base__result_f64 ret;
        ret.status.repr = NULL;
        ret.value = h->negative ? -d : +d;
        return ret;

      } else {  // "if (exp10 < 0)" is effectively "if (true)" here.
        if (exp10 < -22) {
          break;
        }
        d /= wuffs_base__private_implementation__f64_powers_of_10[-exp10];
        wuffs_base__result_f64 ret;
        ret.status.repr = NULL;
        ret.value = h->negative ? -d : +d;
        return ret;
      }
    } while (0);

    // Normalize (and scale the error).
    error <<= wuffs_base__private_implementation__medium_prec_bin__normalize(m);

    // Multiplying two MPB values nominally multiplies two mantissas, call them
    // A and B, which are integer approximations to the precise values (A+a)
    // and (B+b) for some error terms a and b.
    //
    // MPB multiplication calculates (((A+a) * (B+b)) >> 64) to be ((A*B) >>
    // 64). Shifting (truncating) and rounding introduces further error. The
    // difference between the calculated result:
    //  ((A*B                  ) >> 64)
    // and the true result:
    //  ((A*B + A*b + a*B + a*b) >> 64)   + rounding_error
    // is:
    //  ((      A*b + a*B + a*b) >> 64)   + rounding_error
    // which can be re-grouped as:
    //  ((A*b) >> 64) + ((a*(B+b)) >> 64) + rounding_error
    //
    // Now, let A and a be "m->mantissa" and "error", and B and b be the
    // pre-calculated power of 10. A and B are both less than (1 << 64), a is
    // the "error" local variable and b is less than 1.
    //
    // An upper bound (in absolute value) on ((A*b) >> 64) is therefore 1.
    //
    // An upper bound on ((a*(B+b)) >> 64) is a, also known as error.
    //
    // Finally, the rounding_error is at most 1.
    //
    // In total, calling mpb__mul_pow_10 will raise the worst-case error by 2.
    // The subsequent re-normalization can multiply that by a further factor.

    // Multiply by powers_of_10[etc].
    wuffs_base__private_implementation__medium_prec_bin__mul_pow_10(
        m,
        &wuffs_base__private_implementation__powers_of_10[5 * (exp10 + 326)]);
    error += 2;
    error <<= wuffs_base__private_implementation__medium_prec_bin__normalize(m);

    // We have a good approximation of h, but we still have to check whether
    // the error is small enough. Equivalently, whether the number of surplus
    // mantissa bits (the bits dropped when going from m's 64 mantissa bits to
    // the smaller number of double-precision mantissa bits) would always round
    // up or down, even when perturbed by ±error. We start at 11 surplus bits
    // (m has 64, double-precision has 1+52), but it can be higher for
    // subnormals.
    //
    // In many cases, the error is small enough and we return true.
    const int32_t f64_bias = -1023;
    int32_t subnormal_exp2 = f64_bias - 63;
    uint32_t surplus_bits = 11;
    if (subnormal_exp2 >= m->exp2) {
      surplus_bits += 1 + ((uint32_t)(subnormal_exp2 - m->exp2));
    }

    uint64_t surplus_mask =
        (((uint64_t)1) << surplus_bits) - 1;  // e.g. 0x07FF.
    uint64_t surplus = m->mantissa & surplus_mask;
    uint64_t halfway = ((uint64_t)1) << (surplus_bits - 1);  // e.g. 0x0400.

    // Do the final calculation in *signed* arithmetic.
    int64_t i_surplus = (int64_t)surplus;
    int64_t i_halfway = (int64_t)halfway;
    int64_t i_error = (int64_t)error;

    if ((i_surplus > (i_halfway - i_error)) &&
        (i_surplus < (i_halfway + i_error))) {
      goto fail;
    }

    wuffs_base__result_f64 ret;
    ret.status.repr = NULL;
    ret.value = wuffs_base__private_implementation__medium_prec_bin__as_f64(
        m, h->negative);
    return ret;
  } while (0);

fail:
  do {
    wuffs_base__result_f64 ret;
    ret.status.repr = "#base: mpb__parse_number_f64 failed";
    ret.value = 0;
    return ret;
  } while (0);
}

// --------

static wuffs_base__result_f64  //
wuffs_base__parse_number_f64_special(wuffs_base__slice_u8 s,
                                     const char* fallback_status_repr) {
  do {
    uint8_t* p = s.ptr;
    uint8_t* q = s.ptr + s.len;

    for (; (p < q) && (*p == '_'); p++) {
    }
    if (p >= q) {
      goto fallback;
    }

    // Parse sign.
    bool negative = false;
    do {
      if (*p == '+') {
        p++;
      } else if (*p == '-') {
        negative = true;
        p++;
      } else {
        break;
      }
      for (; (p < q) && (*p == '_'); p++) {
      }
    } while (0);
    if (p >= q) {
      goto fallback;
    }

    bool nan = false;
    switch (p[0]) {
      case 'I':
      case 'i':
        if (((q - p) < 3) ||                     //
            ((p[1] != 'N') && (p[1] != 'n')) ||  //
            ((p[2] != 'F') && (p[2] != 'f'))) {
          goto fallback;
        }
        p += 3;

        if ((p >= q) || (*p == '_')) {
          break;
        } else if (((q - p) < 5) ||                     //
                   ((p[0] != 'I') && (p[0] != 'i')) ||  //
                   ((p[1] != 'N') && (p[1] != 'n')) ||  //
                   ((p[2] != 'I') && (p[2] != 'i')) ||  //
                   ((p[3] != 'T') && (p[3] != 't')) ||  //
                   ((p[4] != 'Y') && (p[4] != 'y'))) {
          goto fallback;
        }
        p += 5;

        if ((p >= q) || (*p == '_')) {
          break;
        }
        goto fallback;

      case 'N':
      case 'n':
        if (((q - p) < 3) ||                     //
            ((p[1] != 'A') && (p[1] != 'a')) ||  //
            ((p[2] != 'N') && (p[2] != 'n'))) {
          goto fallback;
        }
        p += 3;

        if ((p >= q) || (*p == '_')) {
          nan = true;
          break;
        }
        goto fallback;

      default:
        goto fallback;
    }

    // Finish.
    for (; (p < q) && (*p == '_'); p++) {
    }
    if (p != q) {
      goto fallback;
    }
    wuffs_base__result_f64 ret;
    ret.status.repr = NULL;
    ret.value = wuffs_base__ieee_754_bit_representation__to_f64(
        (nan ? 0x7FFFFFFFFFFFFFFF : 0x7FF0000000000000) |
        (negative ? 0x8000000000000000 : 0));
    return ret;
  } while (0);

fallback:
  do {
    wuffs_base__result_f64 ret;
    ret.status.repr = fallback_status_repr;
    ret.value = 0;
    return ret;
  } while (0);
}

WUFFS_BASE__MAYBE_STATIC wuffs_base__result_f64  //
wuffs_base__parse_number_f64(wuffs_base__slice_u8 s) {
  wuffs_base__private_implementation__medium_prec_bin m;
  wuffs_base__private_implementation__high_prec_dec h;

  do {
    // powers converts decimal powers of 10 to binary powers of 2. For example,
    // (10000 >> 13) is 1. It stops before the elements exceed 60, also known
    // as WUFFS_BASE__PRIVATE_IMPLEMENTATION__HPD__SHIFT__MAX_INCL.
    static const uint32_t num_powers = 19;
    static const uint8_t powers[19] = {
        0,  3,  6,  9,  13, 16, 19, 23, 26, 29,  //
        33, 36, 39, 43, 46, 49, 53, 56, 59,      //
    };

    wuffs_base__status status =
        wuffs_base__private_implementation__high_prec_dec__parse(&h, s);
    if (status.repr) {
      return wuffs_base__parse_number_f64_special(s, status.repr);
    }

    // Handle zero and obvious extremes. The largest and smallest positive
    // finite f64 values are approximately 1.8e+308 and 4.9e-324.
    if ((h.num_digits == 0) || (h.decimal_point < -326)) {
      goto zero;
    } else if (h.decimal_point > 310) {
      goto infinity;
    }

    wuffs_base__result_f64 mpb_result =
        wuffs_base__private_implementation__medium_prec_bin__parse_number_f64(
            &m, &h, false);
    if (mpb_result.status.repr == NULL) {
      return mpb_result;
    }

    // Scale by powers of 2 until we're in the range [½ .. 1], which gives us
    // our exponent (in base-2). First we shift right, possibly a little too
    // far, ending with a value certainly below 1 and possibly below ½...
    const int32_t f64_bias = -1023;
    int32_t exp2 = 0;
    while (h.decimal_point > 0) {
      uint32_t n = (uint32_t)(+h.decimal_point);
      uint32_t shift =
          (n < num_powers)
              ? powers[n]
              : WUFFS_BASE__PRIVATE_IMPLEMENTATION__HPD__SHIFT__MAX_INCL;

      wuffs_base__private_implementation__high_prec_dec__small_rshift(&h,
                                                                      shift);
      if (h.decimal_point <
          -WUFFS_BASE__PRIVATE_IMPLEMENTATION__HPD__DECIMAL_POINT__RANGE) {
        goto zero;
      }
      exp2 += (int32_t)shift;
    }
    // ...then we shift left, putting us in [½ .. 1].
    while (h.decimal_point <= 0) {
      uint32_t shift;
      if (h.decimal_point == 0) {
        if (h.digits[0] >= 5) {
          break;
        }
        shift = (h.digits[0] <= 2) ? 2 : 1;
      } else {
        uint32_t n = (uint32_t)(-h.decimal_point);
        shift = (n < num_powers)
                    ? powers[n]
                    : WUFFS_BASE__PRIVATE_IMPLEMENTATION__HPD__SHIFT__MAX_INCL;
      }

      wuffs_base__private_implementation__high_prec_dec__small_lshift(&h,
                                                                      shift);
      if (h.decimal_point >
          +WUFFS_BASE__PRIVATE_IMPLEMENTATION__HPD__DECIMAL_POINT__RANGE) {
        goto infinity;
      }
      exp2 -= (int32_t)shift;
    }

    // We're in the range [½ .. 1] but f64 uses [1 .. 2].
    exp2--;

    // The minimum normal exponent is (f64_bias + 1).
    while ((f64_bias + 1) > exp2) {
      uint32_t n = (uint32_t)((f64_bias + 1) - exp2);
      if (n > WUFFS_BASE__PRIVATE_IMPLEMENTATION__HPD__SHIFT__MAX_INCL) {
        n = WUFFS_BASE__PRIVATE_IMPLEMENTATION__HPD__SHIFT__MAX_INCL;
      }
      wuffs_base__private_implementation__high_prec_dec__small_rshift(&h, n);
      exp2 += (int32_t)n;
    }

    // Check for overflow.
    if ((exp2 - f64_bias) >= 0x07FF) {  // (1 << 11) - 1.
      goto infinity;
    }

    // Extract 53 bits for the mantissa (in base-2).
    wuffs_base__private_implementation__high_prec_dec__small_lshift(&h, 53);
    uint64_t man2 =
        wuffs_base__private_implementation__high_prec_dec__rounded_integer(&h);

    // Rounding might have added one bit. If so, shift and re-check overflow.
    if ((man2 >> 53) != 0) {
      man2 >>= 1;
      exp2++;
      if ((exp2 - f64_bias) >= 0x07FF) {  // (1 << 11) - 1.
        goto infinity;
      }
    }

    // Handle subnormal numbers.
    if ((man2 >> 52) == 0) {
      exp2 = f64_bias;
    }

    // Pack the bits and return.
    uint64_t exp2_bits =
        (uint64_t)((exp2 - f64_bias) & 0x07FF);             // (1 << 11) - 1.
    uint64_t bits = (man2 & 0x000FFFFFFFFFFFFF) |           // (1 << 52) - 1.
                    (exp2_bits << 52) |                     //
                    (h.negative ? 0x8000000000000000 : 0);  // (1 << 63).

    wuffs_base__result_f64 ret;
    ret.status.repr = NULL;
    ret.value = wuffs_base__ieee_754_bit_representation__to_f64(bits);
    return ret;
  } while (0);

zero:
  do {
    uint64_t bits = h.negative ? 0x8000000000000000 : 0;

    wuffs_base__result_f64 ret;
    ret.status.repr = NULL;
    ret.value = wuffs_base__ieee_754_bit_representation__to_f64(bits);
    return ret;
  } while (0);

infinity:
  do {
    uint64_t bits = h.negative ? 0xFFF0000000000000 : 0x7FF0000000000000;

    wuffs_base__result_f64 ret;
    ret.status.repr = NULL;
    ret.value = wuffs_base__ieee_754_bit_representation__to_f64(bits);
    return ret;
  } while (0);
}

// --------

static inline size_t  //
wuffs_base__private_implementation__render_inf(wuffs_base__slice_u8 dst,
                                               bool neg,
                                               uint32_t options) {
  if (neg) {
    if (dst.len < 4) {
      return 0;
    }
    wuffs_base__store_u32le__no_bounds_check(dst.ptr, 0x666E492D);  // '-Inf'le.
    return 4;
  }

  if (options & WUFFS_BASE__RENDER_NUMBER_XXX__LEADING_PLUS_SIGN) {
    if (dst.len < 4) {
      return 0;
    }
    wuffs_base__store_u32le__no_bounds_check(dst.ptr, 0x666E492B);  // '+Inf'le.
    return 4;
  }

  if (dst.len < 3) {
    return 0;
  }
  wuffs_base__store_u24le__no_bounds_check(dst.ptr, 0x666E49);  // 'Inf'le.
  return 3;
}

static inline size_t  //
wuffs_base__private_implementation__render_nan(wuffs_base__slice_u8 dst) {
  if (dst.len < 3) {
    return 0;
  }
  wuffs_base__store_u24le__no_bounds_check(dst.ptr, 0x4E614E);  // 'NaN'le.
  return 3;
}

static size_t  //
wuffs_base__private_implementation__high_prec_dec__render_exponent_absent(
    wuffs_base__slice_u8 dst,
    wuffs_base__private_implementation__high_prec_dec* h,
    uint32_t precision,
    uint32_t options) {
  size_t n = (h->negative ||
              (options & WUFFS_BASE__RENDER_NUMBER_XXX__LEADING_PLUS_SIGN))
                 ? 1
                 : 0;
  if (h->decimal_point <= 0) {
    n += 1;
  } else {
    n += (size_t)(h->decimal_point);
  }
  if (precision > 0) {
    n += precision + 1;  // +1 for the '.'.
  }

  // Don't modify dst if the formatted number won't fit.
  if (n > dst.len) {
    return 0;
  }

  // Align-left or align-right.
  uint8_t* ptr = (options & WUFFS_BASE__RENDER_NUMBER_XXX__ALIGN_RIGHT)
                     ? &dst.ptr[dst.len - n]
                     : &dst.ptr[0];

  // Leading "±".
  if (h->negative) {
    *ptr++ = '-';
  } else if (options & WUFFS_BASE__RENDER_NUMBER_XXX__LEADING_PLUS_SIGN) {
    *ptr++ = '+';
  }

  // Integral digits.
  if (h->decimal_point <= 0) {
    *ptr++ = '0';
  } else {
    uint32_t m =
        wuffs_base__u32__min(h->num_digits, (uint32_t)(h->decimal_point));
    uint32_t i = 0;
    for (; i < m; i++) {
      *ptr++ = (uint8_t)('0' | h->digits[i]);
    }
    for (; i < (uint32_t)(h->decimal_point); i++) {
      *ptr++ = '0';
    }
  }

  // Separator and then fractional digits.
  if (precision > 0) {
    *ptr++ =
        (options & WUFFS_BASE__RENDER_NUMBER_FXX__DECIMAL_SEPARATOR_IS_A_COMMA)
            ? ','
            : '.';
    uint32_t i = 0;
    for (; i < precision; i++) {
      uint32_t j = ((uint32_t)(h->decimal_point)) + i;
      *ptr++ = (uint8_t)('0' | ((j < h->num_digits) ? h->digits[j] : 0));
    }
  }

  return n;
}

static size_t  //
wuffs_base__private_implementation__high_prec_dec__render_exponent_present(
    wuffs_base__slice_u8 dst,
    wuffs_base__private_implementation__high_prec_dec* h,
    uint32_t precision,
    uint32_t options) {
  int32_t exp = 0;
  if (h->num_digits > 0) {
    exp = h->decimal_point - 1;
  }
  bool negative_exp = exp < 0;
  if (negative_exp) {
    exp = -exp;
  }

  size_t n = (h->negative ||
              (options & WUFFS_BASE__RENDER_NUMBER_XXX__LEADING_PLUS_SIGN))
                 ? 4
                 : 3;  // Mininum 3 bytes: first digit and then "e±".
  if (precision > 0) {
    n += precision + 1;  // +1 for the '.'.
  }
  n += (exp < 100) ? 2 : 3;

  // Don't modify dst if the formatted number won't fit.
  if (n > dst.len) {
    return 0;
  }

  // Align-left or align-right.
  uint8_t* ptr = (options & WUFFS_BASE__RENDER_NUMBER_XXX__ALIGN_RIGHT)
                     ? &dst.ptr[dst.len - n]
                     : &dst.ptr[0];

  // Leading "±".
  if (h->negative) {
    *ptr++ = '-';
  } else if (options & WUFFS_BASE__RENDER_NUMBER_XXX__LEADING_PLUS_SIGN) {
    *ptr++ = '+';
  }

  // Integral digit.
  if (h->num_digits > 0) {
    *ptr++ = (uint8_t)('0' | h->digits[0]);
  } else {
    *ptr++ = '0';
  }

  // Separator and then fractional digits.
  if (precision > 0) {
    *ptr++ =
        (options & WUFFS_BASE__RENDER_NUMBER_FXX__DECIMAL_SEPARATOR_IS_A_COMMA)
            ? ','
            : '.';
    uint32_t i = 1;
    uint32_t j = wuffs_base__u32__min(h->num_digits, precision + 1);
    for (; i < j; i++) {
      *ptr++ = (uint8_t)('0' | h->digits[i]);
    }
    for (; i <= precision; i++) {
      *ptr++ = '0';
    }
  }

  // Exponent: "e±" and then 2 or 3 digits.
  *ptr++ = 'e';
  *ptr++ = negative_exp ? '-' : '+';
  if (exp < 10) {
    *ptr++ = '0';
    *ptr++ = (uint8_t)('0' | exp);
  } else if (exp < 100) {
    *ptr++ = (uint8_t)('0' | (exp / 10));
    *ptr++ = (uint8_t)('0' | (exp % 10));
  } else {
    int32_t e = exp / 100;
    exp -= e * 100;
    *ptr++ = (uint8_t)('0' | e);
    *ptr++ = (uint8_t)('0' | (exp / 10));
    *ptr++ = (uint8_t)('0' | (exp % 10));
  }

  return n;
}

WUFFS_BASE__MAYBE_STATIC size_t  //
wuffs_base__render_number_f64(wuffs_base__slice_u8 dst,
                              double x,
                              uint32_t precision,
                              uint32_t options) {
  // Decompose x (64 bits) into negativity (1 bit), base-2 exponent (11 bits
  // with a -1023 bias) and mantissa (52 bits).
  uint64_t bits = wuffs_base__ieee_754_bit_representation__from_f64(x);
  bool neg = (bits >> 63) != 0;
  int32_t exp2 = ((int32_t)(bits >> 52)) & 0x7FF;
  uint64_t man = bits & 0x000FFFFFFFFFFFFFul;

  // Apply the exponent bias and set the implicit top bit of the mantissa,
  // unless x is subnormal. Also take care of Inf and NaN.
  if (exp2 == 0x7FF) {
    if (man != 0) {
      return wuffs_base__private_implementation__render_nan(dst);
    }
    return wuffs_base__private_implementation__render_inf(dst, neg, options);
  } else if (exp2 == 0) {
    exp2 = -1022;
  } else {
    exp2 -= 1023;
    man |= 0x0010000000000000ul;
  }

  // Ensure that precision isn't too large.
  if (precision > 4095) {
    precision = 4095;
  }

  // Convert from the (neg, exp2, man) tuple to an HPD.
  wuffs_base__private_implementation__high_prec_dec h;
  wuffs_base__private_implementation__high_prec_dec__assign(&h, man, neg);
  if (h.num_digits > 0) {
    wuffs_base__private_implementation__high_prec_dec__lshift(
        &h, exp2 - 52);  // 52 mantissa bits.
  }

  // Handle the "%e" and "%f" formats.
  switch (options & (WUFFS_BASE__RENDER_NUMBER_FXX__EXPONENT_ABSENT |
                     WUFFS_BASE__RENDER_NUMBER_FXX__EXPONENT_PRESENT)) {
    case WUFFS_BASE__RENDER_NUMBER_FXX__EXPONENT_ABSENT:  // The "%"f" format.
      if (options & WUFFS_BASE__RENDER_NUMBER_FXX__JUST_ENOUGH_PRECISION) {
        wuffs_base__private_implementation__high_prec_dec__round_just_enough(
            &h, exp2, man);
        int32_t p = ((int32_t)(h.num_digits)) - h.decimal_point;
        precision = ((uint32_t)(wuffs_base__i32__max(0, p)));
      } else {
        wuffs_base__private_implementation__high_prec_dec__round_nearest(
            &h, ((int32_t)precision) + h.decimal_point);
      }
      return wuffs_base__private_implementation__high_prec_dec__render_exponent_absent(
          dst, &h, precision, options);

    case WUFFS_BASE__RENDER_NUMBER_FXX__EXPONENT_PRESENT:  // The "%e" format.
      if (options & WUFFS_BASE__RENDER_NUMBER_FXX__JUST_ENOUGH_PRECISION) {
        wuffs_base__private_implementation__high_prec_dec__round_just_enough(
            &h, exp2, man);
        precision = (h.num_digits > 0) ? (h.num_digits - 1) : 0;
      } else {
        wuffs_base__private_implementation__high_prec_dec__round_nearest(
            &h, ((int32_t)precision) + 1);
      }
      return wuffs_base__private_implementation__high_prec_dec__render_exponent_present(
          dst, &h, precision, options);
  }

  // We have the "%g" format and so precision means the number of significant
  // digits, not the number of digits after the decimal separator. Perform
  // rounding and determine whether to use "%e" or "%f".
  int32_t e_threshold = 0;
  if (options & WUFFS_BASE__RENDER_NUMBER_FXX__JUST_ENOUGH_PRECISION) {
    wuffs_base__private_implementation__high_prec_dec__round_just_enough(
        &h, exp2, man);
    precision = h.num_digits;
    e_threshold = 6;
  } else {
    if (precision == 0) {
      precision = 1;
    }
    wuffs_base__private_implementation__high_prec_dec__round_nearest(
        &h, ((int32_t)precision));
    e_threshold = ((int32_t)precision);
    int32_t nd = ((int32_t)(h.num_digits));
    if ((e_threshold > nd) && (nd >= h.decimal_point)) {
      e_threshold = nd;
    }
  }

  // Use the "%e" format if the exponent is large.
  int32_t e = h.decimal_point - 1;
  if ((e < -4) || (e_threshold <= e)) {
    uint32_t p = wuffs_base__u32__min(precision, h.num_digits);
    return wuffs_base__private_implementation__high_prec_dec__render_exponent_present(
        dst, &h, (p > 0) ? (p - 1) : 0, options);
  }

  // Use the "%f" format otherwise.
  int32_t p = ((int32_t)precision);
  if (p > h.decimal_point) {
    p = ((int32_t)(h.num_digits));
  }
  precision = ((uint32_t)(wuffs_base__i32__max(0, p - h.decimal_point)));
  return wuffs_base__private_implementation__high_prec_dec__render_exponent_absent(
      dst, &h, precision, options);
}
