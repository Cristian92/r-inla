%%
%% WARNING! DO NOT EDIT!
%% This file is automatically generated from agaussian.R
%%
 \name{aggregate.gaussian}
 \alias{inla.agaussian}
 \alias{aggregate.gaussian}
 \alias{agaussian}

 \title{Aggregate Gaussian into an equivalent observation}

 \description{Aggreagate Gaussians observerd with the same mean and precision,
              into an eqv triplet, for use with \code{family="agaussian"}}

 \usage{
     inla.agaussian(y, s = NULL)
 }

 \arguments{
   \item{y}{Repeated observations. If \code{y} is a matrix,  then each row represents
            repeated observations. if \code{y} is a list,  then each element of the list is
            a vector of repeated observations. If \code{y} is a vector,  then the whole vector
            represents repeated observations. The optional scaling \code{s},  must have 
            the same format as \code{y},  ie \code{matrix} or \code{vector}.
            \code{NA}'s in \code{y} (and \code{s}) are removed and not used or counted.
            If \code{s} is given, then the \code{NA}-pattern in \code{y} and \code{s} must be the same.}
   \item{s}{Optinonal fixed scaling of the precisions. Must be in the same format as \code{y},  and
            have the same \code{NA}-pattern. See the documentation for details.}
  }

 \value{
   The output is a \code{inla.mdata}-object ready for use
   with \code{family="agaussian"}. See the example in the documentation.
 }

 \author{Havard Rue \email{hrue@r-inla.org}}

 \examples{
  A = matrix(1:25,5,5)
  inla.agaussian(A)

  A[1,-1] = NA
  A[2,-(2:3)] = NA
  inla.agaussian(A)
 }
