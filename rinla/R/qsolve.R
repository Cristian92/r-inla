## Export: inla.qsolve

## ! \name{qsolve}
## ! \alias{inla.qsolve}
## ! \alias{qsolve}
## !
## ! \title{Solves linear SPD systems}
## !
## ! \description{This routine use the GMRFLib implementation
## !              to solve linear systems with a SPD matrix.}
## ! \usage{
## !     inla.qsolve(Q, B, reordering = inla.reorderings(), method = c("solve", "forward", "backward"))
## ! }
## !
## ! \arguments{
## !   \item{Q}{A SPD matrix,  either as a (dense) matrix or sparse-matrix}.
## !   \item{B}{The right hand side matrix, either as a (dense) matrix or  sparse-matrix.}
## !   \item{reordering}{The type of reordering algorithm to be used for \code{TAUCS};
## !        either one of the names listed in \code{inla.reorderings()}
## !        or the output from \code{inla.qreordering(Q)}.
## !        The default is "auto" which try several reordering
## !        algorithm and use the best one for this particular matrix (using the TAUCS library).}
## !   \item{method}{The system to solve, one of \code{"solve"},
## !                 \code{"forward"} or \code{"backward"}. Let \code{Q = L L^T},
## !                 where \code{L} is lower triangular
## !                 (the Cholesky triangle),  then \code{method="solve"} solves \code{L L^T X = B} or
## !                  equivalently \code{Q X = B},  \code{method="forward"} solves \code{L X = B},  and
## !                 \code{method="backward"} solves \code{L^T X  = B}. }
## !}
## !\value{
## !  \code{inla.qsolve} returns a matrix \code{X},
## !  which is the solution of \code{Q X = B},  \code{L X = B} or \code{L^T X = B}
## !  depending on the value of \code{method}.
## !}
## !\author{Havard Rue \email{hrue@r-inla.org}}
## !
## !\examples{
## ! n = 10
## ! nb <- n-1
## ! QQ = matrix(rnorm(n^2), n, n)
## ! QQ <- QQ \%*\% t(QQ)
## !
## ! Q = inla.as.sparse(QQ)
## ! B = matrix(rnorm(n*nb), n, nb)
## !
## ! X = inla.qsolve(Q, B, method = "solve")
## ! XX = inla.qsolve(Q, B, method = "solve", reordering = inla.qreordering(Q))
## ! print(paste("err solve1", sum(abs( Q \%*\% X - B))))
## ! print(paste("err solve2", sum(abs( Q \%*\% XX - B))))
## !
## ! ## the forward and backward solve is tricky, as after permutation and with Q=LL', then L is
## ! ## lower triangular, but L in the orginal ordering is not lower triangular. if the rhs is iid
## ! ## noise, this is not important. to control the reordering, then the 'taucs' library must be
## ! ## used.
## ! inla.setOption(smtp = 'taucs')
## !
## ! ## case 1. use the matrix as is, no reordering
## ! r <- "identity"
## ! L = t(chol(Q))
## ! X = inla.qsolve(Q, B, method = "forward", reordering = r)
## ! XX = inla.qsolve(Q, B, method = "backward", reordering = r)
## ! print(paste("err forward ", sum(abs(L \%*\% X - B))))
## ! print(paste("err backward", sum(abs(t(L) \%*\% XX - B))))
## !
## ! ## case 2. use a reordering from the library
## ! r <- inla.qreordering(Q)
## ! im <- r$ireordering
## ! m <- r$reordering
## ! print(cbind(idx = 1:n, m, im) )
## ! Qr <- Q[im, im]
## ! L = t(chol(Qr))[m, m]
## !
## ! X = inla.qsolve(Q, B, method = "forward", reordering = r)
## ! XX = inla.qsolve(Q, B, method = "backward", reordering = r)
## ! print(paste("err forward ", sum(abs( L \%*\% X - B))))
## ! print(paste("err backward", sum(abs( t(L) \%*\% XX - B))))
## !}

`inla.qsolve` <- function(Q, B, reordering = inla.reorderings(),
                          method = c("solve", "forward", "backward")) {
    t.dir <- inla.tempdir()
    smtp <- match.arg(inla.getOption("smtp"), c("taucs", "band", "default", "pardiso"))
    Q <- inla.sparse.check(Q)
    if (is(Q, "dgTMatrix")) {
        Qfile <- inla.write.fmesher.file(Q, filename = inla.tempfile(tmpdir = t.dir))
    } else if (is.character(Q)) {
        Qfile <- Q
    } else {
        stop("This should not happen.")
    }

    ## ensure B is a dense matrix if its a sparse
    if (is(B, "Matrix")) {
        B <- as.matrix(B)
    }
    if (is.matrix(B)) {
        Bfile <- inla.write.fmesher.file(B, filename = inla.tempfile(tmpdir = t.dir))
    } else if (is.character(B)) {
        Bfile <- B
    } else {
        stop("This should not happen.")
    }

    if (is.list(reordering)) {
        ## argument is the output from inla.qreordering()
        reordering <- reordering$name
    }
    reordering <- match.arg(reordering)
    method <- match.arg(method)

    Xfile <- inla.tempfile(tmpdir = t.dir)
    inla.set.sparselib.env(inla.dir = t.dir)
    if (inla.os("linux") || inla.os("mac")) {
        s <- system(paste(
            shQuote(inla.call.no.remote()), "-s -m qsolve", "-r",
            reordering, "-S", smtp, Qfile, Xfile, Bfile, method
        ), intern = TRUE)
    } else if (inla.os("windows")) {
        s <- system(paste(
            shQuote(inla.call.no.remote()), "-s -m qsolve", "-r",
            reordering, "-S", smtp, Qfile, Xfile, Bfile, method
        ), intern = TRUE)
    } else {
        stop("\n\tNot supported architecture.")
    }

    X <- inla.read.fmesher.file(Xfile)
    unlink(t.dir, recursive = TRUE)

    return(X)
}
