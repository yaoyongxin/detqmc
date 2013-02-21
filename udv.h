/*
 * udv.h
 *
 *  Created on: Feb 12, 2013
 *      Author: gerlach
 */

#ifndef UDV_H_
#define UDV_H_

#include <complex>
#pragma GCC diagnostic ignored "-Weffc++"
#pragma GCC diagnostic ignored "-Wconversion"
#include <armadillo>
#pragma GCC diagnostic warning "-Weffc++"
#pragma GCC diagnostic warning "-Wconversion"


//matrices used in the computation of B-matrices decomposed into
//(U,d,V) = (orthogonal matrix, diagonal matrix elements, row-normalized triangular matrix.
//Initialize at beginning of simulation by member function setupUdVStorage()
template <typename num>
struct UdV {
	arma::Mat<num> U;
	arma::Col<num> d;
	arma::Mat<num> V;
	//default constructor: leaves everything empty
	UdV() : U(), d(), V() {}
	//specify matrix size: initialize to identity
	UdV(unsigned size) :
		U(arma::eye(size,size)), d(arma::ones(size)), V(arma::eye(size,size))
	{ }
};

template<> inline
UdV<std::complex<double>>::UdV(unsigned size) :
	U(arma::eye(size,size), arma::zeros(size,size)),
	d(arma::ones(size), arma::zeros(size,size)),
	V(arma::eye(size,size), arma::zeros(size,size))
{ }

template <typename num>
UdV<num> udvDecompose(const arma::Mat<num>& mat) {
	typedef UdV<num> UdV;
	UdV result;

//	ArmaMat V_transpose;
//	arma::svd(result.U, result.d, V_transpose, mat, "standard");
//	result.V = V_transpose.t();			//potentially it may be advisable to not do this generally

	arma::qr(result.U, result.V, mat);
	//normalize rows of V to obtain scales in d:
	result.d = arma::Col<num>(mat.n_rows);
	for (unsigned rown = 0; rown < mat.n_rows; ++rown) {
		const num norm = arma::norm(result.V.row(rown), 2);
		result.d[rown] = norm;
		result.V.row(rown) /= norm;
	}

	return result;
}






#endif /* UDV_H_ */
