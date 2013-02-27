//============================================================================
// Name        : MGR_NTL.cpp
// Author      : Dusan Klinec (ph4r05)
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C, Ansi-style
//============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <cstdlib>
#include <ctime>

// NTL dependencies
#include <NTL/GF2.h>
#include <NTL/mat_GF2.h>
#include <NTL/vec_long.h>
#include <NTL/new.h>
#include <math.h>
NTL_CLIENT

// field size
#define FSIZE 2
// block matrix default size
#define QSIZE 4

// prototypes / forward declarations
long generateInvertiblePM(mat_GF2& M, int p);
void AddToCol(mat_GF2& x, long j, const vec_GF2& a);
int initMatrix(mat_GF2& M, long *data);
long invP(ref_GF2 d, mat_GF2& X, mat_GF2& Q, const mat_GF2& A);

using namespace std;
using namespace NTL;
int main(void) {
	long i;
	puts("Hello World!!!");
	cout << "Wazzup?" <<endl;

	// very poor PRNG seeding, but just for now
	srand((unsigned)time(0));

	// sample matrix stuff
	mat_GF2 A;
	A.SetDims(QSIZE, QSIZE);
	cout << "I will show you a nice matrix: " << endl << A << endl << endl;

	// now generate invertible matrix
	i = generateInvertiblePM(A, QSIZE);
	if (i>=0){
		cout << "found invertible matrix in [" << i << "] iterations: " << endl << A << endl << endl;
	} else {
		cout << "Invertible matrix was not found" << endl;
	}

	// Now try bigger matrix - mixing bijection 8x8
	i = generateInvertiblePM(A, 8);
	if (i>=0){
		cout << "found invertible matrix in [" << i << "] iterations: " << endl << A << endl << endl;
	} else {
		cout << "Invertible matrix was not found" << endl;
	}

	// now try our Gauss method returning modification matrix P
	mat_GF2 B;
	B.SetDims(QSIZE, QSIZE);
	long const Bdata[][4] = {
	  {1, 0, 1, 0},
	  {1, 1, 0, 0},
	  {0, 0, 1, 1},
	  {0, 0, 1, 0}
	};

	initMatrix(B, (long *)Bdata);
	cout << "My custom init matrix: " << endl << B << endl << endl;
	cout << "Now trying to find inverse: " << endl;
	GF2 d;
	ref_GF2 dd(d);
	mat_GF2 P;
	mat_GF2 Q;
	invP(dd,P,Q,B);
	cout << "Determinant returned: " << d;
	cout << "; P matrix: " << endl << P << endl << endl;
	cout << "; Q matrix: " << endl << Q << endl << endl;
	cout << "; R matrix: " << endl << (P*B*Q) << endl << endl;


	return EXIT_SUCCESS;
}

/**
 * Initializes matrix from specified array.
 * Data must be at least dimension of given matrix.
 */
int initMatrix(mat_GF2& M, long *data){
	long i,j,n,m;
	for(i=0, n=M.NumRows(); i<n; i++){
		for(j=0, m=M.NumCols(); j<m; j++){
			M.put(i,j,data[n*i+j]);
		}
	}

	return 0;
}

/**
 * Generates random matrix of dimension pxp that is invertible in GF(2)
 */
long generateInvertiblePM(mat_GF2& M, int p){
	int rounds=0;
	long i, j;
	GF2 det;

	// Initialize M as square matrix pxp
	M.SetDims(p,p);

	// Iterate until we have some invertible matrix, or to some boundary.
	// Reaching this boundary is highly improbable for small p.
	for(rounds=0; rounds < 100; rounds++){
		// Fill matrix with random values and then compute determinant.
		for(i=0; i<p; i++){
			for(j=0; j<p; j++){
				M.put(i,j,rand()%2);
			}
		}

		// test for determinant. If determinant != 0 then matrix is non-singular, invertible
		determinant(det, M);
		if (det!=0){
			return rounds;
		}
	}

	return -1;
}

/**
 * Extended Inversion version - should return also P matrix in
 * matrix A decomposition PAQ = R where R is in canonical form.
 *
 * Returns rank of matrix
 */
long invP(ref_GF2 d, mat_GF2& X, mat_GF2& Q, const mat_GF2& A)
{
   long n = A.NumRows();
   if (A.NumCols() != n)
      Error("solve: nonsquare matrix");

   if (n == 0) {
      X.SetDims(0, 0);
      set(d);
   }

   long i, j, k, pos;
   long rank=n;

   //
   // Gauss Jordan Elimination. Matrix M is extended version
   // with 2*N columns. Copy of A is in the left half, unity
   // matrix is in the right half.
   //
   mat_GF2 M;
   M.SetDims(n, 2*n);

   vec_GF2 aa;
   aa.SetLength(2*n);

   // initializing Q matrix as unit matrix, will corespond to
   // column operations performed to obtain canonical form.
   // Since matrix is represented as array of vectors (rows),
   // we will work with transpose version of matrix.
   mat_GF2 I;
   ident(I, n);
   transpose(Q, I);
   for (i = 0; i < n; i++) {
      aa = A[i];
      aa.SetLength(2*n);
      aa.put(n+i, 1);
      M[i] = aa;
   }

   long wn = ((2*n) + NTL_BITS_PER_LONG - 1)/NTL_BITS_PER_LONG;

   for (k = 0; k < n; k++) {
      long wk = k/NTL_BITS_PER_LONG;
      long bk = k - wk*NTL_BITS_PER_LONG;
      _ntl_ulong k_mask = 1UL << bk;

      cout << "Intermediate result in step=" << k <<  "; Matrix" << endl << M << endl;

      // Find leading one in rows on k-th position in row.
      // Search in interval [k,n] (thus from current row down).
      pos = -1;
      for (i = k; i < n; i++) {
         if (M[i].rep.elts()[wk] & k_mask) {
            pos = i;
            break;
         }
      }

      cout << "Line pos: [" << pos << "] has leading 1 on [" << k << "]. position" << endl;
      if (pos == -1) {
		  // If here it means we have zero vector obtained,
		  // thus it is linearly dependent to others.
		  // Now we have to swap some columns to be able to continue
		  // in elimination.
		  //
		  // In this situation there is no row such that would have
		  // leading 1 on k-th place.
    	  //
    	  // Thus now look in rows [k,n] and find some row that has
    	  // 1 element on position > k.
    	  //
    	  // FInding unit element on position k+1 in all rows [k,n].
    	  // If fails, look for unit element on position k+2 in all rows [k,n]...
    	  int kk, ii, colpos=-1;
			for (kk = k+1; kk < n; kk++) {
				long kwk = kk / NTL_BITS_PER_LONG;
				long kbk = kk - kwk * NTL_BITS_PER_LONG;
				_ntl_ulong kk_mask = 1UL << kbk;
				colpos=kk;

				// Find leading one in rows on k-th position in row.
				// Search in interval [k,n] (thus from current row down).
				cout << "Looking for leading 1 element in column: " << kk << "; mask: " << kk_mask << endl;
				pos = -1;
				for (ii = k; ii < n; ii++) {
					if (M[ii].rep.elts()[kwk] & kk_mask) {
						cout << "Found in row: " << ii << endl;
						pos = ii;
						break;
					}
				}
				if (pos!=-1) break;
			}


			if (pos==-1){
				// No such column exists, thus just simply null rest of columns in Q matrix.
				rank = k;
				cout << "No such column exists, we are already in canonical form;"\
						"nulling all columns from: " << k << "; Rank: " << rank << endl;
				for(kk=k; kk<n; kk++){
					for(ii=0; ii<n; ii++){
						Q.put(kk, ii, 0);
					}
				}

				break;
			}

			// swaping columns kk with k.
			cout << "Swaping column [" << k << "] with column [" << colpos << "]. Matrix: " <<endl;

			// do column swap
			for(ii=0; ii<n; ii++){
				GF2 tmp = M.get(ii, k);
				M.put(ii, k, M.get(ii, colpos));
				M.put(ii, colpos, tmp);
			}

			// reflect this swap to Q matrix, swap rows (transpose form)
			swap(Q[colpos], Q[k]);
			cout << M << endl << "Qmatrix: " << endl << Q << endl << endl;

		   //clear(d);
		   //return;
		}

      if (pos != -1) {
    	  // row number <pos> has leading one on k-th position
         if (k != pos) {
        	 cout << "Swap line " << pos << " with line " << k << endl;
            swap(M[pos], M[k]);
         }

         // obtain bit representation of vector in i-th row
         _ntl_ulong *y = M[k].rep.elts();

         for (i = k+1; i < n; i++) {
            // M[i] = M[i] + M[k]*M[i,k]

        	 // By another words, we are re-seting other 1s
        	 // in rows > k (down).
            if (M[i].rep.elts()[wk] & k_mask) {
               _ntl_ulong *x = M[i].rep.elts();

               // simple element-by-element addition
               for (j = wk; j < wn; j++)
                  x[j] ^= y[j];
            }
         }
      }
   }

   vec_GF2 XX;
   XX.SetLength(2*n);

   X.SetDims(n, n);
   clear(X);

   for (j = 0; j < n; j++) {
      XX.SetLength(n+j+1);
      clear(XX);
      XX.put(n+j, to_GF2(1));

      for (i = n-1; i >= 0; i--) {
         XX.put(i, XX*M[i]);
      }

      XX.SetLength(n);
      AddToCol(X, j, XX);
   }

   // transpose Q matrix finally
   Q = transpose(Q);

   set(d);
   return rank;
}

void AddToCol(mat_GF2& x, long j, const vec_GF2& a)
// add a to column j of x
// ALIAS RESTRICTION: a should not alias any row of x
{
   long n = x.NumRows();
   long m = x.NumCols();

   if (a.length() != n || j < 0 || j >= m)
      Error("AddToCol: bad args");

   long wj = j/NTL_BITS_PER_LONG;
   long bj = j - wj*NTL_BITS_PER_LONG;
   _ntl_ulong j_mask = 1UL << bj;

   const _ntl_ulong *ap = a.rep.elts();
   _ntl_ulong a_mask = 1;

   long i;
   for (i = 0; i < n; i++) {
      if (*ap & a_mask)
         x[i].rep.elts()[wj] ^= j_mask;

      a_mask <<= 1;
      if (!a_mask) {
         a_mask = 1;
         ap++;
      }
   }
}

