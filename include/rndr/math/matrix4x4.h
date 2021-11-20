// MIT License
//
// Copyright (c) 2021 Marko Kostić
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include "rndr/rndr.h"

namespace rndr
{

struct Matrix4x4
{
    real m[4][4];

    Matrix4x4();
    Matrix4x4(const real mat[4][4]);

    // clang-format off
    Matrix4x4(real t00, real t01, real t02, real t03,
              real t10, real t11, real t12, real t13,
              real t20, real t21, real t22, real t23,
              real t30, real t31, real t32, real t33);
    // clang-format on

    bool operator==(const Matrix4x4& other) const;
    bool operator!=(const Matrix4x4& other) const;

    Matrix4x4 Transpose() const;

    Matrix4x4 Inverse() const;
};

Matrix4x4 Multiply(const Matrix4x4 m1, const Matrix4x4 m2);

}  // namespace rndr
