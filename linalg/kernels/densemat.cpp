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

#include "../../general/okina.hpp"

namespace mfem
{

// *****************************************************************************
template <class T> __device__ __host__
inline void Swap(T &a, T &b)
{
   T tmp(a);
   a = b;
   b = tmp;
}

// *****************************************************************************
__kernel void kLSolve(const int m, const int n,
		      const double *data, const int *ipiv, double *x)
{
   MFEM_FORALL(k, n,
   {
      double *mx = &x[k*m];
      // X <- P X
      for (int i = 0; i < m; i++)
      {
	 //Swap<double>(mx[i], mx[ipiv[i]]);
         const double tmp = mx[i];
         mx[i] = mx[ipiv[i]];
         mx[ipiv[i]] = tmp;
      }
      // X <- L^{-1} X
      for (int j = 0; j < m; j++)
      {
         const double mx_j = mx[j];
         for (int i = j+1; i < m; i++)
         {
            mx[i] -= data[i+j*m] * mx_j;
         }
      }
   });
}

// *****************************************************************************
__kernel void kUSolve(const int m, const int n,
                      const double *data, double *x)
{
   MFEM_FORALL(k, n,
   {
      double *mx = &x[k*m];
      for (int j = m-1; j >= 0; j--)
      {
         const double x_j = ( mx[j] /= data[j+j*m] );
         for (int i = 0; i < j; i++)
         {
            mx[i] -= data[i+j*m] * x_j;
         }
      }
   });
}

// *****************************************************************************
__kernel void kFactorPrint(const int s, const double *data)
{
   MFEM_FORALL(i, s,
   {
      printf("\n\tdata[%ld]=%f",i,data[i]);
   });
}

// *****************************************************************************
__kernel void kFactorSet(const int s, const double *adata, double *ludata)
{
   MFEM_FORALL(i, s,
   {
      ludata[i] = adata[i];
   });
}

// *****************************************************************************
__kernel void kFactor(const int m, int *ipiv, double *data)
{
   MFEM_FORALL(i, m,
   {
      // pivoting
      {
         int piv = i;
         double a = fabs(data[piv+i*m]);
         for (int j = i+1; j < m; j++)
         {
            const double b = fabs(data[j+i*m]);
            if (b > a)
            {
               a = b;
               piv = j;
            }
         }
         ipiv[i] = piv;
         if (piv != (int) i)
         {
            // swap rows i and piv in both L and U parts
            for (int j = 0; j < m; j++)
            {
               //Swap<double>(data[i+j*m], data[piv+j*m]);
               const double tmp = data[i+j*m];
               data[i+j*m] = data[piv+j*m];
               data[piv+j*m] = tmp;
            }
         }
      }
      const double diim = data[i+i*m];
      assert(diim != 0.0);
      const double a_ii_inv = 1.0/data[i+i*m];
      for (int j = i+1; j < m; j++)
      {
         data[j+i*m] *= a_ii_inv;
      }
      for (int k = i+1; k < m; k++)
      {
         const double a_ik = data[i+k*m];
         for (int j = i+1; j < m; j++)
         {
            data[j+k*m] -= a_ik * data[j+i*m];
         }
      }
   });
}

// **************************************************************************
__kernel void DenseMatrixSet(const double dd,
                             const size_t size,
                             double *data)
{
   MFEM_FORALL(i, size, data[i] = dd;);
}

// **************************************************************************
__kernel void DenseMatrixTranspose(const size_t height,
                                   const size_t width,
                                   double *data,
                                   const double *mdata)
{
   MFEM_FORALL(i, height,
   {
      for (size_t j=0; j<width; j+=1)
      {
         data[i+j*height] = mdata[j+i*height];
      }
   });
}

// *****************************************************************************
__kernel void kMultAAt(const size_t height, const size_t width,
                       const double *a, double *aat)
{
   MFEM_FORALL(i, height,
   {
      for (size_t j=0; j<=i; j++)
      {
         double temp = 0.0;
         for (size_t k=0; k<width; k++)
         {
            temp += a[i+k*height] * a[j+k*height];
         }
         aat[j+i*height] = aat[i+j*height] = temp;
      }
   });
}

// *****************************************************************************
__kernel void kGradToDiv(const size_t n, const double *data, double *ddata)
{
   MFEM_FORALL(i, n, ddata[i] = data[i];);
}

// *****************************************************************************
__kernel void kAddMult_a_VVt(const size_t n, const double a,
                             const double *v, const size_t height, double *VVt)
{
   MFEM_FORALL(i, n,
   {
      double avi = a * v[i];
      for (size_t j = 0; j < i; j++)
      {
         double avivj = avi * v[j];
         VVt[i+j*height] += avivj;
         VVt[j+i*height] += avivj;
      }
      VVt[i+i*height] += avi * v[i];
   });
}

// *****************************************************************************
__kernel void kMultWidth0(const size_t height, double *y)
{
   MFEM_FORALL(row, height, y[row] = 0.0;);
}

// *****************************************************************************
__kernel void kMult(const size_t height, const size_t width,
                    const double *data, const double *x, double *y)
{
   MFEM_FORALL(i, height,
   {
      double sum = 0.0;
      for (size_t j=0; j<width; j+=1)
      {
         sum += x[j]*data[i+j*height];
      }
      y[i] = sum;
   });
}

// *****************************************************************************
__kernel void kMult(const size_t ah, const size_t aw, const size_t bw,
                    const double *bd, const double *cd, double *ad)
{
   MFEM_FORALL(i, ah*aw, ad[i] = 0.0;);
   MFEM_FORALL(j, aw,
   {
      for (size_t k = 0; k < bw; k++)
      {
         for (size_t i = 0; i < ah; i++)
         {
            ad[i+j*ah] += bd[i+k*ah] * cd[k+j*bw];
         }
      }
   });
}

// *****************************************************************************
__kernel void kOpEq(const size_t hw, const double *m, double *data)
{
   MFEM_FORALL(i, hw, data[i] = m[i];);
}

// *****************************************************************************
__kernel void kDiag(const size_t n, const size_t N, const double c,
		    double *data)
{
   MFEM_FORALL(i, N, data[i] = 0.0;);
   MFEM_FORALL(i, n, data[i*(n+1)] = c;);
}

// *****************************************************************************
double kDet2(const double *data)
{
   MFEM_GPU_CANNOT_PASS;
   GET_ADRS(data);
   return d_data[0] * d_data[3] - d_data[1] * d_data[2];
}

// *****************************************************************************
double kDet3(const double *data)
{
   MFEM_GPU_CANNOT_PASS;
   GET_ADRS(data);
   return
      d_data[0] * (d_data[4] * d_data[8] - d_data[5] * d_data[7]) +
      d_data[3] * (d_data[2] * d_data[7] - d_data[1] * d_data[8]) +
      d_data[6] * (d_data[1] * d_data[5] - d_data[2] * d_data[4]);
}

// *****************************************************************************
double kFNormMax(const size_t hw, const double *data)
{
   MFEM_GPU_CANNOT_PASS;
   GET_ADRS(data);
   double max_norm = 0.0;
   for (size_t i = 0; i < hw; i++)
   {
      const double entry = fabs(d_data[i]);
      if (entry > max_norm)
      {
         max_norm = entry;
      }
   }
   return max_norm;
}

// *****************************************************************************
double kFNorm2(const size_t hw, const double max_norm, const double *data)
{
   MFEM_GPU_CANNOT_PASS;
   GET_ADRS(data);
   double fnorm2 = 0.0;
   for (size_t i = 0; i < hw; i++)
   {
      const double entry = d_data[i] / max_norm;
      fnorm2 += entry * entry;
   }
   return fnorm2;
}

// *****************************************************************************
__kernel void kCalcInverse2D(const double t, const double *a, double *inva)
{
   //MFEM_GPU_CANNOT_PASS;
   inva[0+2*0] =  a[1+2*1] * t ;
   inva[0+2*1] = -a[0+2*1] * t ;
   inva[1+2*0] = -a[1+2*0] * t ;
   inva[1+2*1] =  a[0+2*0] * t ;
}

// *****************************************************************************
__kernel void kCalcInverse3D(const double t, const double *a, double *inva)
{
   //MFEM_GPU_CANNOT_PASS;
   inva[0+3*0] = (a[1+3*1]*a[2+3*2]-a[1+3*2]*a[2+3*1])*t;
   inva[0+3*1] = (a[0+3*2]*a[2+3*1]-a[0+3*1]*a[2+3*2])*t;
   inva[0+3*2] = (a[0+3*1]*a[1+3*2]-a[0+3*2]*a[1+3*1])*t;

   inva[1+3*0] = (a[1+3*2]*a[2+3*0]-a[1+3*0]*a[2+3*2])*t;
   inva[1+3*1] = (a[0+3*0]*a[2+3*2]-a[0+3*2]*a[2+3*0])*t;
   inva[1+3*2] = (a[0+3*2]*a[1+3*0]-a[0+3*0]*a[1+3*2])*t;

   inva[2+3*0] = (a[1+3*0]*a[2+3*1]-a[1+3*1]*a[2+3*0])*t;
   inva[2+3*1] = (a[0+3*1]*a[2+3*0]-a[0+3*0]*a[2+3*1])*t;
   inva[2+3*2] = (a[0+3*0]*a[1+3*1]-a[0+3*1]*a[1+3*0])*t;
}

} // namespace mfem