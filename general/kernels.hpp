// Copyright (c) 2010, Lawrence Livermore National Security, LLC. Produced at
// the Lawrence Livermore National Laboratory. LLNL-CODE-443211. All Rights
// reserved. See file COPYRIGHT for details.
//
// This file is part of the MFEM library. For more information and source code
// availability see http://mfem.org.
//
// MFEM is free software; you can redistribute it and/or modify it under the
// terms of the GNU Lesser General Public License (as published by the Free
// Software Foundation) version 2.1 dated February 1999.

#ifndef MFEM_KERNELS_HPP
#define MFEM_KERNELS_HPP

// *****************************************************************************
#ifdef __NVCC__
template <typename BODY> __global__
void kernel(const size_t N, BODY body) {
  const size_t k = blockDim.x*blockIdx.x + threadIdx.x;
  if (k >= N) return;
  body(k);
}
#endif // __NVCC__

// *****************************************************************************
template <typename DBODY, typename HBODY>
void wrap(const size_t N, DBODY &&d_body, HBODY &&h_body){
#ifdef __NVCC__
   const bool gpu = config::Get().Cuda();
   if (gpu){
      const size_t blockSize = 256;
      const size_t gridSize = (N+blockSize-1)/blockSize;
      kernel<<<gridSize, blockSize>>>(N,d_body);
      return;
   }
#endif // __NVCC__
   for(size_t k=0; k<N; k+=1){ h_body(k); }
}

// *****************************************************************************
// * FORALL split
// *****************************************************************************
#define forall(i,end,body) wrap(end,                                  \
                                [=] __device__ (size_t i){body},      \
                                [=] (size_t i){body})

// *****************************************************************************
#ifdef __NVCC__
#define CUDA_STD_BLOCK 256
#define cuKer(name,end,...) name ## 0<<<((end+256-1)/256),256>>>(end,__VA_ARGS__)
#define call0(name,id,grid,blck,...) call[id]<<<grid,blck>>>(__VA_ARGS__);
#else
#define cuKer(name,end,...) name ## 0(end,__VA_ARGS__)
#define call0(name,id,grid,blck,...) call[id](__VA_ARGS__);
#endif // __NVCC__

#endif // MFEM_KERNELS_HPP