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

// Implementation of Bilinear Form Integrators for
// BilinearFormOperator.

#include "fem.hpp"
#include <cmath>
#include <algorithm>

namespace mfem
{

static void ComputeBasis1d(const FiniteElement *fe,
                           const TensorBasisElement *tfe, int ir_order,
                           DenseMatrix &shape1d)
{
   // Compute the 1d shape functions and gradients
   const Poly_1D::Basis &basis1d = tfe->GetBasis1D();
   const IntegrationRule &ir1d = IntRules.Get(Geometry::SEGMENT, ir_order);

   const int quads1d = ir1d.GetNPoints();
   const int dofs = fe->GetOrder() + 1;

   shape1d.SetSize(dofs, quads1d);

   Vector u(dofs);
   for (int k = 0; k < quads1d; k++)
   {
      const IntegrationPoint &ip = ir1d.IntPoint(k);
      basis1d.Eval(ip.x, u);
      for (int i = 0; i < dofs; i++)
      {
         shape1d(i, k) = u(i);
      }
   }
}

static void ComputeBasis1d(const FiniteElement *fe,
                           const TensorBasisElement *tfe, int ir_order,
                           DenseMatrix &shape1d, DenseMatrix &dshape1d)
{
   // Compute the 1d shape functions and gradients
   const Poly_1D::Basis &basis1d = tfe->GetBasis1D();
   const IntegrationRule &ir1d = IntRules.Get(Geometry::SEGMENT, ir_order);

   const int quads1d = ir1d.GetNPoints();
   const int dofs = fe->GetOrder() + 1;

   shape1d.SetSize(dofs, quads1d);
   dshape1d.SetSize(dofs, quads1d);

   Vector u(dofs);
   Vector d(dofs);
   for (int k = 0; k < quads1d; k++)
   {
      const IntegrationPoint &ip = ir1d.IntPoint(k);
      basis1d.Eval(ip.x, u, d);
      for (int i = 0; i < dofs; i++)
      {
         shape1d(i, k) = u(i);
         dshape1d(i, k) = d(i);
      }
   }
}

void PADiffusionIntegrator::ComputePA(const int ir_order)
{
   // Get the corresponding tensor basis element
   const TensorBasisElement *tfe = dynamic_cast<const TensorBasisElement*>(fe);

   // Store the 1d shape functions and gradients
   ComputeBasis1d(fes->GetFE(0), tfe, ir_order, shape1d, dshape1d);

   // Create the operator
   const int elems   = fes->GetNE();
   const int dim     = fe->GetDim();
   const int quads   = IntRule->GetNPoints();
   const int entries = dim * (dim + 1) / 2;
   Dtensor.SetSize(entries, quads, elems);

   DenseMatrix invdfdx(dim, dim);
   DenseMatrix mat(dim, dim);
   DenseMatrix cmat(dim, dim);

   for (int e = 0; e < fes->GetNE(); e++)
   {
      ElementTransformation *Tr = fes->GetElementTransformation(e);
      DenseMatrix &Dmat = Dtensor(e);
      for (int k = 0; k < quads; k++)
      {
         const IntegrationPoint &ip = IntRule->IntPoint(k);
         Tr->SetIntPoint(&ip);
         const DenseMatrix &temp = Tr->AdjugateJacobian();
         MultABt(temp, temp, mat);
         mat *= ip.weight / Tr->Weight();

         if (coeff != NULL)
         {
            const double c = coeff->Eval(*Tr, ip);
            for (int j = 0, l = 0; j < dim; j++)
               for (int i = j; i < dim; i++, l++)
               {
                  Dmat(l, k) = c * mat(i, j);
               }

         }
         else if (mcoeff != NULL)
         {
            mcoeff->Eval(cmat, *Tr, ip);
            for (int j = 0, l = 0; j < dim; j++)
               for (int i = j; i < dim; i++, l++)
               {
                  Dmat(l, k) = cmat(i, j) * mat(i, j);
               }

         }
         else
         {
            for (int j = 0, l = 0; j < dim; j++)
               for (int i = j; i < dim; i++, l++)
               {
                  Dmat(l, k) = mat(i, j);
               }
         }

      }
   }
}

PADiffusionIntegrator::PADiffusionIntegrator(
   FiniteElementSpace *_fes, const int ir_order)
   : BilinearFormIntegrator(&IntRules.Get(_fes->GetFE(0)->GetGeomType(), ir_order)),
     fes(_fes),
     fe(fes->GetFE(0)),
     dim(fe->GetDim()),
     vdim(fes->GetVDim()),
     coeff(NULL),
     mcoeff(NULL) { ComputePA(ir_order); }


PADiffusionIntegrator::PADiffusionIntegrator(
   FiniteElementSpace *_fes, const int ir_order, Coefficient &_coeff)
   : BilinearFormIntegrator(&IntRules.Get(_fes->GetFE(0)->GetGeomType(), ir_order)),
     fes(_fes),
     fe(fes->GetFE(0)),
     dim(fe->GetDim()),
     vdim(fes->GetVDim()),
     coeff(&_coeff),
     mcoeff(NULL) { ComputePA(ir_order); }


PADiffusionIntegrator::PADiffusionIntegrator(
   FiniteElementSpace *_fes, const int ir_order, MatrixCoefficient &_mcoeff)
   : BilinearFormIntegrator(&IntRules.Get(_fes->GetFE(0)->GetGeomType(), ir_order)),
     fes(_fes),
     fe(fes->GetFE(0)),
     dim(fe->GetDim()),
     vdim(fes->GetVDim()),
     coeff(NULL),
     mcoeff(&_mcoeff) { ComputePA(ir_order); }

void PADiffusionIntegrator::MultSeg(const Vector &V, Vector &U)
{
   const int dofs1d = shape1d.Height();
   const int quads1d = shape1d.Width();
   const int quads = quads1d;

   Vector Q(quads1d);

   int offset = 0;
   for (int e = 0; e < fes->GetNE(); ++e)
   {
      for (int vd = 0; vd < vdim; ++vd)
      {
         const Vector Vmat(V.GetData() + offset, dofs1d);
         Vector Umat(U.GetData() + offset, dofs1d);

         // Q_k1 = dshape_j1_k1 * V_i1
         dshape1d.MultTranspose(Vmat, Q);

         double *data_q = Q.GetData();
         const double *data_d = Dtensor(e).GetData();
         for (int k = 0; k < quads; ++k)
         {
            data_q[k] *= data_d[k];
         }

         // Q_k1 = dshape_j1_k1 * Q_k1
         dshape1d.AddMult(Q, Umat);

         // increment offset into E-vectors.
         offset += dofs1d;
      }
   }
}


void PADiffusionIntegrator::MultQuad(const Vector &V, Vector &U)
{
   const int dim = 2;
   const int terms = dim*(dim+1)/2;

   const int dofs   = fe->GetDof();
   const int quads  = IntRule->GetNPoints();

   const int dofs1d = shape1d.Height();
   const int quads1d = shape1d.Width();

   DenseTensor QQ(quads1d, quads1d, dim);
   DenseMatrix DQ(dofs1d, quads1d);

   int offset = 0;
   for (int e = 0; e < fes->GetNE(); ++e)
   {
      for (int vd = 0; vd < vdim; ++vd)
      {
         const DenseMatrix Vmat(V.GetData() + offset, dofs1d, dofs1d);
         DenseMatrix Umat(U.GetData() + offset, dofs1d, dofs1d);

         // DQ_j2_k1   = E_j1_j2  * dshape_j1_k1 -- contract in x direction
         // QQ_0_k1_k2 = DQ_j2_k1 * shape_j2_k2  -- contract in y direction
         MultAtB(Vmat, dshape1d, DQ);
         MultAtB(DQ, shape1d, QQ(0));

         // DQ_j2_k1   = E_j1_j2  * shape_j1_k1  -- contract in x direction
         // QQ_1_k1_k2 = DQ_j2_k1 * dshape_j2_k2 -- contract in y direction
         MultAtB(Vmat, shape1d, DQ);
         MultAtB(DQ, dshape1d, QQ(1));

         // QQ_c_k1_k2 = Dmat_c_d_k1_k2 * QQ_d_k1_k2
         // NOTE: (k1, k2) = k -- 1d index over tensor product of quad points
         double *data_qq = QQ(0).GetData();
         const double *data_d = Dtensor(e).GetData();
         for (int k = 0; k < quads; ++k)
         {
            const double D00 = data_d[terms*k + 0];
            const double D01 = data_d[terms*k + 1];
            const double D11 = data_d[terms*k + 2];

            const double q0 = data_qq[0*quads + k];
            const double q1 = data_qq[1*quads + k];

            data_qq[0*quads + k] = D00 * q0 + D01 * q1;
            data_qq[1*quads + k] = D01 * q0 + D11 * q1;
         }

         // DQ_i2_k1   = shape_i2_k2  * QQ_0_k1_k2
         // U_i1_i2   += dshape_i1_k1 * DQ_i2_k1
         MultABt(shape1d, QQ(0), DQ);
         AddMultABt(dshape1d, DQ, Umat);

         // DQ_i2_k1   = dshape_i2_k2 * QQ_1_k1_k2
         // U_i1_i2   += shape_i1_k1  * DQ_i2_k1
         MultABt(dshape1d, QQ(1), DQ);
         AddMultABt(shape1d, DQ, Umat);

         // increment offset
         offset += dofs;
      }
   }
}

void PADiffusionIntegrator::MultHex(const Vector &V, Vector &U)
{
   const int dim = 3;
   const int terms = dim*(dim+1)/2;

   const int dofs   = fe->GetDof();
   const int quads  = IntRule->GetNPoints();

   const int dofs1d = shape1d.Height();
   const int quads1d = shape1d.Width();

   DenseMatrix Q(quads1d, dim);
   DenseTensor QQ(quads1d, quads1d, dim);

   Array<double> QQQmem(quads1d * quads1d * quads1d * dim);
   double *data_qqq = QQQmem.GetData();
   DenseTensor QQQ0(data_qqq + 0*quads, quads1d, quads1d, quads1d);
   DenseTensor QQQ1(data_qqq + 1*quads, quads1d, quads1d, quads1d);
   DenseTensor QQQ2(data_qqq + 2*quads, quads1d, quads1d, quads1d);

   int offset = 0;
   for (int e = 0; e < fes->GetNE(); ++e)
   {
      for (int vd = 0; vd < vdim; ++vd)
      {
         const DenseTensor Vmat(V.GetData() + offset, dofs1d, dofs1d, dofs1d);
         DenseTensor Umat(U.GetData() + offset, dofs1d, dofs1d, dofs1d);

         // QQQ_0_k1_k2_k3 = dshape_j1_k1 * shape_j2_k2  * shape_j3_k3  * Vmat_j1_j2_j3
         // QQQ_1_k1_k2_k3 = shape_j1_k1  * dshape_j2_k2 * shape_j3_k3  * Vmat_j1_j2_j3
         // QQQ_2_k1_k2_k3 = shape_j1_k1  * shape_j2_k2  * dshape_j3_k3 * Vmat_j1_j2_j3
         QQQ0 = 0.; QQQ1 = 0.; QQQ2 = 0.;
         for (int j3 = 0; j3 < dofs1d; ++j3)
         {
            QQ = 0.;
            for (int j2 = 0; j2 < dofs1d; ++j2)
            {
               Q = 0.;
               for (int j1 = 0; j1 < dofs1d; ++j1)
               {
                  for (int k1 = 0; k1 < quads1d; ++k1)
                  {
                     Q(k1, 0) += Vmat(j1, j2, j3) * dshape1d(j1, k1);
                     Q(k1, 1) += Vmat(j1, j2, j3) * shape1d(j1, k1);
                  }
               }
               for (int k2 = 0; k2 < quads1d; ++k2)
                  for (int k1 = 0; k1 < quads1d; ++k1)
                  {
                     QQ(k1, k2, 0) += Q(k1, 0) * shape1d(j2, k2);
                     QQ(k1, k2, 1) += Q(k1, 1) * dshape1d(j2, k2);
                     QQ(k1, k2, 2) += Q(k1, 1) * shape1d(j2, k2);
                  }
            }
            for (int k3 = 0; k3 < quads1d; ++k3)
               for (int k2 = 0; k2 < quads1d; ++k2)
                  for (int k1 = 0; k1 < quads1d; ++k1)
                  {
                     QQQ0(k1, k2, k3) += QQ(k1, k2, 0) * shape1d(j3, k3);
                     QQQ1(k1, k2, k3) += QQ(k1, k2, 1) * shape1d(j3, k3);
                     QQQ2(k1, k2, k3) += QQ(k1, k2, 2) * dshape1d(j3, k3);
                  }
         }

         // QQQ_c_k1_k2_k3 = Dmat_c_d_k1_k2_k3 * QQQ_d_k1_k2_k3
         // NOTE: (k1, k2, k3) = q -- 1d quad point index
         const double *data_d = Dtensor(e).GetData();
         for (int k = 0; k < quads; ++k)
         {
            const double D00 = data_d[terms*k + 0];
            const double D01 = data_d[terms*k + 1];
            const double D02 = data_d[terms*k + 2];
            const double D11 = data_d[terms*k + 3];
            const double D12 = data_d[terms*k + 4];
            const double D22 = data_d[terms*k + 5];

            const double q0 = data_qqq[0*quads + k];
            const double q1 = data_qqq[1*quads + k];
            const double q2 = data_qqq[2*quads + k];

            data_qqq[0*quads + k] = D00 * q0 + D01 * q1 + D02 * q2;
            data_qqq[1*quads + k] = D01 * q0 + D11 * q1 + D12 * q2;
            data_qqq[2*quads + k] = D02 * q0 + D12 * q1 + D22 * q2;
         }

         // Apply transpose of the first operator that takes V -> QQQd -- QQQd -> U
         for (int k3 = 0; k3 < quads1d; ++k3)
         {
            QQ = 0.;
            for (int k2 = 0; k2 < quads1d; ++k2)
            {
               Q = 0.;
               for (int k1 = 0; k1 < quads1d; ++k1)
               {
                  for (int i1 = 0; i1 < dofs1d; ++i1)
                  {
                     Q(i1, 0) += QQQ0(k1, k2, k3) * dshape1d(i1, k1);
                     Q(i1, 1) += QQQ1(k1, k2, k3) * shape1d(i1, k1);
                     Q(i1, 2) += QQQ2(k1, k2, k3) * shape1d(i1, k1);
                  }
               }
               for (int i2 = 0; i2 < dofs1d; ++i2)
                  for (int i1 = 0; i1 < dofs1d; ++i1)
                  {
                     QQ(i1, i2, 0) += Q(i1, 0) * shape1d(i2, k2);
                     QQ(i1, i2, 1) += Q(i1, 1) * dshape1d(i2, k2);
                     QQ(i1, i2, 2) += Q(i1, 2) * shape1d(i2, k2);
                  }
            }
            for (int i3 = 0; i3 < dofs1d; ++i3)
               for (int i2 = 0; i2 < dofs1d; ++i2)
                  for (int i1 = 0; i1 < dofs1d; ++i1)
                  {
                     Umat(i1, i2, i3) +=
                        QQ(i1, i2, 0) * shape1d(i3, k3) +
                        QQ(i1, i2, 1) * shape1d(i3, k3) +
                        QQ(i1, i2, 2) * dshape1d(i3, k3);
                  }
         }

         // increment offset
         offset += dofs;
      }
   }
}

void PADiffusionIntegrator::AssembleVector(const FiniteElementSpace &fes, const Vector &fun, Vector &vect)
{
   switch (dim)
   {
   case 1: MultSeg(fun, vect); break;
   case 2: MultQuad(fun, vect); break;
   case 3: MultHex(fun, vect); break;
   default: mfem_error("Not yet supported"); break;
   }
}

void PAMassIntegrator::ComputePA(const int ir_order)
{
   // Get the corresponding tensor basis element
   const TensorBasisElement *tfe = dynamic_cast<const TensorBasisElement*>(fe);

   ComputeBasis1d(fes->GetFE(0), tfe, ir_order, shape1d);

   // Create the operator
   const int nelem   = fes->GetNE();
   const int dim     = fe->GetDim();
   const int quads   = IntRule->GetNPoints();
   Dmat.SetSize(quads, nelem);

   DenseMatrix invdfdx(dim, dim);
   DenseMatrix mat(dim, dim);
   for (int e = 0; e < fes->GetNE(); e++)
   {
      ElementTransformation *Tr = fes->GetElementTransformation(e);
      for (int k = 0; k < quads; k++)
      {
         const IntegrationPoint &ip = IntRule->IntPoint(k);
         Tr->SetIntPoint(&ip);
         const double weight = ip.weight * Tr->Weight();
         Dmat(k, e) = (coeff == NULL) ? weight : coeff->Eval(*Tr, ip) * weight;
      }
   }
}

PAMassIntegrator::PAMassIntegrator(
   FiniteElementSpace *_fes, const int ir_order)
   : BilinearFormIntegrator(&IntRules.Get(_fes->GetFE(0)->GetGeomType(), ir_order)),
     fes(_fes),
     fe(fes->GetFE(0)),
     dim(fe->GetDim()),
     vdim(fes->GetVDim()),
     coeff(NULL) { ComputePA(ir_order); }

PAMassIntegrator::PAMassIntegrator(
   FiniteElementSpace *_fes, const int ir_order, Coefficient &_coeff)
   : BilinearFormIntegrator(&IntRules.Get(_fes->GetFE(0)->GetGeomType(), ir_order)),
     fes(_fes),
     fe(fes->GetFE(0)),
     dim(fe->GetDim()),
     vdim(fes->GetVDim()),
     coeff(&_coeff) { ComputePA(ir_order); }



void PAMassIntegrator::MultSeg(const Vector &V, Vector &U)
{
   const int dofs1d = shape1d.Height();
   const int quads1d = shape1d.Width();
   const int quads = quads1d;

   Vector Q(quads1d);

   int offset = 0;
   for (int e = 0; e < fes->GetNE(); ++e)
   {
      for (int vd = 0; vd < vdim; ++vd)
      {
         const Vector Vmat(V.GetData() + offset, dofs1d);
         Vector Umat(U.GetData() + offset, dofs1d);

         // Q_k1 = dshape_j1_k1 * V_i1
         shape1d.MultTranspose(Vmat, Q);

         double *data_q = Q.GetData();
         const double *data_d = Dmat.GetColumn(e);
         for (int k = 0; k < quads; ++k) { data_q[k] *= data_d[k]; }

         // Q_k1 = dshape_j1_k1 * Q_k1
         shape1d.AddMult(Q, Umat);

         // Increment offset into E-vectors.
         offset += dofs1d;
      }
   }
}

void PAMassIntegrator::MultQuad(const Vector &V, Vector &U)
{
   const int dofs   = fe->GetDof();
   const int quads  = IntRule->GetNPoints();

   const int dofs1d = shape1d.Height();
   const int quads1d = shape1d.Width();

   DenseMatrix QQ(quads1d, quads1d);
   DenseMatrix DQ(dofs1d, quads1d);

   int offset = 0;
   for (int e = 0; e < fes->GetNE(); ++e)
   {
      for (int vd = 0; vd < vdim; ++vd)
      {
         const DenseMatrix Vmat(V.GetData() + offset, dofs1d, dofs1d);
         DenseMatrix Umat(U.GetData() + offset, dofs1d, dofs1d);

         // DQ_j2_k1   = E_j1_j2  * dshape_j1_k1 -- contract in x direction
         // QQ_0_k1_k2 = DQ_j2_k1 * shape_j2_k2  -- contract in y direction
         MultAtB(Vmat, shape1d, DQ);
         MultAtB(DQ, shape1d, QQ);

         // QQ_c_k1_k2 = Dmat_c_d_k1_k2 * QQ_d_k1_k2
         // NOTE: (k1, k2) = k -- 1d index over tensor product of quad points
         double *data_qq = QQ.GetData();
         const double *data_d = Dmat.GetColumn(e);
         for (int k = 0; k < quads; ++k) { data_qq[k] *= data_d[k]; }

         // DQ_i2_k1   = shape_i2_k2  * QQ_0_k1_k2
         // U_i1_i2   += dshape_i1_k1 * DQ_i2_k1
         MultABt(shape1d, QQ, DQ);
         AddMultABt(shape1d, DQ, Umat);

         // increment offset
         offset += dofs;
      }
   }
}

void PAMassIntegrator::MultHex(const Vector &V, Vector &U)
{
   const int dofs   = fe->GetDof();
   const int quads  = IntRule->GetNPoints();

   const int dofs1d = shape1d.Height();
   const int quads1d = shape1d.Width();

   Vector Q(quads1d);
   DenseMatrix QQ(quads1d, quads1d);
   DenseTensor QQQ(quads1d, quads1d, quads1d);

   int offset = 0;
   for (int e = 0; e < fes->GetNE(); ++e)
   {
      for (int vd = 0; vd < vdim; ++vd)
      {
         const DenseTensor Vmat(V.GetData() + offset, dofs1d, dofs1d, dofs1d);
         DenseTensor Umat(U.GetData() + offset, dofs1d, dofs1d, dofs1d);

         // QQQ_k1_k2_k3 = shape_j1_k1 * shape_j2_k2  * shape_j3_k3  * Vmat_j1_j2_j3
         QQQ = 0.;
         for (int j3 = 0; j3 < dofs1d; ++j3)
         {
            QQ = 0.;
            for (int j2 = 0; j2 < dofs1d; ++j2)
            {
               Q = 0.;
               for (int j1 = 0; j1 < dofs1d; ++j1)
               {
                  for (int k1 = 0; k1 < quads1d; ++k1)
                  {
                     Q(k1) += Vmat(j1, j2, j3) * shape1d(j1, k1);
                  }
               }
               for (int k2 = 0; k2 < quads1d; ++k2)
                  for (int k1 = 0; k1 < quads1d; ++k1)
                  {
                     QQ(k1, k2) += Q(k1) * shape1d(j2, k2);
                  }
            }
            for (int k3 = 0; k3 < quads1d; ++k3)
               for (int k2 = 0; k2 < quads1d; ++k2)
                  for (int k1 = 0; k1 < quads1d; ++k1)
                  {
                     QQQ(k1, k2, k3) += QQ(k1, k2) * shape1d(j3, k3);
                  }
         }

         // QQQ_k1_k2_k3 = Dmat_k1_k2_k3 * QQQ_k1_k2_k3
         // NOTE: (k1, k2, k3) = q -- 1d quad point index
         double *data_qqq = QQQ.GetData(0);
         const double *data_d = Dmat.GetColumn(e);
         for (int k = 0; k < quads; ++k) { data_qqq[k] *= data_d[k]; }

         // Apply transpose of the first operator that takes V -> QQQ -- QQQ -> U
         for (int k3 = 0; k3 < quads1d; ++k3)
         {
            QQ = 0.;
            for (int k2 = 0; k2 < quads1d; ++k2)
            {
               Q = 0.;
               for (int k1 = 0; k1 < quads1d; ++k1)
               {
                  for (int i1 = 0; i1 < dofs1d; ++i1)
                  {
                     Q(i1) += QQQ(k1, k2, k3) * shape1d(i1, k1);
                  }
               }
               for (int i2 = 0; i2 < dofs1d; ++i2)
                  for (int i1 = 0; i1 < dofs1d; ++i1)
                  {
                     QQ(i1, i2) += Q(i1) * shape1d(i2, k2);
                  }
            }
            for (int i3 = 0; i3 < dofs1d; ++i3)
               for (int i2 = 0; i2 < dofs1d; ++i2)
                  for (int i1 = 0; i1 < dofs1d; ++i1)
                  {
                     Umat(i1, i2, i3) += shape1d(i3, k3) * QQ(i1, i2);
                  }
         }

         // increment offset
         offset += dofs;
      }
   }
}

void PAMassIntegrator::AssembleVector(const FiniteElementSpace &fespace,
                                      const Vector &fun, Vector &vect)
{
   // NOTE: fun and vect are E-vectors at this point
   MFEM_ASSERT(fespace.GetFE(0)->GetDim() == dim, "");

   switch (dim)
   {
   case 1: MultSeg(fun, vect); break;
   case 2: MultQuad(fun, vect); break;
   case 3: MultHex(fun, vect); break;
   default: mfem_error("Not yet supported"); break;
   }
}


}