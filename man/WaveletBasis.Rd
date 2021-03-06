% Generated by roxygen2: do not edit by hand
% Please edit documentation in R/basis.R
\name{WaveletBasis}
\alias{WaveletBasis}
\title{Finds the Discrete Wavelet Transform Matrix}
\usage{
WaveletBasis(N, train = NULL, wavelet = "Haar")
}
\arguments{
\item{N}{number of wavelet basis functions to include in matrix. Note that
 N must be a
power of 2, otherwise the matrix will include NA's. The reason for this has
to do with how the wavelet basis is defined.}

\item{train}{indices corresponding to which rows of the matrix to keep.
Default is to keep all rows.}

\item{wavelet}{the type of wavelet basis to use. See
\code{\link[wmtsa]{wavDaubechies}} from \code{wmtsa} for types.}
}
\value{
A P x N discrete wavelet transform matrix, where P is equal to the
       length of \code{train} and N is the number of basis. If \code{train}
       is \code{NULL} then P equals N.
}
\description{
Uses the functions \code{\link[wmtsa]{wavDWT}} and
\code{\link[wmtsa]{reconstruct}} from the \code{wmtsa} package to find the
transformation matrix of the given wavelet basis type. Each column of the
matrix is a wavelet basis function.
}
\examples{
# Find first 8 basis functions of the Haar wavelet type
w.Haar <- WaveletBasis(8)

# Find first 8 basis functions of the d4 wavelet type, keeping the first
# half of the rows
w.d4 <- WaveletBasis(8, 1:4, wavelet='d4')

}

