// Copyright (c) 2010, Lawrence Livermore National Security, LLC. Produced at
// the Lawrence Livermore National Laboratory. LLNL-CODE-443211. All Rights
// reserved. See file COPYRIGHT for details.
//
// This file is part of the MFEM library. For more information and source code
// availability see http://mfem.googlecode.com.
//
// MFEM is free software; you can redistribute it and/or modify it under the
// terms of the GNU Lesser General Public License (as published by the Free
// Software Foundation) version 2.1 dated February 1999.

#ifndef BLOCKMATRIX_HPP_
#define BLOCKMATRIX_HPP_

class BlockMatrix : public SparseRowMatrix {
public:
	//! Constructor
	/**
	 *  nRowBlocks: number of block rows
	 *  nColBlocks: number of block columns
	 */
	BlockMatrix(int nRowBlocks, int nColBlocks);
	//! Set A(i,j) = mat
	void SetBlock(int i, int j, SparseMatrix & mat);
	//! Return the number of row blocks
	int NumRowBlocks() const {return nRowBlocks; }
	//! Return the number of column blocks
	int NumColBlocks() const {return nColBlocks; }
	//! Return the total number of rows (Finalize must be called!)
	int NumRows() const {return is_filled ? row_offsets[nRowBlocks]:-1;}
	//! Return the total number of columns (Finalize must be called!)
	int NumCols() const {return is_filled ? col_offsets[nColBlocks]:-1;}
	//! Return a reference to block (i,j). Reference may be invalid if Aij(i,j) == NULL
	SparseMatrix & Block(int i, int j);
	//! Return a reference to block (i,j). Reference may be invalid if Aij(i,j) == NULL. (const version)
	const SparseMatrix & Block(int i, int j) const;
	//! Check if block (i,j) is a zero block.
	int IsZeroBlock(int i, int j) const {return (Aij(i,j)==NULL) ? 1 : 0; }
	//! Return the row offsets for block starts (Finalize must be called!)
	int * GetRowOffsets() { return row_offsets.GetData(); }
	//! Return the columns offsets for block starts (Finalize must be called!)
	int * GetColOffsets() { return col_offsets.GetData(); }
	//! Return the row offsets for block starts (const version)
	const int * GetRowOffsets() const { return row_offsets.GetData(); }
	//! Return the row offsets for block starts (const version)
	const int * GetColOffsets() const { return col_offsets.GetData(); }
	//! Return the number of non zeros in row i
	int RowSize(const int i) const;
	//! Symmetric elimination of the marked degree of freedom.
	/**
	 * ess_bc_dofs: marker of the degree of freedom to be eliminated
	 *              dof i is eliminated if ess_bc_dofs[i] = 1.
	 * sol: vector that stores the values of the degree of freedom that need to be eliminated
	 * rhs: vector that stores the rhs of the system.
	 */
	void EliminateRowCol(Array<int> & ess_bc_dofs, Vector & sol, Vector & rhs);
	//! Returns a monolithic CSR matrix that represents this operator.
	SparseMatrix * Monolithic();
	//! Export the monolithic matrix to file.
	void PrintMatlab(std::ostream & os = std::cout);

	//@{ Operator interface
	// Return the size (number of rows) of the BlockMatrix. (Finalize must be called first).
	int Size() const { return is_filled ? row_offsets[nRowBlocks]:-1;}
	//@}

	//@{ Matrix interface
	/// Returns reference to a_{ij}.  Index i, j = 0 .. size-1
	virtual double& Elem (int i, int j);
	/// Returns constant reference to a_{ij}.  Index i, j = 0 .. size-1
	virtual const double& Elem (int i, int j) const;
    /// Returns a pointer to (approximation) of the matrix inverse.
	virtual MatrixInverse * Inverse() const { mfem_error("BlockMatrix::Inverse not implemented \n"); return static_cast<MatrixInverse*>(NULL); }
	/// Finalize the matrix (no more blocks allowed).
	virtual void Finalize();
	//@}

	//@{ SparseRowMatrix interface
	//! Returns the width (total number of columns) of the matrix (Finalize must be called first)
	int Width() const {return is_filled ? col_offsets[nColBlocks]:-1;}
	//! Returns the total number of non zeros in the matrix (Finalize must be called first)
	virtual int NumNonZeroElems() const { if(!is_filled) mfem_error("BlockMatrix::NumNonZeroElems()"); return nnz_elem; }
	/// Gets the columns indexes and values for row *row*.
	/// The return value is always 0 since cols and srow are copies of the values in the matrix.
	int GetRow(const int row, Array<int> &cols, Vector &srow) const;
	/// If the matrix is square, it will place 1 on the diagonal (i,i) if row i has "almost" zero l1-norm.
	/*
	 * If entry (i,i) does not belong to the sparsity pattern of A, then a error will occur.
	 */
	void EliminateZeroRows();

	/// Matrix-Vector Multiplication y = A*x
	void Mult(const Vector & x, Vector & y) const;
	/// Matrix-Vector Multiplication y = y + val*A*x
	void AddMult(const Vector & x, Vector & y, const double val = 1.) const;
	/// MatrixTranspose-Vector Multiplication y = A'*x
	void MultTranspose(const Vector & x, Vector & y) const;
	/// MatrixTranspose-Vector Multiplication y = y + val*A'*x
	void AddMultTranspose(const Vector & x, Vector & y, const double val = 1.) const;
	//@}

	//! Destructor
	virtual ~BlockMatrix();
	//! if owns_blocks the SparseMatrix objects Aij will be deallocated.
	int owns_blocks;

private:

	//! Given a global row iglobal finds to which row iloc in block iblock belongs to.
	inline void findGlobalRow(int iglobal, int & iblock, int & iloc) const
	{
		if(!is_filled || iglobal > row_offsets[nRowBlocks])
			mfem_error("BlockMatrix::findGlobalRow");

		for(iblock = 0; iblock < nRowBlocks; ++iblock)
			if(row_offsets[iblock+1] > iglobal)
				break;

		iloc = iglobal - row_offsets[iblock];
	}

	//! Given a global column jglobal finds to which column jloc in block jblock belongs to.
	inline void findGlobalCol(int jglobal, int & jblock, int & jloc) const
	{
		if(!is_filled || jglobal > col_offsets[nColBlocks])
			mfem_error("BlockMatrix::findGlobalCol");

		for(jblock = 0; jblock < nColBlocks; ++jblock)
			if(col_offsets[jblock+1] > jglobal)
				break;

		jloc = jglobal - col_offsets[jblock];
	}

	//! Number of row blocks
	int nRowBlocks;
	//! Number of columns blocks
	int nColBlocks;
	//! row offsets for each block start (length nRowBlocks+1). This array is initialized in the Finalize() method.
	Array<int> row_offsets;
	//! column offsets for each block start (length nColBlocks+1). This array is initialized in the Finalize() method.
	Array<int> col_offsets;
	//! Total number of non-zeros element. nnz_elem is computed in the Finalize() method.
	int nnz_elem;
	//! True if Finalize() is called
	bool is_filled;
	//! 2D array that stores each block of the BlockMatrix. Aij(iblock, jblock) == NULL if block (iblock, jblock) is all zeros.
	Array2D<SparseMatrix *> Aij;
};

//! Transpose a BlockMatrix: result = A'
BlockMatrix * Transpose(const BlockMatrix & A);
//! Multiply BlockMatrix matrices: result = A*B
BlockMatrix * Mult(const BlockMatrix & A, const BlockMatrix & B);

#endif /* BLOCKMATRIX_HPP_ */
