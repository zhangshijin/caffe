/*
All modification made by Cambricon Corporation: © 2018--2019 Cambricon Corporation
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

#ifdef USE_MLU

#include <algorithm>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "caffe/filler.hpp"
#include "caffe/layers/mlu_crop_layer.hpp"
#include "caffe/mlu/fusion.hpp"
#include "caffe/util/math_functions.hpp"

namespace caffe {

template <typename Dtype>
void MLUCropLayer<Dtype>::LayerSetUp(const vector<Blob<Dtype>*>& bottom,
      const vector<Blob<Dtype>*>& top) {
  CropLayer<Dtype>::LayerSetUp(bottom, top);
}

template <typename Dtype>
void MLUCropLayer<Dtype>::Reshape_tensor(const vector<Blob<Dtype>*>& bottom,
                           const vector<Blob<Dtype>*>& top) {
  BaseDataType cpu_dtype = sizeof(Dtype) == 4 ? DT_FLOAT32 : DT_DOUBLE;
  BaseDataType mlu_dtype = DT_FLOAT16;

  const CropParameter& param = this->layer_param_.crop_param();
  const int start_axis = bottom[0]->CanonicalAxisIndex(param.axis());
  vector<int> new_shape(bottom[0]->shape());
  //  this is redundant for now since reshape happens only once when layer
  //  is set up. But it is necessary in the future, so do it anyway.
  std::fill(offsets_v_.begin(), offsets_v_.end(), 0);
  for (int i = start_axis; i < bottom[0]->num_axes(); i++) {
    new_shape[i] = bottom[1]->shape(i);
    if (param.offset_size() == 1) {
      offsets_v_[i] = param.offset(0);
    } else if (param.offset_size() > 1) {
      offsets_v_[i] = param.offset(i-start_axis);
    }
    // Check that the crop and offset are within the dimension's bounds.
    CHECK_GE(bottom[0]->shape(i) - offsets_v_[i], bottom[1]->shape(i))
          << "the crop for dimension " << i << " is out-of-bounds with "
          << "size " << bottom[1]->shape(i) << " and offset " << offsets_v_[i];
  }
  top[0]->Reshape(new_shape, cpu_dtype, mlu_dtype, CNML_TENSOR);
}


template <typename Dtype>
void MLUCropLayer<Dtype>::MLUDestroyOp() {
  if (crop_param_ptr_ != nullptr) {
    MLU_CHECK(cnmlDestroyGrepOpParam(&crop_param_ptr_));
    crop_param_ptr_ = nullptr;
  }
  if (crop_op_ptr_ != nullptr) {
    MLU_CHECK(cnmlDestroyBaseOp(&crop_op_ptr_));
    crop_op_ptr_ = nullptr;
  }
}

template <typename Dtype>
void MLUCropLayer<Dtype>::MLUCompileOp() {
  MLU_CHECK(cnmlCompileBaseOp(crop_op_ptr_, Caffe::rt_core(),
            Caffe::model_parallel()));
}

template <typename Dtype>
void MLUCropLayer<Dtype>::fuse(MFusion<Dtype>* fuser) {
  fuser->fuse(crop_op_ptr_);
}

template <typename Dtype>
void MLUCropLayer<Dtype>::MLUCreateOpBindData(
    const vector<Blob<Dtype>*>& bottom,
    const vector<Blob<Dtype>*>& top) {
  MLU_CHECK(cnmlCreateGrepOpParam(&crop_param_ptr_,
         offsets_v_[0],
         offsets_v_[2],
         offsets_v_[3],
         offsets_v_[1]));
  MLU_CHECK(cnmlCreateGrepOp(&crop_op_ptr_,
          crop_param_ptr_,
          bottom[0]->mlu_tensor(),
          top[0]->mlu_tensor()));
}

template <typename Dtype>
void MLUCropLayer<Dtype>::Forward_mlu(
        const vector<Blob<Dtype>*>& bottom,
        const vector<Blob<Dtype>*>& top) {
  MLU_CHECK(cnmlComputeGrepOpForward_V3(crop_op_ptr_,
            bottom[0]->mutable_mlu_data(),
            top[0]->mutable_mlu_data(),
            Caffe::forward_param(),
            Caffe::queue()));
}

INSTANTIATE_CLASS(MLUCropLayer);

}  // namespace caffe

#endif  // USE_MLU
