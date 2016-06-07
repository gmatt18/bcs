// [[Rcpp::depends(RcppGSL)]]
// [[Rcpp::depends(RcppArmadillo)]]
#include <RcppArmadillo.h>
#include <RcppGSL.h>
#include <Rmath.h>
#include <RcppArmadilloExtensions/sample.h>
#include <math.h>
#include <stdlib.h>
#include <iostream>
#include <iomanip>
using namespace Rcpp;

//' @useDynLib bcs
//' @importFrom Rcpp sourceCpp

// [[Rcpp::export]]
arma::umat intersect(arma::umat first, arma::umat second){
  int m = first.n_rows;
  int n = second.n_rows;
  int size;
  if(m<n){
    size = n;
  }
  else{
    size = m;
  }
  int count = 0;
  arma::umat inter;
  inter.ones(size,2);
  for(int i=0; i<m; i++){
    double value = first(i);
    for(int j=0; j<n; j++){
      if(value==second(j)){
        inter(count,0) = value;
        inter(count,1) = j;
        count++;
        break;
      }
    }
  }
  if(count==0){
    arma::umat null_matrix;
    return null_matrix;
  }
  else{
    inter = inter.rows(0,count-1);
    arma::umat s_ind = sort_index(inter.col(0));
    return inter.rows(s_ind);
  }
}

// [[Rcpp::export]]
arma::umat setdiff(arma::umat first, arma::umat second){
  int m = first.n_rows;
  int n = second.n_rows;
  int size;
  if(m<n){
    size = n;
  }
  else{
    size = m;
  }
  arma::umat set_bool;
  int count = 0;
  arma::umat set_return;
  set_return.ones(size,1);
  for(int i=0; i<m; i++){
    set_bool = sum(any(second == first(i)),1);
    if(set_bool(0)==0){
      set_return(count) = first(i);
      count++;
    }
  }
  if(count==0){
    arma::umat null_answer;
    return null_answer;
  }
  else{
    return set_return.rows(0,count-1);
  }
}

//'   Implements the fast Laplace algorithm.
//'
//'   This code implements the fast Laplace algorithm from [1], which is based on
//'   the BCS code available from [2].
//'
//' @param PHI measurement matrix.
//' @param y CS measurements.
//' @param sigma2 Initial noise variance.
//' @param eta Threshold in determining convergence of marginal likelihood.
//' @param verbose Print to screen which basis are added, re-estimated, or deleted.
//' @return A list containing the following elements:
//' \tabular{lll}{
//'   \code{weights} \tab \tab sparse weights\cr
//'   \code{used} \tab \tab the positions of sparse weights\cr
//'   \code{sigma2} \tab \tab re-estimated noise variance\cr
//'   \code{errbars} \tab \tab one standard deviation around the sparse weights\cr
//'   \code{alpha} \tab \tab sparse hyperparameters (1/gamma)
//' }
//' @references [1] S. D. Babacan, R. Molina, A. K. Katsaggelos. "Bayesian Compressive
//' Sensing using Laplace Priors",
//' @references [2] S. Ji, Y. Xue, L. Carin, "Bayesian Compressive Sensing," IEEE Trans.
//' Signal Processing, vol. 56, no. 6, June 2008.
//' @references [3] M. Tipping and A. Faul, “Fast marginal likelihood maximisation
//' for sparse Bayesian models,” in Proc. 9th Int. Workshop Artificial Intelligence
//' and Statistics, C. M. Bishop and B. J. Frey, Eds., 2003.
//' @export
// [[Rcpp::export]]
List FastLaplace(arma::mat PHI, arma::vec y, double sigma2, double eta, bool verbose){
  double M = PHI.n_rows;
  double N = PHI.n_cols;

  // Calculates initial alpha, from equation (26) in [3]
  // Note: alpha = 1/gamma where gamma is the parameters used in [1]
  arma::mat PHIy = PHI.t()*y;
  arma::mat PHI2(sum(square(PHI),0)); // Square of 2-Norm of all basis
  PHI2 = PHI2.t();
  arma::mat ratio(square(PHIy)/PHI2);
  arma::uword index;
  double maxr = ratio.max(index); // Finds the best basis to start off with, see [3].
  arma::mat alpha; alpha.zeros(N,1);
  alpha(0,0) = PHI2(index)/(maxr-sigma2);

  // Computes initial Sig.
  arma::mat phi = PHI.col(index);
  arma::mat phitphi = (phi.t()*phi);
  arma::mat Sig; Sig << 1/(alpha(0) + phitphi(0)/sigma2); //Equation 25 of [1]

  // Computes initial mu.
  arma::mat mu; mu.zeros(N,1);
  mu(0,0) = Sig(0)*PHIy(index)/sigma2; //Equation 24 of [1]

  // Computes initial S and Q for ALL basis vectors.
  arma::mat left = (PHI.t()*phi)/sigma2;
  arma::mat S = PHI2/sigma2-Sig(0)*square(left); //Equation 51 of [1]
  arma::mat Q = PHIy/sigma2-Sig(0)*PHIy(index)/sigma2*left; //Equation 52 of [1]

  // Indices that are deleted.
  arma::umat deleted; deleted.zeros(N,1);

  int max_it = 1000;
  // This will later be changed if the algorithm converges before reaching max_it.
  int final_count = max_it;
  // Initializes the Maximum Likelihood array. It will later be truncated to the
  // actual number of iterations.
  arma::rowvec ML(max_it);
  arma::umat indices; indices.zeros(N,1);
  indices(0,0) = index;

  arma::mat delta;
  arma::mat ki;
  arma::mat mui;
  arma::mat Sigi;
  arma::mat Alpha;
  // Vector of the possible basis indices.
  arma::uvec range_mat = arma::linspace<arma::uvec>(0,N-1,N);
  arma::umat range_m = arma::umat(range_mat);
  // Keeps track of which index to add basis. Starts at 1 since we added above.
  arma::uword add_count = 1;
  // Keeps track of how many deleted basis there are in deleted vector
  arma::uword delete_count = 0;
  for(int count=0; count<max_it; count++){
    //***************** First calculate all of the alphas **********************
    // For the alphas that are infinity.
    arma::mat s = S;
    arma::mat q = Q;
    // For the alphas that are not infinity
    //Equation 53 in [1]
    s(indices.head_rows(add_count)) = alpha.head_rows(add_count)%S(indices.head_rows(add_count))/
                                      (alpha.head_rows(add_count)-S(indices.head_rows(add_count)));
    //Equation 54 in [1]
    q(indices.head_rows(add_count)) = alpha.head_rows(add_count)%Q(indices.head_rows(add_count))/
                                      (alpha.head_rows(add_count)-S(indices.head_rows(add_count)));

    //Equation 35 in [1]
    double lambda = 2*(add_count - 1)/sum(sum(1/alpha.head_rows(add_count)));

    arma::mat A = lambda + s - square(q);
    arma::mat B = 2*lambda*s + square(s);
    arma::mat C = lambda*square(s);

    arma::mat theta = square(q)-s;
    arma::mat discriminant = square(B) - 4*A%C;
    // Sometimes the discriminant can get values that are essentially 0 but are
    // technically negative, therefore outputting nan's in the sqrt expression
    // below in calculating nextAphas. The next line therefore forces these to
    // 0.
    discriminant.elem(find(discriminant<0)).zeros();
    arma::mat nextAlphas = (-B - sqrt(discriminant) ) / (2*A);

    // Chooses the next alpha that maximizes marginal likelihood.
    double inf = 1.0/0.0;
    arma::mat Ones; Ones.ones(N);
    arma::mat ml = -inf*Ones;

    // Finds the indices of the alphas that are not infinity.
    arma::umat ig0 = find(theta>lambda);
    arma::umat ire = ::intersect(ig0,indices.head_rows(add_count));
    // These are the indices of where the values appear in the vector "indices" above.
//     if(count==5){
//       test1 = ig0;
//       test2 = ire;
//       return List::create(Named("test1",test1),
//                           Named("test2",test2));
//     }
    arma::umat which = ire.col(1);
    // Indices for re-estimation.
    ire = ire.col(0);

    // If there are coefficients to re-estimate.
    if(ire.n_rows>0){
      Alpha = nextAlphas(ire);
      // We are subtracting off the marginal likelihood of already calc. alpha
      ml(ire) = pow(q(ire),2)/(Alpha + s(ire)) + log(Alpha/(Alpha + s(ire))) -
                lambda/Alpha - pow(q(ire),2)/(alpha(which) + s(ire)) -
                log(alpha(which)/(alpha(which) + s(ire))) + lambda/alpha(which);
    }

    // Indices for adding.
    arma::umat iad = ::setdiff(ig0,ire);

    if(iad.n_rows>0){
      Alpha = nextAlphas(iad);
      ml(iad) = log(Alpha/(Alpha + s(iad))) + pow(q(iad),2)/(Alpha + s(iad)) - lambda/Alpha;
      which = ::intersect(deleted.head_rows(delete_count),iad);
      // Makes sure the deleted basis stay deleted
      if(which.n_rows>0){
        which = which.col(0);
        ml(which).fill(-inf);
      }
    }

    arma::umat is0 = ::setdiff(range_m,ig0);

    // Indices for deleting.
    arma::umat ide = ::intersect(is0,indices.head_rows(add_count));

    if(ide.n_cols>0){
      which = ide.col(1);
      ide = ide.col(0);
      if(add_count==1){
        ml(ide).fill(-inf);
      }
      else{
        // TODO: Not sure why we don't just put that this equals -inf here
        ml(ide) = -pow(q(ide),2)/(alpha(which) + s(ide)) -
          log(alpha(which)/(alpha(which) + s(ide))) + lambda/alpha(which);
      }
    }

    // Finds the max of the marginal likelihood.
    arma::uword idx;
    arma::mat round_ml = round(ml*pow(10,7))/pow(10,7);
    // TODO: When comparing this with matlab code it is only accurate to within
    // seven decimal places, then you start getting little errors
    ML(count) = round_ml.max(idx);

    // Checks convergence.
    if(count>1){
      if(std::abs(ML(count)-ML(count-1)) < std::abs(ML(count)-ML(0))*eta){
        final_count = count + 1;
        break;
      }
    }

    // Finds any basis that needs to be re-estimated.
    arma::uvec w = find(indices.head_rows(add_count)==idx);
    // The update formulas below can be found in the appendix of [3].
//     if(count==8){
//       Rcout << theta(idx) << " " << lambda << "\n";
//       Rcout << round_ml << "\n";
//     }
    if(theta(idx) > lambda){
      if(w.n_elem>0){  // Re-estimates basis.
        Alpha = nextAlphas(idx);
        arma::mat Sigii = Sig(w,w);
        mui = mu(w);
        Sigi = Sig.cols(w);
        delta = Alpha-alpha(w);
        ki = delta/(1+Sigii*delta);
        mu.head_rows(add_count) -= ki(0)*mui(0)*Sigi;

        Sig -= ki(0)*Sigi*Sigi.t();
        arma::mat comm = PHI.t()*(phi*Sigi)/sigma2;
        S += ki(0)*square(comm);
        Q += ki(0)*mui(0)*comm;
        alpha(w) = Alpha;

        if(verbose==TRUE){
          Rcout << "Reestimate " << idx << "\n";
        }
      }
      else{  // Add basis.
        Alpha = nextAlphas(idx);
        arma::mat phii = PHI.col(idx);
        double Sigii = 1/(Alpha(0)+S(idx));
        mui = Sigii*Q(idx);
        arma::mat comm1 = Sig*(phi.t()*phii)/sigma2;
        arma::mat ei = phii-phi*comm1;
        arma::mat off = -Sigii*comm1;
        // get dimensions
        arma::uword offN = off.n_cols;
        arma::uword sigM = Sig.n_rows;
        arma::uword sigN = Sig.n_cols;
        arma::mat newSig;
        newSig.zeros((sigM+offN),(sigN+offN)); //TODO: Is off missing sigma2?
        newSig.submat(0,0,(sigM-1),(sigN-1)) = Sig+Sigii*comm1*comm1.t();
        newSig.submat(0,sigN,(sigM-1),(sigN+offN-1)) = off;
        newSig.submat(sigM,0,(sigM+offN-1),(sigN-1)) = off.t();
        newSig.submat((sigM+offN-1),(sigN+offN-1),(sigM+offN-1),(sigN+offN-1)) = Sigii;
        Sig = newSig;
        mu.head_rows(add_count) -= mui(0)*comm1;
        mu(add_count,0) = mui(0);
        arma::mat comm2 = PHI.t()*ei/sigma2;
        S -= Sigii*pow(comm2,2);
        Q -= mui(0)*comm2;
        indices(add_count,0) = idx;
        alpha(add_count,0) = Alpha(0);
        phi = join_rows(phi,phii);
        add_count++;
        if(verbose==TRUE){
          Rcout << "Add " << idx << "\n";
        }
      }
    }
    else{
      if(w.n_elem > 0 & add_count > 1){ // Deletes basis.
        deleted(delete_count,0) = idx;
        delete_count++;
        arma::mat Sigii = Sig(w,w);
        mui = mu(w);
        Sigi = Sig.cols(w);
        Sig -= Sigi*Sigi.t()/Sigii(0);
        Sig.shed_col(w(0));
        Sig.shed_row(w(0));
        mu.head_rows(add_count) -= mui(0)*Sigi/Sigii(0);
        mu.shed_row(w(0));
        arma::mat comm = PHI.t()*(phi*Sigi)/sigma2;
        S += pow(comm,2)/Sigii(0);
        Q += mui(0)/Sigii(0)*comm;
        indices.shed_row(w(0));
        indices.insert_rows(N-1,1);
        alpha.shed_row(w(0));
        alpha.insert_rows(N-1,1);
        phi.shed_col(w(0));
        add_count -= 1;
        if(verbose==TRUE){
          Rcout << "Delete " << idx << "\n";
        }
      }
      else if(w.n_elem>0 & add_count==1){
        // Something is wrong, trying to delete the only coefficient that has been added.
        break;
      }
    }
  } // end of for loop

    arma::mat weights = mu.head_rows(add_count);
    //  Add 1 to put indices in terms of R syntax
    arma::umat used = indices.head_rows(add_count)+1;
    // Re-estimates sigma2
    arma::mat sigma2_re = sum(pow((y-phi*mu.head_rows(add_count)),2))/
    (M-add_count+alpha.head_rows(add_count).t()*Sig.diag());
    arma::mat errbars = sqrt(Sig.diag());

    if(verbose==TRUE){
      Rcout << "Algorithm converged, # iterations : " << final_count <<  "\n";
    }
  return List::create(Named("weights",weights),Named("used",used),
                      Named("sigma2",sigma2_re),Named("errbars",errbars),
                      Named("alpha",alpha.head_rows(add_count)));

}

