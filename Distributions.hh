#include "gmd.h"
#include "lavd.h"

typedef LaGenMatDouble Matrix;
typedef LaVectorDouble Vector;


class PDF {
public:

  /* The feature dimensionality */
  int dim() { return m_dim; };

  /* Different training modes */
  enum EstimationMode { ML, MMI };
  
  /* The likelihood of the current feature given this model */
  virtual double compute_likelihood(const FeatureVec &f) const = 0;
  /* The log likelihood of the current feature given this model */
  virtual double compute_log_likelihood(const FeatureVec &f) const = 0;
  /* Write the parameters of this distribution to the stream os */
  virtual void write(std::ostream &os) const = 0;
  /* Read the parameters of this distribution from the stream is */
  virtual void read(const std::istream &is) = 0;

  /* Set the current estimation mode */
  void set_estimation_mode(const EstimationMode &m) { m_mode = m; };
  /* Get the current estimation mode */
  EstimationMode &get_estimation_mode() { return m_mode; };

private:
  Gaussian::EstimationMode m_mode;
  int m_dim;
}


class Gaussian : public PDF {
public:
  
  // ABSTRACT FUNCTIONS, SHOULD BE OVERWRITTEN IN THE GAUSSIAN IMPLEMENTATIONS

  /* Constructor */
  virtual Gaussian() = 0;
  /* Copy constructor */
  virtual Gaussian(const Gaussian &g) = 0;
  /* Destructor */
  virtual ~Gaussian() = 0;
  /* Resets the Gaussian to have dimensionality dim and all values zeroed */
  virtual void reset(int dim) = 0;
  /* Initializes the accumulator buffers */  
  virtual void start_accumulating() = 0;
  /* Accumulates the maximum likelihood statistics for the Gaussian
     weighed with a prior. */
  virtual void accumulate_ml(double prior, const FeatureVec &f) = 0;
  /* Accumulates the maximum mutual information denominator statistics
     weighed with priors. The numerator statistics should be accumulated
     using the accumulate_ml function */
  virtual void accumulate_mmi_denominator(std::vector<double> priors,
					  std::vector<const FeatureVec*> const features) = 0;
  /* Use the accumulated statistics to update the current model parameters.
     Empties the accumulators */
  virtual void estimate_parameters() = 0;
  /* Returns the mean vector for this Gaussian */
  virtual void get_mean(Vector &mean) const = 0;
  /* Returns the covariance matrix for this Gaussian */
  virtual void get_covariance(Matrix &covariance) const = 0;
  /* Sets the mean vector for this Gaussian */
  virtual void set_mean(const Vector &mean) = 0;
  /* Sets the covariance matrix for this Gaussian */
  virtual void set_covariance(const Matrix &covariance) = 0;
  
  // THESE FUNCTIONS HAVE ALSO A COMMON IMPLEMANTATION, BUT CAN BE OVERWRITTEN

  /* Splits the current Gaussian to two by disturbing the mean */
  virtual void split(Gaussian &s1, Gaussian &s2) const;
  /* Sets the parameters for the current Gaussian by merging m1 and m2 */
  virtual void merge(const Gaussian &m1, const Gaussian &m2);
  /* Sets the parameters for the current Gaussian by merging m and the current one */
  virtual void merge(Gaussian &m);
  /* Compute the Kullback-Leibler divergence KL(current||g) */
  virtual double kullback_leibler(Gaussian &g) const;
}


class DiagonalAccumulator {
public:
  DiagonalAccumulator(int dim) { 
    ml_mean.resize(dim);
    ml_cov.resize(dim);
    mmi_mean.resize(dim);
    mmi_cov.resize(dim);
    ml_mean=0;
    mmi_mean=0;
    ml_cov=0;
    mmi_cov=0;
  }
  Vector ml_mean;
  Vector mmi_mean;
  Vector ml_cov;
  Vector mmi_cov;
}


class DiagonalGaussian : public Gaussian {
public:
  DiagonalGaussian(int dim);
  DiagonalGaussian(const DiagonalGaussian &g);
  ~DiagonalGaussian();
  virtual void reset(int dim);

  // From pdf
  virtual double compute_likelihood(const FeatureVec &f);
  virtual double compute_log_likelihood(const FeatureVec &f);
  virtual void write(std::ostream &os);
  virtual void read(const std::istream &is);

  // Gaussian-specific
  virtual void start_accumulating();
  virtual void accumulate_ml(double prior,
			     const FeatureVec &f);
  virtual void accumulate_mmi_denominator(std::vector<double> priors,
					  std::vector<const FeatureVec*> const features);
  virtual void estimate_parameters();
  virtual void get_mean(Vector &mean);
  virtual void get_covariance(Matrix &covariance);
  virtual void set_mean(const Vector &mean);
  virtual void set_covariance(const Matrix &covariance);

  // Diagonal-specific
  /* Get the diagonal of the covariance matrix */
  void get_covariance(Vector &covariance);
  /* Set the diagonal of the covariance matrix */
  void set_covariance(const Vector &covariance);

private:
  Vector m_mean;
  Vector m_covariance;
  Vector m_precision;

  DiagonalAccumulator *m_accum;
}


class FullCovarianceAccumulator {
public:
  DiagonalAccumulator(int dim) { 
    ml_mean.resize(dim);
    ml_cov.resize(dim,dim);
    mmi_mean.resize(dim);
    mmi_cov.resize(dim,dim);
    outer.resize(dim,dim);
    ml_mean=0;
    mmi_mean=0;
    ml_cov=0;
    mmi_cov=0;
    outer=0;
  }
  Vector ml_mean;
  Vector mmi_mean;
  Matrix ml_cov;
  Matrix mmi_cov;
  Matrix outer;
}


class FullCovarianceGaussian : public Gaussian {
public:
  FullCovarianceGaussian(int dim);
  ~FullCovarianceGaussian();
  virtual void reset(int dim);

  // From pdf
  virtual double compute_likelihood(const FeatureVec &f);
  virtual double compute_log_likelihood(const FeatureVec &f);
  virtual void write(std::ostream &os);
  virtual void read(const std::istream &is);

  // Gaussian-specific
  virtual void start_accumulating();
  virtual void accumulate_ml(double prior,
			     const FeatureVec &f);
  virtual void accumulate_mmi_denominator(std::vector<double> priors,
					  std::vector<const FeatureVec*> const features);
  virtual void estimate_parameters();
  virtual void get_mean(Vector &mean);
  virtual void get_covariance(Matrix &covariance);
  virtual void set_mean(const Vector &mean);
  virtual void set_covariance(const Matrix &covariance);
  
private:
  double m_determinant;
  double m_constant;

  // Parameters
  Vector m_mean;
  Matrix m_covariance;

  FullCovarianceAccumulator *m_accum;
}


class PrecisionConstrainedGaussian : public Gaussian {
public:
  PrecisionConstrainedGaussian(int dim);
  ~PrecisionConstrainedGaussian();
  virtual void reset(int dim);

  // From pdf
  double compute_likelihood(const FeatureVec &f);
  double compute_log_likelihood(const FeatureVec &f);
  void write(std::ostream &os);
  void read(const std::istream &is);
  
  // Gaussian-specific
  virtual void start_accumulating();
  virtual void accumulate_ml(double prior, const FeatureVec &f);
  virtual void accumulate_mmi_denominator(std::vector<double> priors,
					  std::vector<const FeatureVec*> const features);
  virtual void estimate_parameters();
  virtual vector &get_mean();
  virtual Matrix &get_covariance();
  virtual void set_mean(vector &mean);
  virtual void set_covariance(Matrix &covariance);

  // PCGMM-specific
  /* Get the coefficients for the subspace constrained precision matrix */
  vector &get_precision_coeffs();
  /* Set the coefficients for the subspace constrained precision matrix */
  void set_precision_coeffs(vector &coeffs);

private:
  double determinant;
  Vector mean;
  Vector precision_coeffs;
  PrecisionSubspace &ps;
}


class SubspaceConstrainedGaussian : public Gaussian {
public:
  SubspaceConstrainedGaussian(int dim);
  ~SubspaceConstrainedGaussian();
  virtual void reset(int dim);

  // From pdf
  virtual double compute_likelihood(const FeatureVec &f);
  virtual double compute_log_likelihood(const FeatureVec &f);
  virtual void write(std::ostream &os);
  virtual void read(const std::istream &is);

  // Gaussian-specific
  virtual void start_accumulating();
  virtual void accumulate_ml(double prior, const FeatureVec &f);
  virtual void accumulate_mmi_denominator(std::vector<double> priors const,
					  std::vector<const FeatureVec*> const features);
  virtual void estimate_parameters();
  virtual vector &get_mean();
  virtual Matrix &get_covariance;
  virtual void set_mean(vector &mean);
  virtual void set_covariance(Matrix &covariance);

  // SCGMM-specific
  /* Get the coefficients for the subspace constrained exponential parameters */
  vector &get_subspace_coeffs();
  /* Set the coefficients for the subspace constrained exponential parameters */
  void set_subspace_coeffs(vector &coeffs);

private:
  Vector subspace_coeffs;
  ExponentialSubspace &es;
}


class Mixture : public PDF {
public:
  // Mixture-specific
  Mixture(PDFPool &pool);
  ~Mixture();
  void reset();
  /* Set the mixture components, clear existing mixture */
  void set_components(const std::vector<int> &pointers,
		      const std::vector<double> &weights);
  /* Get the mixture components */
  void get_components(std::vector<int> &pointers,
		      std::vector<double> &weights);
  /* Add one new component to the mixture. 
     Doesn't normalize the coefficients in between */
  void add_component(int pool_index, double weight);
  /* Normalize the weights to have a sum of 1 */
  void normalize_weights();

  // From pdf
  virtual double compute_likelihood(const FeatureVec &f);
  virtual double compute_log_likelihood(const FeatureVec &f);
  virtual void write(std::ostream &os);
  virtual void read(const std::istream &is);

private:
  std::vector<int> m_pointers;
  std::vector<double> m_weights;
  PDFPool &m_pp;
}


class PDFPool {
public:

  PDFPool(int dim) { m_dim=dim };
  ~PDFPool();

  /* The dimensionality of the distributions in this pool */
  int dim() { return m_dim; };

  /* Get the pdf from the position index */
  PDF* get_pdf(int index);
  /* Set the pdf in the position index */
  void set_pdf(int index, PDF *pdf);

  /* Read the distributions from a .gk -file */
  void read_gk(const std::string &filename);
  /* Write the distributions to a .gk -file */
  void write_gk(const std::string &filename);

  /* Compute all likelihoods to the cache */
  void cache_likelihood(const FeatureVec &f);
  /* Compute likelihood of one pdf to the cache */
  void cache_likelihood(const FeatureVec &f, int index);
  /* Compute likelihoods of pdfs given in indices to the cache */
  void cache_likelihood(const FeatureVec &f, std::vector<int> indices);
  /* Instant computation of pdf at position index */
  double compute_likelihood(const FeatureVec &f, int index);
  /* Get the likelihood from the cache */
  double get_likelihood(int index);
  
private:
  std::vector<PDF*> m_pool;
  std::vector<double> m_likelihoods;
  int m_dim;
}
