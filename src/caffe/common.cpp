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

#include <boost/thread.hpp>
#include <glog/logging.h>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <memory>
#include "caffe/common.hpp"
#include "caffe/util/rng.hpp"

namespace caffe {

// Make sure each thread can have different values.
static boost::thread_specific_ptr<Caffe> thread_instance_;

#ifdef USE_MLU
// Use real device by default
int Caffe::DeviceFlag = 0;
#endif

Caffe& Caffe::Get() {
  if (!thread_instance_.get()) {
    thread_instance_.reset(new Caffe());
  }
  return *(thread_instance_.get());
}

// random seeding
int64_t cluster_seedgen(void) {
  int64_t s, seed, pid;
  FILE* f = fopen("/dev/urandom", "rb");
  if (f && fread(&seed, 1, sizeof(seed), f) == sizeof(seed)) {
    fclose(f);
    return seed;
  }

  LOG(INFO) << "System entropy source not available, "
              "using fallback algorithm to generate seed instead.";
  if (f)
    fclose(f);

  pid = getpid();
  s = time(NULL);
  seed = std::abs(((s * 181) * ((pid - 83) * 359)) % 104729);
  return seed;
}


void GlobalInit(int* pargc, char*** pargv) {
  // Google flags.
  ::gflags::ParseCommandLineFlags(pargc, pargv, true);
  // Google logging.
  ::google::InitGoogleLogging(*(pargv)[0]);
  // Provide a backtrace on segfault.
  ::google::InstallFailureSignalHandler();
}

Caffe::Caffe()
    :
#ifdef USE_CUDA
      cublas_handle_(NULL), curand_generator_(NULL),
#endif
      random_generator_(), mode_(Caffe::CPU),
      solver_count_(1), solver_rank_(0), multiprocess_(false)
#ifdef USE_MLU
      , data_parallel_(1), model_parallel_(1), channel_id_(0),
      in_datastrategy_(-1), out_datastrategy_(-1), affinity_(0x01),
      queue_(nullptr), core_version_(CNML_C10), in_dataorder_(0),
      out_dataorder_(0)
#endif
{
#ifdef USE_CUDA
    // Try to create a cublas handler, and report an error if failed (but we will
    // keep the program running as one might just want to run CPU code).
    if (cublasCreate(&cublas_handle_) != CUBLAS_STATUS_SUCCESS) {
      LOG(ERROR) << "Cannot create Cublas handle. Cublas won't be available.";
    }
    // Try to create a curand handler.
    if (curandCreateGenerator(&curand_generator_, CURAND_RNG_PSEUDO_DEFAULT)
        != CURAND_STATUS_SUCCESS ||
        curandSetPseudoRandomGeneratorSeed(curand_generator_, cluster_seedgen())
        != CURAND_STATUS_SUCCESS) {
      LOG(ERROR) << "Cannot create Curand generator. Curand won't be available.";
    }
#endif
#ifdef USE_MLU
    compute_forw_param_.data_parallelism = &data_parallel_;
    compute_forw_param_.affinity = &affinity_;
    compute_forw_param_.end = CNRT_PARAM_END;
#endif
}

Caffe::~Caffe() {
#ifdef USE_CUDA
  if (cublas_handle_) CUBLAS_CHECK(cublasDestroy(cublas_handle_));
  if (curand_generator_) {
    CURAND_CHECK(curandDestroyGenerator(curand_generator_));
  }
#endif
}

void Caffe::set_random_seed(const unsigned int seed) {
#ifdef USE_CUDA
  // Curand seed
  static bool g_curand_availability_logged = false;
  if (Get().curand_generator_) {
    CURAND_CHECK(curandSetPseudoRandomGeneratorSeed(curand_generator(),
        seed));
    CURAND_CHECK(curandSetGeneratorOffset(curand_generator(), 0));
  } else {
    if (!g_curand_availability_logged) {
        LOG(ERROR) <<
            "Curand not available. Skipping setting the curand seed.";
        g_curand_availability_logged = true;
    }
  }
#endif
  // RNG seed
  Get().random_generator_.reset(new RNG(seed));
}

void Caffe::SetDevice(const int device_id) {
#ifdef USE_CUDA
  int current_device;
  CUDA_CHECK(cudaGetDevice(&current_device));
  if (current_device == device_id) {
    return;
  }
  // The call to cudaSetDevice must come before any calls to Get, which
  // may perform initialization using the GPU.
  CUDA_CHECK(cudaSetDevice(device_id));
  if (Get().cublas_handle_) CUBLAS_CHECK(cublasDestroy(Get().cublas_handle_));
  if (Get().curand_generator_) {
    CURAND_CHECK(curandDestroyGenerator(Get().curand_generator_));
  }
  CUBLAS_CHECK(cublasCreate(&Get().cublas_handle_));
  CURAND_CHECK(curandCreateGenerator(&Get().curand_generator_,
      CURAND_RNG_PSEUDO_DEFAULT));
  CURAND_CHECK(curandSetPseudoRandomGeneratorSeed(Get().curand_generator_,
      cluster_seedgen()));
#else
  NO_GPU;
#endif
}

void Caffe::DeviceQuery() {
#ifdef USE_CUDA
  cudaDeviceProp prop;
  int device;
  if (cudaSuccess != cudaGetDevice(&device)) {
    printf("No cuda device present.\n");
    return;
  }
  CUDA_CHECK(cudaGetDeviceProperties(&prop, device));
  LOG(INFO) << "Device id:                     " << device;
  LOG(INFO) << "Major revision number:         " << prop.major;
  LOG(INFO) << "Minor revision number:         " << prop.minor;
  LOG(INFO) << "Name:                          " << prop.name;
  LOG(INFO) << "Total global memory:           " << prop.totalGlobalMem;
  LOG(INFO) << "Total shared memory per block: " << prop.sharedMemPerBlock;
  LOG(INFO) << "Total registers per block:     " << prop.regsPerBlock;
  LOG(INFO) << "Warp size:                     " << prop.warpSize;
  LOG(INFO) << "Maximum memory pitch:          " << prop.memPitch;
  LOG(INFO) << "Maximum threads per block:     " << prop.maxThreadsPerBlock;
  LOG(INFO) << "Maximum dimension of block:    "
      << prop.maxThreadsDim[0] << ", " << prop.maxThreadsDim[1] << ", "
      << prop.maxThreadsDim[2];
  LOG(INFO) << "Maximum dimension of grid:     "
      << prop.maxGridSize[0] << ", " << prop.maxGridSize[1] << ", "
      << prop.maxGridSize[2];
  LOG(INFO) << "Clock rate:                    " << prop.clockRate;
  LOG(INFO) << "Total constant memory:         " << prop.totalConstMem;
  LOG(INFO) << "Texture alignment:             " << prop.textureAlignment;
  LOG(INFO) << "Concurrent copy and execution: "
      << (prop.deviceOverlap ? "Yes" : "No");
  LOG(INFO) << "Number of multiprocessors:     " << prop.multiProcessorCount;
  LOG(INFO) << "Kernel execution timeout:      "
      << (prop.kernelExecTimeoutEnabled ? "Yes" : "No");
  return;
#else
  NO_GPU;
#endif
}

bool Caffe::CheckDevice(const int device_id) {
#ifdef USE_CUDA
  // This function checks the availability of GPU #device_id.
  // It attempts to create a context on the device by calling cudaFree(0).
  // cudaSetDevice() alone is not sufficient to check the availability.
  // It lazily records device_id, however, does not initialize a
  // context. So it does not know if the host thread has the permission to use
  // the device or not.
  //
  // In a shared environment where the devices are set to EXCLUSIVE_PROCESS
  // or EXCLUSIVE_THREAD mode, cudaSetDevice() returns cudaSuccess
  // even if the device is exclusively occupied by another process or thread.
  // Cuda operations that initialize the context are needed to check
  // the permission. cudaFree(0) is one of those with no side effect,
  // except the context initialization.
  bool r = ((cudaSuccess == cudaSetDevice(device_id)) &&
            (cudaSuccess == cudaFree(0)));
  // reset any error that may have occurred.
  cudaGetLastError();
  return r;
#else
  NO_GPU;
  return false;
#endif
}

int Caffe::FindDevice(const int start_id) {
#ifdef USE_CUDA
  // This function finds the first available device by checking devices with
  // ordinal from start_id to the highest available value. In the
  // EXCLUSIVE_PROCESS or EXCLUSIVE_THREAD mode, if it succeeds, it also
  // claims the device due to the initialization of the context.
  int count = 0;
  CUDA_CHECK(cudaGetDeviceCount(&count));
  for (int i = start_id; i < count; i++) {
    if (CheckDevice(i)) return i;
  }
  return -1;
#else
  NO_GPU;
  return -1;
#endif
}

class Caffe::RNG::Generator {
  public:
  Generator() : rng_(new caffe::rng_t(cluster_seedgen())) {}
  explicit Generator(unsigned int seed) : rng_(new caffe::rng_t(seed)) {}
  caffe::rng_t* rng() { return rng_.get(); }
  private:
  shared_ptr<caffe::rng_t> rng_;
};

Caffe::RNG::RNG() : generator_(new Generator()) { }

Caffe::RNG::RNG(unsigned int seed) : generator_(new Generator(seed)) { }

Caffe::RNG& Caffe::RNG::operator=(const RNG& other) {
  generator_ = other.generator_;
  return *this;
}

void* Caffe::RNG::generator() {
  return static_cast<void*>(generator_->rng());
}

#ifdef USE_MLU
const char* mluGetErrorString(cnmlStatus_t status) {
  switch (status) {
  case CNML_STATUS_SUCCESS:
    return "CNML_STATUS_SUCCESS";
  case CNML_STATUS_DOMAINERR:
    return "CNML_STATUS_DOMAINERR";
  case CNML_STATUS_INVALIDARG:
    return "CNML_STATUS_INVALIDARG";
  case CNML_STATUS_LENGTHERR:
    return "CNML_STATUS_LENGTHERR";
  case CNML_STATUS_OUTOFRANGE:
    return "CNML_STATUS_OUTOFRANGE";
  case CNML_STATUS_RANGEERR:
    return "CNML_STATUS_RANGEERR";
  case CNML_STATUS_OVERFLOWERR:
    return "CNML_STATUS_OVERFLOWERR";
  case CNML_STATUS_UNDERFLOWERR:
    return "CNML_STATUS_UNDERFLOWERR";
  case CNML_STATUS_INVALIDPARAM:
    return "CNML_STATUS_INVALIDPARAM";
  case CNML_STATUS_BADALLOC:
    return "CNML_STATUS_BADALLOC";
  case CNML_STATUS_BADTYPEID:
    return "CNML_STATUS_BADTYPEID";
  case CNML_STATUS_BADCAST:
    return "CNML_STATUS_BADCAST";
  case CNML_STATUS_UNSUPPORT:
    return "CNML_STATUS_UNSUPPORT";
  case CNML_STATUS_NODEVICE:
    return "CNML_STATUS_NODEVICE";
  }
  return "Unknown mlu status";
}

const char* cnrtGetErrorString(cnrtRet_t status) {
  switch (status) {
  case CNRT_RET_SUCCESS:
    return "No error";
  case CNRT_RET_ERR_INVALID:
    return "Invalid argument";
  case CNRT_RET_ERR_NOMEM:
    return "Out of memory";
  case CNRT_RET_ERR_NODEV:
    return "No such device";
  case CNRT_RET_ERR_IO:
    return "I/O error";
  case CNRT_RET_ERR_SYS:
    return "System error";
  case CNRT_RET_ERR_ACCES:
    return "Permission denied";
  case CNRT_RET_ERR_FAULT:
    return "Bad address";
  case CNRT_RET_ERR_BUSY:
    return "Device or resource busy";
  case CNRT_RET_ERR_TIMEOUT:
    return "Time expired";
  case CNRT_RET_ERR_EXIST:
    return "Resource or file already exists";
  case CNRT_RET_ERR_NOSYS:
    return "Function not implemenmted";
  case CNRT_RET_ERR_AGAIN:
    return "try again later";
  case CNRT_RET_ERR_NORES:
    return "Out of resource";
  case CNRT_RET_ERR_UNSUPPORTED:
    return "Unsupported operation";
  case CNRT_RET_ERR_INVALID_POINTER:
    return "Invalid pointer";
  case CNRT_RET_ERR_NO_EXIST:
    return "Resource or file doesn't exist";
  case CNRT_RET_ERR_BROKEN:
    return "Data transmission is broken";
  case CNRT_RET_ERR_INIT:
    return "Uninitialized";
  case CNRT_RET_ERR_QUEUE:
    return "Failure on Stream";
  case CNRT_RET_ERR_OUT_RANGE:
    return "Number out of range";
  case CNRT_RET_ERR_MATH_OVERFLOW:
    return "Math result not representable";
  case CNRT_RET_ERR_FUNC_CALL:
    return "Failure to call runtime functions";
  case CNRT_RET_ERR_UNHANDLED:
    return "Unhandled error";
  case CNRT_RET_ERR_INVALID_TYPE:
    return "Invalid type";
  case CNRT_RET_ERR_INVALID_OP:
    return "Invalid operation";
  case CNRT_RET_ERR_MLU:
    return "MLU error";
  case CNRT_RET_ERR_NOTIFIER:
    return "Failure on event operation";
  case CNRT_RET_ERR_UNKNOWN:
    return "Unknown error";
  case CNRT_RET_ERR_MAX:
    return "The last one";
  default:
    return "Unknown CNRT status";
  }
  return "Unknown CNRT status";
}

#endif

#ifdef USE_CUDA
const char* cublasGetErrorString(cublasStatus_t error) {
  switch (error) {
  case CUBLAS_STATUS_SUCCESS:
    return "CUBLAS_STATUS_SUCCESS";
  case CUBLAS_STATUS_NOT_INITIALIZED:
    return "CUBLAS_STATUS_NOT_INITIALIZED";
  case CUBLAS_STATUS_ALLOC_FAILED:
    return "CUBLAS_STATUS_ALLOC_FAILED";
  case CUBLAS_STATUS_INVALID_VALUE:
    return "CUBLAS_STATUS_INVALID_VALUE";
  case CUBLAS_STATUS_ARCH_MISMATCH:
    return "CUBLAS_STATUS_ARCH_MISMATCH";
  case CUBLAS_STATUS_MAPPING_ERROR:
    return "CUBLAS_STATUS_MAPPING_ERROR";
  case CUBLAS_STATUS_EXECUTION_FAILED:
    return "CUBLAS_STATUS_EXECUTION_FAILED";
  case CUBLAS_STATUS_INTERNAL_ERROR:
    return "CUBLAS_STATUS_INTERNAL_ERROR";
#if CUDA_VERSION >= 6000
  case CUBLAS_STATUS_NOT_SUPPORTED:
    return "CUBLAS_STATUS_NOT_SUPPORTED";
#endif
#if CUDA_VERSION >= 6050
  case CUBLAS_STATUS_LICENSE_ERROR:
    return "CUBLAS_STATUS_LICENSE_ERROR";
#endif
  }
  return "Unknown cublas status";
}

const char* curandGetErrorString(curandStatus_t error) {
  switch (error) {
  case CURAND_STATUS_SUCCESS:
    return "CURAND_STATUS_SUCCESS";
  case CURAND_STATUS_VERSION_MISMATCH:
    return "CURAND_STATUS_VERSION_MISMATCH";
  case CURAND_STATUS_NOT_INITIALIZED:
    return "CURAND_STATUS_NOT_INITIALIZED";
  case CURAND_STATUS_ALLOCATION_FAILED:
    return "CURAND_STATUS_ALLOCATION_FAILED";
  case CURAND_STATUS_TYPE_ERROR:
    return "CURAND_STATUS_TYPE_ERROR";
  case CURAND_STATUS_OUT_OF_RANGE:
    return "CURAND_STATUS_OUT_OF_RANGE";
  case CURAND_STATUS_LENGTH_NOT_MULTIPLE:
    return "CURAND_STATUS_LENGTH_NOT_MULTIPLE";
  case CURAND_STATUS_DOUBLE_PRECISION_REQUIRED:
    return "CURAND_STATUS_DOUBLE_PRECISION_REQUIRED";
  case CURAND_STATUS_LAUNCH_FAILURE:
    return "CURAND_STATUS_LAUNCH_FAILURE";
  case CURAND_STATUS_PREEXISTING_FAILURE:
    return "CURAND_STATUS_PREEXISTING_FAILURE";
  case CURAND_STATUS_INITIALIZATION_FAILED:
    return "CURAND_STATUS_INITIALIZATION_FAILED";
  case CURAND_STATUS_ARCH_MISMATCH:
    return "CURAND_STATUS_ARCH_MISMATCH";
  case CURAND_STATUS_INTERNAL_ERROR:
    return "CURAND_STATUS_INTERNAL_ERROR";
  }
  return "Unknown curand status";
}
#endif  // USE_CUDA

}  // namespace caffe
