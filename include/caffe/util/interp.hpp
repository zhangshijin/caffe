/*
All modification made by Cambricon Corporation: © 2018 Cambricon Corporation
All rights reserved.
All other contributions:
Copyright (c) 2014--2018, the respective contributors
All rights reserved.
For the list of contributors go to https://github.com/BVLC/caffe/blob/master/CONTRIBUTORS.md
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of Intel Corporation nor the names of its contributors
      may be used to endorse or promote products derived from this software
      without specific prior written permission.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef INCLUDE_CAFFE_UTIL_INTERP_HPP_
#define INCLUDE_CAFFE_UTIL_INTERP_HPP_

#include "caffe/proto/caffe.pb.h"

namespace caffe {

template <typename Dtype>
void caffe_cpu_interp2(const int channels,
  const Dtype *data1, const int x1, const int y1,
  const int height1, const int width1, const int Height1,
  const int Width1, Dtype *data2, const int x2, const int y2,
  const int height2, const int width2, const int Height2,
  const int Width2, bool packed);

// Backward (adjoint) operation
template <typename Dtype>
void caffe_cpu_interp2_backward(const int channels, Dtype *data1,
  const int x1, const int y1, const int height1, const int width1,
  const int Height1, const int Width1, const Dtype *data2,
  const int x2, const int y2, const int height2, const int width2,
  const int Height2, const int Width2, bool packed);

template <typename Dtype>
void caffe_cpu_pyramid2(const int channels,
    const Dtype *data, const int height, const int width,
    Dtype *data_pyr, const int levels, bool packed);

}  // namespace caffe

#endif  // INCLUDE_CAFFE_UTIL_INTERP_HPP_
