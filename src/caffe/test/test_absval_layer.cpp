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

#include <vector>
#include "caffe/blob.hpp"
#include "caffe/common.hpp"
#include "caffe/filler.hpp"
#include "caffe/layers/absval_layer.hpp"
#include "caffe/layers/mlu_absval_layer.hpp"
#include "caffe/test/test_caffe_main.hpp"
#include "gtest/gtest.h"

namespace caffe {

template <typename Dtype>
float caffe_absval(const Blob<Dtype>* bottom,
    const Blob<Dtype>* top_blob, float errRate) {
  const Dtype* bottom_data = bottom->cpu_data();
  const Dtype* top_data = top_blob->cpu_data();
  float err_sum = 0, sum = 0;
  for (int i = 0; i < bottom->count(); i++) {
     Dtype top = fabs(bottom_data[i]);
     EXPECT_NEAR(top_data[i], top, errRate);
     err_sum += std::abs(top_data[i] - top);
     sum += std::abs(top);
  }
  EXPECT_LE(err_sum/sum, 5e-4);
  return err_sum/sum;
}

// CPUDeviceTest

template <typename TypeParam>
class AbsValLayerTest : public CPUDeviceTest<TypeParam> {
typedef typename TypeParam::Dtype Dtype;

  protected:
    AbsValLayerTest()
       : blob_bottom_(new Blob<Dtype>(3, 5, 8, 10)),
         blob_top_(new Blob<Dtype>()) {}
    void SetUp() {
      FillerParameter filler_param;
      GaussianFiller<Dtype> filler(filler_param);
      this->blob_bottom_vec_.clear();
      this->blob_top_vec_.clear();
      filler.Fill(this->blob_bottom_);
      blob_bottom_vec_.push_back(blob_bottom_);
      blob_top_vec_.push_back(blob_top_);
    }
    virtual ~AbsValLayerTest() {
      delete blob_bottom_;
      delete blob_top_;
    }
    virtual void TestForward() {
      SetUp();
      LayerParameter layer_param;
      AbsValLayer<Dtype> layer(layer_param);
      layer.SetUp(this->blob_bottom_vec_, this->blob_top_vec_);
      layer.Forward(this->blob_bottom_vec_, this->blob_top_vec_);
      caffe_absval(this->blob_bottom_, this->blob_top_, 1e-5);
    }

  Blob<Dtype>* const blob_bottom_;
  Blob<Dtype>* const blob_top_;
  vector<Blob<Dtype>*> blob_bottom_vec_;
  vector<Blob<Dtype>*> blob_top_vec_;
};

TYPED_TEST_CASE(AbsValLayerTest, TestDtypesAndDevices);

TYPED_TEST(AbsValLayerTest, TestSetup) {
  typedef typename TypeParam::Dtype Dtype;
  LayerParameter layer_param;
  AbsValLayer<Dtype> layer(layer_param);
  layer.SetUp(this->blob_bottom_vec_, this->blob_top_vec_);
  EXPECT_EQ(this->blob_top_->num(), 3);
  EXPECT_EQ(this->blob_top_->channels(), 5);
  EXPECT_EQ(this->blob_top_->height(), 8);
  EXPECT_EQ(this->blob_top_->width(), 10);
}

TYPED_TEST(AbsValLayerTest, TestForward) { this->TestForward(); }

TYPED_TEST(AbsValLayerTest, TestForward1Batch) {
  this->blob_bottom_->Reshape(1, 8, 10, 10);
  this->TestForward();
}

TYPED_TEST(AbsValLayerTest, TestForward1Dim) {
  vector<int> shape;
  shape.push_back(51);
  this->blob_bottom_->Reshape(shape);
  this->TestForward();
}

TYPED_TEST(AbsValLayerTest, TestForward2Dim) {
  vector<int> shape;
  shape.push_back(51);
  shape.push_back(31);
  this->blob_bottom_->Reshape(shape);
  this->TestForward();
}

#ifdef USE_MLU

template <typename TypeParam>
class MLUAbsValLayerTest : public MLUDeviceTest<TypeParam> {
typedef typename TypeParam::Dtype Dtype;

  protected:
    MLUAbsValLayerTest()
       : blob_bottom_(new Blob<Dtype>(3, 5, 8, 10)),
         blob_top_(new Blob<Dtype>()) {}
    void SetUp() {
      FillerParameter filler_param;
      GaussianFiller<Dtype> filler(filler_param);
      this->blob_bottom_vec_.clear();
      this->blob_top_vec_.clear();
      filler.Fill(this->blob_bottom_);
      blob_bottom_vec_.push_back(blob_bottom_);
      blob_top_vec_.push_back(blob_top_);
    }
    virtual ~MLUAbsValLayerTest() {
      delete blob_bottom_;
      delete blob_top_;
    }
    virtual void TestForward() {
      SetUp();
      LayerParameter layer_param;
      MLUAbsValLayer<Dtype> layer(layer_param);
      layer.SetUp(this->blob_bottom_vec_, this->blob_top_vec_);
      layer.Reshape_dispatch(this->blob_bottom_vec_, this->blob_top_vec_);
      layer.Forward(this->blob_bottom_vec_, this->blob_top_vec_);
      float rate = caffe_absval(this->blob_bottom_, this->blob_top_, 3e-3);
      std::ostringstream stream;
      stream << "bottom1:" << blob_bottom_->shape_string().c_str();
      BOTTOM(stream);
      ERR_RATE(rate);
      EVENT_TIME(layer.get_event_time());
    }

  Blob<Dtype>* const blob_bottom_;
  Blob<Dtype>* const blob_top_;
  vector<Blob<Dtype>*> blob_bottom_vec_;
  vector<Blob<Dtype>*> blob_top_vec_;
};

TYPED_TEST_CASE(MLUAbsValLayerTest, TestMLUDevices);

TYPED_TEST(MLUAbsValLayerTest, TestSetup) {
  typedef typename TypeParam::Dtype Dtype;
  LayerParameter layer_param;
  MLUAbsValLayer<Dtype> layer(layer_param);
  layer.SetUp(this->blob_bottom_vec_, this->blob_top_vec_);
  EXPECT_EQ(this->blob_top_->num(), 3);
  EXPECT_EQ(this->blob_top_->channels(), 5);
  EXPECT_EQ(this->blob_top_->height(), 8);
  EXPECT_EQ(this->blob_top_->width(), 10);
}

TYPED_TEST(MLUAbsValLayerTest, TestForward) { this->TestForward(); }

TYPED_TEST(MLUAbsValLayerTest, TestForward1Batch) {
  this->blob_bottom_->Reshape(1, 10, 28, 28);
  this->TestForward();
}

TYPED_TEST(MLUAbsValLayerTest, TestForward1Dim) {
  vector<int> shape;
  shape.push_back(51);
  this->blob_bottom_->Reshape(shape);
  this->TestForward();
}

TYPED_TEST(MLUAbsValLayerTest, TestForward2Dim) {
  vector<int> shape;
  shape.push_back(51);
  shape.push_back(31);
  this->blob_bottom_->Reshape(shape);
  this->TestForward();
}


template <typename TypeParam>
class MFUSAbsValLayerTest : public MFUSDeviceTest<TypeParam> {
typedef typename TypeParam::Dtype Dtype;

  protected:
    MFUSAbsValLayerTest()
       : blob_bottom_(new Blob<Dtype>(3, 5, 8, 10)),
         blob_top_(new Blob<Dtype>()) {}
    void SetUp() {
      FillerParameter filler_param;
      GaussianFiller<Dtype> filler(filler_param);
      this->blob_bottom_vec_.clear();
      this->blob_top_vec_.clear();
      filler.Fill(this->blob_bottom_);
      blob_bottom_vec_.push_back(blob_bottom_);
      blob_top_vec_.push_back(blob_top_);
    }
    virtual ~MFUSAbsValLayerTest() {
      delete blob_bottom_;
      delete blob_top_;
    }
    virtual void TestForward() {
      SetUp();
      LayerParameter layer_param;
      MLUAbsValLayer<Dtype> layer(layer_param);
      layer.SetUp(this->blob_bottom_vec_, this->blob_top_vec_);
      ASSERT_TRUE(layer.mfus_supported());

      MFusion<Dtype> fuser;
      fuser.reset();
      fuser.addInputs(this->blob_bottom_vec_);
      fuser.addOutputs(this->blob_top_vec_);
      layer.Reshape_dispatch(this->blob_bottom_vec_, this->blob_top_vec_);
      layer.fuse(&fuser);
      fuser.compile();
      fuser.forward();
      float rate = caffe_absval(this->blob_bottom_, this->blob_top_, 3e-3);
      std::ostringstream stream;
      stream << "bottom1:" << blob_bottom_->shape_string().c_str();
      BOTTOM(stream);
      ERR_RATE(rate);
      EVENT_TIME(fuser.get_event_time());
    }
    Blob<Dtype>* const blob_bottom_;
    Blob<Dtype>* const blob_top_;
    vector<Blob<Dtype>*> blob_bottom_vec_;
    vector<Blob<Dtype>*> blob_top_vec_;
};

TYPED_TEST_CASE(MFUSAbsValLayerTest, TestMLUDevices);

TYPED_TEST(MFUSAbsValLayerTest, TestSetup) {
  typedef typename TypeParam::Dtype Dtype;
  LayerParameter layer_param;
  MLUAbsValLayer<Dtype> layer(layer_param);
  layer.SetUp(this->blob_bottom_vec_, this->blob_top_vec_);
  EXPECT_EQ(this->blob_top_->num(), 3);
  EXPECT_EQ(this->blob_top_->channels(), 5);
  EXPECT_EQ(this->blob_top_->height(), 8);
  EXPECT_EQ(this->blob_top_->width(), 10);
}

TYPED_TEST(MFUSAbsValLayerTest, TestForward) {
  this->TestForward();
}

TYPED_TEST(MFUSAbsValLayerTest, TestForward1Batch) {
  this->blob_bottom_->Reshape(1, 10, 28, 28);
  this->TestForward();
}

TYPED_TEST(MFUSAbsValLayerTest, TestForward1Dim) {
  vector<int> shape;
  shape.push_back(51);
  this->blob_bottom_->Reshape(shape);
  this->TestForward();
}

TYPED_TEST(MFUSAbsValLayerTest, TestForward2Dim) {
  vector<int> shape;
  shape.push_back(51);
  shape.push_back(31);
  this->blob_bottom_->Reshape(shape);
  this->TestForward();
}

#endif

}  // namespace caffe
