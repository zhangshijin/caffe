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

#ifdef USE_OPENCV
#include <opencv2/core/core.hpp>
#endif  // USE_OPENCV

#include <vector>

#include "caffe/layers/memory_data_layer.hpp"

namespace caffe {

template <typename Dtype>
void MemoryDataLayer<Dtype>::DataLayerSetUp(const vector<Blob<Dtype>*>& bottom,
     const vector<Blob<Dtype>*>& top) {
  batch_size_ = this->layer_param_.memory_data_param().batch_size();
  channels_ = this->layer_param_.memory_data_param().channels();
  height_ = this->layer_param_.memory_data_param().height();
  width_ = this->layer_param_.memory_data_param().width();
  size_ = channels_ * height_ * width_;
  CHECK_GT(batch_size_ * size_, 0) <<
      "batch_size, channels, height, and width must be specified and"
      " positive in memory_data_param";
  vector<int> label_shape(1, batch_size_);
  top[0]->Reshape(batch_size_, channels_, height_, width_);
  top[1]->Reshape(label_shape);
  added_data_.Reshape(batch_size_, channels_, height_, width_);
  added_label_.Reshape(label_shape);
  data_ = NULL;
  labels_ = NULL;
  added_data_.cpu_data();
  added_label_.cpu_data();
}

template <typename Dtype>
void MemoryDataLayer<Dtype>::AddDatumVector(const vector<Datum>& datum_vector) {
  CHECK(!has_new_data_) <<
      "Can't add data until current data has been consumed.";
  size_t num = datum_vector.size();
  CHECK_GT(num, 0) << "There is no datum to add.";
  CHECK_EQ(num % batch_size_, 0) <<
      "The added data must be a multiple of the batch size.";
  added_data_.Reshape(num, channels_, height_, width_);
  added_label_.Reshape(num, 1, 1, 1);
  // Apply data transformations (mirror, scale, crop...)
  this->data_transformer_->Transform(datum_vector, &added_data_);
  // Copy Labels
  Dtype* top_label = added_label_.mutable_cpu_data();
  for (int item_id = 0; item_id < num; ++item_id) {
    top_label[item_id] = datum_vector[item_id].label();
  }
  // num_images == batch_size_
  Dtype* top_data = added_data_.mutable_cpu_data();
  Reset(top_data, top_label, num);
  has_new_data_ = true;
}

#ifdef USE_OPENCV
template <typename Dtype>
void MemoryDataLayer<Dtype>::AddMatVector(const vector<cv::Mat>& mat_vector,
    const vector<int>& labels) {
  size_t num = mat_vector.size();
  CHECK(!has_new_data_) <<
      "Can't add mat until current data has been consumed.";
  CHECK_GT(num, 0) << "There is no mat to add";
  CHECK_EQ(num % batch_size_, 0) <<
      "The added data must be a multiple of the batch size.";
  added_data_.Reshape(num, channels_, height_, width_);
  added_label_.Reshape(num, 1, 1, 1);
  // Apply data transformations (mirror, scale, crop...)
  this->data_transformer_->Transform(mat_vector, &added_data_);
  // Copy Labels
  Dtype* top_label = added_label_.mutable_cpu_data();
  for (int item_id = 0; item_id < num; ++item_id) {
    top_label[item_id] = labels[item_id];
  }
  // num_images == batch_size_
  Dtype* top_data = added_data_.mutable_cpu_data();
  Reset(top_data, top_label, num);
  has_new_data_ = true;
}
#endif  // USE_OPENCV

template <typename Dtype>
void MemoryDataLayer<Dtype>::Reset(Dtype* data, Dtype* labels, int n) {
  CHECK(data);
  CHECK(labels);
  CHECK_EQ(n % batch_size_, 0) << "n must be a multiple of batch size";
  // Warn with transformation parameters since a memory array is meant to
  // be generic and no transformations are done with Reset().
  if (this->layer_param_.has_transform_param()) {
    LOG(WARNING) << this->type() << " does not transform array data on Reset()";
  }
  data_ = data;
  labels_ = labels;
  n_ = n;
  pos_ = 0;
}

template <typename Dtype>
void MemoryDataLayer<Dtype>::set_batch_size(int new_size) {
  CHECK(!has_new_data_) <<
      "Can't change batch_size until current data has been consumed.";
  batch_size_ = new_size;
  added_data_.Reshape(batch_size_, channels_, height_, width_);
  added_label_.Reshape(batch_size_, 1, 1, 1);
}

template <typename Dtype>
void MemoryDataLayer<Dtype>::Forward_cpu(const vector<Blob<Dtype>*>& bottom,
      const vector<Blob<Dtype>*>& top) {
  CHECK(data_) << "MemoryDataLayer needs to be initialized by calling Reset";
  top[0]->Reshape(batch_size_, channels_, height_, width_);
  top[1]->Reshape(batch_size_, 1, 1, 1);
  top[0]->set_cpu_data(data_ + pos_ * size_);
  top[1]->set_cpu_data(labels_ + pos_);
  pos_ = (pos_ + batch_size_) % n_;
  if (pos_ == 0)
    has_new_data_ = false;
}

INSTANTIATE_CLASS(MemoryDataLayer);
REGISTER_LAYER_CLASS(MemoryData);

}  // namespace caffe
